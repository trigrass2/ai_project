/*
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2002 Berin Lautenbach.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by
 *                   Berin Lautenbach"
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "XSEC", "xml-security-c" and Berin Lautenbach must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact berin@users.sourceforge.net.
 *
 * 5. Products derived from this software may not be called "xml-security-c",
 *    nor may "xml-security-c" appear in their name, without prior written
 *    permission of Berin Lautenbach.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL BERIN LAUTENBACH OR OTHER
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 */

/*
 * XSEC
 *
 * DSIGReferenceList := Class for Loading and storing a list of references
 *					 
 *
 */

// XSEC Includes
#include <xsec/dsig/DSIGReferenceList.hpp>
#include <xsec/dsig/DSIGReference.hpp>

DSIGReferenceList::DSIGReferenceList() {}

DSIGReferenceList::~DSIGReferenceList() {

	// Delete all the references contained in the list

	ReferenceListVectorType::iterator iterator = m_referenceList.begin();

	if (iterator != m_referenceList.end()) {
		
		delete *iterator;
		iterator++;

	}

}


void DSIGReferenceList::addReference(DSIGReference * ref) {

	m_referenceList.push_back(ref);

}

DSIGReferenceList::size_type DSIGReferenceList::getSize() {

	return m_referenceList.size();

}

DSIGReference * DSIGReferenceList::removeReference(size_type index) {

	if (index < m_referenceList.size())
		return m_referenceList[index];

	return NULL;

}

/*
DSIGReference * DSIGReferenceList::getFirstReference() {

	DSIGReference * retValue;

	m_iterator = m_referenceList.begin();

	if (m_iterator != m_referenceList.end()) {
		
		retValue = *m_iterator;
		m_iterator++;
		return retValue;

	}

	return NULL;

}

DSIGReference * DSIGReferenceList::getNextReference() {

	DSIGReference * retValue;

	if (m_iterator == m_referenceList.end())
		return NULL;

	retValue = *m_iterator;
	m_iterator++;

	return retValue;

}
*/

DSIGReference * DSIGReferenceList::item(ReferenceListVectorType::size_type index) {

	if (index < m_referenceList.size())
		return m_referenceList[index];

	return NULL;

}
bool DSIGReferenceList::empty() {

	// Clear out the list - note we do NOT delete the reference elements

	return m_referenceList.empty();

}


