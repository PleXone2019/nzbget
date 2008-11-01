/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2008 Andrei Prygounkov <hugbug@users.sourceforge.net>
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


#ifndef OPTIONS_H
#define OPTIONS_H

#include <vector>

class Options
{
public:
	enum EClientOperation
	{
		opClientNoOperation,
		opClientRequestDownload,
		opClientRequestList,
		opClientRequestPause,
		opClientRequestUnpause,
		opClientRequestSetRate,
		opClientRequestDumpDebug,
		opClientRequestEditQueue,
		opClientRequestLog,
		opClientRequestShutdown,
		opClientRequestVersion,
		opClientRequestPostQueue,
		opClientRequestWriteLog
	};
	enum EMessageTarget
	{
		mtNone,
		mtScreen,
		mtLog,
		mtBoth
	};
	enum EOutputMode
	{
		omLoggable,
		omColored,
		omNCurses
	};
	enum ELoadPars
	{
		lpNone,
		lpOne,
		lpAll
	};
	enum EScriptLogKind
	{
		slNone,
		slDetail,
		slInfo,
		slWarning,
		slError,
		slDebug
	};

private:
	struct OptEntry
	{
		char* name;
		char* value;
	};
	
	std::vector< struct OptEntry >	optEntries;

	bool				m_bConfigInitialized;

	// Options
	char*				m_szConfigFilename;
	char*				m_szDestDir;
	char*				m_szTempDir;
	char*				m_szQueueDir;
	char*				m_szNzbDir;
	EMessageTarget		m_eInfoTarget;
	EMessageTarget		m_eWarningTarget;
	EMessageTarget		m_eErrorTarget;
	EMessageTarget		m_eDebugTarget;
	EMessageTarget		m_eDetailTarget;
	bool				m_bDecode;
	bool				m_bCreateBrokenLog;
	bool				m_bResetLog;
	int					m_iConnectionTimeout;
	int					m_iTerminateTimeout;
	bool				m_bAppendNZBDir;
	bool				m_bAppendCategoryDir;
	bool				m_bContinuePartial;
	bool				m_bRenameBroken;
	int					m_iRetries;
	int					m_iRetryInterval;
	bool				m_bSaveQueue;
	bool				m_bDupeCheck;
	char*				m_szServerIP;
	char*				m_szServerPassword;
	int					m_szServerPort;
	char*				m_szLockFile;
	char*				m_szDaemonUserName;
	EOutputMode			m_eOutputMode;
	bool				m_bReloadQueue;
	bool				m_bReloadPostQueue;
	int					m_iLogBufferSize;
	bool				m_bCreateLog;
	char*				m_szLogFile;
	ELoadPars			m_eLoadPars;
	bool				m_bParCheck;
	bool				m_bParRepair;
	char*				m_szPostProcess;
	char*				m_szNZBProcess;
	bool				m_bStrictParName;
	bool				m_bNoConfig;
	int					m_iUMask;
	int					m_iUpdateInterval;
	bool				m_bCursesNZBName;
	bool				m_bCursesTime;
	bool				m_bCursesGroup;
	bool				m_bCrcCheck;
	bool				m_bRetryOnCrcError;
	int					m_iThreadLimit;
	bool				m_bDirectWrite;
	int					m_iWriteBufferSize;
	int					m_iNzbDirInterval;
	int					m_iNzbDirFileAge;
	bool				m_bParCleanupQueue;
	int					m_iDiskSpace;
	EScriptLogKind		m_ePostLogKind;
	EScriptLogKind		m_eNZBLogKind;
	bool				m_bAllowReProcess;
	bool				m_bTLS;
	bool				m_bDumpCore;
	bool				m_bParPauseQueue;
	bool				m_bPostPauseQueue;
	bool				m_bNzbCleanupDisk;
	bool				m_bDeleteCleanupDisk;
	bool				m_bMergeNzb;
	int					m_iParTimeLimit;

	// Parsed command-line parameters
	bool				m_bServerMode;
	bool				m_bDaemonMode;
	bool				m_bRemoteClientMode;
	int					m_iEditQueueAction;
	int					m_iEditQueueOffset;
	int*				m_pEditQueueIDList;
	int					m_iEditQueueIDCount;
	char*				m_szEditQueueText;
	char*				m_szArgFilename;
	char*				m_szCategory;
	char*				m_szLastArg;
	bool				m_bPrintOptions;
	bool				m_bAddTop;
	float				m_fSetRate;
	int					m_iLogLines;
	int					m_iWriteLogKind;
	bool				m_bTestBacktrace;

	// Current state
	bool				m_bPause;
	float				m_fDownloadRate;
	EClientOperation	m_eClientOperation;

	void				InitDefault();
	void				InitOptFile();
	void				InitCommandLine(int argc, char* argv[]);
	void				InitOptions();
	void				InitFileArg(int argc, char* argv[]);
	void				InitServers();
	void				InitScheduler();
	void				CheckOptions();
	void				PrintUsage(char* com);
	void				Dump();
	int					ParseOptionValue(const char* OptName, const char* OptValue, int argc, const char* argn[], const int argv[]);
	const char*			GetOption(const char* optname);
	void				DelOption(const char* optname);
	void				SetOption(const char* optname, const char* value);
	bool				SetOptionString(const char* option);
	bool				ValidateOptionName(const char* optname);
	void				LoadConfig(const char* configfile);
	void				CheckDir(char** dir, const char* szOptionName);
	void				ParseFileIDList(int argc, char* argv[], int optind);
	bool				ParseTime(const char* szTime, int* pHours, int* pMinutes);
	bool				ParseWeekDays(const char* szWeekDays, int* pWeekDaysBits);

public:
						Options(int argc, char* argv[]);
						~Options();

	// Options
	const char*			GetDestDir() { return m_szDestDir; }
	const char*			GetTempDir() { return m_szTempDir; }
	const char*			GetQueueDir() { return m_szQueueDir; }
	const char*			GetNzbDir() { return m_szNzbDir; }
	bool				GetCreateBrokenLog() const { return m_bCreateBrokenLog; }
	bool				GetResetLog() const { return m_bResetLog; }
	EMessageTarget		GetInfoTarget() const { return m_eInfoTarget; }
	EMessageTarget		GetWarningTarget() const { return m_eWarningTarget; }
	EMessageTarget		GetErrorTarget() const { return m_eErrorTarget; }
	EMessageTarget		GetDebugTarget() const { return m_eDebugTarget; }
	EMessageTarget		GetDetailTarget() const { return m_eDetailTarget; }
	int					GetConnectionTimeout() { return m_iConnectionTimeout; }
	int					GetTerminateTimeout() { return m_iTerminateTimeout; }
	bool				GetDecode() { return m_bDecode; };
	bool				GetAppendNZBDir() { return m_bAppendNZBDir; }
	bool				GetAppendCategoryDir() { return m_bAppendCategoryDir; }
	bool				GetContinuePartial() { return m_bContinuePartial; }
	bool				GetRenameBroken() { return m_bRenameBroken; }
	int					GetRetries() { return m_iRetries; }
	int					GetRetryInterval() { return m_iRetryInterval; }
	bool				GetSaveQueue() { return m_bSaveQueue; }
	bool				GetDupeCheck() { return m_bDupeCheck; }
	char*				GetServerIP() { return m_szServerIP; }
	char*				GetServerPassword() { return m_szServerPassword; }
	int					GetServerPort() { return m_szServerPort; }
	char*				GetLockFile() { return m_szLockFile; }
	char*				GetDaemonUserName() { return m_szDaemonUserName; }
	EOutputMode			GetOutputMode() { return m_eOutputMode; }
	bool				GetReloadQueue() { return m_bReloadQueue; }
	bool				GetReloadPostQueue() { return m_bReloadPostQueue; }
	int					GetLogBufferSize() { return m_iLogBufferSize; }
	bool				GetCreateLog() { return m_bCreateLog; }
	char*				GetLogFile() { return m_szLogFile; }
	ELoadPars			GetLoadPars() { return m_eLoadPars; }
	bool				GetParCheck() { return m_bParCheck; }
	bool				GetParRepair() { return m_bParRepair; }
	const char*			GetPostProcess() { return m_szPostProcess; }
	const char*			GetNZBProcess() { return m_szNZBProcess; }
	bool				GetStrictParName() { return m_bStrictParName; }
	int					GetUMask() { return m_iUMask; }
	int					GetUpdateInterval() {return m_iUpdateInterval; }
	bool				GetCursesNZBName() { return m_bCursesNZBName; }
	bool				GetCursesTime() { return m_bCursesTime; }
	bool				GetCursesGroup() { return m_bCursesGroup; }
	bool				GetCrcCheck() { return m_bCrcCheck; }
	bool				GetRetryOnCrcError() { return m_bRetryOnCrcError; }
	int					GetThreadLimit() { return m_iThreadLimit; }
	bool				GetDirectWrite() { return m_bDirectWrite; }
	int					GetWriteBufferSize() { return m_iWriteBufferSize; }
	int					GetNzbDirInterval() { return m_iNzbDirInterval; }
	int					GetNzbDirFileAge() { return m_iNzbDirFileAge; }
	bool				GetParCleanupQueue() { return m_bParCleanupQueue; }
	int					GetDiskSpace() { return m_iDiskSpace; }
	EScriptLogKind		GetPostLogKind() { return m_ePostLogKind; }
	EScriptLogKind		GetNZBLogKind() { return m_eNZBLogKind; }
	bool				GetAllowReProcess() { return m_bAllowReProcess; }
	bool				GetTLS() { return m_bTLS; }
	bool				GetDumpCore() { return m_bDumpCore; }
	bool				GetParPauseQueue() { return m_bParPauseQueue; }
	bool				GetPostPauseQueue() { return m_bPostPauseQueue; }
	bool				GetNzbCleanupDisk() { return m_bNzbCleanupDisk; }
	bool				GetDeleteCleanupDisk() { return m_bDeleteCleanupDisk; }
	bool				GetMergeNzb() { return m_bMergeNzb; }
	int					GetParTimeLimit() { return m_iParTimeLimit; }

	// Parsed command-line parameters
	bool				GetServerMode() { return m_bServerMode; }
	bool				GetDaemonMode() { return m_bDaemonMode; }
	bool				GetRemoteClientMode() { return m_bRemoteClientMode; }
	EClientOperation	GetClientOperation() { return m_eClientOperation; }
	int					GetEditQueueAction() { return m_iEditQueueAction; }
	int					GetEditQueueOffset() { return m_iEditQueueOffset; }
	int*				GetEditQueueIDList() { return m_pEditQueueIDList; }
	int					GetEditQueueIDCount() { return m_iEditQueueIDCount; }
	const char*			GetEditQueueText() { return m_szEditQueueText; }
	const char*			GetArgFilename() { return m_szArgFilename; }
	const char*			GetCategory() { return m_szCategory; }
	const char*			GetLastArg() { return m_szLastArg; }
	bool				GetAddTop() { return m_bAddTop; }
	float				GetSetRate() { return m_fSetRate; }
	int					GetLogLines() { return m_iLogLines; }
	int					GetWriteLogKind() { return m_iWriteLogKind; }
	bool				GetTestBacktrace() { return m_bTestBacktrace; }

	// Current state
	void				SetPause(bool bOnOff) { m_bPause = bOnOff; }
	bool				GetPause() const { return m_bPause; }
	void				SetDownloadRate(float fRate) { m_fDownloadRate = fRate; }
	float				GetDownloadRate() const { return m_fDownloadRate; }
};

#endif
