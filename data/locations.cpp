/**************************************************************

location_dlg.cpp (C-Munipack project)
The 'Location's coordinates' dialog
Copyright (C) 2009 David Motl, dmotl@volny.cz

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

**************************************************************/

#include <stdlib.h>
#include <string.h>
#include <glib/gstdio.h>

#include "locations.h"
#include "utils.h"

//-----------------------------   TABLE OF LOCATIONS   ------------------------

//
// Constructor
//
CLocations::CLocations():m_State(0), m_Level(0), m_Str(NULL)
{
	m_List = gtk_list_store_new(LOCATION_NCOLS, G_TYPE_STRING, G_TYPE_STRING, 
		G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(m_List), 
		LOCATION_NAME, GTK_SORT_ASCENDING);
}

//
// Destructor
//
CLocations::~CLocations()
{
	if (m_Str)
		g_string_free(m_Str, true);
	g_object_unref(m_List);
}


//
// Load from file
//
void CLocations::Load(void)
{
	size_t bytes;
	char buf[4096];
	GMarkupParser parser;
	GMarkupParseContext *context;

	memset(&parser, 0, sizeof(GMarkupParser));
	parser.start_element = StartElement;
	parser.end_element = EndElement;
	parser.text = CharacterData;

	gtk_list_store_clear(m_List);
	m_State = m_Level = 0;
	if (m_Str) {
		g_string_free(m_Str, true);
		m_Str = NULL;
	}

	gchar *filepath = g_build_filename(get_user_config_dir(), "locations.xml", NULL);
	FILE *f = open_file(filepath, "rb");
	if (f) {
		context = g_markup_parse_context_new(&parser, (GMarkupParseFlags)0, this, NULL);
		while ((bytes=fread(buf, 1, 4096, f))>0) {
			if (!g_markup_parse_context_parse(context, buf, bytes, NULL))
				break;
		}
		g_markup_parse_context_end_parse(context, NULL);
		g_markup_parse_context_free(context);
		fclose(f);
	}
	g_free(filepath);

	if (m_Str) {
		g_string_free(m_Str, true);
		m_Str = NULL;
	}
}


// 
// Start element handler
//
void CLocations::StartElement(GMarkupParseContext *context, const gchar *element_name, 
	const gchar **attribute_names, const gchar **attribute_values, gpointer user_data, 
	GError **error)
{
	CLocations *pMe = (CLocations*)user_data;

	switch (pMe->m_State)
	{
	case 0:
		if (pMe->m_Level==0 && strcmp(element_name, "locations")==0) 
			pMe->m_State = 1;
		else
			pMe->m_State = 9;
		break;

	case 1:
		if (pMe->m_Level==1 && strcmp(element_name, "location")==0) {
			pMe->m_Tmp.Clear();
			pMe->m_State = 2;
		} else
			pMe->m_State = 8;
		break;

	case 2:
		if (pMe->m_Level==2) {
			if (pMe->m_Str)
				g_string_free(pMe->m_Str, true);
			pMe->m_Str = g_string_new("");
			pMe->m_State = 3;
		} else
			pMe->m_State = 7;
		break;
	}
	pMe->m_Level++;
}


//
// XML end element
//
void CLocations::EndElement(GMarkupParseContext *context,
		const gchar *element_name, gpointer user_data, GError **error)
{
	CLocations *pMe = (CLocations*)user_data;

	switch (pMe->m_State) 
	{
	case 1:
		if (pMe->m_Level==1)
			pMe->m_State = 0;
		break;

	case 2:
		if (pMe->m_Level==2) {
			if (pMe->m_Tmp.Valid())
				gtk_tree_path_free(pMe->Add(&pMe->m_Tmp));
			pMe->m_State = 1;
		}
		break;

	case 3:
		if (pMe->m_Level==3) {
			if (pMe->m_Str) {
				gchar *buf = g_strstrip(g_string_free(pMe->m_Str, false));
				if (strcmp(element_name, "name")==0)
					pMe->m_Tmp.SetName(buf);
				else if (strcmp(element_name, "lon")==0)
					pMe->m_Tmp.SetLon(buf);
				else if (strcmp(element_name, "lat")==0)
					pMe->m_Tmp.SetLat(buf);
				else if (strcmp(element_name, "remarks")==0)
					pMe->m_Tmp.SetComment(buf);
				g_free(buf);
				pMe->m_Str = NULL;
			}
			pMe->m_State = 2;
		}
		break;

	case 7:
		if (pMe->m_Level==3)
			pMe->m_State = 2;
		break;

	case 8:
		if (pMe->m_Level==2) 
			pMe->m_State = 1;
		break;

	case 9:
		if (pMe->m_Level==1)
			pMe->m_State = 0;
		break;
	}

	if (pMe->m_Level>0)
		pMe->m_Level--;
}


//
// XML character data
//
void CLocations::CharacterData(GMarkupParseContext *context, const gchar *text, 
	gsize text_len, gpointer user_data, GError **error)
{
	CLocations *pMe = (CLocations*)user_data;

	if (pMe->m_State==3 && pMe->m_Level==3) 
		pMe->m_Str = g_string_append_len(pMe->m_Str, text, text_len);
}

void CLocations::Save(void)
{
	gchar *backpath, *newpath, *filepath, *buf;
	gchar *name, *lon, *lat, *rem;
	GtkTreeIter iter;

	newpath = g_build_filename(get_user_config_dir(), "locations.$$$", NULL);
	filepath = g_build_filename(get_user_config_dir(), "locations.xml", NULL);
	backpath = g_build_filename(get_user_config_dir(), "locations.old", NULL);
	force_directory(get_user_config_dir());

	// Save a new content
	FILE *f = open_file(newpath, "w");
	if (f) {
		fprintf(f, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n");
		if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(m_List), &iter)) {
			fprintf(f, "<locations>\n");
			do {
				gtk_tree_model_get(GTK_TREE_MODEL(m_List), &iter, 
					LOCATION_NAME, &name, LOCATION_LON, &lon, LOCATION_LAT, &lat,
					LOCATION_REMARKS, &rem, -1);
				fprintf(f, "\t<location>\n");
				if (name && *name!='\0') {
					buf = g_markup_escape_text(name, -1);
					fprintf(f, "\t\t<name>%s</name>\n", buf);
					g_free(buf);
				}
				if (lon && *lon!='\0') {
					buf = g_markup_escape_text(lon, -1);
					fprintf(f, "\t\t<lon>%s</lon>\n", buf);
					g_free(buf);
				}
				if (lat && *lat!='\0') {
					buf = g_markup_escape_text(lat, -1);
					fprintf(f, "\t\t<lat>%s</lat>\n", buf);
					g_free(buf);
				}
				if (rem && *rem!='\0') {
					buf = g_markup_escape_text(rem, -1);
					fprintf(f, "\t\t<remarks>%s</remarks>\n", buf);
					g_free(buf);
				}
				fprintf(f, "\t</location>\n");
				g_free(name);
				g_free(lon);
				g_free(lat);
				g_free(rem);
			} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(m_List), &iter));
			fprintf(f, "</locations>\n");
		} else {
			fprintf(f, "<locations/>\n");
		}
		fclose(f);
	}

	// Switch files
	g_unlink(backpath);
	g_rename(filepath, backpath);
	g_rename(newpath, filepath);

	g_free(filepath);
	g_free(backpath);
	g_free(newpath);
}


//
// Get coordinates
//
bool CLocations::Get(GtkTreePath *pPath, CLocation *data)
{
	GtkTreeIter iter;
	gchar *name, *lon, *lat, *rem;

	if (data)
		data->Clear();
	if (pPath) {
		if (gtk_tree_model_get_iter(GTK_TREE_MODEL(m_List), &iter, pPath)) {
			gtk_tree_model_get(GTK_TREE_MODEL(m_List), &iter, 
				LOCATION_NAME, &name, LOCATION_LON, &lon, LOCATION_LAT, &lat,
				LOCATION_REMARKS, &rem, -1);
			if (data) {
				if (name)
					data->SetName(g_strstrip(name));
				if (lon)
					data->SetLon(g_strstrip(lon));
				if (lat)
					data->SetLat(g_strstrip(lat));
				if (rem)
					data->SetComment(g_strstrip(rem));
			}
			g_free(name);
			g_free(lon);
			g_free(lat);
			g_free(rem);
			return true;
		}
	}
	return false;
}


//
// Get number of items
//
int CLocations::GetCount(void)
{
	return gtk_tree_model_iter_n_children(GTK_TREE_MODEL(m_List), NULL);
}


//
// Add new item
//
GtkTreePath *CLocations::Add(const CLocation *data)
{
	GtkTreeIter iter;

	if (data) {
		gtk_list_store_append(m_List, &iter);
		gtk_list_store_set(m_List, &iter, LOCATION_NAME, data->Name(), 
			LOCATION_LON, data->Lon(), LOCATION_LAT, data->Lat(),
			LOCATION_REMARKS, data->Comment(), -1);
		return gtk_tree_model_get_path(GTK_TREE_MODEL(m_List), &iter); 
	}
	return NULL;
}


//
// Update item
//
void CLocations::Update(GtkTreePath *pPath, const CLocation *data)
{
	GtkTreeIter iter;

	if (pPath && data) {
		if (gtk_tree_model_get_iter(GTK_TREE_MODEL(m_List), &iter, pPath)) {
			gtk_list_store_set(m_List, &iter, LOCATION_NAME, data->Name(),
				LOCATION_LON, data->Lon(), LOCATION_LAT, data->Lat(), 
				LOCATION_REMARKS, data->Comment(), -1);
		}
	}
}


//
// Delete item
//
void CLocations::Remove(GtkTreePath *pPath)
{
	GtkTreeIter iter;

	if (gtk_tree_model_get_iter(GTK_TREE_MODEL(m_List), &iter, pPath)) 
		gtk_list_store_remove(m_List, &iter);
}


//
// Import items from CSV file
//
int CLocations::Import(const gchar *filepath)
{
	int res = 0;
	const char *name, *lon, *lat;
	char buf1[128], buf2[128];
	double x, y;
	GtkTreeIter iter;

	FILE *f = open_file(filepath, "r");
	if (f) {
		CCSVReader reader(f);
		if (reader.Columns()>=3) {
			while (reader.ReadLine()) {
				name = reader.GetStr(0);
				lon = reader.GetStr(1);
				lat = reader.GetStr(2);
				if (name && lon && lat && *name!='\0') {
					if (cmpack_strtolon(lon, &x)==0 && cmpack_strtolat(lat, &y)==0) {
						cmpack_lontostr(x, buf1, 128);
						cmpack_lattostr(y, buf2, 128);
						gtk_list_store_append(m_List, &iter);
						gtk_list_store_set(m_List, &iter, 
							LOCATION_NAME, name, LOCATION_LON, buf1, LOCATION_LAT, buf2, 
							LOCATION_REMARKS, reader.GetStr(3), -1);
						res++;
					}
				}
			}
		}
		fclose(f);
	}
	return res;
}

void CLocations::Export(const gchar *filepath)
{
	gboolean ok;
	gchar *name, *lon, *lat, *rem;
	GtkTreeIter iter;

	FILE *f = open_file(filepath, "w");
	if (f) {
		CCSVWriter writer(f);
		writer.AddColumn("NAME");
		writer.AddColumn("LONGITUDE");
		writer.AddColumn("LATITUDE");
		writer.AddColumn("REMARKS");
		ok = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(m_List), &iter);
		while (ok) {
			gtk_tree_model_get(GTK_TREE_MODEL(m_List), &iter, 
				LOCATION_NAME, &name, LOCATION_LON, &lon, LOCATION_LAT, &lat,
				LOCATION_REMARKS, &rem, -1);
			writer.Append();
			writer.SetStr(0, name);
			writer.SetStr(1, lon);
			writer.SetStr(2, lat);
			writer.SetStr(3, rem);
			g_free(name);
			g_free(lon);
			g_free(lat);
			g_free(rem);
			ok = gtk_tree_model_iter_next(GTK_TREE_MODEL(m_List), &iter);
		}
		writer.Close();
		fclose(f);
	}
}
