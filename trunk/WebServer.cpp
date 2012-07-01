/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2012 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 * $Revision: 1 $
 * $Date: 2012-05-11 12:00:00 +0200 (Fr, 11 Mai 2012) $
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
#include <cstdio>
#ifndef WIN32
#include <unistd.h>
#endif

#include "nzbget.h"
#include "WebServer.h"
#include "XmlRpc.h"
#include "Log.h"
#include "Options.h"
#include "Util.h"

extern Options* g_pOptions;

static const char* ERR_HTTP_BAD_REQUEST = "400 Bad Request";
static const char* ERR_HTTP_NOT_FOUND = "404 Not Found";
static const char* ERR_HTTP_SERVICE_UNAVAILABLE = "503 Service Unavailable";

static const int MAX_UNCOMPRESSED_SIZE = 500;

//*****************************************************************
// WebProcessor

WebProcessor::WebProcessor()
{
	m_pConnection = NULL;
	m_szClientIP = NULL;
	m_szRequest = NULL;
	m_szUrl = NULL;
}

void WebProcessor::SetUrl(const char* szUrl)
{
	m_szUrl = strdup(szUrl);
}

WebProcessor::~WebProcessor()
{
	if (m_szRequest)
	{
		free(m_szRequest);
	}
	if (m_szUrl)
	{
		free(m_szUrl);
	}
}


void WebProcessor::Execute()
{
	m_bGZip =false;
	char szAuthInfo[1024];
	szAuthInfo[0] = '\0';

	// reading http header
	char szBuffer[1024];
	bool bBody = false;
	int iContentLen = 0;
	while (char* p = m_pConnection->ReadLine(szBuffer, sizeof(szBuffer), NULL))
	{
		if (char* pe = strrchr(p, '\r')) *pe = '\0';
		debug("header=%s", p);
		if (!strncasecmp(p, "Content-Length: ", 16))
		{
			iContentLen = atoi(p + 16);
		}
		if (!strncasecmp(p, "Authorization: Basic ", 21))
		{
			char* szAuthInfo64 = p + 21;
			if (strlen(szAuthInfo64) > sizeof(szAuthInfo))
			{
				error("invalid-request: auth-info too big");
				return;
			}
			szAuthInfo[WebUtil::DecodeBase64(szAuthInfo64, 0, szAuthInfo)] = '\0';
		}
		if (!strncasecmp(p, "Accept-Encoding: ", 17))
		{
			m_bGZip = strstr(p, "gzip");
		}
		if (*p == '\0')
		{
			bBody = true;
			break;
		}
	}

	debug("URL=%s", m_szUrl);
	debug("Authorization=%s", szAuthInfo);

	if (m_eHttpMethod == hmPost && iContentLen <= 0)
	{
		error("invalid-request: content length is 0");
		return;
	}
	
	if (strlen(szAuthInfo) == 0)
	{
		SendAuthResponse();
		return;
	}

	// Authorization
	char* pw = strchr(szAuthInfo, ':');
	if (pw) *pw++ = '\0';
	if (strcmp(szAuthInfo, "nzbget") || strcmp(pw, g_pOptions->GetServerPassword()))
	{
		warn("request received on port %i from %s, but password invalid", g_pOptions->GetServerPort(), m_szClientIP);
		SendAuthResponse();
		return;
	}

	if (m_eHttpMethod == hmPost)
	{
		// reading http body (request content)
		m_szRequest = (char*)malloc(iContentLen + 1);
		m_szRequest[iContentLen] = '\0';
		
		if (!m_pConnection->RecvAll(m_szRequest, iContentLen))
		{
			free(m_szRequest);
			error("invalid-request: could not read data");
			return;
		}
		debug("Request=%s", m_szRequest);
	}
	
	debug("request received from %s", m_szClientIP);

	Dispatch();
}

void WebProcessor::Dispatch()
{
	if (*m_szUrl != '/')
	{
		SendErrorResponse(ERR_HTTP_BAD_REQUEST);
		return;
	}

	if (XmlRpcProcessor::IsRpcRequest(m_szUrl))
	{
		XmlRpcProcessor processor;
		processor.SetRequest(m_szRequest);
		processor.SetClientIP(m_szClientIP);
		processor.SetHttpMethod(m_eHttpMethod == hmGet ? XmlRpcProcessor::hmGet : XmlRpcProcessor::hmPost);
		processor.SetUrl(m_szUrl);
		processor.Execute();
		SendBodyResponse(processor.GetResponse(), strlen(processor.GetResponse()), processor.GetContentType()); 
		return;
	}

	if (!g_pOptions->GetWebDir() || strlen(g_pOptions->GetWebDir()) == 0)
	{
		SendErrorResponse(ERR_HTTP_SERVICE_UNAVAILABLE);
		return;
	}
	
	if (m_eHttpMethod != hmGet)
	{
		SendErrorResponse(ERR_HTTP_BAD_REQUEST);
		return;
	}
	
	// for security reasons we allow only characters "0..9 A..Z a..z . - _ /" in the URLs
	// we also don't allow ".." in the URLs
	for (char *p = m_szUrl; *p; p++)
	{
		if (!((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
			*p == '.' || *p == '-' || *p == '_' || *p == '/') || (*p == '.' && p[1] == '.'))
		{
			SendErrorResponse(ERR_HTTP_NOT_FOUND);
			return;
		}
	}

	char disk_filename[1024];
	snprintf(disk_filename, sizeof(disk_filename), "%s%s", g_pOptions->GetWebDir(), m_szUrl + 1);
	disk_filename[sizeof(disk_filename)-1] = '\0';

	SendFileResponse(disk_filename);
}

void WebProcessor::SendAuthResponse()
{
	const char* AUTH_RESPONSE_HEADER =
		"HTTP/1.0 401 Unauthorized\r\n"
		"WWW-Authenticate: Basic realm=\"NZBGet\"\r\n"
		"Connection: close\r\n"
		"Content-Length: %i\r\n"
		"Content-Type: text/plain\r\n"
		"Server: nzbget-%s\r\n"
		"\r\n";
	char szResponseHeader[1024];
	snprintf(szResponseHeader, 1024, AUTH_RESPONSE_HEADER, sizeof(AUTH_RESPONSE_HEADER), Util::VersionRevision());
	 
	// Send the response answer
	debug("ResponseHeader=%s", szResponseHeader);
	m_pConnection->Send(szResponseHeader, strlen(szResponseHeader));
}

void WebProcessor::SendErrorResponse(const char* szErrCode)
{
	const char* RESPONSE_HEADER = 
		"HTTP/1.0 %s\r\n"
		"Connection: close\r\n"
		"Content-Length: %i\r\n"
		"Content-Type: text/html\r\n"
		"Server: nzbget-%s\r\n"
		"\r\n";

	warn("Web-Server: %s, Resource: %s", szErrCode, m_szUrl);

	char szResponseBody[1024];
	snprintf(szResponseBody, 1024, "<html><head><title>%s</title></head><body>Error: %s</body></html>", szErrCode, szErrCode);
	int iPageContentLen = strlen(szResponseBody);

	char szResponseHeader[1024];
	snprintf(szResponseHeader, 1024, RESPONSE_HEADER, szErrCode, iPageContentLen, Util::VersionRevision());

	// Send the response answer
	m_pConnection->Send(szResponseHeader, strlen(szResponseHeader));
	m_pConnection->Send(szResponseBody, iPageContentLen);
}

void WebProcessor::SendBodyResponse(const char* szBody, int iBodyLen, const char* szContentType)
{
	const char* RESPONSE_HEADER = 
	"HTTP/1.0 200 OK\r\n"
	"Connection: close\r\n"
	"Content-Length: %i\r\n"
	"%s"					// Content-Type: xxx
	"Server: nzbget-%s\r\n"
	"\r\n";
	
	char szContentTypeHeader[1024];
	if (szContentType)
	{
		snprintf(szContentTypeHeader, 1024, "Content-Type: %s\r\n", szContentType);
	}
	else
	{
		szContentTypeHeader[0] = '\0';
	}
	
	char szResponseHeader[1024];
	snprintf(szResponseHeader, 1024, RESPONSE_HEADER, iBodyLen, szContentTypeHeader,
			 Util::VersionRevision());
	
	// Send the request answer
	m_pConnection->Send(szResponseHeader, strlen(szResponseHeader));
	m_pConnection->Send(szBody, iBodyLen);
}

void WebProcessor::SendFileResponse(const char* szFilename)
{
	debug("serving file: %s", szFilename);

	char *szBody;
	int iBodyLen;
	if (!Util::LoadFileIntoBuffer(szFilename, &szBody, &iBodyLen))
	{
		SendErrorResponse(ERR_HTTP_NOT_FOUND);
		return;
	}
	
	// "LoadFileIntoBuffer" adds a trailing NULL, which we don't need here
	iBodyLen--;
	
	SendBodyResponse(szBody, iBodyLen, DetectContentType(szFilename));

	free(szBody);
}

const char* WebProcessor::DetectContentType(const char* szFilename)
{
	if (const char *szExt = strrchr(szFilename, '.'))
	{
		if (!strcasecmp(szExt, ".css"))
		{
			return "text/css";
		}
		else if (!strcasecmp(szExt, ".html"))
		{
			return "text/html";
		}
		else if (!strcasecmp(szExt, ".js"))
		{
			return "application/javascript";
		}
		else if (!strcasecmp(szExt, ".png"))
		{
			return "image/png";
		}
		else if (!strcasecmp(szExt, ".jpeg"))
		{
			return "image/jpeg";
		}
		else if (!strcasecmp(szExt, ".gif"))
		{
			return "image/gif";
		}
	}
	return NULL;
}
