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

#ifndef _SMANET_H_
#define _SMANET_H_

#include "osselect.h"
#include "bluetooth.h"

#define maxpcktBufsize 520

//Function prototypes
void writeLong(unsigned char *btbuffer, unsigned long v);
void writeByte(unsigned char *btbuffer, unsigned char v);
void writeArray(unsigned char *btbuffer, const unsigned char bytes[], int count);
void writePacket(unsigned char *btbuffer, unsigned char ctrl1, unsigned char ctrl2, unsigned char packetcount, unsigned char a, unsigned char b, unsigned char c);
void writePacketTrailer(unsigned char *btbuffer);
void writePacketHeader(unsigned char *btbuffer, unsigned int control, unsigned char *destaddress);
void writePacketLength(unsigned char *btbuffer);
int validateChecksum(void);
short get_short(unsigned char *buf);
long get_long(unsigned char *buf);
long long get_longlong(unsigned char *buf);

#endif
