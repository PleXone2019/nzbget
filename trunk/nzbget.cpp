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
#include <winsvc.h>
#else
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>

#include "nzbget.h"
#include "ServerPool.h"
#include "Log.h"
#include "NZBFile.h"
#include "Options.h"
#include "Thread.h"
#include "ColoredFrontend.h"
#include "NCursesFrontend.h"
#include "QueueCoordinator.h"
#include "RemoteServer.h"
#include "RemoteClient.h"
#include "MessageBase.h"
#include "PrePostProcessor.h"
#include "ParChecker.h"
#ifdef WIN32
#include "NTService.h"
#endif

// Prototypes
void Run();
void Cleanup();
void ProcessClientRequest();
#ifndef WIN32
void InstallSignalHandlers();
void Daemonize();
#endif
#ifdef DEBUG
void DoTest();
#endif

Thread* g_pFrontend	= NULL;
Options* g_pOptions		= NULL;
ServerPool* g_pServerPool = NULL;
QueueCoordinator* g_pQueueCoordinator = NULL;
RemoteServer* g_pRemoteServer = NULL;
DownloadSpeedMeter* g_pDownloadSpeedMeter = NULL;
Log* g_pLog = NULL;
PrePostProcessor* g_pPrePostProcessor;

/*
 * Main loop
 */
int main(int argc, char *argv[])
{
#ifdef WIN32
	_set_fmode(_O_BINARY);
	InstallUninstallServiceCheck(argc, argv);
#endif

	// Init options & get the name of the .nzb file
	g_pLog = new Log();
	g_pServerPool = new ServerPool();
	debug("Options parsing");
	g_pOptions = new Options(argc, argv);

#ifndef WIN32
	if (g_pOptions->GetUMask() < 01000)
	{
		/* set newly created file permissions */
		umask(g_pOptions->GetUMask());
	}
#endif
	
	if (g_pOptions->GetServerMode() && g_pOptions->GetCreateLog() && g_pOptions->GetResetLog())
	{
		debug("deleting old log-file");
		g_pLog->ResetLog();
	}

	if (g_pOptions->GetDaemonMode())
	{
#ifdef WIN32
		info("nzbget service-mode");
		StartService(Run);
		return 0;
#else
		Daemonize();
		info("nzbget daemon-mode");
#endif
	}
	else if (g_pOptions->GetServerMode())
	{
		info("nzbget server-mode");
	}
	else if (g_pOptions->GetRemoteClientMode())
	{
		info("nzbget remote-mode");
	}

	Run();
	return 0;
}

void Run()
{
#ifndef WIN32
	InstallSignalHandlers();
#endif

	Thread::Init();
	Connection::Init();

	// client request
	if (g_pOptions->GetClientOperation() != Options::opClientNoOperation)
	{
		ProcessClientRequest();
		Cleanup();
		return;
	}

	// Create the queue coordinator
	if (!g_pOptions->GetRemoteClientMode() && !g_pOptions->GetTest())
	{                                    
		g_pQueueCoordinator = new QueueCoordinator();
		g_pDownloadSpeedMeter = g_pQueueCoordinator;
	}

	// Setup the network-server
	if (g_pOptions->GetServerMode())
	{
		g_pRemoteServer = new RemoteServer();
		g_pRemoteServer->Start();
	}

	// Create the front-end
	if (!g_pOptions->GetDaemonMode())
	{
		switch (g_pOptions->GetOutputMode())
		{
			case Options::omNCurses:
#ifndef DISABLE_CURSES
				g_pFrontend = new NCursesFrontend();
				break;
#endif
			case Options::omColored:
				g_pFrontend = new ColoredFrontend();
				break;
			case Options::omLoggable:
				g_pFrontend = new LoggableFrontend();
				break;
		}
	}

	// Starting a thread with the frontend
	if (g_pFrontend)
	{
		g_pFrontend->Start();
	}

	// Start QueueCoordinator
	if (!g_pOptions->GetRemoteClientMode() && !g_pOptions->GetTest())
	{
		g_pPrePostProcessor = new PrePostProcessor();

		g_pPrePostProcessor->Start();

		// Standalone-mode
		if (!g_pOptions->GetServerMode() && !g_pQueueCoordinator->AddFileToQueue(g_pOptions->GetArgFilename()))
		{
			abort("FATAL ERROR: Parsing NZB-document %s failed!!\n\n", g_pOptions->GetArgFilename() ? g_pOptions->GetArgFilename() : "N/A");
			return;
		}

		g_pQueueCoordinator->Start();

		// enter main program-loop
		while (g_pQueueCoordinator->IsRunning() || g_pPrePostProcessor->IsRunning())
		{
			if (!g_pOptions->GetServerMode() && !g_pQueueCoordinator->HasMoreJobs() && !g_pPrePostProcessor->HasMoreJobs())
			{
				// Standalone-mode: download completed
				if (!g_pQueueCoordinator->IsStopped())
				{
					g_pQueueCoordinator->Stop();
				}
				if (!g_pPrePostProcessor->IsStopped())
				{
					g_pPrePostProcessor->Stop();
				}
			}
			usleep(100 * 1000);
		}

		// main program-loop is terminated
		debug("QueueCoordinator stopped");
		debug("PrePostProcessor stopped");
	}

	// Stop network-server
	if (g_pRemoteServer)
	{
		debug("stopping RemoteServer");
		g_pRemoteServer->Stop();
		int iMaxWaitMSec = 1000;
		while (g_pRemoteServer->IsRunning() && iMaxWaitMSec > 0)
		{
			usleep(100 * 1000);
			iMaxWaitMSec -= 100;
		}
		if (g_pRemoteServer->IsRunning())
		{
			debug("Killing RemoteServer");
			g_pRemoteServer->Kill();
		}
		debug("RemoteServer stopped");
	}
	
	// Stop Frontend
	if (g_pFrontend)
	{
		if (!g_pOptions->GetRemoteClientMode())
		{
			debug("Stopping Frontend");
			g_pFrontend->Stop();
		}
		while (g_pFrontend->IsRunning())
		{
			usleep(50 * 1000);
		}
		debug("Frontend stopped");
	}

	Cleanup();
}

void ProcessClientRequest()
{
	RemoteClient* Client = new RemoteClient();

	if (g_pOptions->GetClientOperation() == Options::opClientRequestList)
	{
		Client->RequestServerList();
	}
	else if (g_pOptions->GetClientOperation() == Options::opClientRequestPause)
	{
		Client->RequestServerPauseUnpause(true);
	}
	else if (g_pOptions->GetClientOperation() == Options::opClientRequestUnpause)
	{
		Client->RequestServerPauseUnpause(false);
	}
	else if (g_pOptions->GetClientOperation() == Options::opClientRequestSetRate)
	{
		Client->RequestServerSetDownloadRate(g_pOptions->GetSetRate());
	}
	else if (g_pOptions->GetClientOperation() == Options::opClientRequestDumpDebug)
	{
		Client->RequestServerDumpDebug();
	}
	else if (g_pOptions->GetClientOperation() == Options::opClientRequestEditQueue)
	{
		Client->RequestServerEditQueue(g_pOptions->GetEditQueueAction(), g_pOptions->GetEditQueueOffset(),
			g_pOptions->GetEditQueueIDList(), g_pOptions->GetEditQueueIDCount(), true);
	}
	else if (g_pOptions->GetClientOperation() == Options::opClientRequestLog)
	{
		Client->RequestServerLog(g_pOptions->GetLogLines());
	}
	else if (g_pOptions->GetClientOperation() == Options::opClientRequestShutdown)
	{
		Client->RequestServerShutdown();
	}
	else if (g_pOptions->GetClientOperation() == Options::opClientRequestDownload)
	{
		Client->RequestServerDownload(g_pOptions->GetArgFilename(), g_pOptions->GetAddTop());
	}

	delete Client;
}

void ExitProc()
{
	info("Stopping, please wait...");
	if (g_pOptions->GetRemoteClientMode())
	{
		if (g_pFrontend)
		{
			debug("Stopping Frontend");
			g_pFrontend->Stop();
		}
	}
	else
	{
		if (g_pQueueCoordinator)
		{
			debug("Stopping QueueCoordinator");
			g_pQueueCoordinator->Stop();
			g_pPrePostProcessor->Stop();
		}
	}
}

#ifndef WIN32
#ifdef DEBUG
typedef void(*sighandler)(int);
std::vector<sighandler> SignalProcList;
#endif

/*
 * Signal handler
 */
void SignalProc(int iSignal)
{
	switch (iSignal)
	{
		case SIGINT:
			signal(SIGINT, SIG_DFL);   // Reset the signal handler
			debug("SIGINT received");
			ExitProc();
			break;

		case SIGTERM:
			signal(SIGTERM, SIG_DFL);   // Reset the signal handler
			debug("SIGTERM received");
			ExitProc();
			break;

#ifdef DEBUG
		case SIGPIPE:
			debug("SIGPIPE received, ignoring");
			break;
			
		case SIGSEGV:
			signal(SIGSEGV, SIG_DFL);   // Reset the signal handler
			debug("SIGSEGV received");
			break;
		
		default:
			debug("Signal %i received", iSignal);
			if (SignalProcList[iSignal - 1])
			{
				SignalProcList[iSignal - 1](iSignal);
			}
			break;
#endif
	}
}

void InstallSignalHandlers()
{
	signal(SIGINT, SignalProc);
	signal(SIGTERM, SignalProc);
	signal(SIGPIPE, SIG_IGN);
#ifdef DEBUG
	SignalProcList.clear();
	for (int i = 1; i <= 32; i++)
	{
		SignalProcList.push_back((sighandler)signal(i, SignalProc));
	}
	signal(SIGWINCH, SIG_DFL);
#endif
}
#endif

void Cleanup()
{
	debug("Cleaning up global objects");

	debug("Deleting QueueCoordinator");
	if (g_pQueueCoordinator)
	{
		delete g_pQueueCoordinator;
		g_pQueueCoordinator = NULL;
	}
	debug("QueueCoordinator deleted");

	debug("Deleting RemoteServer");
	if (g_pRemoteServer)
	{
		delete g_pRemoteServer;
		g_pRemoteServer = NULL;
	}
	debug("RemoteServer deleted");

	debug("Deleting PrePostProcessor");
	if (g_pPrePostProcessor)
	{
		delete g_pPrePostProcessor;
		g_pPrePostProcessor = NULL;
	}
	debug("PrePostProcessor deleted");

	debug("Deleting Frontend");
	if (g_pFrontend)
	{
		delete g_pFrontend;
		g_pFrontend = NULL;
	}
	debug("Frontend deleted");

	debug("Deleting Options");
	if (g_pOptions)
	{
		if (g_pOptions->GetDaemonMode())
		{
			remove(g_pOptions->GetLockFile());
		}
		delete g_pOptions;
		g_pOptions = NULL;
	}
	debug("Options deleted");

	debug("Deleting ServerPool");
	if (g_pServerPool)
	{
		delete g_pServerPool;
		g_pServerPool = NULL;
	}
	debug("ServerPool deleted");

	Thread::Final();
	Connection::Final();

	debug("Global objects cleaned up");

	if (g_pLog)
	{
		delete g_pLog;
		g_pLog = NULL;
	}
}

#ifndef WIN32
void Daemonize()
{
	int i, lfp;
	char str[10];
	if (getppid() == 1) return; /* already a daemon */
	i = fork();
	if (i < 0) exit(1); /* fork error */
	if (i > 0) exit(0); /* parent exits */
	/* child (daemon) continues */
	setsid(); /* obtain a new process group */
	for (i = getdtablesize();i >= 0;--i) close(i); /* close all descriptors */
	i = open("/dev/null", O_RDWR); dup(i); dup(i); /* handle standart I/O */
	chdir(g_pOptions->GetDestDir()); /* change running directory */
	lfp = open(g_pOptions->GetLockFile(), O_RDWR | O_CREAT, 0640);
	if (lfp < 0) exit(1); /* can not open */
	if (lockf(lfp, F_TLOCK, 0) < 0) exit(0); /* can not lock */

	/* Drop user if there is one, and we were run as root */
	if ( getuid() == 0 || geteuid() == 0 )
	{
		struct passwd *pw = getpwnam(g_pOptions->GetDaemonUserName());
		if (pw)
		{
			setgroups( 0, (const gid_t*) 0 ); /* Set aux groups to null. */
			setgid(pw->pw_gid); /* Set primary group. */
			/* Try setting aux groups correctly - not critical if this fails. */
			initgroups( g_pOptions->GetDaemonUserName(),pw->pw_gid); 
			/* Finally, set uid. */
			setuid(pw->pw_uid);
		}
	}

	/* first instance continues */
	sprintf(str, "%d\n", getpid());
	write(lfp, str, strlen(str)); /* record pid to lockfile */
	signal(SIGCHLD, SIG_IGN); /* ignore child */
	signal(SIGTSTP, SIG_IGN); /* ignore tty signals */
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
}
#endif
