#include "URL.h"

void URL::CleanUp()
{
	// Replace back slashes with forward ones
	for (char* ptr = url; *ptr; ptr++)
	{
		if (*ptr == '\\')
			*ptr = '/';
	}

	// Collapse /./ and /../ 
	char* directory;
	while (directory = strstr(url, "/./"))
	{
		strcpy(directory, directory + 2);
	}
	directory = strstr(url, "/../");
	char* protocolEnd = strstr(url, "://");
	if (protocolEnd)
		protocolEnd += 3;

	while (directory && directory > url)
	{
		char* prevSlash = directory - 1;
		while (prevSlash > url && *prevSlash != '/' && prevSlash > protocolEnd)
		{
			prevSlash--;
		}
		if (*prevSlash == '/' && prevSlash > url)
		{
			strcpy(prevSlash, directory + 3);
			directory = strstr(prevSlash, "/../");
		}
		else
		{
			// At most top level, collapse remaining /../ instances
			while (directory)
			{
				strcpy(directory, directory + 3);
				directory = strstr(url, "/../");
			}
			break;
		}
	}

	// Fix &amp escape sequences
	char* match;
	while (match = strstr(url, "&amp;"))
	{
		memmove(match + 1, match + 5, strlen(match + 1) - 3);
	}
}

const URL& URL::GenerateFromRelative(const char* baseURL, const char* relativeURL)
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

	// Starting with // should be treated as an absolte URL but we will prepend with http://
	if (strstr(relativeURL, "//") == relativeURL)
	{
		strcpy(result.url, "http:");
		strcpy(result.url + 5, relativeURL);
		return result;
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
	if (*relativeURL == '/')
	{
		while (*relativeURL == '/')
		{
			relativeURL++;
		}

		// Move lastSlashPos to the first slash position
		const char* domainPos = strstr(baseURL, "://");
		if (domainPos)
		{
			domainPos += 3;
			const char* firstSlashPos = strchr(domainPos, '/');
			if (firstSlashPos)
			{
				lastSlashPos = firstSlashPos;
			}
		}
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

	result.CleanUp();

	return result;
}
