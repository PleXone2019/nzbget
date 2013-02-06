/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2013 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include <fstream>
#ifndef WIN32
#include <unistd.h>
#endif

#include "nzbget.h"
#include "Unpack.h"
#include "Log.h"
#include "Util.h"
#include "ParCoordinator.h"

extern Options* g_pOptions;
extern DownloadQueueHolder* g_pDownloadQueueHolder;

UnpackController::~UnpackController()
{
	for (FileList::iterator it = m_archiveFiles.begin(); it != m_archiveFiles.end(); it++)
	{
		free(*it);
	}
}

void UnpackController::StartUnpackJob(PostInfo* pPostInfo)
{
	UnpackController* pUnpackController = new UnpackController();
	pUnpackController->m_pPostInfo = pPostInfo;
	pUnpackController->SetAutoDestroy(false);

	pPostInfo->SetScriptThread(pUnpackController);

	pUnpackController->Start();
}

void UnpackController::Run()
{
	// the locking is needed for accessing the members of NZBInfo
	g_pDownloadQueueHolder->LockQueue();

	strncpy(m_szDestDir, m_pPostInfo->GetNZBInfo()->GetDestDir(), 1024);
	m_szDestDir[1024-1] = '\0';

	char szName[1024];
	strncpy(szName, m_pPostInfo->GetNZBInfo()->GetName(), 1024);
	szName[1024-1] = '\0';

	bool bUnpack = true;
	m_szPassword[0] = '\0';
	
	for (NZBParameterList::iterator it = m_pPostInfo->GetNZBInfo()->GetParameters()->begin(); it != m_pPostInfo->GetNZBInfo()->GetParameters()->end(); it++)
	{
		NZBParameter* pParameter = *it;
		if (!strcasecmp(pParameter->GetName(), "*Unpack:") && !strcasecmp(pParameter->GetValue(), "no"))
		{
			bUnpack = false;
		}
		if (!strcasecmp(pParameter->GetName(), "*Unpack:Password"))
		{
			strncpy(m_szPassword, pParameter->GetValue(), 1024-1);
			m_szPassword[1024-1] = '\0';
		}
	}
	
	g_pDownloadQueueHolder->UnlockQueue();

	snprintf(m_szInfoName, 1024, "unpack for %s", szName);
	m_szInfoName[1024-1] = '\0';

	snprintf(m_szInfoNameUp, 1024, "Unpack for %s", szName); // first letter in upper case
	m_szInfoNameUp[1024-1] = '\0';
	
#ifndef DISABLE_PARCHECK
	if (bUnpack && HasBrokenFiles() && !m_pPostInfo->GetParCheck() && HasParFiles())
	{
		info("%s has broken files", szName);
		RequestParCheck();
		m_pPostInfo->SetWorking(false);
		return;
	}
#endif

	if (bUnpack)
	{
		CheckArchiveFiles();
	}

	if (bUnpack && (m_bHasRarFiles || m_bHasSevenZipFiles || m_bHasSevenZipMultiFiles))
	{
		SetInfoName(m_szInfoName);
		SetDefaultLogKind(g_pOptions->GetProcessLogKind());
		SetWorkingDir(m_szDestDir);

		PrintMessage(Message::mkInfo, "Unpacking %s", szName);

		CreateUnpackDir();

		m_bUnpackOK = true;
		m_bUnpackStartError = false;

		if (m_bHasRarFiles)
		{
			m_pPostInfo->SetProgressLabel("");
			ExecuteUnrar();
		}

		if (m_bHasSevenZipFiles && m_bUnpackOK)
		{
			m_pPostInfo->SetProgressLabel("");
			ExecuteSevenZip(false);
		}

		if (m_bHasSevenZipMultiFiles && m_bUnpackOK)
		{
			m_pPostInfo->SetProgressLabel("");
			ExecuteSevenZip(true);
		}

		Completed();
	}
	else
	{
		PrintMessage(Message::mkInfo, (bUnpack ? "Nothing to unpack for %s" : "Unpack for %s skipped"), szName);

#ifndef DISABLE_PARCHECK
		if (bUnpack && !m_pPostInfo->GetParCheck() && HasParFiles())
		{
			RequestParCheck();
		}
		else
#endif
		{
			m_pPostInfo->SetUnpackStatus(PostInfo::usSkipped);
			m_pPostInfo->GetNZBInfo()->SetUnpackStatus(NZBInfo::usSkipped);
			m_pPostInfo->SetStage(PostInfo::ptQueued);
		}
	}

	m_pPostInfo->SetWorking(false);
}

void UnpackController::ExecuteUnrar()
{
	// Format: 
	//   unrar x -y -p- -o+ *.rar ./_unpack

	char szPasswordParam[1024];
	const char* szArgs[8];
	szArgs[0] = g_pOptions->GetUnrarCmd();
	szArgs[1] = "x";
	szArgs[2] = "-y";
	szArgs[3] = "-p-";
	if (strlen(m_szPassword) > 0)
	{
		snprintf(szPasswordParam, 1024, "-p%s", m_szPassword);
		szArgs[3] = szPasswordParam;
	}
	szArgs[4] = "-o+";
	szArgs[5] = "*.rar";
	szArgs[6] = "./_unpack";
	szArgs[7] = NULL;
	SetArgs(szArgs, false);

	SetScript(g_pOptions->GetUnrarCmd());
	SetDefaultKindPrefix("Unrar: ");

	m_bAllOKMessageReceived = false;
	m_eUnpacker = upUnrar;
	
	int iExitCode = Execute();

	m_bUnpackOK = iExitCode == 0 && m_bAllOKMessageReceived && !GetTerminated();
	m_bUnpackStartError = iExitCode == -1;

	if (!m_bUnpackOK && iExitCode > 0)
	{
		PrintMessage(Message::mkError, "Unrar error code: %i", iExitCode);
	}
}

void UnpackController::ExecuteSevenZip(bool bMultiVolumes)
{
	// Format: 
	//   7z x -y -p- -o./_unpack *.7z
	// OR
	//   7z x -y -p- -o./_unpack *.7z.001

	char szPasswordParam[1024];
	const char* szArgs[7];
	szArgs[0] = g_pOptions->GetSevenZipCmd();
	szArgs[1] = "x";
	szArgs[2] = "-y";
	szArgs[3] = "-p-";
	if (strlen(m_szPassword) > 0)
	{
		snprintf(szPasswordParam, 1024, "-p%s", m_szPassword);
		szArgs[3] = szPasswordParam;
	}
	szArgs[4] = "-o./_unpack";
	szArgs[5] = bMultiVolumes ? "*.7z.001" : "*.7z";
	szArgs[6] = NULL;
	SetArgs(szArgs, false);

	SetScript(g_pOptions->GetSevenZipCmd());
	SetDefaultKindPrefix("7-Zip: ");

	m_bAllOKMessageReceived = false;
	m_eUnpacker = upSevenZip;

	PrintMessage(Message::mkInfo, "Executing 7-Zip");
	int iExitCode = Execute();

	m_bUnpackOK = iExitCode == 0 && m_bAllOKMessageReceived && !GetTerminated();
	m_bUnpackStartError = iExitCode == -1;

	if (!m_bUnpackOK && iExitCode > 0)
	{
		PrintMessage(Message::mkError, "7-Zip error code: %i", iExitCode);
	}
}

void UnpackController::Completed()
{
	bool bCleanupSuccess = Cleanup();

	if (m_bUnpackOK && bCleanupSuccess)
	{
		PrintMessage(Message::mkInfo, "%s %s", m_szInfoNameUp, "successful");
		m_pPostInfo->SetUnpackStatus(PostInfo::usSuccess);
		m_pPostInfo->GetNZBInfo()->SetUnpackStatus(NZBInfo::usSuccess);
		m_pPostInfo->SetStage(PostInfo::ptQueued);
	}
	else
	{
#ifndef DISABLE_PARCHECK
		if (!m_bUnpackOK && !m_pPostInfo->GetParCheck() && !m_bUnpackStartError && !GetTerminated() && HasParFiles())
		{
			RequestParCheck();
		}
		else
#endif
		{
			PrintMessage(Message::mkError, "%s failed", m_szInfoNameUp);
			m_pPostInfo->SetUnpackStatus(PostInfo::usFailure);
			m_pPostInfo->GetNZBInfo()->SetUnpackStatus(NZBInfo::usFailure);
			m_pPostInfo->SetStage(PostInfo::ptQueued);
		}
	}
}

#ifndef DISABLE_PARCHECK
void UnpackController::RequestParCheck()
{
	PrintMessage(Message::mkInfo, "%s requested par-check/repair", m_szInfoNameUp);
	m_pPostInfo->SetRequestParCheck(PostInfo::rpAll);
	m_pPostInfo->SetStage(PostInfo::ptFinished);
}
#endif

bool UnpackController::HasParFiles()
{
	return ParCoordinator::FindMainPars(m_szDestDir, NULL);
}

bool UnpackController::HasBrokenFiles()
{
	char szBrokenLog[1024];
	snprintf(szBrokenLog, 1024, "%s%c%s", m_szDestDir, PATH_SEPARATOR, "_brokenlog.txt");
	szBrokenLog[1024-1] = '\0';
	return Util::FileExists(szBrokenLog);
}

void UnpackController::CreateUnpackDir()
{
	snprintf(m_szUnpackDir, 1024, "%s%c%s", m_szDestDir, PATH_SEPARATOR, "_unpack");
	m_szUnpackDir[1024-1] = '\0';
	Util::ForceDirectories(m_szUnpackDir);
}


void UnpackController::CheckArchiveFiles()
{
	m_bHasRarFiles = false;
	m_bHasSevenZipFiles = false;
	m_bHasSevenZipMultiFiles = false;

	RegEx regExRar(".*\\.rar$");
	RegEx regExSevenZip(".*\\.7z$");
	RegEx regExSevenZipMulti(".*\\.7z\\.[0-9]*$");

	DirBrowser dir(m_szDestDir);
	while (const char* filename = dir.Next())
	{
		char szFullFilename[1024];
		snprintf(szFullFilename, 1024, "%s%c%s", m_szDestDir, PATH_SEPARATOR, filename);
		szFullFilename[1024-1] = '\0';

		if (strcmp(filename, ".") && strcmp(filename, "..") && !Util::DirectoryExists(szFullFilename))
		{
			if (regExRar.Match(filename))
			{
				m_bHasRarFiles = true;
			}
			if (regExSevenZip.Match(filename))
			{
				m_bHasSevenZipFiles = true;
			}
			if (regExSevenZipMulti.Match(filename))
			{
				m_bHasSevenZipMultiFiles = true;
			}
		}
	}
}

bool UnpackController::Cleanup()
{
	// By success:
	//   - move unpacked files to destination dir;
	//   - remove _unpack-dir;
	//   - delete archive-files.
	// By failure:
	//   - remove _unpack-dir.

	bool bOK = true;

	if (m_bUnpackOK)
	{
		// moving files back
		DirBrowser dir(m_szUnpackDir);
		while (const char* filename = dir.Next())
		{
			if (strcmp(filename, ".") && strcmp(filename, ".."))
			{
				char szSrcFile[1024];
				snprintf(szSrcFile, 1024, "%s%c%s", m_szUnpackDir, PATH_SEPARATOR, filename);
				szSrcFile[1024-1] = '\0';

				char szDstFile[1024];
				snprintf(szDstFile, 1024, "%s%c%s", m_szDestDir, PATH_SEPARATOR, filename);
				szDstFile[1024-1] = '\0';

				// silently overwrite existing files
				remove(szDstFile);

				if (!Util::MoveFile(szSrcFile, szDstFile))
				{
					PrintMessage(Message::mkError, "Could not move file %s to %s", szSrcFile, szDstFile);
					bOK = false;
				}
			}
		}
	}

	if (bOK && !Util::DeleteDirectoryWithContent(m_szUnpackDir))
	{
		PrintMessage(Message::mkError, "Could not remove temporary directory %s", m_szUnpackDir);
	}

	if (m_bUnpackOK && bOK && g_pOptions->GetUnpackCleanupDisk())
	{
		PrintMessage(Message::mkInfo, "Deleting archive files");

		// Delete rar-files (only files which were used by unrar)
		for (FileList::iterator it = m_archiveFiles.begin(); it != m_archiveFiles.end(); it++)
		{
			char* szFilename = *it;

			char szFullFilename[1024];
			snprintf(szFullFilename, 1024, "%s%c%s", m_szDestDir, PATH_SEPARATOR, szFilename);
			szFullFilename[1024-1] = '\0';

			PrintMessage(Message::mkDetail, "Deleting file %s", szFilename);

			if (remove(szFullFilename) != 0)
			{
				PrintMessage(Message::mkError, "Could not delete file %s", szFullFilename);
			}
		}

		// Unfortunately 7-Zip doesn't print the processed archive-files to the output.
		// Therefore we don't know for sure which files were extracted.
		// We just delete all 7z-files in the directory.

		RegEx regExSevenZip(".*\\.7z$|.*\\.7z\\.[0-9]*$");

		DirBrowser dir(m_szDestDir);
		while (const char* filename = dir.Next())
		{
			char szFullFilename[1024];
			snprintf(szFullFilename, 1024, "%s%c%s", m_szDestDir, PATH_SEPARATOR, filename);
			szFullFilename[1024-1] = '\0';

			if (strcmp(filename, ".") && strcmp(filename, "..") && !Util::DirectoryExists(szFullFilename))
			{
				if (regExSevenZip.Match(filename))
				{
					PrintMessage(Message::mkDetail, "Deleting file %s", filename);

					if (remove(szFullFilename) != 0)
					{
						PrintMessage(Message::mkError, "Could not delete file %s", szFullFilename);
					}
				}
			}
		}
	}

	return bOK;
}

/**
 * Unrar prints progress information into the same line using backspace control character.
 * In order to print progress continuously we analyze the output after every char
 * and update post-job progress information.
 */
bool UnpackController::ReadLine(char* szBuf, int iBufSize, FILE* pStream)
{
	bool bPrinted = false;

	int i = 0;

	for (; i < iBufSize - 1; i++)
	{
		int ch = fgetc(pStream);
		szBuf[i] = ch;
		szBuf[i+1] = '\0';
		if (ch == EOF)
		{
			break;
		}
		if (ch == '\n')
		{
			i++;
			break;
		}

		char* szBackspace = strrchr(szBuf, '\b');
		if (szBackspace)
		{
			if (!bPrinted)
			{
				char tmp[1024];
				strncpy(tmp, szBuf, 1024);
				tmp[1024-1] = '\0';
				char* szTmpPercent = strrchr(tmp, '\b');
				if (szTmpPercent)
				{
					*szTmpPercent = '\0';
				}
				if (strncmp(szBuf, "...", 3))
				{
					ProcessOutput(tmp);
				}
				bPrinted = true;
			}
			if (strchr(szBackspace, '%'))
			{
				int iPercent = atoi(szBackspace + 1);
				m_pPostInfo->SetStageProgress(iPercent * 10);
			}
		}
	}

	szBuf[i] = '\0';

	if (bPrinted)
	{
		szBuf[0] = '\0';
	}

	return i > 0;
}

void UnpackController::AddMessage(Message::EKind eKind, bool bDefaultKind, const char* szText)
{
	ScriptController::AddMessage(eKind, bDefaultKind, szText);
	m_pPostInfo->AppendMessage(eKind, szText);

	if (m_eUnpacker == upUnrar && !strncmp(szText, "Extracting from ", 16))
	{
		const char *szFilename = szText + 16;
		debug("Filename: %s", szFilename);
		m_archiveFiles.push_back(strdup(szFilename));
		m_pPostInfo->SetProgressLabel(szText);
	}

	if (m_eUnpacker == upUnrar && !strncmp(szText, "Extracting ", 11))
	{
		m_pPostInfo->SetProgressLabel(szText);
	}

	if ((m_eUnpacker == upUnrar && !strncmp(szText, "All OK", 6)) ||
		(m_eUnpacker == upSevenZip && !strncmp(szText, "Everything is Ok", 16)))
	{
		m_bAllOKMessageReceived = true;
	}
}

void UnpackController::Stop()
{
	debug("Stopping unpack");
	Thread::Stop();
	Terminate();
}
