//
// Copyright (C) 2021 James Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

#ifndef _URL_H_
#define _URL_H_

#include <string.h>
#include <stdio.h>
#pragma warning(disable:4996)

#define MAX_URL_LENGTH 512

struct URL
{
	URL() { url[0] = '\0'; }
	URL(const char* inURL)
	{
		strcpy(url, inURL);
	}
	URL(const URL& other)
	{
		strcpy(url, other.url);
	}

	URL& operator= (const URL& other)
	{
		strcpy(url, other.url);
		return *this;
	}

	URL& operator= (const char* other)
	{
		strcpy(url, other);
		return *this;
	}

	static void ProcessEscapeCodes(URL& url)
	{
		char* match;

		while (match = strstr(url.url, "/./"))
		{
			memmove(match, match + 2, strlen(match) - 1);
		}

		while (match = strstr(url.url, "&amp;"))
		{
			memmove(match + 1, match + 5, strlen(match + 1) - 3);
		}
	}

	static const URL& GenerateFromRelative(const char* baseURL, const char* relativeURL)
	{
		static URL result;

		// First check if the relative URL is actually an absolute URL
		const char* protocolLocation = strstr(relativeURL, "://");
		if (protocolLocation)
		{
			const char* questionMarkLocation = strchr(relativeURL, '?');

			if (!questionMarkLocation || protocolLocation < questionMarkLocation)
			{
				result = relativeURL;
				return result;
			}
		}

		// Check if this is a hash link
		if (relativeURL[0] == '#')
		{
			strcpy(result.url, baseURL);

			// Strip any existing hash link
			char* existing = strchr(result.url, '#');
			if (existing)
			{
				*existing = '\0';
			}

			strcpy(result.url + strlen(result.url), relativeURL);
			return result;
		}

		// find last '/' in base
		const char* baseQuestionMarkLocation = strchr(baseURL, '?');
		const char* lastSlashPos = NULL;
		
		if (baseQuestionMarkLocation)
		{
			lastSlashPos = strchr(baseURL, '/');

			while (lastSlashPos)
			{
				const char* slashPos = strchr(lastSlashPos + 1, '/');
				if (!slashPos)
				{
					break;
				}
				else if (slashPos < baseQuestionMarkLocation)
				{
					lastSlashPos = slashPos;
				}
				else break;
			}
		}
		else
		{
			const char* baseProtocolLocation = strstr(baseURL, "://");
			if (baseProtocolLocation)
			{
				lastSlashPos = strrchr(baseProtocolLocation + 3, '/');
			}
			else
			{
				lastSlashPos = strrchr(baseURL, '/');
			}
			if (!lastSlashPos)
			{
				lastSlashPos = baseURL + strlen(baseURL);
			}
		}

			
		// Skip leading '/' on relative URL
		while (*relativeURL == '/')
		{
			relativeURL++;
		}

		if (lastSlashPos)
		{
			char* writePtr = result.url;

			for (const char* p = baseURL; p != lastSlashPos; p++)
			{
				*writePtr++ = *p;
			}
			*writePtr++ = '/';
			for (const char* p = relativeURL; *p; p++)
			{
				*writePtr++ = *p;
			}
			*writePtr = '\0';
		}
		else
		{
			strcpy(result.url, "http://");
			strcpy(result.url + 7, relativeURL);
		}

		ProcessEscapeCodes(result);

		return result;
	}

	char url[MAX_URL_LENGTH];
};

#endif
