/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004  Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007  Andrei Prygounkov <hugbug@users.sourceforge.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Revision$
 * $Date$
 *
 */


#ifndef DECODER_H
#define DECODER_H

#include "Thread.h"

class Decoder
{
public:
	enum EStatus
	{
		eUnknownError,
		eFinished,
		eArticleIncomplete,
		eCrcError
	};

protected:
	const char*				m_szSrcFilename;
	const char*				m_szDestFilename;
	char*					m_szArticleFilename;

public:
							Decoder();
	virtual					~Decoder();
	virtual EStatus			Execute() = 0;
	void					SetSrcFilename(const char* szSrcFilename) { m_szSrcFilename = szSrcFilename; }
	void					SetDestFilename(const char* szDestFilename) { m_szDestFilename = szDestFilename; }
	const char*				GetArticleFilename() { return m_szArticleFilename; }
};

class UULibDecoder: public Decoder
{
private:
	static Mutex			m_mutexDecoder;

public:
	virtual EStatus			Execute();
};

class YDecoder: public Decoder
{
protected:
	static unsigned int		crc_tab[256];
	bool					m_bBody;
	bool					m_bEnd;
	unsigned long			m_lExpectedCRC;
	unsigned long			m_lCalculatedCRC;
	unsigned long			m_iBegin;
	unsigned long			m_iEnd;
	bool					m_bAutoSeek;
	bool					m_bNeedSetPos;
	bool					m_bCrcCheck;

	unsigned int			DecodeBuffer(char* buffer);
	static void				crc32gentab();
	unsigned long			crc32m(unsigned long startCrc, unsigned char *block, unsigned int length);

public:
							YDecoder();
	virtual EStatus			Execute();
	void					Clear();
	bool					Write(char* buffer, FILE* outfile);
	void					SetAutoSeek(bool bAutoSeek) { m_bAutoSeek = m_bNeedSetPos = bAutoSeek; }
	void					SetCrcCheck(bool bCrcCheck) { m_bCrcCheck = bCrcCheck; }

	static void				Init();
	static void				Final();
};

#endif
