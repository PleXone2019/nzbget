/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
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


#ifndef DOWNLOADINFO_H
#define DOWNLOADINFO_H

#include <vector>
#include <deque>
#include <time.h>

#include "Log.h"
#include "Thread.h"

class NZBInfo;
class DownloadQueue;

class ArticleInfo
{
public:
	enum EStatus
	{
		aiUndefined,
		aiRunning,
		aiFinished,
		aiFailed
	};
	
private:
	int					m_iPartNumber;
	char*				m_szMessageID;
	int					m_iSize;
	EStatus				m_eStatus;
	char*				m_szResultFilename;

public:
						ArticleInfo();
						~ArticleInfo();
	void 				SetPartNumber(int s) { m_iPartNumber = s; }
	int 				GetPartNumber() { return m_iPartNumber; }
	const char* 		GetMessageID() { return m_szMessageID; }
	void 				SetMessageID(const char* szMessageID);
	void 				SetSize(int s) { m_iSize = s; }
	int 				GetSize() { return m_iSize; }
	EStatus				GetStatus() { return m_eStatus; }
	void				SetStatus(EStatus Status) { m_eStatus = Status; }
	const char*			GetResultFilename() { return m_szResultFilename; }
	void 				SetResultFilename(const char* v);
};

class FileInfo
{
public:
	typedef std::vector<ArticleInfo*>	Articles;
	typedef std::vector<char*>			Groups;

private:
	int					m_iID;
	NZBInfo*			m_pNZBInfo;
	Articles			m_Articles;
	Groups				m_Groups;
	char* 				m_szSubject;
	char*				m_szFilename;
	long long 			m_lSize;
	long long 			m_lRemainingSize;
	long long			m_lSuccessSize;
	long long			m_lFailedSize;
	long long			m_lMissedSize;
	int					m_iTotalArticles;
	int					m_iMissedArticles;
	int					m_iFailedArticles;
	int					m_iSuccessArticles;
	time_t				m_tTime;
	bool				m_bPaused;
	bool				m_bDeleted;
	bool				m_bFilenameConfirmed;
	bool				m_bParFile;
	int					m_iCompleted;
	bool				m_bOutputInitialized;
	char*				m_szOutputFilename;
	Mutex*				m_pMutexOutputFile;
	int					m_iPriority;
	bool				m_bExtraPriority;
	int					m_iActiveDownloads;
	bool				m_bAutoDeleted;

	static int			m_iIDGen;

public:
						FileInfo();
						~FileInfo();
	int					GetID() { return m_iID; }
	void				SetID(int iID);
	NZBInfo*			GetNZBInfo() { return m_pNZBInfo; }
	void				SetNZBInfo(NZBInfo* pNZBInfo);
	Articles* 			GetArticles() { return &m_Articles; }
	Groups* 			GetGroups() { return &m_Groups; }
	const char*			GetSubject() { return m_szSubject; }
	void 				SetSubject(const char* szSubject);
	const char*			GetFilename() { return m_szFilename; }
	void 				SetFilename(const char* szFilename);
	void				MakeValidFilename();
	bool				GetFilenameConfirmed() { return m_bFilenameConfirmed; }
	void				SetFilenameConfirmed(bool bFilenameConfirmed) { m_bFilenameConfirmed = bFilenameConfirmed; }
	void 				SetSize(long long lSize) { m_lSize = lSize; m_lRemainingSize = lSize; }
	long long 			GetSize() { return m_lSize; }
	long long 			GetRemainingSize() { return m_lRemainingSize; }
	void 				SetRemainingSize(long long lRemainingSize) { m_lRemainingSize = lRemainingSize; }
	long long			GetMissedSize() { return m_lMissedSize; }
	void 				SetMissedSize(long long lMissedSize) { m_lMissedSize = lMissedSize; }
	long long			GetSuccessSize() { return m_lSuccessSize; }
	void 				SetSuccessSize(long long lSuccessSize) { m_lSuccessSize = lSuccessSize; }
	long long			GetFailedSize() { return m_lFailedSize; }
	void 				SetFailedSize(long long lFailedSize) { m_lFailedSize = lFailedSize; }
	int					GetTotalArticles() { return m_iTotalArticles; }
	void 				SetTotalArticles(int iTotalArticles) { m_iTotalArticles = iTotalArticles; }
	int					GetMissedArticles() { return m_iMissedArticles; }
	void 				SetMissedArticles(int iMissedArticles) { m_iMissedArticles = iMissedArticles; }
	int					GetFailedArticles() { return m_iFailedArticles; }
	void 				SetFailedArticles(int iFailedArticles) { m_iFailedArticles = iFailedArticles; }
	int					GetSuccessArticles() { return m_iSuccessArticles; }
	void 				SetSuccessArticles(int iSuccessArticles) { m_iSuccessArticles = iSuccessArticles; }
	time_t				GetTime() { return m_tTime; }
	void				SetTime(time_t tTime) { m_tTime = tTime; }
	bool				GetPaused() { return m_bPaused; }
	void				SetPaused(bool Paused) { m_bPaused = Paused; }
	bool				GetDeleted() { return m_bDeleted; }
	void				SetDeleted(bool Deleted) { m_bDeleted = Deleted; }
	int					GetCompleted() { return m_iCompleted; }
	void				SetCompleted(int iCompleted) { m_iCompleted = iCompleted; }
	bool				GetParFile() { return m_bParFile; }
	void				SetParFile(bool bParFile) { m_bParFile = bParFile; }
	void				ClearArticles();
	void				LockOutputFile();
	void				UnlockOutputFile();
	const char*			GetOutputFilename() { return m_szOutputFilename; }
	void 				SetOutputFilename(const char* szOutputFilename);
	bool				GetOutputInitialized() { return m_bOutputInitialized; }
	void				SetOutputInitialized(bool bOutputInitialized) { m_bOutputInitialized = bOutputInitialized; }
	int					GetPriority() { return m_iPriority; }
	void				SetPriority(int iPriority) { m_iPriority = iPriority; }
	bool				GetExtraPriority() { return m_bExtraPriority; }
	void				SetExtraPriority(bool bExtraPriority) { m_bExtraPriority = bExtraPriority; };
	int					GetActiveDownloads() { return m_iActiveDownloads; }
	void				SetActiveDownloads(int iActiveDownloads);
	bool				GetAutoDeleted() { return m_bAutoDeleted; }
	void				SetAutoDeleted(bool bAutoDeleted) { m_bAutoDeleted = bAutoDeleted; }
};
                              
typedef std::deque<FileInfo*> FileQueue;

class GroupInfo
{
private:
	NZBInfo*			m_pNZBInfo;
	int					m_iFirstID;
	int					m_iLastID;
	int		 			m_iRemainingFileCount;
	int					m_iPausedFileCount;
	long long 			m_lRemainingSize;
	long long 			m_lPausedSize;
	int					m_iRemainingParCount;
	time_t				m_tMinTime;
	time_t				m_tMaxTime;
	int					m_iMinPriority;
	int					m_iMaxPriority;
	int					m_iActiveDownloads;

	friend class DownloadQueue;

public:
						GroupInfo();
						~GroupInfo();
	NZBInfo*			GetNZBInfo() { return m_pNZBInfo; }
	int					GetFirstID() { return m_iFirstID; }
	int					GetLastID() { return m_iLastID; }
	long long 			GetRemainingSize() { return m_lRemainingSize; }
	long long 			GetPausedSize() { return m_lPausedSize; }
	int					GetRemainingFileCount() { return m_iRemainingFileCount; }
	int					GetPausedFileCount() { return m_iPausedFileCount; }
	int					GetRemainingParCount() { return m_iRemainingParCount; }
	time_t				GetMinTime() { return m_tMinTime; }
	time_t				GetMaxTime() { return m_tMaxTime; }
	int					GetMinPriority() { return m_iMinPriority; }
	int					GetMaxPriority() { return m_iMaxPriority; }
	int					GetActiveDownloads() { return m_iActiveDownloads; }
};

typedef std::deque<GroupInfo*> GroupQueueBase;

class GroupQueue : public GroupQueueBase
{
public:
						~GroupQueue();
	void				Clear();
};


class NZBParameter
{
private:
	char* 				m_szName;
	char* 				m_szValue;

	void				SetValue(const char* szValue);

	friend class NZBParameterList;

public:
						NZBParameter(const char* szName);
						~NZBParameter();
	const char*			GetName() { return m_szName; }
	const char*			GetValue() { return m_szValue; }
};

typedef std::deque<NZBParameter*> NZBParameterListBase;

class NZBParameterList : public NZBParameterListBase
{
public:
						~NZBParameterList();
	void				SetParameter(const char* szName, const char* szValue);
	NZBParameter*		Find(const char* szName, bool bCaseSensitive);
	void				Clear();
	void				CopyFrom(NZBParameterList* pSourceParameters);
};

class ScriptStatus
{
public:
	enum EStatus
	{
		srNone,
		srFailure,
		srSuccess
	};

private:
	char* 				m_szName;
	EStatus				m_eStatus;
	
	friend class ScriptStatusList;
	
public:
						ScriptStatus(const char* szName, EStatus eStatus);
						~ScriptStatus();
	const char*			GetName() { return m_szName; }
	EStatus				GetStatus() { return m_eStatus; }
};

typedef std::deque<ScriptStatus*> ScriptStatusListBase;

class ScriptStatusList : public ScriptStatusListBase
{
public:
						~ScriptStatusList();
	void				Add(const char* szScriptName, ScriptStatus::EStatus eStatus);
	void				Clear();
	ScriptStatus::EStatus	CalcTotalStatus();
};

class ServerStat
{
private:
	int					m_iServerID;
	int					m_iSuccessArticles;
	int					m_iFailedArticles;

public:
						ServerStat(int iServerID);
	int					GetServerID() { return m_iServerID; }
	int					GetSuccessArticles() { return m_iSuccessArticles; }
	void				SetSuccessArticles(int iSuccessArticles) { m_iSuccessArticles = iSuccessArticles; }
	int					GetFailedArticles() { return m_iFailedArticles; }
	void				SetFailedArticles(int iFailedArticles) { m_iFailedArticles = iFailedArticles; }
};

typedef std::vector<ServerStat*>	ServerStatListBase;

class ServerStatList : public ServerStatListBase
{
public:
						~ServerStatList();
	void				SetStat(int iServerID, int iSuccessArticles, int iFailedArticles, bool bAdd);
	void				Add(ServerStatList* pServerStats);
	void				Clear();
};

class NZBInfoList;

class NZBInfo
{
public:
	enum ERenameStatus
	{
		rsNone,
		rsSkipped,
		rsFailure,
		rsSuccess
	};

	enum EParStatus
	{
		psNone,
		psSkipped,
		psFailure,
		psSuccess,
		psRepairPossible,
		psManual
	};

	enum EUnpackStatus
	{
		usNone,
		usSkipped,
		usFailure,
		usSuccess
	};

	enum ECleanupStatus
	{
		csNone,
		csFailure,
		csSuccess
	};

	enum EMoveStatus
	{
		msNone,
		msFailure,
		msSuccess
	};

	enum EDeleteStatus
	{
		dsNone,
		dsManual,
		dsHealth,
		dsDupe
	};

	typedef std::vector<char*>			Files;
	typedef std::deque<Message*>		Messages;

private:
	int					m_iID;
	int					m_iRefCount;
	char* 				m_szFilename;
	char*				m_szName;
	char* 				m_szDestDir;
	char* 				m_szFinalDir;
	char* 				m_szCategory;
	int		 			m_iFileCount;
	int		 			m_iParkedFileCount;
	long long 			m_lSize;
	long long			m_lSuccessSize;
	long long			m_lFailedSize;
	long long			m_lCurrentSuccessSize;
	long long			m_lCurrentFailedSize;
	long long			m_lParSize;
	long long			m_lParSuccessSize;
	long long			m_lParFailedSize;
	long long			m_lParCurrentSuccessSize;
	long long			m_lParCurrentFailedSize;
	int					m_iTotalArticles;
	int					m_iSuccessArticles;
	int					m_iFailedArticles;
	Files				m_completedFiles;
	bool				m_bPostProcess;
	ERenameStatus		m_eRenameStatus;
	EParStatus			m_eParStatus;
	EUnpackStatus		m_eUnpackStatus;
	ECleanupStatus		m_eCleanupStatus;
	EMoveStatus			m_eMoveStatus;
	EDeleteStatus		m_eDeleteStatus;
	char*				m_szQueuedFilename;
	bool				m_bDeleting;
	bool				m_bHealthPaused;
	bool				m_bParCleanup;
	bool				m_bParManual;
	bool				m_bCleanupDisk;
	bool				m_bUnpackCleanedUpDisk;
	char*				m_szDupeKey;
	int					m_iDupeScore;
	bool				m_bNoDupeCheck;
	bool				m_bDupe;
	unsigned int		m_iFullContentHash;
	unsigned int		m_iFilteredContentHash;
	NZBInfoList*		m_Owner;
	NZBParameterList	m_ppParameters;
	ScriptStatusList	m_scriptStatuses;
	ServerStatList		m_ServerStats;
	Mutex				m_mutexLog;
	Messages			m_Messages;
	int					m_iIDMessageGen;

	static int			m_iIDGen;

	friend class NZBInfoList;

public:
						NZBInfo();
						~NZBInfo();
	void				Retain();
	void				Release();
	int					GetID() { return m_iID; }
	void				SetID(int iID);
	const char*			GetFilename() { return m_szFilename; }
	void				SetFilename(const char* szFilename);
	static void			MakeNiceNZBName(const char* szNZBFilename, char* szBuffer, int iSize, bool bRemoveExt);
	const char*			GetDestDir() { return m_szDestDir; }   // needs locking (for shared objects)
	void				SetDestDir(const char* szDestDir);     // needs locking (for shared objects)
	const char*			GetFinalDir() { return m_szFinalDir; }   // needs locking (for shared objects)
	void				SetFinalDir(const char* szFinalDir);     // needs locking (for shared objects)
	const char*			GetCategory() { return m_szCategory; } // needs locking (for shared objects)
	void				SetCategory(const char* szCategory);   // needs locking (for shared objects)
	const char*			GetName() { return m_szName; } 	   // needs locking (for shared objects)
	void				SetName(const char* szName);	   // needs locking (for shared objects)
	int					GetFileCount() { return m_iFileCount; }
	void 				SetFileCount(int iFileCount) { m_iFileCount = iFileCount; }
	int					GetParkedFileCount() { return m_iParkedFileCount; }
	void 				SetParkedFileCount(int iParkedFileCount) { m_iParkedFileCount = iParkedFileCount; }
	long long 			GetSize() { return m_lSize; }
	void 				SetSize(long long lSize) { m_lSize = lSize; }
	long long			GetSuccessSize() { return m_lSuccessSize; }
	void 				SetSuccessSize(long long lSuccessSize) { m_lSuccessSize = lSuccessSize; }
	long long			GetFailedSize() { return m_lFailedSize; }
	void 				SetFailedSize(long long lFailedSize) { m_lFailedSize = lFailedSize; }
	long long			GetCurrentSuccessSize() { return m_lCurrentSuccessSize; }
	void 				SetCurrentSuccessSize(long long lCurrentSuccessSize) { m_lCurrentSuccessSize = lCurrentSuccessSize; }
	long long			GetCurrentFailedSize() { return m_lCurrentFailedSize; }
	void 				SetCurrentFailedSize(long long lCurrentFailedSize) { m_lCurrentFailedSize = lCurrentFailedSize; }
	long long			GetParSize() { return m_lParSize; }
	void 				SetParSize(long long lParSize) { m_lParSize = lParSize; }
	long long			GetParSuccessSize() { return m_lParSuccessSize; }
	void 				SetParSuccessSize(long long lParSuccessSize) { m_lParSuccessSize = lParSuccessSize; }
	long long			GetParFailedSize() { return m_lParFailedSize; }
	void 				SetParFailedSize(long long lParFailedSize) { m_lParFailedSize = lParFailedSize; }
	long long			GetParCurrentSuccessSize() { return m_lParCurrentSuccessSize; }
	void 				SetParCurrentSuccessSize(long long lParCurrentSuccessSize) { m_lParCurrentSuccessSize = lParCurrentSuccessSize; }
	long long			GetParCurrentFailedSize() { return m_lParCurrentFailedSize; }
	void 				SetParCurrentFailedSize(long long lParCurrentFailedSize) { m_lParCurrentFailedSize = lParCurrentFailedSize; }
	int					GetTotalArticles() { return m_iTotalArticles; }
	void 				SetTotalArticles(int iTotalArticles) { m_iTotalArticles = iTotalArticles; }
	int					GetSuccessArticles() { return m_iSuccessArticles; }
	void 				SetSuccessArticles(int iSuccessArticles) { m_iSuccessArticles = iSuccessArticles; }
	int					GetFailedArticles() { return m_iFailedArticles; }
	void 				SetFailedArticles(int iFailedArticles) { m_iFailedArticles = iFailedArticles; }
	void				BuildDestDirName();
	void				BuildFinalDirName(char* szFinalDirBuf, int iBufSize);
	Files*				GetCompletedFiles() { return &m_completedFiles; }		// needs locking (for shared objects)
	void				ClearCompletedFiles();
	bool				GetPostProcess() { return m_bPostProcess; }
	void				SetPostProcess(bool bPostProcess) { m_bPostProcess = bPostProcess; }
	ERenameStatus		GetRenameStatus() { return m_eRenameStatus; }
	void				SetRenameStatus(ERenameStatus eRenameStatus) { m_eRenameStatus = eRenameStatus; }
	EParStatus			GetParStatus() { return m_eParStatus; }
	void				SetParStatus(EParStatus eParStatus) { m_eParStatus = eParStatus; }
	EUnpackStatus		GetUnpackStatus() { return m_eUnpackStatus; }
	void				SetUnpackStatus(EUnpackStatus eUnpackStatus) { m_eUnpackStatus = eUnpackStatus; }
	ECleanupStatus		GetCleanupStatus() { return m_eCleanupStatus; }
	void				SetCleanupStatus(ECleanupStatus eCleanupStatus) { m_eCleanupStatus = eCleanupStatus; }
	EMoveStatus			GetMoveStatus() { return m_eMoveStatus; }
	void				SetMoveStatus(EMoveStatus eMoveStatus) { m_eMoveStatus = eMoveStatus; }
	EDeleteStatus		GetDeleteStatus() { return m_eDeleteStatus; }
	void				SetDeleteStatus(EDeleteStatus eDeleteStatus) { m_eDeleteStatus = eDeleteStatus; }
	const char*			GetQueuedFilename() { return m_szQueuedFilename; }
	void				SetQueuedFilename(const char* szQueuedFilename);
	bool				GetDeleting() { return m_bDeleting; }
	void				SetDeleting(bool bDeleting) { m_bDeleting = bDeleting; }
	bool				GetHealthPaused() { return m_bHealthPaused; }
	void				SetHealthPaused(bool bHealthPaused) { m_bHealthPaused = bHealthPaused; }
	bool				GetParCleanup() { return m_bParCleanup; }
	void				SetParCleanup(bool bParCleanup) { m_bParCleanup = bParCleanup; }
	bool				GetCleanupDisk() { return m_bCleanupDisk; }
	void				SetCleanupDisk(bool bCleanupDisk) { m_bCleanupDisk = bCleanupDisk; }
	bool				GetUnpackCleanedUpDisk() { return m_bUnpackCleanedUpDisk; }
	void				SetUnpackCleanedUpDisk(bool bUnpackCleanedUpDisk) { m_bUnpackCleanedUpDisk = bUnpackCleanedUpDisk; }
	NZBParameterList*	GetParameters() { return &m_ppParameters; }				// needs locking (for shared objects)
	ScriptStatusList*	GetScriptStatuses() { return &m_scriptStatuses; }        // needs locking (for shared objects)
	ServerStatList*		GetServerStats() { return &m_ServerStats; }
	int					CalcHealth();
	int					CalcCriticalHealth();
	const char*			GetDupeKey() { return m_szDupeKey; }					// needs locking (for shared objects)
	void				SetDupeKey(const char* szDupeKey);						// needs locking (for shared objects)
	int					GetDupeScore() { return m_iDupeScore; }
	void				SetDupeScore(int iDupeScore) { m_iDupeScore = iDupeScore; }
	bool				GetNoDupeCheck() { return m_bNoDupeCheck; }
	void				SetNoDupeCheck(bool bNoDupeCheck) { m_bNoDupeCheck = bNoDupeCheck; }
	int					GetDupe() { return m_bDupe; }
	void				SetDupe(bool bDupe) { m_bDupe = bDupe; }
	unsigned int		GetFullContentHash() { return m_iFullContentHash; }
	void				SetFullContentHash(unsigned int iFullContentHash) { m_iFullContentHash = iFullContentHash; }
	unsigned int		GetFilteredContentHash() { return m_iFilteredContentHash; }
	void				SetFilteredContentHash(unsigned int iFilteredContentHash) { m_iFilteredContentHash = iFilteredContentHash; }
	void				AppendMessage(Message::EKind eKind, time_t tTime, const char* szText);
	Messages*			LockMessages();
	void				UnlockMessages();
};

typedef std::deque<NZBInfo*> NZBInfoListBase;

class NZBInfoList : public NZBInfoListBase
{
public:
	void				Add(NZBInfo* pNZBInfo);
	void				Remove(NZBInfo* pNZBInfo);
	void				ReleaseAll();
};

class PostInfo
{
public:
	enum EStage
	{
		ptQueued,
		ptLoadingPars,
		ptVerifyingSources,
		ptRepairing,
		ptVerifyingRepaired,
		ptRenaming,
		ptUnpacking,
		ptMoving,
		ptExecutingScript,
		ptFinished
	};

	typedef std::deque<Message*>	Messages;

private:
	int					m_iID;
	NZBInfo*			m_pNZBInfo;
	char*				m_szInfoName;
	bool				m_bWorking;
	bool				m_bDeleted;
	bool				m_bRequestParCheck;
	bool				m_bRequestParRename;
	EStage				m_eStage;
	char*				m_szProgressLabel;
	int					m_iFileProgress;
	int					m_iStageProgress;
	time_t				m_tStartTime;
	time_t				m_tStageTime;
	Thread*				m_pPostThread;
	
	Mutex				m_mutexLog;
	Messages			m_Messages;
	int					m_iIDMessageGen;

	static int			m_iIDGen;

public:
						PostInfo();
						~PostInfo();
	int					GetID() { return m_iID; }
	NZBInfo*			GetNZBInfo() { return m_pNZBInfo; }
	void				SetNZBInfo(NZBInfo* pNZBInfo);
	const char*			GetInfoName() { return m_szInfoName; }
	void				SetInfoName(const char* szInfoName);
	EStage				GetStage() { return m_eStage; }
	void				SetStage(EStage eStage) { m_eStage = eStage; }
	void				SetProgressLabel(const char* szProgressLabel);
	const char*			GetProgressLabel() { return m_szProgressLabel; }
	int					GetFileProgress() { return m_iFileProgress; }
	void				SetFileProgress(int iFileProgress) { m_iFileProgress = iFileProgress; }
	int					GetStageProgress() { return m_iStageProgress; }
	void				SetStageProgress(int iStageProgress) { m_iStageProgress = iStageProgress; }
	time_t				GetStartTime() { return m_tStartTime; }
	void				SetStartTime(time_t tStartTime) { m_tStartTime = tStartTime; }
	time_t				GetStageTime() { return m_tStageTime; }
	void				SetStageTime(time_t tStageTime) { m_tStageTime = tStageTime; }
	bool				GetWorking() { return m_bWorking; }
	void				SetWorking(bool bWorking) { m_bWorking = bWorking; }
	bool				GetDeleted() { return m_bDeleted; }
	void				SetDeleted(bool bDeleted) { m_bDeleted = bDeleted; }
	bool				GetRequestParCheck() { return m_bRequestParCheck; }
	void				SetRequestParCheck(bool bRequestParCheck) { m_bRequestParCheck = bRequestParCheck; }
	bool				GetRequestParRename() { return m_bRequestParRename; }
	void				SetRequestParRename(bool bRequestParRename) { m_bRequestParRename = bRequestParRename; }
	void				AppendMessage(Message::EKind eKind, const char* szText);
	Thread*				GetPostThread() { return m_pPostThread; }
	void				SetPostThread(Thread* pPostThread) { m_pPostThread = pPostThread; }
	Messages*			LockMessages();
	void				UnlockMessages();
};

typedef std::deque<PostInfo*> PostQueue;

typedef std::vector<int> IDList;

typedef std::vector<char*> NameList;

class UrlInfo
{
public:
	enum EStatus
	{
		aiUndefined,
		aiRunning,
		aiFinished,
		aiFailed,
		aiRetry,
		aiScanSkipped,
		aiScanFailed
	};

private:
	int					m_iID;
	char*				m_szURL;
	char*				m_szNZBFilename;
	char* 				m_szCategory;
	int					m_iPriority;
	char*				m_szDupeKey;
	int					m_iDupeScore;
	bool				m_bNoDupeCheck;
	bool				m_bAddTop;
	bool				m_bAddPaused;
	bool				m_bForce;
	EStatus				m_eStatus;

	static int			m_iIDGen;

public:
						UrlInfo();
						~UrlInfo();
	int					GetID() { return m_iID; }
	void				SetID(int iID);
	const char*			GetURL() { return m_szURL; }			// needs locking (for shared objects)
	void				SetURL(const char* szURL);				// needs locking (for shared objects)
	const char*			GetNZBFilename() { return m_szNZBFilename; }		// needs locking (for shared objects)
	void				SetNZBFilename(const char* szNZBFilename);			// needs locking (for shared objects)
	const char*			GetCategory() { return m_szCategory; }	// needs locking (for shared objects)
	void				SetCategory(const char* szCategory);	// needs locking (for shared objects)
	int					GetPriority() { return m_iPriority; }
	void				SetPriority(int iPriority) { m_iPriority = iPriority; }
	const char*			GetDupeKey() { return m_szDupeKey; }
	void				SetDupeKey(const char* szDupeKey);
	int					GetDupeScore() { return m_iDupeScore; }
	void				SetDupeScore(int iDupeScore) { m_iDupeScore = iDupeScore; }
	bool				GetNoDupeCheck() { return m_bNoDupeCheck; }
	void				SetNoDupeCheck(bool bNoDupeCheck) { m_bNoDupeCheck = bNoDupeCheck; }
	bool				GetAddTop() { return m_bAddTop; }
	void				SetAddTop(bool bAddTop) { m_bAddTop = bAddTop; }
	bool				GetAddPaused() { return m_bAddPaused; }
	void				SetAddPaused(bool bAddPaused) { m_bAddPaused = bAddPaused; }
	void				GetName(char* szBuffer, int iSize);		// needs locking (for shared objects)
	static void			MakeNiceName(const char* szURL, const char* szNZBFilename, char* szBuffer, int iSize);
	bool				GetForce() { return m_bForce; }
	void				SetForce(bool bForce) { m_bForce = bForce; }
	EStatus				GetStatus() { return m_eStatus; }
	void				SetStatus(EStatus Status) { m_eStatus = Status; }
};

typedef std::deque<UrlInfo*> UrlQueue;

class DupInfo
{
public:
	enum EStatus
	{
		dsUndefined,
		dsSuccess,
		dsFailed,
		dsDeleted
	};

private:
	char*				m_szName;
	char*				m_szDupeKey;
	int					m_iDupeScore;
	long long 			m_lSize;
	unsigned int		m_iFullContentHash;
	unsigned int		m_iFilteredContentHash;
	EStatus				m_eStatus;

public:
						DupInfo();
						~DupInfo();
	const char*			GetName() { return m_szName; }			// needs locking (for shared objects)
	void				SetName(const char* szName);			// needs locking (for shared objects)
	const char*			GetDupeKey() { return m_szDupeKey; }	// needs locking (for shared objects)
	void				SetDupeKey(const char* szDupeKey);		// needs locking (for shared objects)
	int					GetDupeScore() { return m_iDupeScore; }
	void				SetDupeScore(int iDupeScore) { m_iDupeScore = iDupeScore; }
	long long			GetSize() { return m_lSize; }
	void 				SetSize(long long lSize) { m_lSize = lSize; }
	unsigned int		GetFullContentHash() { return m_iFullContentHash; }
	void				SetFullContentHash(unsigned int iFullContentHash) { m_iFullContentHash = iFullContentHash; }
	unsigned int		GetFilteredContentHash() { return m_iFilteredContentHash; }
	void				SetFilteredContentHash(unsigned int iFilteredContentHash) { m_iFilteredContentHash = iFilteredContentHash; }
	EStatus				GetStatus() { return m_eStatus; }
	void				SetStatus(EStatus Status) { m_eStatus = Status; }
};

class HistoryInfo
{
public:
	enum EKind
	{
		hkUnknown,
		hkNZBInfo,
		hkUrlInfo,
		hkDupInfo
	};

private:
	int					m_iID;
	EKind				m_eKind;
	void*				m_pInfo;
	time_t				m_tTime;

	static int			m_iIDGen;

public:
						HistoryInfo(NZBInfo* pNZBInfo);
						HistoryInfo(UrlInfo* pUrlInfo);
						HistoryInfo(DupInfo* pDupInfo);
						~HistoryInfo();
	int					GetID() { return m_iID; }
	void				SetID(int iID);
	EKind				GetKind() { return m_eKind; }
	NZBInfo*			GetNZBInfo() { return (NZBInfo*)m_pInfo; }
	UrlInfo*			GetUrlInfo() { return (UrlInfo*)m_pInfo; }
	DupInfo*			GetDupInfo() { return (DupInfo*)m_pInfo; }
	void				DiscardUrlInfo() { m_pInfo = NULL; }
	time_t				GetTime() { return m_tTime; }
	void				SetTime(time_t tTime) { m_tTime = tTime; }
	void				GetName(char* szBuffer, int iSize);		// needs locking (for shared objects)
};

typedef std::deque<HistoryInfo*> HistoryList;

class DownloadQueue
{
protected:
	NZBInfoList			m_NZBInfoList;
	FileQueue			m_FileQueue;
	PostQueue			m_PostQueue;
	HistoryList			m_HistoryList;
	FileQueue			m_ParkedFiles;
	UrlQueue			m_UrlQueue;

public:
	NZBInfoList*		GetNZBInfoList() { return &m_NZBInfoList; }
	FileQueue*			GetFileQueue() { return &m_FileQueue; }
	PostQueue*			GetPostQueue() { return &m_PostQueue; }
	HistoryList*		GetHistoryList() { return &m_HistoryList; }
	FileQueue*			GetParkedFiles() { return &m_ParkedFiles; }
	UrlQueue*			GetUrlQueue() { return &m_UrlQueue; }
	void				BuildGroups(GroupQueue* pGroupQueue);
};

class DownloadQueueHolder
{
public:
	virtual					~DownloadQueueHolder() {};
	virtual DownloadQueue*	LockQueue() = 0;
	virtual void			UnlockQueue() = 0;
};

class FeedInfo
{
public:
	enum EStatus
	{
		fsUndefined,
		fsRunning,
		fsFinished,
		fsFailed
	};

private:
	int					m_iID;
	char*				m_szName;
	char*				m_szUrl;
	int					m_iInterval;
	char*				m_szFilter;
	unsigned int		m_iFilterHash;
	bool				m_bPauseNzb;
	char*				m_szCategory;
	int					m_iPriority;
	time_t				m_tLastUpdate;
	bool				m_bPreview;
	EStatus				m_eStatus;
	char*				m_szOutputFilename;
	bool				m_bFetch;
	bool				m_bForce;

public:
						FeedInfo(int iID, const char* szName, const char* szUrl, int iInterval,
							const char* szFilter, bool bPauseNzb, const char* szCategory, int iPriority);
						~FeedInfo();
	int					GetID() { return m_iID; }
	const char*			GetName() { return m_szName; }
	const char*			GetUrl() { return m_szUrl; }
	int					GetInterval() { return m_iInterval; }
	const char*			GetFilter() { return m_szFilter; }
	unsigned int		GetFilterHash() { return m_iFilterHash; }
	bool				GetPauseNzb() { return m_bPauseNzb; }
	const char*			GetCategory() { return m_szCategory; }
	int					GetPriority() { return m_iPriority; }
	time_t				GetLastUpdate() { return m_tLastUpdate; }
	void				SetLastUpdate(time_t tLastUpdate) { m_tLastUpdate = tLastUpdate; }
	bool				GetPreview() { return m_bPreview; }
	void				SetPreview(bool bPreview) { m_bPreview = bPreview; }
	EStatus				GetStatus() { return m_eStatus; }
	void				SetStatus(EStatus Status) { m_eStatus = Status; }
	const char*			GetOutputFilename() { return m_szOutputFilename; }
	void 				SetOutputFilename(const char* szOutputFilename);
	bool				GetFetch() { return m_bFetch; }
	void				SetFetch(bool bFetch) { m_bFetch = bFetch; }
	bool				GetForce() { return m_bForce; }
	void				SetForce(bool bForce) { m_bForce = bForce; }
};

typedef std::deque<FeedInfo*> Feeds;

class FeedItemInfo
{
public:
	enum EStatus
	{
		isUnknown,
		isBacklog,
		isFetched,
		isNew
	};

	enum EMatchStatus
	{
		msIgnored,
		msAccepted,
		msRejected
	};

private:
	char*				m_szTitle;
	char*				m_szFilename;
	char*				m_szUrl;
	time_t				m_tTime;
	long long			m_lSize;
	char*				m_szCategory;
	int					m_iRating;
	char*				m_szGenre;
	int					m_iImdbId;
	int					m_iRageId;
	char*				m_szSeason;
	char*				m_szEpisode;
	int					m_iSeasonNum;
	int					m_iEpisodeNum;
	char*				m_szAddCategory;
	bool				m_bPauseNzb;
	int					m_iPriority;
	EStatus				m_eStatus;
	EMatchStatus		m_eMatchStatus;
	int					m_iMatchRule;
	char*				m_szDupeKey;
	int					m_iDupeScore;
	bool				m_bNoDupeCheck;

	int					ParsePrefixedInt(const char *szValue);

public:
						FeedItemInfo();
						~FeedItemInfo();
	const char*			GetTitle() { return m_szTitle; }
	void				SetTitle(const char* szTitle);
	const char*			GetFilename() { return m_szFilename; }
	void				SetFilename(const char* szFilename);
	const char*			GetUrl() { return m_szUrl; }
	void				SetUrl(const char* szUrl);
	long long			GetSize() { return m_lSize; }
	void				SetSize(long long lSize) { m_lSize = lSize; }
	const char*			GetCategory() { return m_szCategory; }
	void				SetCategory(const char* szCategory);
	int					GetRating() { return m_iRating; }
	void				SetRating(int iRating) { m_iRating = iRating; }
	const char*			GetGenre() { return m_szGenre; }
	void				SetGenre(const char* szGenre);
	int					GetImdbId() { return m_iImdbId; }
	void				SetImdbId(int iImdbId) { m_iImdbId = iImdbId; }
	int					GetRageId() { return m_iRageId; }
	void				SetRageId(int iRageId) { m_iRageId = iRageId; }
	const char*			GetSeason() { return m_szSeason; }
	void				SetSeason(const char* szSeason);
	const char*			GetEpisode() { return m_szEpisode; }
	void				SetEpisode(const char* szEpisode);
	int					GetSeasonNum() { return m_iSeasonNum; }
	int					GetEpisodeNum() { return m_iEpisodeNum; }
	const char*			GetAddCategory() { return m_szAddCategory; }
	void				SetAddCategory(const char* szAddCategory);
	bool				GetPauseNzb() { return m_bPauseNzb; }
	void				SetPauseNzb(bool bPauseNzb) { m_bPauseNzb = bPauseNzb; }
	int					GetPriority() { return m_iPriority; }
	void				SetPriority(int iPriority) { m_iPriority = iPriority; }
	time_t				GetTime() { return m_tTime; }
	void				SetTime(time_t tTime) { m_tTime = tTime; }
	EStatus				GetStatus() { return m_eStatus; }
	void				SetStatus(EStatus eStatus) { m_eStatus = eStatus; }
	EMatchStatus		GetMatchStatus() { return m_eMatchStatus; }
	void				SetMatchStatus(EMatchStatus eMatchStatus) { m_eMatchStatus = eMatchStatus; }
	int					GetMatchRule() { return m_iMatchRule; }
	void				SetMatchRule(int iMatchRule) { m_iMatchRule = iMatchRule; }
	const char*			GetDupeKey() { return m_szDupeKey; }
	void				SetDupeKey(const char* szDupeKey);
	void				AppendDupeKey(const char* szExtraDupeKey);
	void				BuildDupeKey();
	int					GetDupeScore() { return m_iDupeScore; }
	void				SetDupeScore(int iDupeScore) { m_iDupeScore = iDupeScore; }
	bool				GetNoDupeCheck() { return m_bNoDupeCheck; }
	void				SetNoDupeCheck(bool bNoDupeCheck) { m_bNoDupeCheck = bNoDupeCheck; }
};

typedef std::deque<FeedItemInfo*>	FeedItemInfosBase;

class FeedItemInfos : public FeedItemInfosBase
{
private:
	int					m_iRefCount;

public:
						FeedItemInfos();
						~FeedItemInfos();
	void				Retain();
	void				Release();
};

class FeedHistoryInfo
{
public:
	enum EStatus
	{
		hsUnknown,
		hsBacklog,
		hsFetched
	};

private:
	char*				m_szUrl;
	EStatus				m_eStatus;
	time_t				m_tLastSeen;

public:
						FeedHistoryInfo(const char* szUrl, EStatus eStatus, time_t tLastSeen);
						~FeedHistoryInfo();
	const char*			GetUrl() { return m_szUrl; }
	EStatus				GetStatus() { return m_eStatus; }
	void				SetStatus(EStatus Status) { m_eStatus = Status; }
	time_t				GetLastSeen() { return m_tLastSeen; }
	void				SetLastSeen(time_t tLastSeen) { m_tLastSeen = tLastSeen; }
};

typedef std::deque<FeedHistoryInfo*> FeedHistoryBase;

class FeedHistory : public FeedHistoryBase
{
public:
						~FeedHistory();
	void				Clear();
	void				Add(const char* szUrl, FeedHistoryInfo::EStatus eStatus, time_t tLastSeen);
	void				Remove(const char* szUrl);
	FeedHistoryInfo*	Find(const char* szUrl);
};

#endif
