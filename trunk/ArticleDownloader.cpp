/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2012 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * $Revision$
 * $Date$
 *
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <cstdio>
#ifdef WIN32
#include <direct.h>
#else
#include <unistd.h>
#include <sys/time.h>
#endif
#include <sys/stat.h>
#include <errno.h>

#include "nzbget.h"
#include "ArticleDownloader.h"
#include "Decoder.h"
#include "Log.h"
#include "Options.h"
#include "ServerPool.h"
#include "Util.h"

extern DownloadSpeedMeter* g_pDownloadSpeedMeter;
extern DownloadQueueHolder* g_pDownloadQueueHolder;
extern Options* g_pOptions;
extern ServerPool* g_pServerPool;

ArticleDownloader::ArticleDownloader()
{
	debug("Creating ArticleDownloader");

	m_szResultFilename	= NULL;
	m_szTempFilename	= NULL;
	m_szArticleFilename	= NULL;
	m_szInfoName		= NULL;
	m_szOutputFilename	= NULL;
	m_pConnection		= NULL;
	m_eStatus			= adUndefined;
	m_bDuplicate		= false;
	m_eFormat			= Decoder::efUnknown;
	SetLastUpdateTimeNow();
}

ArticleDownloader::~ArticleDownloader()
{
	debug("Destroying ArticleDownloader");

	if (m_szTempFilename)
	{
		free(m_szTempFilename);
	}
	if (m_szArticleFilename)
	{
		free(m_szArticleFilename);
	}
	if (m_szInfoName)
	{
		free(m_szInfoName);
	}
	if (m_szOutputFilename)
	{
		free(m_szOutputFilename);
	}
}

void ArticleDownloader::SetTempFilename(const char* v)
{
	m_szTempFilename = strdup(v);
}

void ArticleDownloader::SetOutputFilename(const char* v)
{
	m_szOutputFilename = strdup(v);
}

void ArticleDownloader::SetInfoName(const char * v)
{
	m_szInfoName = strdup(v);
}

/*
 * How server management (for one particular article) works:
	- there is a list of failed servers which is initially empty;
	- level is initially 0;

	<loop>
		- request a connection from server pool for current level;
		  Exception: this step is skipped for the very first download attempt, because a
		  level-0 connection is initially passed from queue manager;
		- try to download from server;
		- if connection to server cannot be established or download fails due to interrupted connection,
		  try again (as many times as needed without limit) the same server until connection is OK;
		- if download fails with error "Not-Found" (article or group not found) or with CRC error,
		  add the server to failed server list;
		- if download fails with general failure error (article incomplete, other unknown error
		  codes), try the same server again as many times as defined by option <Retries>; if all attempts
		  fail, add the server to failed server list;
		- if all servers from current level were tried, increase level;
		- if all servers from all levels were tried, break the loop with failure status.
	<end-loop>
*/
void ArticleDownloader::Run()
{
	debug("Entering ArticleDownloader-loop");

	SetStatus(adRunning);
	m_szResultFilename = m_pArticleInfo->GetResultFilename();

	if (g_pOptions->GetContinuePartial())
	{
		if (Util::FileExists(m_szResultFilename))
		{
			// file exists from previous program's start
			detail("Article %s already downloaded, skipping", m_szInfoName);
			SetStatus(adFinished);
			FreeConnection(true);
			return;
		}
	}

	EStatus Status = adFailed;

	int iRetries = g_pOptions->GetRetries() > 0 ? g_pOptions->GetRetries() : 1;
	int iRemainedRetries = iRetries;
	ServerPool::Servers failedServers;
	failedServers.reserve(g_pServerPool->GetServers()->size());
	NewsServer* pWantServer = NULL;
	NewsServer* pLastServer = NULL;
	int iLevel = 0;

	while (!IsStopped())
	{
		Status = adFailed;

		SetStatus(adWaiting);
		while (!IsStopped() && !m_pConnection)
		{
			m_pConnection = g_pServerPool->GetConnection(iLevel, pWantServer, &failedServers);
			usleep(5 * 1000);
		}
		SetLastUpdateTimeNow();
		SetStatus(adRunning);

		if (IsStopped() || g_pOptions->GetPauseDownload() || g_pOptions->GetPauseDownload2())
		{
			Status = adRetry;
			break;
		}

		pLastServer = m_pConnection->GetNewsServer();

		m_pConnection->SetSuppressErrors(false);

		// test connection
		bool bConnected = m_pConnection && m_pConnection->Connect();
		if (bConnected && !IsStopped())
		{
			// Okay, we got a Connection. Now start downloading.
			detail("Downloading %s @ server%i (%s)", m_szInfoName,
				m_pConnection->GetNewsServer()->GetID(), m_pConnection->GetHost());
			Status = Download();
		}

		if (bConnected)
		{
			if (Status == adConnectError)
			{
				m_pConnection->Disconnect();
				bConnected = false;
				Status = adFailed;
			}
			else
			{
				// freeing connection allows other threads to start.
				// we doing this only if the problem was with article or group.
				// if the problem occurs by connecting or authorization we do not
				// free the connection, to prevent starting of thousands of threads 
				// (cause each of them will also free it's connection after the 
				// same connect-error).
				FreeConnection(Status == adFinished || Status == adNotFound);
			}
		}

		if (Status == adFinished || Status == adFatalError)
		{
			break;
		}

		pWantServer = NULL;

		if (bConnected && Status == adFailed)
		{
			iRemainedRetries--;
		}

		if (!bConnected || (Status == adFailed && iRemainedRetries > 1))
		{
			pWantServer = pLastServer;
		}

		if (Status == adNotFound || Status == adCrcError || (Status == adFailed && iRemainedRetries == 0))
		{
			failedServers.push_back(pLastServer);
		}

		if (pWantServer && !IsStopped() &&
			!(g_pOptions->GetPauseDownload() || g_pOptions->GetPauseDownload2()))
		{
			detail("Waiting %i sec to retry", g_pOptions->GetRetryInterval());
			SetStatus(adWaiting);
			int msec = 0;
			while (!IsStopped() && (msec < g_pOptions->GetRetryInterval() * 1000) && 
				!(g_pOptions->GetPauseDownload() || g_pOptions->GetPauseDownload2()))
			{
				usleep(100 * 1000);
				msec += 100;
			}
			SetLastUpdateTimeNow();
			SetStatus(adRunning);
		}

		if (IsStopped() || g_pOptions->GetPauseDownload() || g_pOptions->GetPauseDownload2())
		{
			Status = adRetry;
			break;
		}

		if (!pWantServer)
		{
			// if all servers from current level were tried, increase level
			// if all servers from all levels were tried, break the loop with failure status

			bool bAllServersOnLevelFailed = true;
			for (ServerPool::Servers::iterator it = g_pServerPool->GetServers()->begin(); it != g_pServerPool->GetServers()->end(); it++)
			{
				NewsServer* pCandidateServer = *it;
				if (pCandidateServer->GetLevel() == iLevel)
				{
					bool bServerFailed = false;
					for (ServerPool::Servers::iterator it = failedServers.begin(); it != failedServers.end(); it++)
					{
						NewsServer* pIgnoreServer = *it;
						if (pIgnoreServer == pCandidateServer ||
							(pIgnoreServer->GetGroup() > 0 && pIgnoreServer->GetGroup() == pCandidateServer->GetGroup()))
						{
							bServerFailed = true;
							break;
						}					
					}
					if (!bServerFailed)
					{
						bAllServersOnLevelFailed = false;
						break;
					}
				}
			}

			if (bAllServersOnLevelFailed)
			{
				if (iLevel < g_pServerPool->GetMaxLevel())
				{
					detail("Article %s @ all level %i servers failed, increasing level", m_szInfoName, iLevel);
					iLevel++;
				}
				else
				{
					warn("Article %s @ all servers failed", m_szInfoName);
					Status = adFailed;
					break;
				}
			}
			
			iRemainedRetries = iRetries;
		}
	}

	FreeConnection(Status == adFinished);

	if (m_bDuplicate)
	{
		Status = adFinished;
	}

	if (Status != adFinished && Status != adRetry)
	{
		Status = adFailed;
	}

	if (IsStopped())
	{
		detail("Download %s cancelled", m_szInfoName);
		Status = adRetry;
	}

	if (Status == adFailed)
	{
		warn("Download %s failed", m_szInfoName);
	}

	SetStatus(Status);
	Notify(NULL);

	debug("Exiting ArticleDownloader-loop");
}

ArticleDownloader::EStatus ArticleDownloader::Download()
{
	const char* szResponse = NULL;
	EStatus Status = adRunning;

	if (m_pConnection->GetNewsServer()->GetJoinGroup())
	{
		// change group
		for (FileInfo::Groups::iterator it = m_pFileInfo->GetGroups()->begin(); it != m_pFileInfo->GetGroups()->end(); it++)
		{
			szResponse = m_pConnection->JoinGroup(*it);
			if (szResponse && !strncmp(szResponse, "2", 1))
			{
				break; 
			}
		}

		Status = CheckResponse(szResponse, "could not join group");
		if (Status != adFinished)
		{
			return Status;
		}
	}

	// retrieve article
	char tmp[1024];
	snprintf(tmp, 1024, "ARTICLE %s\r\n", m_pArticleInfo->GetMessageID());
	tmp[1024-1] = '\0';

	for (int retry = 3; retry > 0; retry--)
	{
		szResponse = m_pConnection->Request(tmp);
		if (szResponse && !strncmp(szResponse, "2", 1))
		{
			break;
		}
	}

	Status = CheckResponse(szResponse, "could not fetch article");
	if (Status != adFinished)
	{
		return Status;
	}

	// positive answer!

	if (g_pOptions->GetDecode())
	{
		m_YDecoder.Clear();
		m_YDecoder.SetAutoSeek(g_pOptions->GetDirectWrite());
		m_YDecoder.SetCrcCheck(g_pOptions->GetCrcCheck());

		m_UDecoder.Clear();
	}

	m_pOutFile = NULL;
	bool bBody = false;
	bool bEnd = false;
	const int LineBufSize = 1024*10;
	char* szLineBuf = (char*)malloc(LineBufSize);
	Status = adRunning;

	while (!IsStopped())
	{
		SetLastUpdateTimeNow();

		// Throttle the bandwidth
		while (!IsStopped() && (g_pOptions->GetDownloadRate() > 0.0f) &&
		        (g_pDownloadSpeedMeter->CalcCurrentDownloadSpeed() > g_pOptions->GetDownloadRate()))
		{
			SetLastUpdateTimeNow();
			usleep(10 * 1000);
		}

		int iLen = 0;
		char* line = m_pConnection->ReadLine(szLineBuf, LineBufSize, &iLen);
		g_pDownloadSpeedMeter->AddSpeedReading(iLen);

		// Have we encountered a timeout?
		if (!line)
		{
			if (!IsStopped())
			{
				warn("Article %s @ server%i (%s) failed: Unexpected end of article", m_szInfoName,
					m_pConnection->GetNewsServer()->GetID(), m_pConnection->GetHost());
			}
			Status = adFailed;
			break;
		}

		//detect end of article
		if (!strcmp(line, ".\r\n") || !strcmp(line, ".\n"))
		{
			bEnd = true;
			break;
		}

		//detect lines starting with "." (marked as "..")
		if (!strncmp(line, "..", 2))
		{
			line++;
			iLen--;
		}

		if (!bBody)
		{
			// detect body of article
			if (*line == '\r' || *line == '\n')
			{
				bBody = true;
			}
			// check id of returned article
			else if (!strncmp(line, "Message-ID: ", 12))
			{
				char* p = line + 12;
				if (strncmp(p, m_pArticleInfo->GetMessageID(), strlen(m_pArticleInfo->GetMessageID())))
				{
					if (char* e = strrchr(p, '\r')) *e = '\0'; // remove trailing CR-character
					warn("Article %s @ server%i (%s) failed: Wrong message-id, expected %s, returned %s", m_szInfoName,
						m_pConnection->GetNewsServer()->GetID(), m_pConnection->GetHost(), m_pArticleInfo->GetMessageID(), p);
					Status = adFailed;
					break;
				}
			}
		}
		else if (m_eFormat == Decoder::efUnknown && g_pOptions->GetDecode())
		{
			m_eFormat = Decoder::DetectFormat(line, iLen);
		}

		// write to output file
		if (((bBody && m_eFormat != Decoder::efUnknown) || !g_pOptions->GetDecode()) && !Write(line, iLen))
		{
			Status = adFatalError;
			break;
		}
	}

	free(szLineBuf);

	if (m_pOutFile)
	{
		fclose(m_pOutFile);
	}

	if (!bEnd && Status == adRunning && !IsStopped())
	{
		warn("Article %s @ server%i (%s) failed: article incomplete", m_szInfoName,
			m_pConnection->GetNewsServer()->GetID(), m_pConnection->GetHost());
		Status = adFailed;
	}

	if (IsStopped())
	{
		Status = adFailed;
	}

	if (Status == adRunning)
	{
		FreeConnection(true);
		return DecodeCheck();
	}
	else
	{
		remove(m_szTempFilename);
		return Status;
	}
}

ArticleDownloader::EStatus ArticleDownloader::CheckResponse(const char* szResponse, const char* szComment)
{
	if (!szResponse)
	{
		if (!IsStopped())
		{
			warn("Article %s @ server%i (%s) failed, %s: Connection closed by remote host", m_szInfoName, 
				m_pConnection->GetNewsServer()->GetID(), m_pConnection->GetHost(), szComment);
		}
		return adConnectError;
	}
	else if (m_pConnection->GetAuthError() || !strncmp(szResponse, "400", 3) || !strncmp(szResponse, "499", 3))
	{
		warn("Article %s @ server%i (%s) failed, %s: %s", m_szInfoName,
			 m_pConnection->GetNewsServer()->GetID(), m_pConnection->GetHost(), szComment, szResponse);
		return adConnectError;
	}
	else if (!strncmp(szResponse, "41", 2) || !strncmp(szResponse, "42", 2) || !strncmp(szResponse, "43", 2))
	{
		warn("Article %s @ server%i (%s) failed, %s: %s", m_szInfoName,
			 m_pConnection->GetNewsServer()->GetID(), m_pConnection->GetHost(), szComment, szResponse);
		return adNotFound;
	}
	else if (!strncmp(szResponse, "2", 1))
	{
		// OK
		return adFinished;
	}
	else 
	{
		// unknown error, no special handling
		warn("Article %s @ server%i (%s) failed, %s: %s", m_szInfoName,
			 m_pConnection->GetNewsServer()->GetID(), m_pConnection->GetHost(), szComment, szResponse);
		return adFailed;
	}
}

bool ArticleDownloader::Write(char* szLine, int iLen)
{
	if (!m_pOutFile && !PrepareFile(szLine))
	{
		return false;
	}

	if (g_pOptions->GetDecode())
	{
		bool bOK = false;
		if (m_eFormat == Decoder::efYenc)
		{
			bOK = m_YDecoder.Write(szLine, iLen, m_pOutFile);
		}
		else if (m_eFormat == Decoder::efUx)
		{
			bOK = m_UDecoder.Write(szLine, iLen, m_pOutFile);
		}
		else
		{
			warn("Decoding %s failed: unsupported encoding", m_szInfoName);
			return false;
		}
		if (!bOK)
		{
			debug("Failed line: %s", szLine);
			warn("Decoding %s failed", m_szInfoName);
		}
		return bOK;
	}
	else
	{
		return fwrite(szLine, 1, iLen, m_pOutFile) > 0;
	}
}

bool ArticleDownloader::PrepareFile(char* szLine)
{
	bool bOpen = false;

	// prepare file for writing
	if (m_eFormat == Decoder::efYenc)
	{
		if (!strncmp(szLine, "=ybegin ", 8))
		{
			if (g_pOptions->GetDupeCheck())
			{
				m_pFileInfo->LockOutputFile();
				if (!m_pFileInfo->GetOutputInitialized())
				{
					char* pb = strstr(szLine, " name=");
					if (pb)
					{
						pb += 6; //=strlen(" name=")
						char* pe;
						for (pe = pb; *pe != '\0' && *pe != '\n' && *pe != '\r'; pe++) ;
						if (!m_szArticleFilename)
						{
							m_szArticleFilename = (char*)malloc(pe - pb + 1);
							strncpy(m_szArticleFilename, pb, pe - pb);
							m_szArticleFilename[pe - pb] = '\0';
						}
						if (m_pFileInfo->IsDupe(m_szArticleFilename))
						{
							m_bDuplicate = true;
							return false;
						}
					}
				}
				if (!g_pOptions->GetDirectWrite())
				{
					m_pFileInfo->SetOutputInitialized(true);
				}
				m_pFileInfo->UnlockOutputFile();
			}

			if (g_pOptions->GetDirectWrite())
			{
				char* pb = strstr(szLine, " size=");
				if (pb) 
				{
					m_pFileInfo->LockOutputFile();
					if (!m_pFileInfo->GetOutputInitialized())
					{
						pb += 6; //=strlen(" size=")
						long iArticleFilesize = atol(pb);
						if (!CreateOutputFile(iArticleFilesize))
						{
							return false;
						}
						m_pFileInfo->SetOutputInitialized(true);
					}
					m_pFileInfo->UnlockOutputFile();
					bOpen = true;
				}
			}
			else
			{
				bOpen = true;
			}
		}
	}
	else
	{
		bOpen = true;
	}

	if (bOpen)
	{
		bool bDirectWrite = g_pOptions->GetDirectWrite() && m_eFormat == Decoder::efYenc;
		const char* szFilename = bDirectWrite ? m_szOutputFilename : m_szTempFilename;
		m_pOutFile = fopen(szFilename, bDirectWrite ? "rb+" : "wb");
		if (!m_pOutFile)
		{
			error("Could not %s file %s", bDirectWrite ? "open" : "create", szFilename);
			return false;
		}
		if (g_pOptions->GetWriteBufferSize() == -1)
		{
			setvbuf(m_pOutFile, (char *)NULL, _IOFBF, m_pArticleInfo->GetSize());
		}
		else if (g_pOptions->GetWriteBufferSize() > 0)
		{
			setvbuf(m_pOutFile, (char *)NULL, _IOFBF, g_pOptions->GetWriteBufferSize());
		}
	}

	return true;
}

/* creates output file and subdirectores */
bool ArticleDownloader::CreateOutputFile(int iSize)
{
	// delete eventually existing old file from previous program session
	remove(m_szOutputFilename);

	// ensure the directory exist
	char szDestDir[1024];
	int iMaxlen = Util::BaseFileName(m_szOutputFilename) - m_szOutputFilename;
	strncpy(szDestDir, m_szOutputFilename, iMaxlen < 1024 ? iMaxlen : 1024-1);
	szDestDir[iMaxlen] = '\0';

	if (!Util::ForceDirectories(szDestDir))
	{
		error("Could not create directory %s! Errcode: %i", szDestDir, errno);
		return false;
	}

	if (!Util::CreateSparseFile(m_szOutputFilename, iSize))
	{
		error("Could not create file %s!", m_szOutputFilename);
		return false;
	}

	return true;
}

ArticleDownloader::EStatus ArticleDownloader::DecodeCheck()
{
	bool bDirectWrite = g_pOptions->GetDirectWrite() && m_eFormat == Decoder::efYenc;

	if (g_pOptions->GetDecode())
	{
		SetStatus(adDecoding);

		Decoder* pDecoder = NULL;

		if (m_eFormat == Decoder::efYenc)
		{
			pDecoder = &m_YDecoder;
		}
		else if (m_eFormat == Decoder::efUx)
		{
			pDecoder = &m_UDecoder;
		}
		else
		{
			warn("Decoding %s failed: no binary data or unsupported encoding format", m_szInfoName);
			return adFatalError;
		}

		Decoder::EStatus eStatus = pDecoder->Check();
		bool bOK = eStatus == Decoder::eFinished;

		if (!bDirectWrite && bOK)
		{
			if (!Util::MoveFile(m_szTempFilename, m_szResultFilename))
			{
				error("Could not rename file %s to %s! Errcode: %i", m_szTempFilename, m_szResultFilename, errno);
			}
		}

		if (!m_szArticleFilename && pDecoder->GetArticleFilename())
		{
			m_szArticleFilename = strdup(pDecoder->GetArticleFilename());
		}

		remove(m_szTempFilename);

		if (bOK)
		{
			detail("Successfully downloaded %s", m_szInfoName);

			if (bDirectWrite && g_pOptions->GetContinuePartial())
			{
				// create empty flag-file to indicate that the artcile was downloaded
				FILE* flagfile = fopen(m_szResultFilename, "wb");
				if (!flagfile)
				{
					error("Could not create file %s", m_szResultFilename);
					// this error can be ignored
				}
				fclose(flagfile);
			}

			return adFinished;
		}
		else
		{
			remove(m_szResultFilename);
			if (eStatus == Decoder::eCrcError)
			{
				warn("Decoding %s failed: CRC-Error", m_szInfoName);
				return adCrcError;
			}
			else if (eStatus == Decoder::eArticleIncomplete)
			{
				warn("Decoding %s failed: article incomplete", m_szInfoName);
				return adFailed;
			}
			else if (eStatus == Decoder::eInvalidSize)
			{
				warn("Decoding %s failed: size mismatch", m_szInfoName);
				return adFailed;
			}
			else if (eStatus == Decoder::eNoBinaryData)
			{
				warn("Decoding %s failed: no binary data found", m_szInfoName);
				return adFailed;
			}
			else
			{
				warn("Decoding %s failed", m_szInfoName);
				return adFailed;
			}
		}
	}
	else 
	{
		// rawmode
		if (Util::MoveFile(m_szTempFilename, m_szResultFilename))
		{
			detail("Article %s successfully downloaded", m_szInfoName);
		}
		else
		{
			error("Could not move file %s to %s! Errcode: %i", m_szTempFilename, m_szResultFilename, errno);
		}
		return adFinished;
	}
}

void ArticleDownloader::LogDebugInfo()
{
	char szTime[50];
#ifdef HAVE_CTIME_R_3
		ctime_r(&m_tLastUpdateTime, szTime, 50);
#else
		ctime_r(&m_tLastUpdateTime, szTime);
#endif

	debug("      Download: status=%i, LastUpdateTime=%s, filename=%s", m_eStatus, szTime, Util::BaseFileName(GetTempFilename()));
}

void ArticleDownloader::Stop()
{
	debug("Trying to stop ArticleDownloader");
	Thread::Stop();
	m_mutexConnection.Lock();
	if (m_pConnection)
	{
		m_pConnection->SetSuppressErrors(true);
		m_pConnection->Cancel();
	}
	m_mutexConnection.Unlock();
	debug("ArticleDownloader stopped successfully");
}

bool ArticleDownloader::Terminate()
{
	NNTPConnection* pConnection = m_pConnection;
	bool terminated = Kill();
	if (terminated && pConnection)
	{
		debug("Terminating connection");
		pConnection->SetSuppressErrors(true);
		pConnection->Cancel();
		pConnection->Disconnect();
		g_pServerPool->FreeConnection(pConnection, true);
	}
	return terminated;
}

void ArticleDownloader::FreeConnection(bool bKeepConnected)
{
	if (m_pConnection)							
	{
		debug("Releasing connection");
		m_mutexConnection.Lock();
		if (!bKeepConnected || m_pConnection->GetStatus() == Connection::csCancelled)
		{
			m_pConnection->Disconnect();
		}
		g_pServerPool->FreeConnection(m_pConnection, true);
		m_pConnection = NULL;
		m_mutexConnection.Unlock();
	}
}

void ArticleDownloader::CompleteFileParts()
{
	debug("Completing file parts");
	debug("ArticleFilename: %s", m_pFileInfo->GetFilename());

	SetStatus(adJoining);

	bool bDirectWrite = g_pOptions->GetDirectWrite() && m_pFileInfo->GetOutputInitialized();

	char szNZBName[1024];
	char szNZBDestDir[1024];
	// the locking is needed for accessing the memebers of NZBInfo
	g_pDownloadQueueHolder->LockQueue();
	strncpy(szNZBName, m_pFileInfo->GetNZBInfo()->GetName(), 1024);
	strncpy(szNZBDestDir, m_pFileInfo->GetNZBInfo()->GetDestDir(), 1024);
	g_pDownloadQueueHolder->UnlockQueue();
	szNZBName[1024-1] = '\0';
	szNZBDestDir[1024-1] = '\0';
	
	char InfoFilename[1024];
	snprintf(InfoFilename, 1024, "%s%c%s", szNZBName, (int)PATH_SEPARATOR, m_pFileInfo->GetFilename());
	InfoFilename[1024-1] = '\0';

	if (!g_pOptions->GetDecode())
	{
		detail("Moving articles for %s", InfoFilename);
	}
	else if (bDirectWrite)
	{
		detail("Checking articles for %s", InfoFilename);
	}
	else
	{
		detail("Joining articles for %s", InfoFilename);
	}

	// Ensure the DstDir is created
	if (!Util::ForceDirectories(szNZBDestDir))
	{
		error("Could not create directory %s! Errcode: %i", szNZBDestDir, errno);
		SetStatus(adJoined);
		return;
	}

	char ofn[1024];
	snprintf(ofn, 1024, "%s%c%s", szNZBDestDir, (int)PATH_SEPARATOR, m_pFileInfo->GetFilename());
	ofn[1024-1] = '\0';

	// prevent overwriting existing files
	int dupcount = 0;
	while (Util::FileExists(ofn))
	{
		dupcount++;
		snprintf(ofn, 1024, "%s%c%s_duplicate%d", szNZBDestDir, (int)PATH_SEPARATOR, m_pFileInfo->GetFilename(), dupcount);
		ofn[1024-1] = '\0';
	}

	FILE* outfile = NULL;
	char tmpdestfile[1024];
	snprintf(tmpdestfile, 1024, "%s.tmp", ofn);
	tmpdestfile[1024-1] = '\0';

	if (g_pOptions->GetDecode() && !bDirectWrite)
	{
		remove(tmpdestfile);
		outfile = fopen(tmpdestfile, "wb+");
		if (!outfile)
		{
			error("Could not create file %s!", tmpdestfile);
			SetStatus(adJoined);
			return;
		}
		if (g_pOptions->GetWriteBufferSize() == -1 && (*m_pFileInfo->GetArticles())[0])
		{
			setvbuf(outfile, (char *)NULL, _IOFBF, (*m_pFileInfo->GetArticles())[0]->GetSize());
		}
		else if (g_pOptions->GetWriteBufferSize() > 0)
		{
			setvbuf(outfile, (char *)NULL, _IOFBF, g_pOptions->GetWriteBufferSize());
		}
	}
	else if (!g_pOptions->GetDecode())
	{
		remove(tmpdestfile);
		if (!Util::CreateDirectory(ofn))
		{
			error("Could not create directory %s! Errcode: %i", ofn, errno);
			SetStatus(adJoined);
			return;
		}
	}

	bool complete = true;
	int iBrokenCount = 0;
	static const int BUFFER_SIZE = 1024 * 50;
	char* buffer = NULL;

	if (g_pOptions->GetDecode() && !bDirectWrite)
	{
		buffer = (char*)malloc(BUFFER_SIZE);
	}

	for (FileInfo::Articles::iterator it = m_pFileInfo->GetArticles()->begin(); it != m_pFileInfo->GetArticles()->end(); it++)
	{
		ArticleInfo* pa = *it;
		if (pa->GetStatus() != ArticleInfo::aiFinished)
		{
			iBrokenCount++;
			complete = false;
		}
		else if (g_pOptions->GetDecode() && !bDirectWrite)
		{
			FILE* infile;
			const char* fn = pa->GetResultFilename();

			infile = fopen(fn, "rb");
			if (infile)
			{
				int cnt = BUFFER_SIZE;

				while (cnt == BUFFER_SIZE)
				{
					cnt = (int)fread(buffer, 1, BUFFER_SIZE, infile);
					fwrite(buffer, 1, cnt, outfile);
					SetLastUpdateTimeNow();
				}

				fclose(infile);
			}
			else
			{
				complete = false;
				iBrokenCount++;
				detail("Could not find file %s. Status is broken", fn);
			}
		}
		else if (!g_pOptions->GetDecode())
		{
			const char* fn = pa->GetResultFilename();
			char dstFileName[1024];
			snprintf(dstFileName, 1024, "%s%c%03i", ofn, (int)PATH_SEPARATOR, pa->GetPartNumber());
			dstFileName[1024-1] = '\0';
			if (!Util::MoveFile(fn, dstFileName))
			{
				error("Could not move file %s to %s! Errcode: %i", fn, dstFileName, errno);
			}
		}
	}

	if (buffer)
	{
		free(buffer);
	}

	if (outfile)
	{
		fclose(outfile);
		if (!Util::MoveFile(tmpdestfile, ofn))
		{
			error("Could not move file %s to %s! Errcode: %i", tmpdestfile, ofn, errno);
		}
	}

	if (bDirectWrite)
	{
		if (!Util::MoveFile(m_szOutputFilename, ofn))
		{
			error("Could not move file %s to %s! Errcode: %i", m_szOutputFilename, ofn, errno);
		}

		// if destination directory was changed delete the old directory (if empty)
		int iLen = strlen(szNZBDestDir);
		if (!(!strncmp(szNZBDestDir, m_szOutputFilename, iLen) && 
			(m_szOutputFilename[iLen] == PATH_SEPARATOR || m_szOutputFilename[iLen] == ALT_PATH_SEPARATOR)))
		{
			debug("Checking old dir for: %s", m_szOutputFilename);
			char szOldDestDir[1024];
			int iMaxlen = Util::BaseFileName(m_szOutputFilename) - m_szOutputFilename;
			strncpy(szOldDestDir, m_szOutputFilename, iMaxlen < 1024 ? iMaxlen : 1024-1);
			szOldDestDir[iMaxlen] = '\0';
			if (Util::DirEmpty(szOldDestDir))
			{
				debug("Deleting old dir: %s", szOldDestDir);
				rmdir(szOldDestDir);
			}
		}
	}

	if (!bDirectWrite || g_pOptions->GetContinuePartial())
	{
		for (FileInfo::Articles::iterator it = m_pFileInfo->GetArticles()->begin(); it != m_pFileInfo->GetArticles()->end(); it++)
		{
			ArticleInfo* pa = *it;
			remove(pa->GetResultFilename());
		}
	}

	if (complete)
	{
		info("Successfully downloaded %s", InfoFilename);
	}
	else
	{
		warn("%i of %i article downloads failed for \"%s\"", iBrokenCount, m_pFileInfo->GetArticles()->size(), InfoFilename);

		if (g_pOptions->GetRenameBroken())
		{
			char brokenfn[1024];
			snprintf(brokenfn, 1024, "%s_broken", ofn);
			brokenfn[1024-1] = '\0';
			if (Util::MoveFile(ofn, brokenfn))
			{
				detail("Renaming broken file from %s to %s", ofn, brokenfn);
			}
			else
			{
				warn("Renaming broken file from %s to %s failed", ofn, brokenfn);
			}
			strncpy(ofn, brokenfn, 1024);
			ofn[1024-1] = '\0';
		}
		else
		{
			detail("Not renaming broken file %s", ofn);
		}

		if (g_pOptions->GetCreateBrokenLog())
		{
			char szBrokenLogName[1024];
			snprintf(szBrokenLogName, 1024, "%s%c_brokenlog.txt", szNZBDestDir, (int)PATH_SEPARATOR);
			szBrokenLogName[1024-1] = '\0';
			FILE* file = fopen(szBrokenLogName, "ab");
			fprintf(file, "%s (%i/%i)%s", m_pFileInfo->GetFilename(), m_pFileInfo->GetArticles()->size() - iBrokenCount, m_pFileInfo->GetArticles()->size(), LINE_ENDING);
			fclose(file);
		}
	}

	// the locking is needed for accessing the memebers of NZBInfo
	g_pDownloadQueueHolder->LockQueue();
	m_pFileInfo->GetNZBInfo()->GetCompletedFiles()->push_back(strdup(ofn));
	if (strcmp(m_pFileInfo->GetNZBInfo()->GetDestDir(), szNZBDestDir))
	{
		// destination directory was changed during completion, need to move the file
		MoveCompletedFiles(m_pFileInfo->GetNZBInfo(), szNZBDestDir);
	}
	g_pDownloadQueueHolder->UnlockQueue();

	SetStatus(adJoined);
}

bool ArticleDownloader::MoveCompletedFiles(NZBInfo* pNZBInfo, const char* szOldDestDir)
{
	if (pNZBInfo->GetCompletedFiles()->empty())
	{
		return true;
	}

	// Ensure the DstDir is created
	if (!Util::ForceDirectories(pNZBInfo->GetDestDir()))
	{
		error("Could not create directory %s! Errcode: %i", pNZBInfo->GetDestDir(), errno);
		return false;
	}

	// move already downloaded files to new destination
	for (NZBInfo::Files::iterator it = pNZBInfo->GetCompletedFiles()->begin(); it != pNZBInfo->GetCompletedFiles()->end(); it++)
    {
		char* szFileName = *it;
		char szNewFileName[1024];
		snprintf(szNewFileName, 1024, "%s%c%s", pNZBInfo->GetDestDir(), (int)PATH_SEPARATOR, Util::BaseFileName(szFileName));
		szNewFileName[1024-1] = '\0';

		// check if file was not moved already
		if (strcmp(szFileName, szNewFileName))
		{
			// prevent overwriting of existing files
			int dupcount = 0;
			while (Util::FileExists(szNewFileName))
			{
				dupcount++;
				snprintf(szNewFileName, 1024, "%s%c%s_duplicate%d", pNZBInfo->GetDestDir(), (int)PATH_SEPARATOR, Util::BaseFileName(szFileName), dupcount);
				szNewFileName[1024-1] = '\0';
			}

			detail("Moving file %s to %s", szFileName, szNewFileName);
			if (Util::MoveFile(szFileName, szNewFileName))
			{
				free(szFileName);
				*it = strdup(szNewFileName);
			}
			else
			{
				error("Could not move file %s to %s! Errcode: %i", szFileName, szNewFileName, errno);
			}
		}
    }

	// move brokenlog.txt
	if (g_pOptions->GetCreateBrokenLog())
	{
		char szOldBrokenLogName[1024];
		snprintf(szOldBrokenLogName, 1024, "%s%c_brokenlog.txt", szOldDestDir, (int)PATH_SEPARATOR);
		szOldBrokenLogName[1024-1] = '\0';
		if (Util::FileExists(szOldBrokenLogName))
		{
			char szBrokenLogName[1024];
			snprintf(szBrokenLogName, 1024, "%s%c_brokenlog.txt", pNZBInfo->GetDestDir(), (int)PATH_SEPARATOR);
			szBrokenLogName[1024-1] = '\0';

			detail("Moving file %s to %s", szOldBrokenLogName, szBrokenLogName);
			if (Util::FileExists(szBrokenLogName))
			{
				// copy content to existing new file, then delete old file
				FILE* outfile;
				outfile = fopen(szBrokenLogName, "ab");
				if (outfile)
				{
					FILE* infile;
					infile = fopen(szOldBrokenLogName, "rb");
					if (infile)
					{
						static const int BUFFER_SIZE = 1024 * 50;
						int cnt = BUFFER_SIZE;
						char* buffer = (char*)malloc(BUFFER_SIZE);
						while (cnt == BUFFER_SIZE)
						{
							cnt = (int)fread(buffer, 1, BUFFER_SIZE, infile);
							fwrite(buffer, 1, cnt, outfile);
						}
						fclose(infile);
						free(buffer);
						remove(szOldBrokenLogName);
					}
					else
					{
						error("Could not open file %s", szOldBrokenLogName);
					}
					fclose(outfile);
				}
				else
				{
					error("Could not open file %s", szBrokenLogName);
				}
			}
			else 
			{
				// move to new destination
				if (!Util::MoveFile(szOldBrokenLogName, szBrokenLogName))
				{
					error("Could not move file %s to %s! Errcode: %i", szOldBrokenLogName, szBrokenLogName, errno);
				}
			}
		}
	}

	// delete old directory (if empty)
	if (Util::DirEmpty(szOldDestDir))
	{
		rmdir(szOldDestDir);
	}

	return true;
}
