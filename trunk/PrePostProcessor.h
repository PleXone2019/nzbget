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


#ifndef PREPOSTPROCESSOR_H
#define PREPOSTPROCESSOR_H

#include <deque>

#include "Thread.h"
#include "Observer.h"
#include "DownloadInfo.h"
#include "Scanner.h"
#include "ParCoordinator.h"

class PrePostProcessor : public Thread
{
public:
	enum EEditAction
	{
		eaPostMoveOffset = 51,			// move post to m_iOffset relative to the current position in post-queue
		eaPostMoveTop,
		eaPostMoveBottom,
		eaPostDelete,
		eaHistoryDelete,
		eaHistoryReturn,
		eaHistoryProcess
	};

private:
	class QueueCoordinatorObserver: public Observer
	{
	public:
		PrePostProcessor* m_pOwner;
		virtual void	Update(Subject* Caller, void* Aspect) { m_pOwner->QueueCoordinatorUpdate(Caller, Aspect); }
	};

	class PostParCoordinator: public ParCoordinator
	{
	private:
		PrePostProcessor*	m_pOwner;
	protected:
		virtual bool		PauseDownload() { return m_pOwner->PauseDownload(); }
		virtual bool		UnpauseDownload() { return m_pOwner->UnpauseDownload(); }

		friend class PrePostProcessor;
	};
	
private:
	PostParCoordinator	m_ParCoordinator;
	QueueCoordinatorObserver	m_QueueCoordinatorObserver;
	bool				m_bHasMoreJobs;
	bool				m_bSchedulerPauseChanged;
	bool				m_bSchedulerPause;
	bool				m_bPostPause;
	Scanner				m_Scanner;
	const char*			m_szPauseReason;

	bool				IsNZBFileCompleted(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo, 
							bool bIgnorePausedPars, bool bAllowOnlyOneDeleted);
	void				CheckPostQueue();
	void				JobCompleted(DownloadQueue* pDownloadQueue, PostInfo* pPostInfo);
	void				StartJob(DownloadQueue* pDownloadQueue, PostInfo* pPostInfo);
	void				SaveQueue(DownloadQueue* pDownloadQueue);
	void				SanitisePostQueue(PostQueue* pPostQueue);
	void				CheckDiskSpace();
	void				ApplySchedulerState();
	void				CheckScheduledResume();
	void				UpdatePauseState(bool bNeedPause, const char* szReason);
	bool				PauseDownload();
	bool				UnpauseDownload();
	void				NZBAdded(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	void				NZBDownloaded(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	void				NZBDeleted(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	void				NZBCompleted(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo, bool bSaveQueue);
	void				DeleteQueuedFile(const char* szQueuedFile);
	NZBInfo*			MergeGroups(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	bool				PostQueueMove(IDList* pIDList, EEditAction eAction, int iOffset);
	bool				PostQueueDelete(IDList* pIDList);
	bool				HistoryDelete(IDList* pIDList);
	bool				HistoryReturn(IDList* pIDList, bool bReprocess);
	void				Cleanup();
	FileInfo*			GetQueueGroup(DownloadQueue* pDownloadQueue, NZBInfo* pNZBInfo);
	void				CheckHistory();
	void				DeletePostThread(PostInfo* pPostInfo);

public:
						PrePostProcessor();
	virtual				~PrePostProcessor();
	virtual void		Run();
	virtual void		Stop();
	void				QueueCoordinatorUpdate(Subject* Caller, void* Aspect);
	bool				HasMoreJobs() { return m_bHasMoreJobs; }
	void				ScanNZBDir(bool bSyncMode);
	bool				QueueEditList(IDList* pIDList, EEditAction eAction, int iOffset);
};

#endif
