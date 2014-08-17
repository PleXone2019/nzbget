/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2014 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include <ctype.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <stdio.h>
#include <algorithm>

#include "nzbget.h"
#include "QueueScript.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"

extern Options* g_pOptions;
extern QueueScriptCoordinator* g_pQueueScriptCoordinator;

class QueueScriptController : public Thread, public NZBScriptController
{
private:
	char*				m_szNZBName;
	char*				m_szNZBFilename;
	char*				m_szUrl;
	char*				m_szCategory;
	char*				m_szDestDir;
	int					m_iID;
	int					m_iPriority;
	NZBParameterList	m_Parameters;
	int					m_iPrefixLen;
	Options::Script*	m_pScript;
	QueueScriptCoordinator::EEvent	m_eEvent;
	bool				m_bMarkBad;

	void				PrepareParams(const char* szScriptName);

protected:
	virtual void		ExecuteScript(Options::Script* pScript);
	virtual void		AddMessage(Message::EKind eKind, const char* szText);

public:
	virtual				~QueueScriptController();
	virtual void		Run();
	static void			StartScript(NZBInfo* pNZBInfo, Options::Script* pScript, QueueScriptCoordinator::EEvent eEvent);
};


/**
 * If szStripPrefix is not NULL, only pp-parameters, whose names start with the prefix
 * are processed. The prefix is then stripped from the names.
 * If szStripPrefix is NULL, all pp-parameters are processed; without stripping.
 */
void NZBScriptController::PrepareEnvParameters(NZBParameterList* pParameters, const char* szStripPrefix)
{
	int iPrefixLen = szStripPrefix ? strlen(szStripPrefix) : 0;

	for (NZBParameterList::iterator it = pParameters->begin(); it != pParameters->end(); it++)
	{
		NZBParameter* pParameter = *it;
		const char* szValue = pParameter->GetValue();
		
#ifdef WIN32
		char* szAnsiValue = strdup(szValue);
		WebUtil::Utf8ToAnsi(szAnsiValue, strlen(szAnsiValue) + 1);
		szValue = szAnsiValue;
#endif

		if (szStripPrefix && !strncmp(pParameter->GetName(), szStripPrefix, iPrefixLen) && (int)strlen(pParameter->GetName()) > iPrefixLen)
		{
			SetEnvVarSpecial("NZBPR", pParameter->GetName() + iPrefixLen, szValue);
		}
		else if (!szStripPrefix)
		{
			SetEnvVarSpecial("NZBPR", pParameter->GetName(), szValue);
		}

#ifdef WIN32
		free(szAnsiValue);
#endif
	}
}

void NZBScriptController::PrepareEnvScript(NZBParameterList* pParameters, const char* szScriptName)
{
	if (pParameters)
	{
		PrepareEnvParameters(pParameters, NULL);
	}

	char szParamPrefix[1024];
	snprintf(szParamPrefix, 1024, "%s:", szScriptName);
	szParamPrefix[1024-1] = '\0';

	if (pParameters)
	{
		PrepareEnvParameters(pParameters, szParamPrefix);
	}

	PrepareEnvOptions(szParamPrefix);
}

void NZBScriptController::ExecuteScriptList(const char* szScriptList)
{
	Options::ScriptList scriptList;
	g_pOptions->LoadScriptList(&scriptList);

	for (Options::ScriptList::iterator it = scriptList.begin(); it != scriptList.end(); it++)
	{
		Options::Script* pScript = *it;

		if (szScriptList && *szScriptList)
		{
			// split szScriptList into tokens
			Tokenizer tok(szScriptList, ",;");
			while (const char* szScriptName = tok.Next())
			{
				if (Util::SameFilename(szScriptName, pScript->GetName()))
				{
					ExecuteScript(pScript);
					break;
				}
			}
		}
	}
}


void ScanScriptController::ExecuteScripts(const char* szNZBFilename,
	const char* szUrl, const char* szDirectory, char** pNZBName, char** pCategory,
	int* iPriority, NZBParameterList* pParameters, bool* bAddTop, bool* bAddPaused)
{
	ScanScriptController* pScriptController = new ScanScriptController();

	pScriptController->m_szNZBFilename = szNZBFilename;
	pScriptController->m_szUrl = szUrl;
	pScriptController->m_szDirectory = szDirectory;
	pScriptController->m_pNZBName = pNZBName;
	pScriptController->m_pCategory = pCategory;
	pScriptController->m_pParameters = pParameters;
	pScriptController->m_iPriority = iPriority;
	pScriptController->m_bAddTop = bAddTop;
	pScriptController->m_bAddPaused = bAddPaused;
	pScriptController->m_iPrefixLen = 0;

	pScriptController->ExecuteScriptList(g_pOptions->GetScanScript());

	delete pScriptController;
}

void ScanScriptController::ExecuteScript(Options::Script* pScript)
{
	if (!pScript->GetScanScript() || !Util::FileExists(m_szNZBFilename))
	{
		return;
	}

	PrintMessage(Message::mkInfo, "Executing scan-script %s for %s", pScript->GetName(), Util::BaseFileName(m_szNZBFilename));

	SetScript(pScript->GetLocation());
	SetArgs(NULL, false);

	char szInfoName[1024];
	snprintf(szInfoName, 1024, "scan-script %s for %s", pScript->GetName(), Util::BaseFileName(m_szNZBFilename));
	szInfoName[1024-1] = '\0';
	SetInfoName(szInfoName);

	SetLogPrefix(pScript->GetDisplayName());
	m_iPrefixLen = strlen(pScript->GetDisplayName()) + 2; // 2 = strlen(": ");
	PrepareParams(pScript->GetName());

	Execute();

	SetLogPrefix(NULL);
}

void ScanScriptController::PrepareParams(const char* szScriptName)
{
	ResetEnv();

	SetEnvVar("NZBNP_FILENAME", m_szNZBFilename);
	SetEnvVar("NZBNP_URL", m_szUrl);
	SetEnvVar("NZBNP_NZBNAME", strlen(*m_pNZBName) > 0 ? *m_pNZBName : Util::BaseFileName(m_szNZBFilename));
	SetEnvVar("NZBNP_CATEGORY", *m_pCategory);
	SetIntEnvVar("NZBNP_PRIORITY", *m_iPriority);
	SetIntEnvVar("NZBNP_TOP", *m_bAddTop ? 1 : 0);
	SetIntEnvVar("NZBNP_PAUSED", *m_bAddPaused ? 1 : 0);

	// remove trailing slash
	char szDir[1024];
	strncpy(szDir, m_szDirectory, 1024);
	szDir[1024-1] = '\0';
	int iLen = strlen(szDir);
	if (szDir[iLen-1] == PATH_SEPARATOR)
	{
		szDir[iLen-1] = '\0';
	}
	SetEnvVar("NZBNP_DIRECTORY", szDir);

	PrepareEnvScript(m_pParameters, szScriptName);
}

void ScanScriptController::AddMessage(Message::EKind eKind, const char* szText)
{
	const char* szMsgText = szText + m_iPrefixLen;

	if (!strncmp(szMsgText, "[NZB] ", 6))
	{
		debug("Command %s detected", szMsgText + 6);
		if (!strncmp(szMsgText + 6, "NZBNAME=", 8))
		{
			free(*m_pNZBName);
			*m_pNZBName = strdup(szMsgText + 6 + 8);
		}
		else if (!strncmp(szMsgText + 6, "CATEGORY=", 9))
		{
			free(*m_pCategory);
			*m_pCategory = strdup(szMsgText + 6 + 9);
		}
		else if (!strncmp(szMsgText + 6, "NZBPR_", 6))
		{
			char* szParam = strdup(szMsgText + 6 + 6);
			char* szValue = strchr(szParam, '=');
			if (szValue)
			{
				*szValue = '\0';
				m_pParameters->SetParameter(szParam, szValue + 1);
			}
			else
			{
				error("Invalid command \"%s\" received from %s", szMsgText, GetInfoName());
			}
			free(szParam);
		}
		else if (!strncmp(szMsgText + 6, "PRIORITY=", 9))
		{
			*m_iPriority = atoi(szMsgText + 6 + 9);
		}
		else if (!strncmp(szMsgText + 6, "TOP=", 4))
		{
			*m_bAddTop = atoi(szMsgText + 6 + 4) != 0;
		}
		else if (!strncmp(szMsgText + 6, "PAUSED=", 7))
		{
			*m_bAddPaused = atoi(szMsgText + 6 + 7) != 0;
		}
		else
		{
			error("Invalid command \"%s\" received from %s", szMsgText, GetInfoName());
		}
	}
	else
	{
		ScriptController::AddMessage(eKind, szText);
	}
}


QueueScriptController::~QueueScriptController()
{
	free(m_szNZBName);
	free(m_szNZBFilename);
	free(m_szUrl);
	free(m_szCategory);
	free(m_szDestDir);
}

void QueueScriptController::StartScript(NZBInfo* pNZBInfo, Options::Script* pScript, QueueScriptCoordinator::EEvent eEvent)
{
	QueueScriptController* pScriptController = new QueueScriptController();

	pScriptController->m_szNZBName = strdup(pNZBInfo->GetName());
	pScriptController->m_szNZBFilename = strdup(pNZBInfo->GetFilename());
	pScriptController->m_szUrl = strdup(pNZBInfo->GetURL());
	pScriptController->m_szCategory = strdup(pNZBInfo->GetCategory());
	pScriptController->m_szDestDir = strdup(pNZBInfo->GetDestDir());
	pScriptController->m_iID = pNZBInfo->GetID();
	pScriptController->m_iPriority = pNZBInfo->GetPriority();
	pScriptController->m_Parameters.CopyFrom(pNZBInfo->GetParameters());
	pScriptController->m_pScript = pScript;
	pScriptController->m_eEvent = eEvent;
	pScriptController->m_iPrefixLen = 0;
	pScriptController->m_bMarkBad = false;
	pScriptController->SetAutoDestroy(true);

	pScriptController->Start();
}

void QueueScriptController::Run()
{
	ExecuteScript(m_pScript);

	SetLogPrefix(NULL);

	if (m_bMarkBad)
	{
		DownloadQueue* pDownloadQueue = DownloadQueue::Lock();
		NZBInfo* pNZBInfo = pDownloadQueue->GetQueue()->Find(m_iID);
		if (pNZBInfo)
		{
			PrintMessage(Message::mkWarning, "Cancelling download and deleting %s", m_szNZBName);
			pNZBInfo->SetDeleteStatus(NZBInfo::dsBad);
			pDownloadQueue->EditEntry(m_iID, DownloadQueue::eaGroupDelete, 0, NULL);
		}
		DownloadQueue::Unlock();
	}

	g_pQueueScriptCoordinator->CheckQueue();
}

void QueueScriptController::ExecuteScript(Options::Script* pScript)
{
	PrintMessage(m_eEvent == QueueScriptCoordinator::qeFileDownloaded ? Message::mkDetail : Message::mkInfo,
		"Executing queue-script %s for %s", pScript->GetName(), Util::BaseFileName(m_szNZBName));

	SetScript(pScript->GetLocation());
	SetArgs(NULL, false);

	char szInfoName[1024];
	snprintf(szInfoName, 1024, "queue-script %s for %s", pScript->GetName(), Util::BaseFileName(m_szNZBName));
	szInfoName[1024-1] = '\0';
	SetInfoName(szInfoName);

	SetLogPrefix(pScript->GetDisplayName());
	m_iPrefixLen = strlen(pScript->GetDisplayName()) + 2; // 2 = strlen(": ");
	PrepareParams(pScript->GetName());

	Execute();

	SetLogPrefix(NULL);
}

void QueueScriptController::PrepareParams(const char* szScriptName)
{
	ResetEnv();

	SetEnvVar("NZBNA_NZBNAME", m_szNZBName);
	SetIntEnvVar("NZBNA_NZBID", m_iID);
	SetEnvVar("NZBNA_FILENAME", m_szNZBFilename);
	SetEnvVar("NZBNA_DIRECTORY", m_szDestDir);
	SetEnvVar("NZBNA_URL", m_szUrl);
	SetEnvVar("NZBNA_CATEGORY", m_szCategory);
	SetIntEnvVar("NZBNA_PRIORITY", m_iPriority);
	SetIntEnvVar("NZBNA_LASTID", m_iID);	// deprecated

    const char* szEventName[] = { "FILE_DOWNLOADED", "NZB_ADDED", "NZB_DOWNLOADED" };
	SetEnvVar("NZBNA_EVENT", szEventName[m_eEvent]);

	PrepareEnvScript(&m_Parameters, szScriptName);
}

void QueueScriptController::AddMessage(Message::EKind eKind, const char* szText)
{
	const char* szMsgText = szText + m_iPrefixLen;

	if (!strncmp(szMsgText, "[NZB] ", 6))
	{
		debug("Command %s detected", szMsgText + 6);
		if (!strncmp(szMsgText + 6, "NZBPR_", 6))
		{
			char* szParam = strdup(szMsgText + 6 + 6);
			char* szValue = strchr(szParam, '=');
			if (szValue)
			{
				*szValue = '\0';
				DownloadQueue* pDownloadQueue = DownloadQueue::Lock();
				NZBInfo* pNZBInfo = pDownloadQueue->GetQueue()->Find(m_iID);
				if (pNZBInfo)
				{
					pNZBInfo->GetParameters()->SetParameter(szParam, szValue + 1);
				}
				DownloadQueue::Unlock();
			}
			else
			{
				error("Invalid command \"%s\" received from %s", szMsgText, GetInfoName());
			}
			free(szParam);
		}
		else if (!strncmp(szMsgText + 6, "MARK=BAD", 8))
		{
			m_bMarkBad = true;
			DownloadQueue* pDownloadQueue = DownloadQueue::Lock();
			NZBInfo* pNZBInfo = pDownloadQueue->GetQueue()->Find(m_iID);
			if (pNZBInfo)
			{
				SetLogPrefix(NULL);
				PrintMessage(Message::mkWarning, "Marking %s as bad", m_szNZBName);
				SetLogPrefix(m_pScript->GetDisplayName());
				pNZBInfo->SetMarkStatus(NZBInfo::ksBad);
			}
			DownloadQueue::Unlock();
		}
		else
		{
			error("Invalid command \"%s\" received from %s", szMsgText, GetInfoName());
		}
	}
	else
	{
		ScriptController::AddMessage(eKind, szText);
	}
}


QueueScriptCoordinator::QueueItem::QueueItem(int iNZBID, Options::Script* pScript, EEvent eEvent)
{
	m_iNZBID = iNZBID;
	m_pScript = pScript;
	m_eEvent = eEvent;
}

QueueScriptCoordinator::QueueScriptCoordinator()
{
	m_pCurItem = NULL;
}

QueueScriptCoordinator::~QueueScriptCoordinator()
{
	delete m_pCurItem;
	for (Queue::iterator it = m_Queue.begin(); it != m_Queue.end(); )
	{
		delete *it;
	}
}

void QueueScriptCoordinator::InitOptions()
{
	m_ScriptList.Clear();
	g_pOptions->LoadScriptList(&m_ScriptList);

	m_bHasQueueScripts = false;
	for (Options::ScriptList::iterator it = m_ScriptList.begin(); it != m_ScriptList.end(); it++)
	{
		Options::Script* pScript = *it;
		if (pScript->GetQueueScript())
		{
			m_bHasQueueScripts = true;
			break;
		}
	}
}

void QueueScriptCoordinator::EnqueueScript(NZBInfo* pNZBInfo, EEvent eEvent)
{
	m_mutexQueue.Lock();

	if (eEvent == qeNzbAdded || eEvent == qeNzbDownloaded)
	{
		// reread script list on important events
		InitOptions();
	}

	if (!m_bHasQueueScripts)
	{
		m_mutexQueue.Unlock();
		return;
	}

	if (eEvent == qeNzbDownloaded)
	{
		// delete all other queued scripts for this nzb
		for (Queue::iterator it = m_Queue.begin(); it != m_Queue.end(); )
		{
			QueueItem* pQueueItem = *it;
			if (pQueueItem->GetNZBID() == pNZBInfo->GetID())
			{
				delete pQueueItem;
				it = m_Queue.erase(it);
				continue;
			}
			it++;
		}
	}

	for (Options::ScriptList::iterator it = m_ScriptList.begin(); it != m_ScriptList.end(); it++)
	{
		Options::Script* pScript = *it;

		if (!pScript->GetQueueScript())
		{
			continue;
		}

		bool bUseScript = false;

		// check queue-scripts
		const char* szQueueScript = g_pOptions->GetQueueScript();
		if (!Util::EmptyStr(szQueueScript))
		{
			// split szQueueScript into tokens
			Tokenizer tok(szQueueScript, ",;");
			while (const char* szScriptName = tok.Next())
			{
				if (Util::SameFilename(szScriptName, pScript->GetName()))
				{
					bUseScript = true;
					break;
				}
			}
		}

		// check post-processing-scripts
		if (!bUseScript)
		{
			for (NZBParameterList::iterator it = pNZBInfo->GetParameters()->begin(); it != pNZBInfo->GetParameters()->end(); it++)
			{
				NZBParameter* pParameter = *it;
				const char* szVarname = pParameter->GetName();
				if (strlen(szVarname) > 0 && szVarname[0] != '*' && szVarname[strlen(szVarname)-1] == ':' &&
					(!strcasecmp(pParameter->GetValue(), "yes") || !strcasecmp(pParameter->GetValue(), "on") || !strcasecmp(pParameter->GetValue(), "1")))
				{
					char szScriptName[1024];
					strncpy(szScriptName, szVarname, 1024);
					szScriptName[1024-1] = '\0';
					szScriptName[strlen(szScriptName)-1] = '\0'; // remove trailing ':'
					if (Util::SameFilename(szScriptName, pScript->GetName()))
					{
						bUseScript = true;
						break;
					}
				}
			}
		}

		if (bUseScript)
		{
			bool bAlreadyQueued = false;
			if (eEvent == qeFileDownloaded)
			{
				// check if this script is already queued for this nzb
				for (Queue::iterator it2 = m_Queue.begin(); it2 != m_Queue.end(); it2++)
				{
					QueueItem* pQueueItem = *it2;
					if (pQueueItem->GetNZBID() == pNZBInfo->GetID() && pQueueItem->GetScript() == pScript)
					{
						bAlreadyQueued = true;
						break;
					}
				}
			}

			if (!bAlreadyQueued)
			{
				QueueItem* pQueueItem = new QueueItem(pNZBInfo->GetID(), pScript, eEvent);
				if (m_pCurItem)
				{
					m_Queue.push_back(pQueueItem);
				}
				else
				{
					StartScript(pNZBInfo, pQueueItem);
				}
			}
		}
	}

	m_mutexQueue.Unlock();
}

void QueueScriptCoordinator::CheckQueue()
{
	DownloadQueue* pDownloadQueue = DownloadQueue::Lock();
	m_mutexQueue.Lock();

	delete m_pCurItem;

	m_pCurItem = NULL;
	NZBInfo* pCurNZBInfo = NULL;
	Queue::iterator itCurItem = m_Queue.end();

	for (Queue::iterator it = m_Queue.begin(); it != m_Queue.end(); )
	{
		QueueItem* pQueueItem = *it;

		NZBInfo* pNZBInfo = pDownloadQueue->GetQueue()->Find(pQueueItem->GetNZBID());

		// in a case this nzb must not be processed further -
		// delete all scripts queued for the nzb from queue
		if (!pNZBInfo || pNZBInfo->GetDeleteStatus() != NZBInfo::dsNone ||
			pNZBInfo->GetMarkStatus() == NZBInfo::ksBad)
		{
			delete pQueueItem;
			it = m_Queue.erase(it);
			continue;
		}

		if (!m_pCurItem || pQueueItem->GetEvent() > m_pCurItem->GetEvent())
		{
			m_pCurItem = pQueueItem;
			itCurItem = it;
			pCurNZBInfo = pNZBInfo;
		}

		it++;
	}

	if (m_pCurItem)
	{
		m_Queue.erase(itCurItem);
		StartScript(pCurNZBInfo, m_pCurItem);
	}

	m_mutexQueue.Unlock();
	DownloadQueue::Unlock();
}

void QueueScriptCoordinator::StartScript(NZBInfo* pNZBInfo, QueueItem* pQueueItem)
{
	m_pCurItem = pQueueItem;
	QueueScriptController::StartScript(pNZBInfo, pQueueItem->GetScript(), pQueueItem->GetEvent());
}

bool QueueScriptCoordinator::HasJob(int iNZBID)
{
	m_mutexQueue.Lock();
	bool bWorking = m_pCurItem && m_pCurItem->GetNZBID() == iNZBID;
	if (!bWorking)
	{
		for (Queue::iterator it = m_Queue.begin(); it != m_Queue.end(); it++)
		{
			QueueItem* pQueueItem = *it;
			bWorking = pQueueItem->GetNZBID() == iNZBID;
			if (bWorking)
			{
				break;
			}
		}
	}
	m_mutexQueue.Unlock();

	return bWorking;
}
