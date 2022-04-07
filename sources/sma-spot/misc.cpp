/************************************************************************************************
	SMAspot - Yet another tool to read power production of SMA solar inverters
	(c)2012-2013, SBF (mailto:s.b.f@skynet.be)

	Latest version found at http://code.google.com/p/sma-spot/

	License: Attribution-NonCommercial-ShareAlike 3.0 Unported (CC BY-NC-SA 3.0)
	http://creativecommons.org/licenses/by-nc-sa/3.0/

	You are free:
		to Share — to copy, distribute and transmit the work
		to Remix — to adapt the work
	Under the following conditions:
	Attribution:
		You must attribute the work in the manner specified by the author or licensor
		(but not in any way that suggests that they endorse you or your use of the work).
	Noncommercial:
		You may not use this work for commercial purposes.
	Share Alike:
		If you alter, transform, or build upon this work, you may distribute the resulting work
		only under the same or similar license to this one.

DISCLAIMER:
	A user of SMAspot software acknowledges that he or she is receiving this
	software on an "as is" basis and the user is not relying on the accuracy
	or functionality of the software for any purpose. The user further
	acknowledges that any use of this software will be at his own risk
	and the copyright owner accepts no responsibility whatsoever arising from
	the use or application of the software.

************************************************************************************************/

#include "misc.h"
#include <time.h>
#include <string.h>

//print time as UTC time
char *strfgmtime_t (const char *format, const time_t rawtime)
{
    static char buffer[256];
    struct tm tm_struct;
	memcpy(&tm_struct, gmtime(&rawtime), sizeof(tm_struct));
    strftime(buffer, sizeof(buffer), format, &tm_struct);
    return buffer;
}

//Print time as local time
char *strftime_t (const char *format, const time_t rawtime)
{
    static char buffer[256];
    struct tm tm_struct;
	memcpy(&tm_struct, localtime(&rawtime), sizeof(tm_struct));
    strftime(buffer, sizeof(buffer), format, &tm_struct);
    return buffer;
}

char *rtrim(char *txt)
{
    if ((txt != NULL) && (*txt != 0))
    {
        char *ptr = txt;

        // Find end-of-string
        while (*ptr != 0) ptr++;
        ptr--;

        while (ptr >= txt && (*ptr == ' ' || *ptr == '\t' || *ptr == '\r' || *ptr == '\n'))
            ptr--;
        *(++ptr) = 0;
    }

	return txt;
}

//V1.4.5 - Fixed issue 14
#ifdef linux
int get_tzOffset(void)
{
	time_t curtime;
	time(&curtime);

	struct tm *loctime = localtime(&curtime);

	return loctime->tm_gmtoff;
}
#elif WIN32

//Get timezone in seconds
//Windows doesn't have tm_gmtoff member in tm struct
//We try to calculate it
int get_tzOffset(void)
{
	time_t curtime;
	time(&curtime);

	// gmtime() and localtime() share the same static tm structure, so we have to make a copy
	// http://www.cplusplus.com/reference/ctime/localtime/

	struct tm loctime;	//Local Time
	memcpy(&loctime, localtime(&curtime), sizeof(loctime));

	struct tm utctime;	//GMT time
	memcpy(&utctime, gmtime(&curtime), sizeof(loctime));

	int tzOffset = (loctime.tm_hour - utctime.tm_hour) * 3600 + (loctime.tm_min - utctime.tm_min) * 60;

	if((loctime.tm_year > utctime.tm_year) || (loctime.tm_mon > utctime.tm_mon) || (loctime.tm_mday > utctime.tm_mday))
		tzOffset += 86400;

	if((loctime.tm_year < utctime.tm_year) || (loctime.tm_mon < utctime.tm_mon) || (loctime.tm_mday < utctime.tm_mday))
		tzOffset -= 86400;

	if(loctime.tm_isdst == 1)
		tzOffset += 3600;

	return tzOffset;
}
#endif

// Create a full directory
int CreatePath(char *dir)
{
	char fullPath[MAX_PATH];
	int rc = 0;
	#ifdef WIN32
	_fullpath(fullPath, dir, sizeof(fullPath));
	//Terminate path with backslash
	char c = fullPath[strlen(fullPath)];
	if ((c != '/') && (c != '\\')) strcat(fullPath, "\\");
	#else //Linux
	realpath(dir, fullPath);
	//Terminate path with slash
	char c = fullPath[strlen(fullPath)];
	if (c != '/') strcat(fullPath, "/");
	#endif

	int idx = 0;
	while (fullPath[idx] != 0)
	{
		c = fullPath[idx++];
		if ((c == '/') || (c == '\\'))
		{
			c = fullPath[idx];
			fullPath[idx] = 0;

			//Create directory
			//Ignore error here, only the last one will be returned
			#ifdef WIN32
			rc = _mkdir(fullPath);
			#else //Linux
			rc = mkdir(fullPath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
			#endif
			fullPath[idx] = c;
		}
	}

	if (rc == 0)
		return rc;
	else
		return errno;
}


/*
//TODO: Implement this for Linux/Windows
#ifdef linux
int getOSVersion(char *VersionString)
{
	return 0;
}

#elif WIN32
int getOSVersion(char *VersionString)
{
    OSVERSIONINFO vi;
    vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&vi);

	return 0;
}
#endif
*/
