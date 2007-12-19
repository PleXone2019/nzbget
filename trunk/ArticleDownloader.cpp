/*
 *  This file if part of nzbget
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>
#ifdef WIN32
#include <direct.h>
#else
#include <unistd.h>
#include <sys/time.h>
#endif
#include <sys/stat.h>

#include "nzbget.h"
#include "ArticleDownloader.h"
#include "Decoder.h"
#include "Log.h"
#include "Options.h"
#include "ServerPool.h"
#include "Util.h"

extern DownloadSpeedMeter* g_pDownloadSpeedMeter;
extern Options* g_pOptions;
extern ServerPool* g_pServerPool;

const char* ArticleDownloader::m_szJobStatus[] = { "WAITING", "RUNNING", "FINISHED", "FAILED", "DECODING", "JOINING", "NOT_FOUND", "FATAL_ERROR" };

ArticleDownloader::ArticleDownloader()
{
	debug("Creating ArticleDownloader");

	m_szResultFilename	= NULL;
	m_szTempFilename	= NULL;
	m_szArticleFilename	= NULL;
	m_szInfoName		= NULL;
	m_pConnection		= NULL;
	m_pDecoder			= NULL;
	m_eStatus			= adUndefined;
	m_iBytes			= 0;
	memset(&m_tStartTime, 0, sizeof(m_tStartTime));
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
	if (m_pDecoder)
	{
		delete m_pDecoder;
	}
}

void ArticleDownloader::SetTempFilename(const char* v)
{
	m_szTempFilename = strdup(v);
}

void ArticleDownloader::SetInfoName(const char * v)
{
	m_szInfoName = strdup(v);
}

void ArticleDownloader::SetStatus(EStatus eStatus)
{
	m_eStatus = eStatus;
	Notify(NULL);
}

void ArticleDownloader::Run()
{
	debug("Entering ArticleDownloader-loop");

	SetStatus(adRunning);
	m_szResultFilename = m_pArticleInfo->GetResultFilename();

	if (g_pOptions->GetContinuePartial())
	{
		struct stat buffer;
		bool fileExists = !stat(m_szResultFilename, &buffer);
		if (fileExists)
		{
			// file exists from previous program's start
			info("Article %s already downloaded, skipping", m_szInfoName);
			m_semInitialized.Post();
			m_semWaited.Wait();
			SetStatus(adFinished);
			return;
		}
	}

	info("Downloading %s", m_szInfoName);

	int retry = g_pOptions->GetRetries();

	EStatus Status = adFailed;
	int iMaxLevel = g_pServerPool->GetMaxLevel();
	int* LevelStatus = (int*)malloc((iMaxLevel + 1) * sizeof(int));
	for (int i = 0; i <= iMaxLevel; i++)
	{
		LevelStatus[i] = 0;
	}
	int level = 0;

	m_semInitialized.Post();
	m_semWaited.Wait();

	//while (true) usleep(10); // DEBUG TEST

	while (!IsStopped() && (retry > 0))
	{
		SetLastUpdateTimeNow();

		Status = adFailed;

		if (!m_pConnection)
		{
			m_pConnection = g_pServerPool->GetConnection(level);
		}

		if (IsStopped())
		{
			Status = adFailed;
			break;
		}

		if (!m_pConnection)
		{
			debug("m_pConnection is NULL");
			error("Serious error: Connection is NULL");
		}
		
		// test connection
		bool connected = m_pConnection && m_pConnection->Connect() >= 0;
		if (connected && !IsStopped())
		{
			// Okay, we got a Connection. Now start downloading!!
			Status = Download();
		}

		if (connected)
		{
			// freeing connection allows other threads to start.
			// we doing this only if the problem was with article or group.
			// if the problem occurs by Connect() we do not free the connection,
			// to prevent starting of thousands of threads (cause each of them
			// will also free it's connection after the same connect-error).
			FreeConnection();
		}

		if ((Status == adFailed) && ((retry > 1) || !connected) && !IsStopped())
		{
			info("Waiting %i sec to retry", g_pOptions->GetRetryInterval());
			int msec = 0;
			while (!IsStopped() && (msec < g_pOptions->GetRetryInterval() * 1000))
			{
				usleep(100 * 1000);
				msec += 100;
			}
		}

		if (IsStopped())
		{
			Status = adFailed;
			break;
		}

		if ((Status == adFinished) || (Status == adFatalError))
		{
			break;
		}

		LevelStatus[level] = Status;

		bool bAllLevelNotFound = true;
		for (int lev = 0; lev <= iMaxLevel; lev++)
		{
			if (LevelStatus[lev] != adNotFound)
			{
				bAllLevelNotFound = false;
				break;
			}
		}
		if (bAllLevelNotFound)
		{
			if (iMaxLevel > 0)
			{
				warn("Aticle %s @ all servers failed: Article not found", m_szInfoName);
			}
			break;
		}

		// do not count connect-errors, only article- and group-errors
		if (connected)
		{
			level++;
			if (level > iMaxLevel)
			{
				level = 0;
			}
			retry--;
		}
	}

	FreeConnection();

	free(LevelStatus);

	if (Status != adFinished)
	{
		Status = adFailed;
	}

	if (Status == adFailed)
	{
		if (IsStopped())
		{
			info("Download %s cancelled", m_szInfoName);
		}
		else
		{
			warn("Download %s failed", m_szInfoName);
		}
	}

	SetStatus(Status);

	debug("Existing ArticleDownloader-loop");
}

ArticleDownloader::EStatus ArticleDownloader::Download()
{
	// at first, change group! dryan's level wants it this way... ;-)
	bool grpchanged = false;
	for (FileInfo::Groups::iterator it = m_pFileInfo->GetGroups()->begin(); it != m_pFileInfo->GetGroups()->end(); it++)
	{
		grpchanged = m_pConnection->JoinGroup(*it) == 0;
		if (grpchanged)
		{
			break;
		}
	}

	if (!grpchanged)
	{
		warn("Article %s @ %s failed: Could not join group", m_szInfoName, m_pConnection->GetServer()->GetHost());
		return adFailed;
	}

	// now, let's begin!
	char tmp[1024];
	snprintf(tmp, 1024, "ARTICLE %s\r\n", m_pArticleInfo->GetMessageID());
	tmp[1024-1] = '\0';

	char* answer = NULL;

	for (int retry = 3; retry > 0; retry--)
	{
		answer = m_pConnection->Request(tmp);
		if (answer && (!strncmp(answer, "2", 1)))
		{
			break;
		}
	}

	if (!answer)
	{
		warn("Article %s @ %s failed: Connection closed by remote host", m_szInfoName, m_pConnection->GetServer()->GetHost());
		return adFailed;
	}
	if (strncmp(answer, "2", 1))
	{
		warn("Article %s @ %s failed: %s", m_szInfoName, m_pConnection->GetServer()->GetHost(), answer);
		return adNotFound;
	}

	// positive answer!

	const char* dnfilename = m_szTempFilename;
	FILE* outfile = fopen(dnfilename, "w");
                                                                        
	if (!outfile)
	{
		error("Could not create file %s", dnfilename);
		return adFatalError;
	}

	gettimeofday(&m_tStartTime, 0);
	m_iBytes = 0;

	EStatus Status = adRunning;
	const int LineBufSize = 1024*10;
	char* szLineBuf = (char*)malloc(LineBufSize);

	while (!IsStopped())
	{
		SetLastUpdateTimeNow();

		// Throttle the bandwidth
		while (!IsStopped() && (g_pOptions->GetDownloadRate() > 0.0f) &&
		        (g_pDownloadSpeedMeter->CalcCurrentDownloadSpeed() > g_pOptions->GetDownloadRate()))
		{
			SetLastUpdateTimeNow();
			usleep(200 * 1000);
		}

		struct _timeval tSpeedReadingStartTime;
		gettimeofday(&tSpeedReadingStartTime, 0);

		char* line = m_pConnection->ReadLine(szLineBuf, LineBufSize);

		// Have we encountered a timeout?
		if (!line)
		{
			Status = adFailed;
			break;
		}

		//detect end of article
		if ((!strcmp(line, ".\r\n")) || (!strcmp(line, ".\n")))
		{
			break;
		}

		// Did we meet an unexpected need for authorization?
		if (!strncmp(line, "480", 3))
		{
			m_pConnection->Authenticate();
		}

		//detect lines starting with "." (marked as "..")
		if (!strncmp(line, "..", 2))
		{
			line++;
		}

		int wrcnt = (int)fwrite(line, 1, strlen(line), outfile);
		if (wrcnt > 0)
		{
			m_iBytes += wrcnt;
		}
	}

	free(szLineBuf);
	fflush(outfile);
	fclose(outfile);

	if (IsStopped())
	{
		remove(dnfilename);
		return adFailed;
	}

	if (Status == adFailed)
	{
		warn("Unexpected end of %s", m_szInfoName);
		remove(dnfilename);
		return adFailed;
	}

	FreeConnection();

	if ((g_pOptions->GetDecoder() == Options::dcUulib) ||
	        (g_pOptions->GetDecoder() == Options::dcYenc))
	{
		// Give time to other threads. Help to avoid hangs on Asus WL500g router.
		usleep(10 * 1000);

		SetStatus(adDecoding);
		struct _timeval StartTime, EndTime;
		gettimeofday(&StartTime, 0);
		bool OK = false;
		if (g_pOptions->GetDecoder() == Options::dcUulib)
		{
			m_pDecoder = new Decoder();
			m_pDecoder->SetKind(Decoder::dcUulib);
		}
		else if (g_pOptions->GetDecoder() == Options::dcYenc)
		{
			m_pDecoder = new Decoder();
			m_pDecoder->SetKind(Decoder::dcYenc);
		}
		if (m_pDecoder)
		{
			m_pDecoder->SetSrcFilename(dnfilename);

			char tmpdestfile[1024];
			snprintf(tmpdestfile, 1024, "%s.dec", m_szResultFilename);
			tmpdestfile[1024-1] = '\0';
					
			m_pDecoder->SetDestFilename(tmpdestfile);
			OK = m_pDecoder->Execute();
			if (OK)
			{
				rename(tmpdestfile, m_szResultFilename);
			}
			else
			{
				remove(tmpdestfile);
			}
			if (m_pDecoder->GetArticleFilename())
			{
				m_szArticleFilename = strdup(m_pDecoder->GetArticleFilename());
			}
			delete m_pDecoder;
			m_pDecoder = NULL;
		}

		gettimeofday(&EndTime, 0);
		remove(dnfilename);
#ifdef WIN32
		float fDeltaTime = (float)((EndTime.time - StartTime.time) * 1000 + (EndTime.millitm - StartTime.millitm));
#else
		float fDeltaTime = ((EndTime.tv_sec - StartTime.tv_sec) * 1000000 + (EndTime.tv_usec - StartTime.tv_usec)) / 1000.0;
#endif
		if (OK)
		{
			info("Successfully downloaded %s", m_szInfoName);
			debug("Decode time %.1f ms", fDeltaTime);
			return adFinished;
		}
		else
		{
			warn("Decoding %s failed", m_szInfoName);
			remove(m_szResultFilename);
			return adFailed;
		}
	}
	else if (g_pOptions->GetDecoder() == Options::dcNone)
	{
		// rawmode
		rename(dnfilename, m_szResultFilename);
		info("Article %s successfully downloaded", m_szInfoName);
		return adFinished;
	}
	else
	{
		// should not occur
		error("Internal error: Decoding %s failed", m_szInfoName);
		return adFatalError;
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

	debug("      Download: status=%s, LastUpdateTime=%s, filename=%s", GetStatusText(), szTime, BaseFileName(GetTempFilename()));
	if (m_pDecoder)
	{
		m_pDecoder->LogDebugInfo();
	}
}

void ArticleDownloader::Stop()
{
	debug("Trying to stop ArticleDownloader");
	Thread::Stop();
	m_mutexConnection.Lock();
	if (m_pConnection)
	{
		m_pConnection->Cancel();
	}
	m_mutexConnection.Unlock();
	debug("ArticleDownloader stopped successfuly");
}

void ArticleDownloader::FreeConnection()
{
	if (m_pConnection)
	{
		debug("Releasing connection");
		m_mutexConnection.Lock();
		m_pConnection->Disconnect();
		g_pServerPool->FreeConnection(m_pConnection);
		m_pConnection = NULL;
		m_mutexConnection.Unlock();
	}
}

void ArticleDownloader::CompleteFileParts()
{
	debug("Completing file parts");
	debug("ArticleFilename: %s", m_pFileInfo->GetFilename());
	SetStatus(adJoining);

	char ofn[1024];
	snprintf(ofn, 1024, "%s%c%s", m_pFileInfo->GetDestDir(), (int)PATH_SEPARATOR, m_pFileInfo->GetFilename());
	ofn[1024-1] = '\0';

	char szNZBNiceName[1024];
	m_pFileInfo->GetNiceNZBName(szNZBNiceName, 1024);
	
	char InfoFilename[1024];
	snprintf(InfoFilename, 1024, "%s%c%s", szNZBNiceName, (int)PATH_SEPARATOR, m_pFileInfo->GetFilename());
	InfoFilename[1024-1] = '\0';

	// Ensure the DstDir is created
	mkdir(m_pFileInfo->GetDestDir(), S_DIRMODE);

	if (g_pOptions->GetDecoder() == Options::dcNone)
	{
		info("Moving articles for %s", InfoFilename);
	}
	else
	{
		info("Joining articles for %s", InfoFilename);
	}

	// prevent overwriting existing files
	struct stat statbuf;
	int dupcount = 0;
	while (!stat(ofn, &statbuf))
	{
		dupcount++;
		snprintf(ofn, 1024, "%s%c%s_duplicate%d", m_pFileInfo->GetDestDir(), (int)PATH_SEPARATOR, m_pFileInfo->GetFilename(), dupcount);
		ofn[1024-1] = '\0';
	}

	FILE* outfile = NULL;

	char tmpdestfile[1024];
	snprintf(tmpdestfile, 1024, "%s.tmp", ofn);
	tmpdestfile[1024-1] = '\0';
	remove(tmpdestfile);

	if ((g_pOptions->GetDecoder() == Options::dcUulib) ||
	        (g_pOptions->GetDecoder() == Options::dcYenc))
	{
		outfile = fopen(tmpdestfile, "w+");
		if (!outfile)
		{
			error("Could not create file %s!", tmpdestfile);
			SetStatus(adFinished);
			return;
		}
	}
	else if (g_pOptions->GetDecoder() == Options::dcNone)
	{
		mkdir(ofn, S_DIRMODE);
	}

	bool complete = true;
	int iBrokenCount = 0;
	static const int BUFFER_SIZE = 1024 * 50;
	char* buffer = (char*)malloc(BUFFER_SIZE);

	for (FileInfo::Articles::iterator it = m_pFileInfo->GetArticles()->begin(); it != m_pFileInfo->GetArticles()->end(); it++)
	{
		ArticleInfo* pa = *it;
		if (pa->GetStatus() != ArticleInfo::aiFinished)
		{
			iBrokenCount++;
			complete = false;
		}
		else if ((g_pOptions->GetDecoder() == Options::dcUulib) ||
		         (g_pOptions->GetDecoder() == Options::dcYenc))
		{
			FILE* infile;
			const char* fn = pa->GetResultFilename();

			infile = fopen(fn, "r");
			if (infile)
			{
				int cnt = BUFFER_SIZE;

				while (cnt == BUFFER_SIZE)
				{
					cnt = (int)fread(buffer, 1, BUFFER_SIZE, infile);
					fwrite(buffer, 1, cnt, outfile);
					SetLastUpdateTimeNow();
					usleep(10); // give time to other threads
				}

				fclose(infile);
			}
			else
			{
				complete = false;
				iBrokenCount++;
				info("Could not find file %s. Status is broken", fn);
			}
		}
		else if (g_pOptions->GetDecoder() == Options::dcNone)
		{
			const char* fn = pa->GetResultFilename();
			char dstFileName[1024];
			snprintf(dstFileName, 1024, "%s%c%03i", ofn, (int)PATH_SEPARATOR, pa->GetPartNumber());
			dstFileName[1024-1] = '\0';
			rename(fn, dstFileName);
		}
	}
	free(buffer);

	if ((g_pOptions->GetDecoder() == Options::dcUulib) ||
	        (g_pOptions->GetDecoder() == Options::dcYenc))
	{
		fclose(outfile);
		rename(tmpdestfile, ofn);
	}

	for (FileInfo::Articles::iterator it = m_pFileInfo->GetArticles()->begin(); it != m_pFileInfo->GetArticles()->end(); it++)
	{
		ArticleInfo* pa = *it;
		remove(pa->GetResultFilename());
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
			bool OK = rename(ofn, brokenfn) == 0;
			if (OK)
			{
				info("Renaming broken file from %s to %s", ofn, brokenfn);
			}
			else
			{
				warn("Renaming broken file from %s to %s failed", ofn, brokenfn);
			}
		}
		else
		{
			info("Not renaming broken file %s", ofn);
		}

		if (g_pOptions->GetCreateBrokenLog())
		{
			char szBrokenLogName[1024];
			snprintf(szBrokenLogName, 1024, "%s%c_brokenlog.txt", m_pFileInfo->GetDestDir(), (int)PATH_SEPARATOR);
			szBrokenLogName[1024-1] = '\0';
			FILE* file = fopen(szBrokenLogName, "a");
			fprintf(file, "%s (%i/%i)\n", m_pFileInfo->GetFilename(), m_pFileInfo->GetArticles()->size() - iBrokenCount, m_pFileInfo->GetArticles()->size());
			fclose(file);
		}

		warn("%s is incomplete!", InfoFilename);
	}

	SetStatus(adFinished);
}

bool ArticleDownloader::Terminate()
{
	NNTPConnection* pConnection = m_pConnection;
	bool terminated = Kill();
	if (terminated && pConnection)
	{
		debug("Terminating connection");
		pConnection->Cancel();
		g_pServerPool->FreeConnection(pConnection);
	}
	return terminated;
}

void ArticleDownloader::WaitInit()
{
	// waiting until the download becomes ready to catch connection,
	// but no longer then 30 seconds
	m_semInitialized.TimedWait(30000);
	m_semWaited.Post();
}
