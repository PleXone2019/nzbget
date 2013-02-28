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
	time_t				m_tTime;
	bool				m_bPaused;
	bool				m_bDeleted;
	bool				m_bFilenameConfirmed;
	int					m_iCompleted;
	bool				m_bOutputInitialized;
	char*				m_szOutputFilename;
	Mutex				m_mutexOutputFile;
	int					m_iPriority;
	bool				m_bExtraPriority;
	int					m_iActiveDownloads;

	static int			m_iIDGen;

public:
						FileInfo();
						~FileInfo();
	int					GetID() { return m_iID; }
	void				SetID(int s);
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
	void 				SetSize(long long s) { m_lSize = s; m_lRemainingSize = s; }
	long long 			GetSize() { return m_lSize; }
	long long 			GetRemainingSize() { return m_lRemainingSize; }
	void 				SetRemainingSize(long long s) { m_lRemainingSize = s; }
	time_t				GetTime() { return m_tTime; }
	void				SetTime(time_t tTime) { m_tTime = tTime; }
	bool				GetPaused() { return m_bPaused; }
	void				SetPaused(bool Paused) { m_bPaused = Paused; }
	bool				GetDeleted() { return m_bDeleted; }
	void				SetDeleted(bool Deleted) { m_bDeleted = Deleted; }
	int					GetCompleted() { return m_iCompleted; }
	void				SetCompleted(int s) { m_iCompleted = s; }
	void				ClearArticles();
	void				LockOutputFile();
	void				UnlockOutputFile();
	const char*			GetOutputFilename() { return m_szOutputFilename; }
	void 				SetOutputFilename(const char* szOutputFilename);
	bool				GetOutputInitialized() { return m_bOutputInitialized; }
	void				SetOutputInitialized(bool bOutputInitialized) { m_bOutputInitialized = bOutputInitialized; }
	bool				IsDupe(const char* szFilename);
	int					GetPriority() { return m_iPriority; }
	void				SetPriority(int iPriority) { m_iPriority = iPriority; }
	bool				GetExtraPriority() { return m_bExtraPriority; }
	void				SetExtraPriority(bool bExtraPriority) { m_bExtraPriority = bExtraPriority; };
	int					GetActiveDownloads() { return m_iActiveDownloads; }
	void				SetActiveDownloads(int iActiveDownloads) { m_iActiveDownloads = iActiveDownloads; }
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

typedef std::deque<GroupInfo*> GroupQueue;

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
	void				SetParameter(const char* szName, const char* szValue);
};

class NZBInfoList;

class NZBInfo
{
public:
	enum EParStatus
	{
		psNone,
		psSkipped,
		psFailure,
		psSuccess,
		psRepairPossible
	};

	enum EUnpackStatus
	{
		usNone,
		usSkipped,
		usFailure,
		usSuccess
	};

	enum EScriptStatus
	{
		srNone,
		srUnknown,
		srFailure,
		srSuccess
	};

	typedef std::vector<char*>			Files;
	typedef std::deque<Message*>		Messages;

private:
	int					m_iID;
	int					m_iRefCount;
	char* 				m_szFilename;
	char*				m_szName;
	char* 				m_szDestDir;
	char* 				m_szCategory;
	int		 			m_iFileCount;
	int		 			m_iParkedFileCount;
	long long 			m_lSize;
	Files				m_completedFiles;
	bool				m_bPostProcess;
	EParStatus			m_eParStatus;
	EUnpackStatus		m_eUnpackStatus;
	EScriptStatus		m_eScriptStatus;
	char*				m_szQueuedFilename;
	bool				m_bDeleted;
	bool				m_bParCleanup;
	bool				m_bCleanupDisk;
	bool				m_bUnpackCleanedUpDisk;
	NZBInfoList*		m_Owner;
	NZBParameterList	m_ppParameters;
	Mutex				m_mutexLog;
	Messages			m_Messages;
	int					m_iIDMessageGen;

	static int			m_iIDGen;

	friend class NZBInfoList;

public:
						NZBInfo();
						~NZBInfo();
	void				AddReference();
	void				Release();
	int					GetID() { return m_iID; }
	const char*			GetFilename() { return m_szFilename; }
	void				SetFilename(const char* szFilename);
	static void			MakeNiceNZBName(const char* szNZBFilename, char* szBuffer, int iSize, bool bRemoveExt);
	const char*			GetDestDir() { return m_szDestDir; }   // needs locking (for shared objects)
	void				SetDestDir(const char* szDestDir);     // needs locking (for shared objects)
	const char*			GetCategory() { return m_szCategory; } // needs locking (for shared objects)
	void				SetCategory(const char* szCategory);   // needs locking (for shared objects)
	const char*			GetName() { return m_szName; } 	   // needs locking (for shared objects)
	void				SetName(const char* szName);	   // needs locking (for shared objects)
	long long 			GetSize() { return m_lSize; }
	void 				SetSize(long long lSize) { m_lSize = lSize; }
	int					GetFileCount() { return m_iFileCount; }
	void 				SetFileCount(int iFileCount) { m_iFileCount = iFileCount; }
	int					GetParkedFileCount() { return m_iParkedFileCount; }
	void 				SetParkedFileCount(int iParkedFileCount) { m_iParkedFileCount = iParkedFileCount; }
	void				BuildDestDirName();
	Files*				GetCompletedFiles() { return &m_completedFiles; }		// needs locking (for shared objects)
	void				ClearCompletedFiles();
	bool				GetPostProcess() { return m_bPostProcess; }
	void				SetPostProcess(bool bPostProcess) { m_bPostProcess = bPostProcess; }
	EParStatus			GetParStatus() { return m_eParStatus; }
	void				SetParStatus(EParStatus eParStatus) { m_eParStatus = eParStatus; }
	EUnpackStatus		GetUnpackStatus() { return m_eUnpackStatus; }
	void				SetUnpackStatus(EUnpackStatus eUnpackStatus) { m_eUnpackStatus = eUnpackStatus; }
	EScriptStatus		GetScriptStatus() { return m_eScriptStatus; }
	void				SetScriptStatus(EScriptStatus eScriptStatus) { m_eScriptStatus = eScriptStatus; }
	const char*			GetQueuedFilename() { return m_szQueuedFilename; }
	void				SetQueuedFilename(const char* szQueuedFilename);
	bool				GetDeleted() { return m_bDeleted; }
	void				SetDeleted(bool bDeleted) { m_bDeleted = bDeleted; }
	bool				GetParCleanup() { return m_bParCleanup; }
	void				SetParCleanup(bool bParCleanup) { m_bParCleanup = bParCleanup; }
	bool				GetCleanupDisk() { return m_bCleanupDisk; }
	void				SetCleanupDisk(bool bCleanupDisk) { m_bCleanupDisk = bCleanupDisk; }
	bool				GetUnpackCleanedUpDisk() { return m_bUnpackCleanedUpDisk; }
	void				SetUnpackCleanedUpDisk(bool bUnpackCleanedUpDisk) { m_bUnpackCleanedUpDisk = bUnpackCleanedUpDisk; }
	NZBParameterList*	GetParameters() { return &m_ppParameters; }				// needs locking (for shared objects)
	void				SetParameter(const char* szName, const char* szValue);	// needs locking (for shared objects)
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
		ptUnpacking,
		ptExecutingScript,
		ptFinished
	};

	enum EParStatus
	{
		psNone,
		psSkipped,
		psFailure,
		psSuccess,
		psRepairPossible
	};

	enum ERequestParCheck
	{
		rpNone,
		rpCurrent,
		rpAll
	};

	enum EUnpackStatus
	{
		usNone,
		usSkipped,
		usFailure,
		usSuccess
	};

	enum EScriptStatus
	{
		srNone,
		srUnknown,
		srFailure,
		srSuccess
	};

	typedef std::deque<Message*>	Messages;

private:
	int					m_iID;
	NZBInfo*			m_pNZBInfo;
	char*				m_szParFilename;
	char*				m_szInfoName;
	bool				m_bWorking;
	bool				m_bDeleted;
	EParStatus			m_eParStatus;
	EUnpackStatus		m_eUnpackStatus;
	EScriptStatus		m_eScriptStatus;
	ERequestParCheck	m_eRequestParCheck;
	EStage				m_eStage;
	char*				m_szProgressLabel;
	int					m_iFileProgress;
	int					m_iStageProgress;
	time_t				m_tStartTime;
	time_t				m_tStageTime;
	Thread*				m_pScriptThread;
	
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
	const char*			GetParFilename() { return m_szParFilename; }
	void				SetParFilename(const char* szParFilename);
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
	EParStatus			GetParStatus() { return m_eParStatus; }
	void				SetParStatus(EParStatus eParStatus) { m_eParStatus = eParStatus; }
	EUnpackStatus		GetUnpackStatus() { return m_eUnpackStatus; }
	void				SetUnpackStatus(EUnpackStatus eUnpackStatus) { m_eUnpackStatus = eUnpackStatus; }
	ERequestParCheck	GetRequestParCheck() { return m_eRequestParCheck; }
	void				SetRequestParCheck(ERequestParCheck eRequestParCheck) { m_eRequestParCheck = eRequestParCheck; }
	EScriptStatus		GetScriptStatus() { return m_eScriptStatus; }
	void				SetScriptStatus(EScriptStatus eScriptStatus) { m_eScriptStatus = eScriptStatus; }
	void				AppendMessage(Message::EKind eKind, const char* szText);
	Thread*				GetScriptThread() { return m_pScriptThread; }
	void				SetScriptThread(Thread* pScriptThread) { m_pScriptThread = pScriptThread; }
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
		aiRetry
	};

private:
	int					m_iID;
	char*				m_szURL;
	char*				m_szNZBFilename;
	char* 				m_szCategory;
	int					m_iPriority;
	bool				m_bAddTop;
	bool				m_bAddPaused;
	EStatus				m_eStatus;

	static int			m_iIDGen;

public:
						UrlInfo();
						~UrlInfo();
	int					GetID() { return m_iID; }
	void				SetID(int s);
	const char*			GetURL() { return m_szURL; }			// needs locking (for shared objects)
	void				SetURL(const char* szURL);				// needs locking (for shared objects)
	const char*			GetNZBFilename() { return m_szNZBFilename; }		// needs locking (for shared objects)
	void				SetNZBFilename(const char* szNZBFilename);			// needs locking (for shared objects)
	const char*			GetCategory() { return m_szCategory; }	// needs locking (for shared objects)
	void				SetCategory(const char* szCategory);	// needs locking (for shared objects)
	int					GetPriority() { return m_iPriority; }
	void				SetPriority(int iPriority) { m_iPriority = iPriority; }
	bool				GetAddTop() { return m_bAddTop; }
	void				SetAddTop(bool bAddTop) { m_bAddTop = bAddTop; }
	bool				GetAddPaused() { return m_bAddPaused; }
	void				SetAddPaused(bool bAddPaused) { m_bAddPaused = bAddPaused; }
	void				GetName(char* szBuffer, int iSize);		// needs locking (for shared objects)
	static void			MakeNiceName(const char* szURL, const char* szNZBFilename, char* szBuffer, int iSize);
	EStatus				GetStatus() { return m_eStatus; }
	void				SetStatus(EStatus Status) { m_eStatus = Status; }
};

typedef std::deque<UrlInfo*> UrlQueue;

class HistoryInfo
{
public:
	enum EKind
	{
		hkUnknown,
		hkNZBInfo,
		hkUrlInfo
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
						~HistoryInfo();
	int					GetID() { return m_iID; }
	void				SetID(int s);
	EKind				GetKind() { return m_eKind; }
	NZBInfo*			GetNZBInfo() { return (NZBInfo*)m_pInfo; }
	UrlInfo*			GetUrlInfo() { return (UrlInfo*)m_pInfo; }
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

#endif
