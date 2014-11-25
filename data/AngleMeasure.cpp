/*
 * Copyright (C) 2009 Matthew Gates
 * Copyright (C) 2014 Georg Zotti
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.
 */

#include "StelUtils.hpp"
#include "StelProjector.hpp"
#include "StelPainter.hpp"
#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelFileMgr.hpp"
#include "StelLocaleMgr.hpp"
#include "StelModuleMgr.hpp"
#include "StelGui.hpp"
#include "StelGuiItems.hpp"
#include "StelVertexArray.hpp"
#include "AngleMeasure.hpp"
#include "AngleMeasureDialog.hpp"

#include <QDebug>
#include <QTimer>
#include <QPixmap>
#include <QtNetwork>
#include <QSettings>
#include <QKeyEvent>
#include <QMouseEvent>
#include <cmath>

//! This method is the one called automatically by the StelModuleMgr just
//! after loading the dynamic library
StelModule* AngleMeasureStelPluginInterface::getStelModule() const
{
	return new AngleMeasure();
}

StelPluginInfo AngleMeasureStelPluginInterface::getPluginInfo() const
{
	// Allow to load the resources when used as a static plugin
	Q_INIT_RESOURCE(AngleMeasure);

	StelPluginInfo info;
	info.id = "AngleMeasure";
	info.displayedName = N_("Angle Measure");
	info.authors = "Matthew Gates, Bogdan Marinov, Alexander Wolf, Georg Zotti";
	info.contact = "http://porpoisehead.net/";
	info.description = N_("Provides an angle measurement tool");
	info.version = ANGLEMEASURE_VERSION;
	return info;
}

AngleMeasure::AngleMeasure()
	: flagShowAngleMeasure(false)
	, dragging(false)
	, angle(0.)
	, flagUseDmsFormat(false)
	, flagShowPA(false)
	, flagShowEquatorial(false)
	, flagShowHorizontal(false)
	, flagShowHorizontalPA(false)
	, flagShowHorizontalStartSkylinked(false)
	, flagShowHorizontalEndSkylinked(false)
	, toolbarButton(NULL)
{
	setObjectName("AngleMeasure");
	font.setPixelSize(16);

	configDialog = new AngleMeasureDialog();
	conf = StelApp::getInstance().getSettings();

	messageTimer = new QTimer(this);
	messageTimer->setInterval(7000);
	messageTimer->setSingleShot(true);

	connect(messageTimer, SIGNAL(timeout()), this, SLOT(clearMessage()));
}

AngleMeasure::~AngleMeasure()
{
	delete configDialog;
}

bool AngleMeasure::configureGui(bool show)
{
	if (show)
		configDialog->setVisible(true);
	return true;
}

//! Determine which "layer" the plugin's drawing will happen on.
double AngleMeasure::getCallOrder(StelModuleActionName actionName) const
{
	if (actionName==StelModule::ActionDraw)
		return StelApp::getInstance().getModuleMgr().getModule("LandscapeMgr")->getCallOrder(actionName)+10.;
	if (actionName==StelModule::ActionHandleMouseClicks)
		return -11;
	return 0;
}

void AngleMeasure::init()
{
	if (!conf->childGroups().contains("AngleMeasure"))
		restoreDefaultSettings();

	loadSettings();

	startPoint.set(0.,0.,0.);
	endPoint.set(0.,0.,0.);
	perp1StartPoint.set(0.,0.,0.);
	perp1EndPoint.set(0.,0.,0.);
	perp2StartPoint.set(0.,0.,0.);
	perp2EndPoint.set(0.,0.,0.);
	startPointHor.set(0.,0.,0.);
	endPointHor.set(0.,0.,0.);
	perp1StartPointHor.set(0.,0.,0.);
	perp1EndPointHor.set(0.,0.,0.);
	perp2StartPointHor.set(0.,0.,0.);
	perp2EndPointHor.set(0.,0.,0.);

	StelApp& app = StelApp::getInstance();

	// Create action for enable/disable & hook up signals	
	addAction("actionShow_Angle_Measure", N_("Angle Measure"), N_("Angle measure"), "enabled", "Ctrl+A");

	// Initialize the message strings and make sure they are translated when
	// the language changes.
	updateMessageText();
	connect(&app, SIGNAL(languageChanged()), this, SLOT(updateMessageText()));

	// Add a toolbar button
	try
	{
		StelGui* gui = dynamic_cast<StelGui*>(app.getGui());
		if (gui!=NULL)
		{
			toolbarButton = new StelButton(NULL,
						       QPixmap(":/angleMeasure/bt_anglemeasure_on.png"),
						       QPixmap(":/angleMeasure/bt_anglemeasure_off.png"),
						       QPixmap(":/graphicGui/glow32x32.png"),
						       "actionShow_Angle_Measure");
			gui->getButtonBar()->addButton(toolbarButton, "065-pluginsGroup");
		}
	}
	catch (std::runtime_error& e)
	{
		qWarning() << "WARNING: unable create toolbar button for AngleMeasure plugin: " << e.what();
	}
}

void AngleMeasure::update(double deltaTime)
{
	messageFader.update((int)(deltaTime*1000));
	lineVisible.update((int)(deltaTime*1000));
	static StelCore *core=StelApp::getInstance().getCore();

	// if altAz endpoint linked to the rotating sky, move respective point(s)
	if (flagShowHorizontalStartSkylinked)
	{
		startPointHor = core->equinoxEquToAltAz(startPoint, StelCore::RefractionAuto);
		calculateEnds();
	}
	if (flagShowHorizontalEndSkylinked)
	{
		endPointHor = core->equinoxEquToAltAz(endPoint, StelCore::RefractionAuto);
		calculateEnds();
	}
}


void AngleMeasure::drawOne(StelCore *core, const StelCore::FrameType frameType, const StelCore::RefractionMode refractionMode, const Vec3f txtColor, const Vec3f lineColor)
{
	const StelProjectorP prj = core->getProjection(frameType, refractionMode);
	StelPainter painter(prj);
	painter.setFont(font);

	if (lineVisible.getInterstate() > 0.000001f)
	{
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		glEnable(GL_TEXTURE_2D);

		Vec3d xy;
		QString displayedText;
		if (frameType==StelCore::FrameEquinoxEqu)
		{
			if (prj->project(perp1EndPoint,xy))
			{
				painter.setColor(txtColor[0], txtColor[1], txtColor[2], lineVisible.getInterstate());
				if (flagShowPA)
					displayedText = QString("%1 (%2%3)").arg(calculateAngle(), messagePA, calculatePositionAngle(startPoint, endPoint));
				else
					displayedText = calculateAngle();
				painter.drawText(xy[0], xy[1], displayedText, 0, 15, 15);
			}
		}
		else
		{
			if (prj->project(perp1EndPointHor,xy))
			{
				painter.setColor(txtColor[0], txtColor[1], txtColor[2], lineVisible.getInterstate());
				if (flagShowHorizontalPA)
					displayedText = QString("%1 (%2%3)").arg(calculateAngle(true), messagePA, calculatePositionAngle(startPointHor, endPointHor));
				else
					displayedText = calculateAngle(true);
				painter.drawText(xy[0], xy[1], displayedText, 0, 15, -5);
			}
		}

		glDisable(GL_TEXTURE_2D);
		// OpenGL ES 2.0 doesn't have GL_LINE_SMOOTH
		// glEnable(GL_LINE_SMOOTH);
		glEnable(GL_BLEND);

		// main line is a great circle
		painter.setColor(lineColor[0], lineColor[1], lineColor[2], lineVisible.getInterstate());
		if (frameType==StelCore::FrameEquinoxEqu)
		{
			painter.drawGreatCircleArc(startPoint, endPoint, NULL);

			// End lines
			painter.drawGreatCircleArc(perp1StartPoint, perp1EndPoint, NULL);
			painter.drawGreatCircleArc(perp2StartPoint, perp2EndPoint, NULL);
		}
		else
		{
			painter.drawGreatCircleArc(startPointHor, endPointHor, NULL);

			// End lines
			painter.drawGreatCircleArc(perp1StartPointHor, perp1EndPointHor, NULL);
			painter.drawGreatCircleArc(perp2StartPointHor, perp2EndPointHor, NULL);
		}
	}
	if (messageFader.getInterstate() > 0.000001f)
	{
		painter.setColor(txtColor[0], txtColor[1], txtColor[2], messageFader.getInterstate());
		int x = 83;
		int y = 120;
		int ls = painter.getFontMetrics().lineSpacing();
		painter.drawText(x, y, messageEnabled);
		y -= ls;
		painter.drawText(x, y, messageLeftButton);
		y -= ls;
		painter.drawText(x, y, messageRightButton);
	}


}

//! Draw any parts on the screen which are for our module
void AngleMeasure::draw(StelCore* core)
{
	if (lineVisible.getInterstate() < 0.000001f && messageFader.getInterstate() < 0.000001f)
		return;
	if (flagShowHorizontal)
	{
		drawOne(core, StelCore::FrameAltAz, StelCore::RefractionOff, horTextColor, horLineColor);
	}
	if (flagShowEquatorial)
	{
		drawOne(core, StelCore::FrameEquinoxEqu, StelCore::RefractionAuto, textColor, lineColor);
	}
}

QString AngleMeasure::calculatePositionAngle(const Vec3d p1, const Vec3d p2) const
{
	double y = cos(p2.latitude())*sin(p2.longitude()-p1.longitude());
	double x = cos(p1.latitude())*sin(p2.latitude()) - sin(p1.latitude())*cos(p2.latitude())*cos(p2.longitude()-p1.longitude());
	double r = std::atan2(y,x);
	// GZ ATAN2 makes many tests unnecessary...
//	if (x<0)
//		r += M_PI;
//	if (y<0)
//		r += 2*M_PI;
//	if (r>(2*M_PI))
//		r -= 2*M_PI;
	if (r<0)
		r+= 2*M_PI;

	unsigned int d, m;
	double s;
	bool sign;
	StelUtils::radToDms(r, sign, d, m, s);
	if (flagUseDmsFormat)
		return QString("%1d %2m %3s").arg(d).arg(m).arg(s, 0, 'f', 2);
	else
		return QString("%1%2 %3' %4\"").arg(d).arg(QChar(0x00B0)).arg(m).arg(s, 0, 'f', 2);
}

void AngleMeasure::handleKeys(QKeyEvent* event)
{
	event->setAccepted(false);
}

void AngleMeasure::handleMouseClicks(class QMouseEvent* event)
{
	if (!flagShowAngleMeasure)
	{
		event->setAccepted(false);
		return;
	}

	if (event->type()==QEvent::MouseButtonPress && event->button()==Qt::LeftButton)
	{
		const StelProjectorP prj = StelApp::getInstance().getCore()->getProjection(StelCore::FrameEquinoxEqu);
		prj->unProject(event->x(),event->y(),startPoint);
		const StelProjectorP prjHor = StelApp::getInstance().getCore()->getProjection(StelCore::FrameAltAz, StelCore::RefractionOff);
		prjHor->unProject(event->x(),event->y(),startPointHor);


		// first click reset the line... only draw it after we've dragged a little.
		if (!dragging)
		{
			lineVisible = false;
			endPoint = startPoint;
			endPointHor=startPointHor;
		}
		else
			lineVisible = true;

		dragging = true;
		calculateEnds();
		event->setAccepted(true);
		return;
	}
	else if (event->type()==QEvent::MouseButtonRelease && event->button()==Qt::LeftButton)
	{
		dragging = false;
		calculateEnds();
		event->setAccepted(true);
		return;
	}
	else if (event->type()==QEvent::MouseButtonPress && event->button()==Qt::RightButton)
	{
		const StelProjectorP prj = StelApp::getInstance().getCore()->getProjection(StelCore::FrameEquinoxEqu);
		prj->unProject(event->x(),event->y(),endPoint);
		const StelProjectorP prjHor = StelApp::getInstance().getCore()->getProjection(StelCore::FrameAltAz, StelCore::RefractionOff);
		prjHor->unProject(event->x(),event->y(),endPointHor);
		calculateEnds();
		event->setAccepted(true);
		return;
	}
	event->setAccepted(false);
}

bool AngleMeasure::handleMouseMoves(int x, int y, Qt::MouseButtons)
{
	if (dragging)
	{
		const StelProjectorP prj = StelApp::getInstance().getCore()->getProjection(StelCore::FrameEquinoxEqu);
		prj->unProject(x,y,endPoint);
		const StelProjectorP prjHor = StelApp::getInstance().getCore()->getProjection(StelCore::FrameAltAz, StelCore::RefractionOff);
		prjHor->unProject(x,y,endPointHor);
		calculateEnds();
		lineVisible = true;
		return true;
	}
	else
		return false;
}

void AngleMeasure::calculateEnds(void)
{
	if (flagShowEquatorial)
		calculateEndsOneLine(startPoint,    endPoint,    perp1StartPoint,    perp1EndPoint,    perp2StartPoint,    perp2EndPoint,    angle);
	if (flagShowHorizontal)
		calculateEndsOneLine(startPointHor, endPointHor, perp1StartPointHor, perp1EndPointHor, perp2StartPointHor, perp2EndPointHor, angleHor);
}

void AngleMeasure::calculateEndsOneLine(const Vec3d start, const Vec3d end, Vec3d &perp1Start, Vec3d &perp1End, Vec3d &perp2Start, Vec3d &perp2End, double &angle)
{
	Vec3d v0 = end - start;
	Vec3d v1 = Vec3d(0,0,0) - start;
	Vec3d p = v0 ^ v1;
	p *= 0.08;  // end width
	perp1Start.set(start[0]-p[0],start[1]-p[1],start[2]-p[2]);
	perp1End.set(start[0]+p[0],start[1]+p[1],start[2]+p[2]);

	v1 = Vec3d(0,0,0) - end;
	p = v0 ^ v1;
	p *= 0.08;  // end width
	perp2Start.set(end[0]-p[0],end[1]-p[1],end[2]-p[2]);
	perp2End.set(end[0]+p[0],end[1]+p[1],end[2]+p[2]);

	angle = start.angle(end);
}

// GZ Misnomer! should be called formatAngleString()
QString AngleMeasure::calculateAngle(bool horizontal) const
{
	unsigned int d, m;
	double s;
	bool sign;

	if (horizontal)
		StelUtils::radToDms(angleHor, sign, d, m, s);
	else
		StelUtils::radToDms(angle, sign, d, m, s);
	if (flagUseDmsFormat)
		return QString("%1d %2m %3s").arg(d).arg(m).arg(s, 0, 'f', 2);
	else
		return QString("%1%2 %3' %4\"").arg(d).arg(QChar(0x00B0)).arg(m).arg(s, 0, 'f', 2);
}

void AngleMeasure::enableAngleMeasure(bool b)
{
	flagShowAngleMeasure = b;
	lineVisible = b;
	messageFader = b;
	if (b)
	{
		//qDebug() << "AngleMeasure::enableAngleMeasure starting timer";
		messageTimer->start();
	}
}

void AngleMeasure::showPositionAngle(bool b)
{
	flagShowPA = b;
}

void AngleMeasure::showPositionAngleHor(bool b)
{
	flagShowHorizontalPA = b;
}
void AngleMeasure::showEquatorial(bool b)
{
	flagShowEquatorial = b;
}
void AngleMeasure::showHorizontal(bool b)
{
	flagShowHorizontal = b;
}
void AngleMeasure::showHorizontalStartSkylinked(bool b)
{
	flagShowHorizontalStartSkylinked = b;
}
void AngleMeasure::showHorizontalEndSkylinked(bool b)
{
	flagShowHorizontalEndSkylinked = b;
}
void AngleMeasure::useDmsFormat(bool b)
{
	flagUseDmsFormat=b;
}

void AngleMeasure::updateMessageText()
{
	// TRANSLATORS: instructions for using the AngleMeasure plugin.
	messageEnabled = q_("The Angle Measure is enabled:");
	// TRANSLATORS: instructions for using the AngleMeasure plugin.
	messageLeftButton = q_("Drag with the left button to measure, left-click to clear.");
	// TRANSLATORS: instructions for using the AngleMeasure plugin.
	messageRightButton = q_("Right-clicking changes the end point only.");
	// TRANSLATORS: PA is abbreviation for phrase "Position Angle"
	messagePA = q_("PA=");
}

void AngleMeasure::clearMessage()
{
	//qDebug() << "AngleMeasure::clearMessage";
	messageFader = false;
}

void AngleMeasure::restoreDefaultSettings()
{
	Q_ASSERT(conf);
	// Remove the old values...
	conf->remove("AngleMeasure");
	// ...load the default values...
	loadSettings();
	// ...and then save them.
	saveSettings();
	// But this doesn't save the colors, so:
	conf->beginGroup("AngleMeasure");
	conf->setValue("text_color", "0,0.5,1");
	conf->setValue("line_color", "0,0.5,1");
	conf->setValue("text_color_horizontal", "0.9,0.6,0.4");
	conf->setValue("line_color_horizontal", "0.9,0.6,0.4");
	conf->endGroup();
}

void AngleMeasure::loadSettings()
{
	conf->beginGroup("AngleMeasure");

	useDmsFormat(conf->value("angle_format_dms", false).toBool());
	showPositionAngle(conf->value("show_position_angle", false).toBool());
	textColor = StelUtils::strToVec3f(conf->value("text_color", "0,0.5,1").toString());
	lineColor = StelUtils::strToVec3f(conf->value("line_color", "0,0.5,1").toString());
	horTextColor = StelUtils::strToVec3f(conf->value("text_color_horizontal", "0.9,0.6,0.4").toString());
	horLineColor = StelUtils::strToVec3f(conf->value("line_color_horizontal", "0.9,0.6,0.4").toString());
	showPositionAngleHor(conf->value("show_position_angle_horizontal", false).toBool());
	flagShowEquatorial = conf->value("show_equatorial", true).toBool();
	flagShowHorizontal = conf->value("show_horizontal", false).toBool();
	flagShowHorizontalStartSkylinked = conf->value("link_horizontal_start_to_sky", false).toBool();
	flagShowHorizontalEndSkylinked = conf->value("link_horizontal_end_to_sky", false).toBool();

	conf->endGroup();
}

void AngleMeasure::saveSettings()
{
	conf->beginGroup("AngleMeasure");

	conf->setValue("angle_format_dms", isDmsFormat());
	conf->setValue("show_position_angle", isPaDisplayed());
	conf->setValue("show_position_angle_horizontal", isHorPaDisplayed());
	conf->setValue("show_equatorial", isEquatorial());
	conf->setValue("show_horizontal", isHorizontal());
	conf->setValue("link_horizontal_start_to_sky", isHorizontalStartSkylinked());
	conf->setValue("link_horizontal_end_to_sky", isHorizontalEndSkylinked());

	conf->endGroup();
}