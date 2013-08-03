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
#include <stdio.h>
#include <ctype.h>

#include "nzbget.h"
#include "Log.h"
#include "DownloadInfo.h"
#include "Util.h"
#include "FeedFilter.h"


FeedFilter::Term::Term()
{
	m_szField = NULL;
	m_szParam = NULL;
	m_iIntParam = 0;
	m_pRegEx = NULL;
}

FeedFilter::Term::~Term()
{
	if (m_szField)
	{
		free(m_szField);
	}
	if (m_szParam)
	{
		free(m_szParam);
	}
	if (m_pRegEx)
	{
		delete m_pRegEx;
	}
}

bool FeedFilter::Term::Match(FeedItemInfo* pFeedItemInfo)
{
	const char* szStrValue = NULL;
	long long iIntValue = 0;
	if (!GetFieldValue(m_szField, pFeedItemInfo, &szStrValue, &iIntValue))
	{
		return false;
	}

	bool bMatch = MatchValue(szStrValue, iIntValue);

	if (m_bPositive != bMatch)
	{
		return false;
	}

	return true;
}

bool FeedFilter::Term::MatchValue(const char* szStrValue, const long long iIntValue)
{
	switch (m_eCommand)
	{
		case fcText:
			return MatchText(szStrValue);

		case fcRegex:
			return MatchRegex(szStrValue);

		case fcLess:
			return iIntValue < m_iIntParam;

		case fcLessEqual:
			return iIntValue <= m_iIntParam;

		case fcGreater:
			return iIntValue > m_iIntParam;

		case fcGreaterEqual:
			return iIntValue >= m_iIntParam;
	}

	return false;
}

bool FeedFilter::Term::MatchText(const char* szStrValue)
{
	const char* WORD_SEPARATORS = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

	// first check if we should make word-search or substring-search
	int iParamLen = strlen(m_szParam);
	bool bSubstr = iParamLen >= 2 && m_szParam[0] == '*' && m_szParam[iParamLen-1] == '*';
	if (!bSubstr)
	{
		for (const char* p = m_szParam; *p; p++)
		{
			char ch = *p;
			if (strchr(WORD_SEPARATORS, ch) && ch != '*' && ch != '?')
			{
				bSubstr = true;
				break;
			}
		}
	}

	bool bMatch = false;

	if (!bSubstr)
	{
		// Word-search

		// split szStrValue into tokens and create pp-parameter for each token
		char* szStrValue2 = strdup(szStrValue);
		char* saveptr;
		char* szWord = strtok_r(szStrValue2, WORD_SEPARATORS, &saveptr);
		while (szWord)
		{
			szWord = Util::Trim(szWord);
			bMatch = *szWord && Util::MatchMask(szWord, m_szParam, false);
			if (bMatch)
			{
				break;
			}
			szWord = strtok_r(NULL, WORD_SEPARATORS, &saveptr);
		}
		free(szStrValue2);
	}
	else
	{
		// Substring-search

		const char* szFormat = "*%s*";
		if (iParamLen >= 2 && m_szParam[0] == '*' && m_szParam[iParamLen-1] == '*')
		{
			szFormat = "%s";
		}
		else if (iParamLen >= 1 && m_szParam[0] == '*')
		{
			szFormat = "%s*";
		}
		else if (iParamLen >= 1 && m_szParam[iParamLen-1] == '*')
		{
			szFormat = "*%s";
		}

		int iMaskLen = strlen(m_szParam) + 2 + 1;
		char* szMask = (char*)malloc(iMaskLen);
		snprintf(szMask, iMaskLen, szFormat, m_szParam);
		szMask[iMaskLen-1] = '\0';

		bMatch = Util::MatchMask(szStrValue, szMask, false);

		free(szMask);
	}

	return bMatch;
}

bool FeedFilter::Term::MatchRegex(const char* szStrValue)
{
	if (!m_pRegEx)
	{
		m_pRegEx = new RegEx(m_szParam);
	}

	bool bFound = m_pRegEx->Match(szStrValue);
	return bFound;
}

bool FeedFilter::Term::Compile(char* szToken)
{
	debug("Token: %s", szToken);

	char ch = szToken[0];

	m_bPositive = ch != '-';
	if (ch == '-' || ch == '+')
	{
		szToken++;
		ch = szToken[0];
	}

	char *szField = NULL;
	m_eCommand = fcText;

	char* szColon = NULL;
	if (ch != '@' && ch != '$' && ch != '<' && ch != '>' && ch != '=')
	{
		szColon = strchr(szToken, ':');
	}
	if (szColon)
	{
		szField = szToken;
		szColon[0] = '\0';
		szToken = szColon + 1;
		ch = szToken[0];
	}

	if (ch == '\0')
	{
		return false;
	}

	char ch2= szToken[1];

	if (ch == '@')
	{
		m_eCommand = fcText;
		szToken++;
	}
	else if (ch == '$')
	{
		m_eCommand = fcRegex;
		szToken++;
	}
	else if (ch == '<')
	{
		m_eCommand = fcLess;
		szToken++;
	}
	else if (ch == '<' && ch2 == '=')
	{
		m_eCommand = fcLessEqual;
		szToken += 2;
	}
	else if (ch == '>')
	{
		m_eCommand = fcGreater;
		szToken++;
	}
	else if (ch == '>' && ch2 == '=')
	{
		m_eCommand = fcGreaterEqual;
		szToken += 2;
	}

	debug("%s, Field: %s, Command: %i, Param: %s", (m_bPositive ? "Positive" : "Negative"), szField, m_eCommand, szToken);

	if (!ValidateFieldName(szField))
	{
		return false;
	}

	if ((szField && !strcasecmp(szField, "size") && !ParseSizeParam(szToken, &m_iIntParam)) ||
		(szField && !strcasecmp(szField, "age") && !ParseAgeParam(szToken, &m_iIntParam)))
	{
		return false;
	}

	m_szField = szField ? strdup(szField) : NULL;
	m_szParam = strdup(szToken);

	return true;
}

bool FeedFilter::Term::ValidateFieldName(const char* szField)
{
	return !szField || !strcasecmp(szField, "title") || !strcasecmp(szField, "filename") ||
		!strcasecmp(szField, "link") || !strcasecmp(szField, "url") || !strcasecmp(szField, "category") ||
		!strcasecmp(szField, "size") || !strcasecmp(szField, "age");
}

bool FeedFilter::Term::GetFieldValue(const char* szField, FeedItemInfo* pFeedItemInfo, const char** StrValue, long long* IntValue)
{
	*StrValue = NULL;
	*IntValue = 0;

	if (!szField || !strcasecmp(szField, "title"))
	{
		*StrValue = pFeedItemInfo->GetTitle();
		return true;
	}
	else if (!strcasecmp(szField, "filename"))
	{
		*StrValue = pFeedItemInfo->GetFilename();
		return true;
	}
	else if (!strcasecmp(szField, "category"))
	{
		*StrValue = pFeedItemInfo->GetCategory();
		return true;
	}
	else if (!strcasecmp(szField, "link") || !strcasecmp(szField, "url"))
	{
		*StrValue = pFeedItemInfo->GetUrl();
		return true;
	}
	else if (!strcasecmp(szField, "size"))
	{
		*IntValue = pFeedItemInfo->GetSize();
		return true;
	}
	else if (!strcasecmp(szField, "age"))
	{
		*IntValue = time(NULL) - pFeedItemInfo->GetTime();
		return true;
	}

	return false;
}

bool FeedFilter::Term::ParseSizeParam(const char* szParam, long long* pIntValue)
{
	*pIntValue = 0;

	double fParam = atof(szParam);

	const char* p;
	for (p = szParam; *p && ((*p >= '0' && *p <='9') || *p == '.'); p++) ;
	if (*p)
	{
		if (!strcasecmp(p, "K") || !strcasecmp(p, "KB"))
		{
			*pIntValue = (long long)(fParam*1024);
		}
		else if (!strcasecmp(p, "M") || !strcasecmp(p, "MB"))
		{
			*pIntValue = (long long)(fParam*1024*1024);
		}
		else if (!strcasecmp(p, "G") || !strcasecmp(p, "GB"))
		{
			*pIntValue = (long long)(fParam*1024*1024*1024);
		}
		else
		{
			return false;
		}
	}
	else
	{
		*pIntValue = (long long)fParam;
	}

	return true;
}

bool FeedFilter::Term::ParseAgeParam(const char* szParam, long long* pIntValue)
{
	*pIntValue = atoll(szParam);

	const char* p;
	for (p = szParam; *p && ((*p >= '0' && *p <='9') || *p == '.'); p++) ;
	if (*p)
	{
		if (!strcasecmp(p, "m"))
		{
			// minutes
			*pIntValue *= 60;
		}
		else if (!strcasecmp(p, "h"))
		{
			// hours
			*pIntValue *= 60 * 60;
		}
		else if (!strcasecmp(p, "d"))
		{
			// days
			*pIntValue *= 60 * 60 * 24;
		}
		else
		{
			return false;
		}
	}
	else
	{
		// days by default
		*pIntValue *= 60 * 60 * 24;
	}

	return true;
}


FeedFilter::Rule::Rule()
{
	m_eCommand = frAccept;
	m_bIsValid = false;
	m_szCategory = NULL;
	m_iPriority = 0;
	m_bPause = false;
	m_bHasCategory = false;
	m_bHasPriority = false;
	m_bHasPause = false;
}

FeedFilter::Rule::~Rule()
{
	if (m_szCategory)
	{
		free(m_szCategory);
	}

	for (TermList::iterator it = m_Terms.begin(); it != m_Terms.end(); it++)
	{
		delete *it;
	}
}

void FeedFilter::Rule::Compile(char* szRule)
{
	debug("Compiling rule: %s", szRule);

	m_bIsValid = true;

	char* szFilter3 = Util::Trim(szRule);

	char* szTerm = CompileCommand(szFilter3);
	if (!szTerm)
	{
		m_bIsValid = false;
		return;
	}
	if (m_eCommand == frComment)
	{
		return;
	}

	szTerm = Util::Trim(szTerm);

	for (char* p = szTerm; *p && m_bIsValid; p++)
	{
		char ch = *p;
		if (ch == ' ')
		{
			*p = '\0';
			m_bIsValid = CompileTerm(szTerm);
			szTerm = p + 1;
			while (*szTerm == ' ') szTerm++;
		}
	}

	m_bIsValid = m_bIsValid && CompileTerm(szTerm);
}

/* Checks if the rule starts with command and compiles it.
 * Returns a pointer to the next (first) term or NULL in a case of compilation error.
 */
char* FeedFilter::Rule::CompileCommand(char* szRule)
{
	if (!strncasecmp(szRule, "A:", 2) || !strncasecmp(szRule, "Accept:", 7) ||
		!strncasecmp(szRule, "A(", 2) || !strncasecmp(szRule, "Accept(", 7))
	{
		m_eCommand = frAccept;
		szRule += szRule[1] == ':' || szRule[1] == '(' ? 2 : 7;
	}
	else if (!strncasecmp(szRule, "O(", 2) || !strncasecmp(szRule, "Options(", 8))
	{
		m_eCommand = frOptions;
		szRule += szRule[1] == ':' || szRule[1] == '(' ? 2 : 7;
	}
	else if (!strncasecmp(szRule, "R:", 2) || !strncasecmp(szRule, "Reject:", 7))
	{
		m_eCommand = frReject;
		szRule += szRule[1] == ':' || szRule[1] == '(' ? 2 : 7;
	}
	else if (!strncasecmp(szRule, "Q:", 2) || !strncasecmp(szRule, "Require:", 8))
	{
		m_eCommand = frRequire;
		szRule += szRule[1] == ':' || szRule[1] == '(' ? 2 : 8;
	}
	else if (*szRule == '#')
	{
		m_eCommand = frComment;
		return szRule;
	}
	else
	{
		// not a command
		return szRule;
	}

	if ((m_eCommand == frAccept || m_eCommand == frOptions) && szRule[-1] == '(')
	{
		if (char* p = strchr(szRule, ')'))
		{
			// split command into tokens
			*p = '\0';
			char* saveptr;
			char* szToken = strtok_r(szRule, ",", &saveptr);
			while (szToken)
			{
				szToken = Util::Trim(szToken);
				if (*szToken)
				{
					if (!strcasecmp(szToken, "paused") || !strcasecmp(szToken, "unpaused"))
					{
						m_bHasPause = true;
						m_bPause = !strcasecmp(szToken, "paused");
					}
					else if (strchr("0123456789-+", *szToken))
					{
						m_bHasPriority = true;
						m_iPriority = atoi(szToken);
					}
					else
					{
						m_bHasCategory = true;
						m_szCategory = strdup(szToken);
					}
				}
				szToken = strtok_r(NULL, ",", &saveptr);
			}

			szRule = p + 1;
			if (*szRule == ':')
			{
				szRule++;
			}
		}
		else
		{
			// error
			return NULL;
		}
	}

	return szRule;
}

bool FeedFilter::Rule::CompileTerm(char* szTerm)
{
	Term* pTerm = new Term();
	if (pTerm->Compile(szTerm))
	{
		m_Terms.push_back(pTerm);
		return true;
	}
	else
	{
		delete pTerm;
		return false;
	}
}

bool FeedFilter::Rule::Match(FeedItemInfo* pFeedItemInfo)
{
	for (TermList::iterator it = m_Terms.begin(); it != m_Terms.end(); it++)
	{
		Term* pTerm = *it;
		if (!pTerm->Match(pFeedItemInfo))
		{
			return false;
		}
	}

	return true;
}


FeedFilter::FeedFilter(const char* szFilter)
{
	Compile(szFilter);
}

FeedFilter::~FeedFilter()
{
	for (RuleList::iterator it = m_Rules.begin(); it != m_Rules.end(); it++)
	{
		delete *it;
	}
}

void FeedFilter::Compile(const char* szFilter)
{
	debug("Compiling filter: %s", szFilter);

	char* szFilter2 = strdup(szFilter);
	char* szRule = szFilter2;

	for (char* p = szRule; *p; p++)
	{
		char ch = *p;
		if (ch == '%')
		{
			*p = '\0';
			CompileRule(szRule);
			szRule = p + 1;
		}
	}

	CompileRule(szRule);

	free(szFilter2);
}

void FeedFilter::CompileRule(char* szRule)
{
	Rule* pRule = new Rule();
	m_Rules.push_back(pRule);
	pRule->Compile(szRule);
}

void FeedFilter::Match(FeedItemInfo* pFeedItemInfo)
{
	int index = 0;
	for (RuleList::iterator it = m_Rules.begin(); it != m_Rules.end(); it++)
	{
		Rule* pRule = *it;
		index++;
		if (pRule->IsValid())
		{
			bool bMatch = pRule->Match(pFeedItemInfo);
			switch (pRule->GetCommand())
			{
				case frAccept:
				case frOptions:
					if (bMatch)
					{
						pFeedItemInfo->SetMatchStatus(FeedItemInfo::msAccepted);
						pFeedItemInfo->SetMatchRule(index);
						if (pRule->HasPause())
						{
							pFeedItemInfo->SetPauseNzb(pRule->GetPause());
						}
						if (pRule->HasCategory())
						{
							pFeedItemInfo->SetAddCategory(pRule->GetCategory());
						}
						if (pRule->HasPriority())
						{
							pFeedItemInfo->SetPriority(pRule->GetPriority());
						}
						if (pRule->GetCommand() == frAccept)
						{
							return;
						}
					}
					break;

				case frReject:
					if (bMatch)
					{
						pFeedItemInfo->SetMatchStatus(FeedItemInfo::msRejected);
						pFeedItemInfo->SetMatchRule(index);
						return;
					}
					break;

				case frRequire:
					if (!bMatch)
					{
						pFeedItemInfo->SetMatchStatus(FeedItemInfo::msRejected);
						pFeedItemInfo->SetMatchRule(index);
						return;
					}
					break;

				case frComment:
					break;
			}
		}
	}

	pFeedItemInfo->SetMatchStatus(FeedItemInfo::msIgnored);
	pFeedItemInfo->SetMatchRule(0);
}
