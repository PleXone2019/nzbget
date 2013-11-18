/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include <sys/wait.h>
#include <signal.h>
#endif
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>

#include "nzbget.h"
#include "ScriptController.h"
#include "Log.h"
#include "Util.h"

// System global variable holding environments variables
extern char** environ;

extern Options* g_pOptions;
extern char* (*g_szEnvironmentVariables)[];
extern DownloadQueueHolder* g_pDownloadQueueHolder;

static const int POSTPROCESS_PARCHECK = 92;
static const int POSTPROCESS_SUCCESS = 93;
static const int POSTPROCESS_ERROR = 94;
static const int POSTPROCESS_NONE = 95;

#ifndef WIN32
#define CHILD_WATCHDOG 1
#endif

#ifdef CHILD_WATCHDOG
/**
 * Sometimes the forked child process doesn't start properly and hangs
 * just during the starting. I didn't find any explanation about what
 * could cause that problem except of a general advice, that
 * "a forking in a multithread application is not recommended".
 *
 * Workaround:
 * 1) child process prints a line into stdout directly after the start;
 * 2) parent process waits for a line for 60 seconds. If it didn't receive it
 *    the cild process assumed to hang and will be killed. Another attempt
 *    will be made.
 */
 
class ChildWatchDog : public Thread
{
private:
	pid_t			m_hProcessID;
protected:
	virtual void	Run();
public:
	void			SetProcessID(pid_t hProcessID) { m_hProcessID = hProcessID; }
};

void ChildWatchDog::Run()
{
	static const int WAIT_SECONDS = 60;
	time_t tStart = time(NULL);
	while (!IsStopped() && (time(NULL) - tStart) < WAIT_SECONDS)
	{
		usleep(10 * 1000);
	}

	if (!IsStopped())
	{
		info("Restarting hanging child process");
		kill(m_hProcessID, SIGKILL);
	}
}
#endif


EnvironmentStrings::EnvironmentStrings()
{
}

EnvironmentStrings::~EnvironmentStrings()
{
	Clear();
}

void EnvironmentStrings::Clear()
{
	for (Strings::iterator it = m_strings.begin(); it != m_strings.end(); it++)
	{
		free(*it);
	}
	m_strings.clear();
}

void EnvironmentStrings::InitFromCurrentProcess()
{
	for (int i = 0; (*g_szEnvironmentVariables)[i]; i++)
	{
		char* szVar = (*g_szEnvironmentVariables)[i];
		Append(strdup(szVar));
	}
}

void EnvironmentStrings::Append(char* szString)
{
	m_strings.push_back(szString);
}

#ifdef WIN32
/*
 * Returns environment block in format suitable for using with CreateProcess. 
 * The allocated memory must be freed by caller using "free()".
 */
char* EnvironmentStrings::GetStrings()
{
	int iSize = 1;
	for (Strings::iterator it = m_strings.begin(); it != m_strings.end(); it++)
	{
		char* szVar = *it;
		iSize += strlen(szVar) + 1;
	}

	char* szStrings = (char*)malloc(iSize);
	char* szPtr = szStrings;
	for (Strings::iterator it = m_strings.begin(); it != m_strings.end(); it++)
	{
		char* szVar = *it;
		strcpy(szPtr, szVar);
		szPtr += strlen(szVar) + 1;
	}
	*szPtr = '\0';

	return szStrings;
}

#else

/*
 * Returns environment block in format suitable for using with execve 
 * The allocated memory must be freed by caller using "free()".
 */
char** EnvironmentStrings::GetStrings()
{
	char** pStrings = (char**)malloc((m_strings.size() + 1) * sizeof(char*));
	char** pPtr = pStrings;
	for (Strings::iterator it = m_strings.begin(); it != m_strings.end(); it++)
	{
		char* szVar = *it;
		*pPtr = szVar;
		pPtr++;
	}
	*pPtr = NULL;

	return pStrings;
}
#endif


ScriptController::ScriptController()
{
	m_szScript = NULL;
	m_szWorkingDir = NULL;
	m_szArgs = NULL;
	m_bFreeArgs = false;
	m_szInfoName = NULL;
	m_szLogPrefix = NULL;
	m_bTerminated = false;
	m_bDetached = false;
	m_environmentStrings.InitFromCurrentProcess();
}

ScriptController::~ScriptController()
{
	if (m_bFreeArgs)
	{
		for (const char** szArgPtr = m_szArgs; *szArgPtr; szArgPtr++)
		{
			free((char*)*szArgPtr);
		}
		free(m_szArgs);
	}
}

void ScriptController::ResetEnv()
{
	m_environmentStrings.Clear();
	m_environmentStrings.InitFromCurrentProcess();
}

void ScriptController::SetEnvVar(const char* szName, const char* szValue)
{
	int iLen = strlen(szName) + strlen(szValue) + 2;
	char* szVar = (char*)malloc(iLen);
	snprintf(szVar, iLen, "%s=%s", szName, szValue);
	m_environmentStrings.Append(szVar);
}

void ScriptController::SetIntEnvVar(const char* szName, int iValue)
{
	char szValue[1024];
	snprintf(szValue, 10, "%i", iValue);
	szValue[1024-1] = '\0';
	SetEnvVar(szName, szValue);
}

/**
 * If szStripPrefix is not NULL, only options, whose names start with the prefix
 * are processed. The prefix is then stripped from the names.
 * If szStripPrefix is NULL, all options are processed; without stripping.
 */
void ScriptController::PrepareEnvOptions(const char* szStripPrefix)
{
	int iPrefixLen = szStripPrefix ? strlen(szStripPrefix) : 0;

	Options::OptEntries* pOptEntries = g_pOptions->LockOptEntries();

	for (Options::OptEntries::iterator it = pOptEntries->begin(); it != pOptEntries->end(); it++)
	{
		Options::OptEntry* pOptEntry = *it;
		
		if (szStripPrefix && !strncmp(pOptEntry->GetName(), szStripPrefix, iPrefixLen) && (int)strlen(pOptEntry->GetName()) > iPrefixLen)
		{
			SetEnvVarSpecial("NZBPO", pOptEntry->GetName() + iPrefixLen, pOptEntry->GetValue());
		}
		else if (!szStripPrefix)
		{
			SetEnvVarSpecial("NZBOP", pOptEntry->GetName(), pOptEntry->GetValue());
		}
	}

	g_pOptions->UnlockOptEntries();
}

/**
 * If szStripPrefix is not NULL, only pp-parameters, whose names start with the prefix
 * are processed. The prefix is then stripped from the names.
 * If szStripPrefix is NULL, all pp-parameters are processed; without stripping.
 */
void ScriptController::PrepareEnvParameters(NZBInfo* pNZBInfo, const char* szStripPrefix)
{
	int iPrefixLen = szStripPrefix ? strlen(szStripPrefix) : 0;

	for (NZBParameterList::iterator it = pNZBInfo->GetParameters()->begin(); it != pNZBInfo->GetParameters()->end(); it++)
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

void ScriptController::SetEnvVarSpecial(const char* szPrefix, const char* szName, const char* szValue)
{
	char szVarname[1024];
	snprintf(szVarname, sizeof(szVarname), "%s_%s", szPrefix, szName);
	szVarname[1024-1] = '\0';
	
	// Original name
	SetEnvVar(szVarname, szValue);
	
	char szNormVarname[1024];
	strncpy(szNormVarname, szVarname, sizeof(szVarname));
	szNormVarname[1024-1] = '\0';
	
	// Replace special characters  with "_" and convert to upper case
	for (char* szPtr = szNormVarname; *szPtr; szPtr++)
	{
		if (strchr(".:*!\"$%&/()=`+~#'{}[]@- ", *szPtr)) *szPtr = '_';
		*szPtr = toupper(*szPtr);
	}
	
	// Another env var with normalized name (replaced special chars and converted to upper case)
	if (strcmp(szVarname, szNormVarname))
	{
		SetEnvVar(szNormVarname, szValue);
	}
}

void ScriptController::PrepareArgs()
{
#ifdef WIN32
	if (!m_szArgs)
	{
		// Special support for script languages:
		// automatically find the app registered for this extension and run it
		const char* szExtension = strrchr(GetScript(), '.');
		if (szExtension && strcasecmp(szExtension, ".exe") && strcasecmp(szExtension, ".bat") && strcasecmp(szExtension, ".cmd"))
		{
			debug("Looking for associated program for %s", szExtension);
			char szCommand[512];
			int iBufLen = 512-1;
			if (Util::RegReadStr(HKEY_CLASSES_ROOT, szExtension, NULL, szCommand, &iBufLen))
			{
				szCommand[iBufLen] = '\0';
				debug("Extension: %s", szCommand);
				
				char szRegPath[512];
				snprintf(szRegPath, 512, "%s\\shell\\open\\command", szCommand);
				szRegPath[512-1] = '\0';
				
				iBufLen = 512-1;
				if (Util::RegReadStr(HKEY_CLASSES_ROOT, szRegPath, NULL, szCommand, &iBufLen))
				{
					szCommand[iBufLen] = '\0';
					debug("Command: %s", szCommand);
					
					DWORD_PTR pArgs[] = { (DWORD_PTR)GetScript(), (DWORD_PTR)0 };
					if (FormatMessage(FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ARGUMENT_ARRAY, szCommand, 0, 0,
									  m_szCmdLine, sizeof(m_szCmdLine), (va_list*)pArgs))
					{
						debug("CmdLine: %s", m_szCmdLine);
						return;
					}
				}
			}
			warn("Could not found associated program for %s. Trying to execute %s directly", szExtension, Util::BaseFileName(GetScript()));
		}
	}
#endif

	if (!m_szArgs)
	{
		m_szStdArgs[0] = GetScript();
		m_szStdArgs[1] = NULL;
		SetArgs(m_szStdArgs, false);
	}
}

int ScriptController::Execute()
{
	PrepareEnvOptions(NULL);
	PrepareArgs();

	int iExitCode = 0;
	int pipein;

#ifdef CHILD_WATCHDOG
	bool bChildConfirmed = false;
	while (!bChildConfirmed && !m_bTerminated)
	{
#endif

#ifdef WIN32
	// build command line

	char* szCmdLine = NULL;
	if (m_szArgs)
	{
		char szCmdLineBuf[2048];
		int iUsedLen = 0;
		for (const char** szArgPtr = m_szArgs; *szArgPtr; szArgPtr++)
		{
			snprintf(szCmdLineBuf + iUsedLen, 2048 - iUsedLen, "\"%s\" ", *szArgPtr);
			iUsedLen += strlen(*szArgPtr) + 3;
		}
		szCmdLineBuf[iUsedLen < 2048 ? iUsedLen - 1 : 2048 - 1] = '\0';
		szCmdLine = szCmdLineBuf;
	}
	else
	{
		szCmdLine = m_szCmdLine;
	}
	
	// create pipes to write and read data
	HANDLE hReadPipe, hWritePipe;
	SECURITY_ATTRIBUTES SecurityAttributes;
	memset(&SecurityAttributes, 0, sizeof(SecurityAttributes));
	SecurityAttributes.nLength = sizeof(SecurityAttributes);
	SecurityAttributes.bInheritHandle = TRUE;

	CreatePipe(&hReadPipe, &hWritePipe, &SecurityAttributes, 0);

	STARTUPINFO StartupInfo;
	memset(&StartupInfo, 0, sizeof(StartupInfo));
	StartupInfo.cb = sizeof(StartupInfo);
	StartupInfo.dwFlags = STARTF_USESTDHANDLES;
	StartupInfo.hStdInput = 0;
	StartupInfo.hStdOutput = hWritePipe;
	StartupInfo.hStdError = hWritePipe;

	PROCESS_INFORMATION ProcessInfo;
	memset(&ProcessInfo, 0, sizeof(ProcessInfo));

	char* szEnvironmentStrings = m_environmentStrings.GetStrings();

	BOOL bOK = CreateProcess(NULL, szCmdLine, NULL, NULL, TRUE, NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW, szEnvironmentStrings, m_szWorkingDir, &StartupInfo, &ProcessInfo);
	if (!bOK)
	{
		DWORD dwErrCode = GetLastError();
		char szErrMsg[255];
		szErrMsg[255-1] = '\0';
		if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwErrCode, 0, szErrMsg, 255, NULL))
		{
			error("Could not start %s: %s", m_szInfoName, szErrMsg);
		}
		else
		{
			error("Could not start %s: error %i", m_szInfoName, dwErrCode);
		}
		if (!Util::FileExists(m_szScript))
		{
			error("Could not find file %s", m_szScript);
		}
		free(szEnvironmentStrings);
		return -1;
	}

	free(szEnvironmentStrings);

	debug("Child Process-ID: %i", (int)ProcessInfo.dwProcessId);

	m_hProcess = ProcessInfo.hProcess;

	// close unused "write" end
	CloseHandle(hWritePipe);

	pipein = _open_osfhandle((intptr_t)hReadPipe, _O_RDONLY);

#else

	int p[2];
	int pipeout;

	// create the pipe
	if (pipe(p))
	{
		error("Could not open pipe: errno %i", errno);
		return -1;
	}

	char** pEnvironmentStrings = m_environmentStrings.GetStrings();

	pipein = p[0];
	pipeout = p[1];

	debug("forking");
	pid_t pid = fork();

	if (pid == -1)
	{
		error("Could not start %s: errno %i", m_szInfoName, errno);
		free(pEnvironmentStrings);
		return -1;
	}
	else if (pid == 0)
	{
		// here goes the second instance

		// create new process group (see Terminate() where it is used)
		setsid();
			
		// close up the "read" end
		close(pipein);
      			
		// make the pipeout to be the same as stdout and stderr
		dup2(pipeout, 1);
		dup2(pipeout, 2);
		
		close(pipeout);

#ifdef CHILD_WATCHDOG
		fwrite("\n", 1, 1, stdout);
		fflush(stdout);
#endif

		chdir(m_szWorkingDir);
		environ = pEnvironmentStrings;
		execvp(m_szScript, (char* const*)m_szArgs);

		if (errno == EACCES)
		{
			fprintf(stdout, "[WARNING] Fixing permissions for %s\n", m_szScript);
			fflush(stdout);
			Util::FixExecPermission(m_szScript);
			execvp(m_szScript, (char* const*)m_szArgs);
		}

		// NOTE: the text "[ERROR] Could not start " is checked later,
		// by changing adjust the dependent code below.
		fprintf(stdout, "[ERROR] Could not start %s: %s", m_szScript, strerror(errno));
		fflush(stdout);
		_exit(254);
	}

	// continue the first instance
	debug("forked");
	debug("Child Process-ID: %i", (int)pid);

	free(pEnvironmentStrings);

	m_hProcess = pid;

	// close unused "write" end
	close(pipeout);
#endif

	// open the read end
	m_pReadpipe = fdopen(pipein, "r");
	if (!m_pReadpipe)
	{
		error("Could not open pipe to %s", m_szInfoName);
		return -1;
	}
	
#ifdef CHILD_WATCHDOG
	debug("Creating child watchdog");
	ChildWatchDog* pWatchDog = new ChildWatchDog();
	pWatchDog->SetAutoDestroy(false);
	pWatchDog->SetProcessID(pid);
	pWatchDog->Start();
#endif
	
	char* buf = (char*)malloc(10240);

	debug("Entering pipe-loop");
	bool bFirstLine = true;
	bool bStartError = false;
	while (!m_bTerminated && !m_bDetached && !feof(m_pReadpipe))
	{
		if (ReadLine(buf, 10240, m_pReadpipe) && m_pReadpipe)
		{
#ifdef CHILD_WATCHDOG
			if (!bChildConfirmed)
			{
				bChildConfirmed = true;
				pWatchDog->Stop();
				debug("Child confirmed");
				continue;
			}
#endif
			if (bFirstLine && !strncmp(buf, "[ERROR] Could not start ", 24))
			{
				bStartError = true;
			}
			ProcessOutput(buf);
			bFirstLine = false;
		}
	}
	debug("Exited pipe-loop");

#ifdef CHILD_WATCHDOG
	debug("Destroying WatchDog");
	if (!bChildConfirmed)
	{
		pWatchDog->Stop();
	}
	while (pWatchDog->IsRunning())
	{
		usleep(1 * 1000);
	}
	delete pWatchDog;
#endif
	
	free(buf);
	if (m_pReadpipe)
	{
		fclose(m_pReadpipe);
	}

	if (m_bTerminated)
	{
		warn("Interrupted %s", m_szInfoName);
	}

	iExitCode = 0;

	if (!m_bTerminated && !m_bDetached)
	{
#ifdef WIN32
	WaitForSingleObject(m_hProcess, INFINITE);
	DWORD dExitCode = 0;
	GetExitCodeProcess(m_hProcess, &dExitCode);
	iExitCode = dExitCode;
#else
	int iStatus = 0;
	waitpid(m_hProcess, &iStatus, 0);
	if (WIFEXITED(iStatus))
	{
		iExitCode = WEXITSTATUS(iStatus);
		if (iExitCode == 254 && bStartError)
		{
			iExitCode = -1;
		}
	}
#endif
	}
	
#ifdef CHILD_WATCHDOG
	}	// while (!bChildConfirmed && !m_bTerminated)
#endif
	
	debug("Exit code %i", iExitCode);

	return iExitCode;
}

void ScriptController::Terminate()
{
	debug("Stopping %s", m_szInfoName);
	m_bTerminated = true;

#ifdef WIN32
	BOOL bOK = TerminateProcess(m_hProcess, -1);
#else
	pid_t hKillProcess = m_hProcess;
	if (getpgid(hKillProcess) == hKillProcess)
	{
		// if the child process has its own group (setsid() was successful), kill the whole group
		hKillProcess = -hKillProcess;
	}
	bool bOK = kill(hKillProcess, SIGKILL) == 0;
#endif

	if (bOK)
	{
		debug("Terminated %s", m_szInfoName);
	}
	else
	{
		error("Could not terminate %s", m_szInfoName);
	}

	debug("Stopped %s", m_szInfoName);
}

void ScriptController::Detach()
{
	debug("Detaching %s", m_szInfoName);
	m_bDetached = true;
	FILE* pReadpipe = m_pReadpipe;
	m_pReadpipe = NULL;
	fclose(pReadpipe);
}
	

bool ScriptController::ReadLine(char* szBuf, int iBufSize, FILE* pStream)
{
	return fgets(szBuf, iBufSize, pStream);
}

void ScriptController::ProcessOutput(char* szText)
{
	debug("Processing output received from script");

	for (char* pend = szText + strlen(szText) - 1; pend >= szText && (*pend == '\n' || *pend == '\r' || *pend == ' '); pend--) *pend = '\0';

	if (szText[0] == '\0')
	{
		// skip empty lines
		return;
	}

	if (!strncmp(szText, "[INFO] ", 7))
	{
		PrintMessage(Message::mkInfo, "%s", szText + 7);
	}
	else if (!strncmp(szText, "[WARNING] ", 10))
	{
		PrintMessage(Message::mkWarning, "%s", szText + 10);
	}
	else if (!strncmp(szText, "[ERROR] ", 8))
	{
		PrintMessage(Message::mkError, "%s", szText + 8);
	}
	else if (!strncmp(szText, "[DETAIL] ", 9))
	{
		PrintMessage(Message::mkDetail, "%s", szText + 9);
	}
	else if (!strncmp(szText, "[DEBUG] ", 8))
	{
		PrintMessage(Message::mkDebug, "%s", szText + 8);
	}
	else 
	{
		PrintMessage(Message::mkInfo, "%s", szText);
	}

	debug("Processing output received from script - completed");
}

void ScriptController::AddMessage(Message::EKind eKind, const char* szText)
{
	switch (eKind)
	{
		case Message::mkDetail:
			detail("%s", szText);
			break;

		case Message::mkInfo:
			info("%s", szText);
			break;

		case Message::mkWarning:
			warn("%s", szText);
			break;

		case Message::mkError:
			error("%s", szText);
			break;

		case Message::mkDebug:
			debug("%s", szText);
			break;
	}
}

void ScriptController::PrintMessage(Message::EKind eKind, const char* szFormat, ...)
{
	char tmp2[1024];

	va_list ap;
	va_start(ap, szFormat);
	vsnprintf(tmp2, 1024, szFormat, ap);
	tmp2[1024-1] = '\0';
	va_end(ap);

	char tmp3[1024];
	if (m_szLogPrefix)
	{
		snprintf(tmp3, 1024, "%s: %s", m_szLogPrefix, tmp2);
	}
	else
	{
		strncpy(tmp3, tmp2, 1024);
	}
	tmp3[1024-1] = '\0';

	AddMessage(eKind, tmp3);
}

void PostScriptController::StartJob(PostInfo* pPostInfo)
{
	PostScriptController* pScriptController = new PostScriptController();
	pScriptController->m_pPostInfo = pPostInfo;
	pScriptController->SetWorkingDir(g_pOptions->GetDestDir());
	pScriptController->SetAutoDestroy(false);
	pScriptController->m_iPrefixLen = 0;

	pPostInfo->SetPostThread(pScriptController);

	pScriptController->Start();
}

void PostScriptController::Run()
{
	FileList activeList;

	// the locking is needed for accessing the members of NZBInfo
	g_pDownloadQueueHolder->LockQueue();
	for (NZBParameterList::iterator it = m_pPostInfo->GetNZBInfo()->GetParameters()->begin(); it != m_pPostInfo->GetNZBInfo()->GetParameters()->end(); it++)
	{
		NZBParameter* pParameter = *it;
		const char* szVarname = pParameter->GetName();
		if (strlen(szVarname) > 0 && szVarname[0] != '*' && szVarname[strlen(szVarname)-1] == ':' &&
			(!strcasecmp(pParameter->GetValue(), "yes") || !strcasecmp(pParameter->GetValue(), "on") || !strcasecmp(pParameter->GetValue(), "1")))
		{
			char* szScriptName = strdup(szVarname);
			szScriptName[strlen(szScriptName)-1] = '\0'; // remove trailing ':'
			activeList.push_back(szScriptName);
		}
	}
	m_pPostInfo->GetNZBInfo()->GetScriptStatuses()->Clear();
	g_pDownloadQueueHolder->UnlockQueue();

	Options::ScriptList scriptList;
	g_pOptions->LoadScriptList(&scriptList);

	for (Options::ScriptList::iterator it = scriptList.begin(); it != scriptList.end(); it++)
	{
		Options::Script* pScript = *it;
		for (FileList::iterator it2 = activeList.begin(); it2 != activeList.end(); it2++)
		{
			char* szActiveName = *it2;
			// if any script has requested par-check, do not execute other scripts
			if (Util::SameFilename(pScript->GetName(), szActiveName) && !m_pPostInfo->GetRequestParCheck())
			{
				ExecuteScript(pScript->GetName(), pScript->GetDisplayName(), pScript->GetLocation());
			}
		}
	}
	
	for (FileList::iterator it = activeList.begin(); it != activeList.end(); it++)
	{
		free(*it);
	}

	m_pPostInfo->SetStage(PostInfo::ptFinished);
	m_pPostInfo->SetWorking(false);
}

void PostScriptController::ExecuteScript(const char* szScriptName, const char* szDisplayName, const char* szLocation)
{
	PrintMessage(Message::mkInfo, "Executing post-process-script %s for %s", szScriptName, m_pPostInfo->GetInfoName());

	SetScript(szLocation);
	SetArgs(NULL, false);

	char szInfoName[1024];
	snprintf(szInfoName, 1024, "post-process-script %s for %s", szScriptName, m_pPostInfo->GetInfoName());
	szInfoName[1024-1] = '\0';
	SetInfoName(szInfoName);

	SetLogPrefix(szDisplayName);
	m_iPrefixLen = strlen(szDisplayName) + 2; // 2 = strlen(": ");
	PrepareParams(szScriptName);

	int iExitCode = Execute();

	szInfoName[0] = 'P'; // uppercase

	SetLogPrefix(NULL);
	ScriptStatus::EStatus eStatus = AnalyseExitCode(iExitCode);
	
	// the locking is needed for accessing the members of NZBInfo
	g_pDownloadQueueHolder->LockQueue();
	m_pPostInfo->GetNZBInfo()->GetScriptStatuses()->Add(szScriptName, eStatus);
	g_pDownloadQueueHolder->UnlockQueue();
}

void PostScriptController::PrepareParams(const char* szScriptName)
{
	// the locking is needed for accessing the members of NZBInfo
	g_pDownloadQueueHolder->LockQueue();

	// Reset
	ResetEnv();

	SetEnvVar("NZBPP_NZBNAME", m_pPostInfo->GetNZBInfo()->GetName());
	SetEnvVar("NZBPP_DIRECTORY", m_pPostInfo->GetNZBInfo()->GetDestDir());
	SetEnvVar("NZBPP_NZBFILENAME", m_pPostInfo->GetNZBInfo()->GetFilename());
	SetEnvVar("NZBPP_FINALDIR", m_pPostInfo->GetNZBInfo()->GetFinalDir());
	SetEnvVar("NZBPP_CATEGORY", m_pPostInfo->GetNZBInfo()->GetCategory());
	SetIntEnvVar("NZBPP_HEALTH", m_pPostInfo->GetNZBInfo()->CalcHealth());
	SetIntEnvVar("NZBPP_CRITICALHEALTH", m_pPostInfo->GetNZBInfo()->CalcCriticalHealth());

	int iParStatus[] = { 0, 0, 1, 2, 3, 4 };
	SetIntEnvVar("NZBPP_PARSTATUS", iParStatus[m_pPostInfo->GetNZBInfo()->GetParStatus()]);

	int iUnpackStatus[] = { 0, 0, 1, 2, 3, 4 };
	SetIntEnvVar("NZBPP_UNPACKSTATUS", iUnpackStatus[m_pPostInfo->GetNZBInfo()->GetUnpackStatus()]);

	SetIntEnvVar("NZBPP_NZBID", m_pPostInfo->GetNZBInfo()->GetID());
	SetIntEnvVar("NZBPP_HEALTHDELETED", (int)m_pPostInfo->GetNZBInfo()->GetDeleteStatus() == NZBInfo::dsHealth);
	SetIntEnvVar("NZBPP_TOTALARTICLES", (int)m_pPostInfo->GetNZBInfo()->GetTotalArticles());
	SetIntEnvVar("NZBPP_SUCCESSARTICLES", (int)m_pPostInfo->GetNZBInfo()->GetSuccessArticles());
	SetIntEnvVar("NZBPP_FAILEDARTICLES", (int)m_pPostInfo->GetNZBInfo()->GetFailedArticles());

	for (ServerStatList::iterator it = m_pPostInfo->GetNZBInfo()->GetServerStats()->begin(); it != m_pPostInfo->GetNZBInfo()->GetServerStats()->end(); it++)
	{
		ServerStat* pServerStat = *it;

		char szName[50];

		snprintf(szName, 50, "NZBPP_SERVER%i_SUCCESSARTICLES", pServerStat->GetServerID());
		szName[50-1] = '\0';
		SetIntEnvVar(szName, pServerStat->GetSuccessArticles());

		snprintf(szName, 50, "NZBPP_SERVER%i_FAILEDARTICLES", pServerStat->GetServerID());
		szName[50-1] = '\0';
		SetIntEnvVar(szName, pServerStat->GetFailedArticles());
	}

	PrepareEnvParameters(m_pPostInfo->GetNZBInfo(), NULL);

	char szParamPrefix[1024];
	snprintf(szParamPrefix, 1024, "%s:", szScriptName);
	szParamPrefix[1024-1] = '\0';
	PrepareEnvParameters(m_pPostInfo->GetNZBInfo(), szParamPrefix);
	PrepareEnvOptions(szParamPrefix);
	
	g_pDownloadQueueHolder->UnlockQueue();
}

ScriptStatus::EStatus PostScriptController::AnalyseExitCode(int iExitCode)
{
	// The ScriptStatus is accumulated for all scripts:
	// If any script has failed the status is "failure", etc.

	switch (iExitCode)
	{
		case POSTPROCESS_SUCCESS:
			PrintMessage(Message::mkInfo, "%s successful", GetInfoName());
			return ScriptStatus::srSuccess;

		case POSTPROCESS_ERROR:
		case -1: // Execute() returns -1 if the process could not be started (file not found or other problem)
			PrintMessage(Message::mkError, "%s failed", GetInfoName());
			return ScriptStatus::srFailure;

		case POSTPROCESS_NONE:
			PrintMessage(Message::mkInfo, "%s skipped", GetInfoName());
			return ScriptStatus::srNone;

#ifndef DISABLE_PARCHECK
		case POSTPROCESS_PARCHECK:
			if (m_pPostInfo->GetNZBInfo()->GetParStatus() > NZBInfo::psSkipped)
			{
				PrintMessage(Message::mkError, "%s requested par-check/repair, but the collection was already checked", GetInfoName());
				return ScriptStatus::srFailure;
			}
			else
			{
				PrintMessage(Message::mkInfo, "%s requested par-check/repair", GetInfoName());
				m_pPostInfo->SetRequestParCheck(true);
				return ScriptStatus::srSuccess;
			}
			break;
#endif

		default:
			PrintMessage(Message::mkError, "%s failed (terminated with unknown status)", GetInfoName());
			return ScriptStatus::srFailure;
	}
}

void PostScriptController::AddMessage(Message::EKind eKind, const char* szText)
{
	const char* szMsgText = szText + m_iPrefixLen;

	if (!strncmp(szMsgText, "[NZB] ", 6))
	{
		debug("Command %s detected", szMsgText + 6);
		if (!strncmp(szMsgText + 6, "FINALDIR=", 9))
		{
			g_pDownloadQueueHolder->LockQueue();
			m_pPostInfo->GetNZBInfo()->SetFinalDir(szMsgText + 6 + 9);
			g_pDownloadQueueHolder->UnlockQueue();
		}
		else if (!strncmp(szMsgText + 6, "NZBPR_", 6))
		{
			char* szParam = strdup(szMsgText + 6 + 6);
			char* szValue = strchr(szParam, '=');
			if (szValue)
			{
				*szValue = '\0';
				g_pDownloadQueueHolder->LockQueue();
				m_pPostInfo->GetNZBInfo()->GetParameters()->SetParameter(szParam, szValue + 1);
				g_pDownloadQueueHolder->UnlockQueue();
			}
			else
			{
				error("Invalid command \"%s\" received from %s", szMsgText, GetInfoName());
			}
			free(szParam);
		}
		else
		{
			error("Invalid command \"%s\" received from %s", szMsgText, GetInfoName());
		}
	}
	else if (!strncmp(szMsgText, "[HISTORY] ", 10))
	{
		m_pPostInfo->GetNZBInfo()->AppendMessage(eKind, 0, szMsgText);
	}
	else
	{
		ScriptController::AddMessage(eKind, szText);
		m_pPostInfo->AppendMessage(eKind, szText);
	}

	if (g_pOptions->GetPausePostProcess())
	{
		time_t tStageTime = m_pPostInfo->GetStageTime();
		time_t tStartTime = m_pPostInfo->GetStartTime();
		time_t tWaitTime = time(NULL);

		// wait until Post-processor is unpaused
		while (g_pOptions->GetPausePostProcess() && !IsStopped())
		{
			usleep(100 * 1000);

			// update time stamps

			time_t tDelta = time(NULL) - tWaitTime;

			if (tStageTime > 0)
			{
				m_pPostInfo->SetStageTime(tStageTime + tDelta);
			}

			if (tStartTime > 0)
			{
				m_pPostInfo->SetStartTime(tStartTime + tDelta);
			}
		}
	}
}

void PostScriptController::Stop()
{
	debug("Stopping post-process-script");
	Thread::Stop();
	Terminate();
}


void NZBScriptController::ExecuteScript(const char* szScript, const char* szNZBFilename, const char* szDirectory,
	char** pNZBName, char** pCategory, int* iPriority, NZBParameterList* pParameters, bool* bAddTop, bool* bAddPaused)
{
	info("Executing nzb-process-script for %s", Util::BaseFileName(szNZBFilename));

	NZBScriptController* pScriptController = new NZBScriptController();
	pScriptController->SetScript(szScript);
	pScriptController->m_pNZBName = pNZBName;
	pScriptController->m_pCategory = pCategory;
	pScriptController->m_pParameters = pParameters;
	pScriptController->m_iPriority = iPriority;
	pScriptController->m_bAddTop = bAddTop;
	pScriptController->m_bAddPaused = bAddPaused;

	char szInfoName[1024];
	snprintf(szInfoName, 1024, "nzb-process-script for %s", Util::BaseFileName(szNZBFilename));
	szInfoName[1024-1] = '\0';
	pScriptController->SetInfoName(szInfoName);

	pScriptController->SetEnvVar("NZBNP_FILENAME", szNZBFilename);
	pScriptController->SetEnvVar("NZBNP_NZBNAME", strlen(*pNZBName) > 0 ? *pNZBName : Util::BaseFileName(szNZBFilename));
	pScriptController->SetEnvVar("NZBNP_CATEGORY", *pCategory);
	pScriptController->SetIntEnvVar("NZBNP_PRIORITY", *iPriority);
	pScriptController->SetIntEnvVar("NZBNP_TOP", *bAddTop ? 1 : 0);
	pScriptController->SetIntEnvVar("NZBNP_PAUSED", *bAddPaused ? 1 : 0);

	// remove trailing slash
	char szDir[1024];
	strncpy(szDir, szDirectory, 1024);
	szDir[1024-1] = '\0';
	int iLen = strlen(szDir);
	if (szDir[iLen-1] == PATH_SEPARATOR)
	{
		szDir[iLen-1] = '\0';
	}
	pScriptController->SetEnvVar("NZBNP_DIRECTORY", szDir);

	char szLogPrefix[1024];
	strncpy(szLogPrefix, Util::BaseFileName(szScript), 1024);
	szLogPrefix[1024-1] = '\0';
	if (char* ext = strrchr(szLogPrefix, '.')) *ext = '\0'; // strip file extension
	pScriptController->SetLogPrefix(szLogPrefix);
	pScriptController->m_iPrefixLen = strlen(szLogPrefix) + 2; // 2 = strlen(": ");

	pScriptController->Execute();

	delete pScriptController;
}

void NZBScriptController::AddMessage(Message::EKind eKind, const char* szText)
{
	szText = szText + m_iPrefixLen;

	if (!strncmp(szText, "[NZB] ", 6))
	{
		debug("Command %s detected", szText + 6);
		if (!strncmp(szText + 6, "NZBNAME=", 8))
		{
			free(*m_pNZBName);
			*m_pNZBName = strdup(szText + 6 + 8);
		}
		else if (!strncmp(szText + 6, "CATEGORY=", 9))
		{
			free(*m_pCategory);
			*m_pCategory = strdup(szText + 6 + 9);
		}
		else if (!strncmp(szText + 6, "NZBPR_", 6))
		{
			char* szParam = strdup(szText + 6 + 6);
			char* szValue = strchr(szParam, '=');
			if (szValue)
			{
				*szValue = '\0';
				m_pParameters->SetParameter(szParam, szValue + 1);
			}
			else
			{
				error("Invalid command \"%s\" received from %s", szText, GetInfoName());
			}
			free(szParam);
		}
		else if (!strncmp(szText + 6, "PRIORITY=", 9))
		{
			*m_iPriority = atoi(szText + 6 + 9);
		}
		else if (!strncmp(szText + 6, "TOP=", 4))
		{
			*m_bAddTop = atoi(szText + 6 + 4) != 0;
		}
		else if (!strncmp(szText + 6, "PAUSED=", 7))
		{
			*m_bAddPaused = atoi(szText + 6 + 7) != 0;
		}
		else
		{
			error("Invalid command \"%s\" received from %s", szText, GetInfoName());
		}
	}
	else
	{
		ScriptController::AddMessage(eKind, szText);
	}
}

void NZBAddedScriptController::StartScript(DownloadQueue* pDownloadQueue, NZBInfo *pNZBInfo, const char* szScript)
{
	NZBAddedScriptController* pScriptController = new NZBAddedScriptController();
	pScriptController->SetScript(szScript);
	pScriptController->m_szNZBName = strdup(pNZBInfo->GetName());
	pScriptController->SetEnvVar("NZBNA_NZBNAME", pNZBInfo->GetName());
	// "NZBNA_NAME" is not correct but kept for compatibility with older versions where this name was used by mistake
	pScriptController->SetEnvVar("NZBNA_NAME", pNZBInfo->GetName());
	pScriptController->SetEnvVar("NZBNA_FILENAME", pNZBInfo->GetFilename());
	pScriptController->SetEnvVar("NZBNA_CATEGORY", pNZBInfo->GetCategory());

	int iLastID = 0;
	int iMaxPriority = 0;

	for (FileQueue::iterator it = pDownloadQueue->GetFileQueue()->begin(); it != pDownloadQueue->GetFileQueue()->end(); it++)
    {
        FileInfo* pFileInfo = *it;
		if (pFileInfo->GetNZBInfo() == pNZBInfo && ( pFileInfo->GetPriority() > iMaxPriority || iLastID == 0))
		{
			iMaxPriority = pFileInfo->GetPriority();
		}
		if (pFileInfo->GetNZBInfo() == pNZBInfo && pFileInfo->GetID() > iLastID)
		{
			iLastID = pFileInfo->GetID();
		}
	}

	pScriptController->SetIntEnvVar("NZBNA_LASTID", iLastID);
	pScriptController->SetIntEnvVar("NZBNA_PRIORITY", iMaxPriority);

	pScriptController->PrepareEnvParameters(pNZBInfo, NULL);

	pScriptController->SetAutoDestroy(true);

	pScriptController->Start();
}

void NZBAddedScriptController::Run()
{
	char szInfoName[1024];
	snprintf(szInfoName, 1024, "nzb-added process-script for %s", m_szNZBName);
	szInfoName[1024-1] = '\0';
	SetInfoName(szInfoName);

	info("Executing %s", szInfoName);

	char szLogPrefix[1024];
	strncpy(szLogPrefix, Util::BaseFileName(GetScript()), 1024);
	szLogPrefix[1024-1] = '\0';
	if (char* ext = strrchr(szLogPrefix, '.')) *ext = '\0'; // strip file extension
	SetLogPrefix(szLogPrefix);

	Execute();

	free(m_szNZBName);
}

void SchedulerScriptController::StartScript(const char* szCommandLine)
{
	char** argv = NULL;
	if (!Util::SplitCommandLine(szCommandLine, &argv))
	{
		error("Could not execute scheduled process-script, failed to parse command line: %s", szCommandLine);
		return;
	}

	info("Executing scheduled process-script %s", Util::BaseFileName(argv[0]));

	SchedulerScriptController* pScriptController = new SchedulerScriptController();
	pScriptController->SetScript(argv[0]);
	pScriptController->SetArgs((const char**)argv, true);
	pScriptController->SetAutoDestroy(true);

	pScriptController->Start();
}

void SchedulerScriptController::Run()
{
	char szInfoName[1024];
	snprintf(szInfoName, 1024, "scheduled process-script %s", Util::BaseFileName(GetScript()));
	szInfoName[1024-1] = '\0';
	SetInfoName(szInfoName);

	char szLogPrefix[1024];
	strncpy(szLogPrefix, Util::BaseFileName(GetScript()), 1024);
	szLogPrefix[1024-1] = '\0';
	if (char* ext = strrchr(szLogPrefix, '.')) *ext = '\0'; // strip file extension
	SetLogPrefix(szLogPrefix);

	Execute();
}
