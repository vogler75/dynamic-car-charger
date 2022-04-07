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

#ifndef _BLUETOOTH_H_
#define _BLUETOOTH_H_

#include "osselect.h"

#ifdef WIN32

//Microsoft Visual Studio 2010 warnings
//warning C4996: 'sscanf': This function or variable may be unsafe. Consider using sscanf_s instead.
//To disable deprecation, use _CRT_SECURE_NO_WARNINGS. See online help for details.

//TODO: Fix the code to avoid these warnings
#pragma warning(disable: 4996)

#define _USE_32BIT_TIME_T

/*
When you get a few warnings C4068 (Unknown Pragma), read this:
http://www.curlybrace.com/words/2011/01/17/bluetoothapis-h-broken-in-windows-sdk/
*/

//Order is important: WinSock2/BluetoothAPIs/ws2bth.h
#include <WinSock2.h>
#include <BluetoothAPIs.h>
#include <ws2bth.h>

//Windows Sockets Error Codes
//http://msdn.microsoft.com/en-us/library/ms740668(v=vs.85).aspx

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wsock32.lib")

typedef ULONGLONG BT_ADDR, *PBT_ADDR;

#endif	/* WIN32 */

#ifdef linux
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#endif	/* linux */

#include <stdio.h>
#include <ctype.h>
unsigned char char2dec(char ch);
unsigned char hexbyte2dec(char *hex);

#define BT_NUMRETRY 10
#define BT_TIMEOUT  10

#define BT_BUFSIZE 128
extern unsigned char BT_buf[BT_BUFSIZE];

extern int packetposition;
extern int MAX_BT_Buf;

extern int debug;
extern int verbose;

//Function prototypes
int BT_Connect(char *btAddr);
int BT_Close();
int BT_Read(unsigned char *buf, unsigned int bufsize);
int BT_Send(unsigned char *btbuffer);
int setBlockingMode();
int setNonBlockingMode();
void BT_Clear();
void HexDump(unsigned char *buf, int count, int radix);

#ifdef WIN32
int str2ba(const char *straddr, BTH_ADDR *btaddr);
int BT_SearchDevices();
#endif	/* WIN32 */

#endif /* _BLUETOOTH_H_ */
