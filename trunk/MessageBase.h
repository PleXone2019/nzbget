/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2005  Bo Cordes Petersen <placebodk@users.sourceforge.net>
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


#ifndef MESSAGEBASE_H
#define MESSAGEBASE_H

static const uint32_t NZBMESSAGE_SIGNATURE = 0x6E7A6202; // = "nzb"-version-2
static const int NZBREQUESTFILENAMESIZE = 512;
static const int NZBREQUESTPASSWORDSIZE = 32;

/**
 * NZBGet communication protocol uses only two basic data types: integer and char.
 * Integer values are passed using network byte order (Big-Endian).
 * To convert them to/from machine (host) byte order the functions
 * "htonl" and "ntohl" can be used.
 * All char-strings must ends with NULL-char.
 */

// The pack-directive prevents aligning of structs.
// This makes them more portable and allows to use together servers and clients
// compiled on different cpu architectures
#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif

namespace NZBMessageRequest
{
	// Possible values for field "m_iType" of struct "SNZBMessageBase":
	enum
	{
		eRequestDownload = 1,
		eRequestPauseUnpause,
		eRequestList,
		eRequestSetDownloadRate,
		eRequestDumpDebug,
		eRequestEditQueue,
		eRequestLog,
		eRequestShutdown
	};

	// Possible values for field "m_iAction" of struct "SNZBEditQueueRequest":
	enum
	{
		eActionMoveOffset = 1,	// move to m_iOffset relative to the current position in queue
		eActionMoveTop,			// move to top of queue
		eActionMoveBottom,		// move to bottom of queue
		eActionPause,			// pause
		eActionResume,			// resume (unpause)
		eActionDelete			// delete
	};
}

// The basic NZBMessageBase struct
struct SNZBMessageBase
{
	uint32_t				m_iSignature;			// Signature must be NZBMESSAGE_SIGNATURE in integer-value
	uint32_t				m_iStructSize;			// Size of the entire struct
	uint32_t				m_iType;				// Message type, see enum in NZBMessageRequest-namespace
	char					m_szPassword[ NZBREQUESTPASSWORDSIZE ];	// Password needs to be in every request
};

// A download request
struct SNZBDownloadRequest
{
	SNZBMessageBase			m_MessageBase;			// Must be the first in the struct
	char					m_szFilename[ NZBREQUESTFILENAMESIZE ];	// Name of nzb-file, may contain full path (local path on client) or only filename
	uint32_t				m_bAddFirst;			// 1 - add file to the top of download queue
	uint32_t				m_iTrailingDataLength;	// Length of nzb-file in bytes
};

// A list and status request
struct SNZBListRequest
{
	SNZBMessageBase			m_MessageBase;			// Must be the first in the struct
	uint32_t				m_bFileList;			// 1 - return file list
	uint32_t				m_bServerState;			// 1 - return server state
};

// A list request-answer
struct SNZBListRequestAnswer
{
	uint32_t				m_iStructSize;			// Size of the entire struct
	uint32_t				m_iEntrySize;			// Size of the SNZBListRequestAnswerEntry-struct
	uint32_t 				m_iRemainingSizeLo;		// Remaining size in bytes, Low 32-bits of 64-bit value
	uint32_t 				m_iRemainingSizeHi;		// Remaining size in bytes, High 32-bits of 64-bit value
	uint32_t				m_iDownloadRate;		// Current download speed, in Bytes pro Second
	uint32_t				m_iDownloadLimit;		// Current download limit, in Bytes pro Second
	uint32_t				m_bServerPaused;		// 1 - server is currently in paused-state
	uint32_t				m_iThreadCount;			// Number of threads running
	uint32_t				m_iNrTrailingEntries;	// Number of List-entries, following to this structure
	uint32_t				m_iTrailingDataLength;	// Length of all List-entries, following to this structure
};

// A list request-answer entry
struct SNZBListRequestAnswerEntry
{
	uint32_t				m_iID;					// Entry-ID
	uint32_t				m_iFileSizeLo;			// Filesize in bytes, Low 32-bits of 64-bit value
	uint32_t				m_iFileSizeHi;			// Filesize in bytes, High 32-bits of 64-bit value
	uint32_t				m_iRemainingSizeLo;		// Remaining size in bytes, Low 32-bits of 64-bit value
	uint32_t				m_iRemainingSizeHi;		// Remaining size in bytes, High 32-bits of 64-bit value
	uint32_t				m_bPaused;				// 1 - file is paused
	uint32_t				m_bFilenameConfirmed;	// 1 - Filename confirmed (read from article body), 0 - Filename parsed from subject (can be changed after reading of article)
	uint32_t				m_iNZBFilenameLen;		// Length of NZBFileName-string (m_szNZBFilename), following to this record
	uint32_t				m_iSubjectLen;			// Length of Subject-string (m_szSubject), following to this record
	uint32_t				m_iFilenameLen;			// Length of Filename-string (m_szFilename), following to this record
	uint32_t				m_iDestDirLen;			// Length of DestDir-string (m_szDestDir), following to this record
	//char					m_szNZBFilename[0];		// variable sized, may contain full path (local path on client) or only filename
	//char					m_szSubject[0];			// variable sized
	//char					m_szFilename[0];		// variable sized
	//char					m_szDestDir[0];			// variable sized
};

// A log request
struct SNZBLogRequest
{
	SNZBMessageBase			m_MessageBase;			// Must be the first in the struct
	uint32_t				m_iIDFrom;				// Only one of these two parameters
	uint32_t				m_iLines;				// can be set. The another one must be set to "0".
};

// A log request-answer
struct SNZBLogRequestAnswer
{
	uint32_t				m_iStructSize;			// Size of the entire struct
	uint32_t				m_iEntrySize;			// Size of the SNZBLogRequestAnswerEntry-struct
	uint32_t				m_iNrTrailingEntries;	// Number of Log-entries, following to this structure
	uint32_t				m_iTrailingDataLength;	// Length of all Log-entries, following to this structure
};

// A log request-answer entry
struct SNZBLogRequestAnswerEntry
{
	uint32_t				m_iID;					// ID of Log-entry
	uint32_t				m_iKind;				// see Message::Kind in "Log.h"
	uint32_t				m_tTime;				// time since the Epoch (00:00:00 UTC, January 1, 1970), measured in seconds.
	uint32_t				m_iTextLen;				// Length of Text-string (m_szText), following to this record
	//char					m_szText[0];			// variable sized
};

// A Pause/Unpause request
struct SNZBPauseUnpauseRequest
{
	SNZBMessageBase			m_MessageBase;			// Must be the first in the struct
	uint32_t				m_bPause;				// 1 - server must be paused, 0 - server must be unpaused
};

// Request setting the download rate
struct SNZBSetDownloadRateRequest
{
	SNZBMessageBase			m_MessageBase;			// Must be the first in the struct
	uint32_t				m_iDownloadRate;		// Speed limit, in Bytes pro Second
};

// A download request
struct SNZBEditQueueRequest
{
	SNZBMessageBase			m_MessageBase;			// Must be the first in the struct
	uint32_t				m_iAction;				// Action to be executed, see enum in NZBMessageRequest-namespace
	int32_t					m_iOffset;				// Offset to move (for m_iAction = 0)
	uint32_t				m_iIDFrom;				// ID of the first file in the range
	uint32_t				m_iIDTo;				// ID of the last file in the range
};

// Request dumping of debug info
struct SNZBDumpDebugRequest
{
	SNZBMessageBase			m_MessageBase;			// Must be the first in the struct
};

// Shutdown server request
struct SNZBShutdownRequest
{
	SNZBMessageBase			m_MessageBase;			// Must be the first in the struct
};

#ifdef HAVE_PRAGMA_PACK
#pragma pack()
#endif

#endif
