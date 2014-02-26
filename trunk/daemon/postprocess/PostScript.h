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


#ifndef POSTSCRIPT_H
#define POSTSCRIPT_H

#include "Thread.h"
#include "Log.h"
#include "QueueScript.h"
#include "DownloadInfo.h"

class PostScriptController : public Thread, public QueueScriptController
{
private:
	PostInfo*			m_pPostInfo;
	char				m_szNZBName[1024];
 	int					m_iPrefixLen;

	void				ExecuteScript(const char* szScriptName, const char* szDisplayName, const char* szLocation);
	void				PrepareParams(const char* szScriptName);
	ScriptStatus::EStatus	AnalyseExitCode(int iExitCode);

	typedef std::deque<char*>		FileList;

protected:
	virtual void		AddMessage(Message::EKind eKind, const char* szText);

public:
	virtual void		Run();
	virtual void		Stop();
	static void			StartJob(PostInfo* pPostInfo);
};

#endif
