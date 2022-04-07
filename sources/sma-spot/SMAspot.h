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
#ifndef _SMASPOT_H_
#define _SMASPOT_H_

#include "misc.h"
#include "SMANet.h"

#include <time.h>

typedef struct
{
	time_t datetime;
	float totalkWh;
	float daykWh;
} MonthData;

typedef struct
{
	time_t datetime;
	float totalkWh;
	float watt;
} DayData;

enum getInverterDataType
{
	EnergyProduction	= 1 << 0,
	SpotDCPower			= 1 << 1,
	SpotDCVoltage		= 1 << 2,
	SpotACPower			= 1 << 3,
	SpotACVoltage		= 1 << 4,
	SpotGridFrequency	= 1 << 5,
	MaxACPower			= 1 << 6,
	MaxACPower2			= 1 << 7,
	SpotACTotalPower	= 1 << 8,
	TypeLabel			= 1 << 9,
	OperationTime		= 1 << 10,
	SoftwareVersion		= 1 << 11,
	DeviceStatus		= 1 << 12
};

typedef struct
{
	char DeviceName[33];  //32 bytes + terminating zero
	unsigned long Serial;
	unsigned char NetID;
	float BT_Signal;	
	time_t InverterDatetime;
	time_t WakeupTime;
	time_t SleepTime;
	long Pdc1;
	long Pdc2;
	long Udc1;
	long Udc2;
	long Idc1;
	long Idc2;
	long Pmax1;
	long Pmax2;
	long Pmax3;
	long TotalPac;
	long Pac1;
	long Pac2;
	long Pac3;
	long Uac1;
	long Uac2;
	long Uac3;
	long Iac1;
	long Iac2;
	long Iac3;
	long GridFreq;
	long long OperationTime;
	long long FeedInTime;
	long long EToday;
	long long ETotal;
	unsigned short modelID;
	char DeviceType[64];
	char DeviceClass[64];
	char SWVersion[16];	//"03.01.05.R"
	int DeviceStatus;
	int flags;
	time_t CurrentTime;
} InverterData;

typedef struct
{
	char	ConfigFile[MAX_PATH];	//Fullpath to configuration file
	char	BT_Address[18];			//Inverter bluetooth address 12:34:56:78:9A:BC
	int		BT_Timeout;
	int		BT_ConnectRetries;
	char	SMA_Password[13];
	float	latitude;
	float	longitude;
	time_t	archdata_from;
	time_t	archdata_to;
	char	delimiter;			//CSV field delimiter
	int		precision;			//CSV value precision
	char	decimalpoint;		//CSV decimal point
	char	outputPath[256];
	char	plantname[32];
	char	sqlDatabase[256];
	char	sqlHostname[256];
	char	sqlUsername[64];
	char	sqlUserPassword[32];
	int		synchTime;			// 1=Synch inverter time with computer time (default=0)
	time_t	sunrise;
	time_t	sunset;
	int		isLight;
	int		calcMissingSpot;	// 0-1
	char	DateTimeFormat[32];
	char	DateFormat[32];
	char	TimeFormat[32];
	int		CSV_Export;
	int		CSV_Header;
	int		CSV_ExtendedHeader;
	int		CSV_SaveZeroPower;
	int		SunRSOffset;		// Offset to start before sunrise and end after sunset
	int		userGroup;			// USER/INSTALLER
	char	prgVersion[16];

	int		PVoutput;			// 0-1
	int		PVoutput_SID;
	char	PVoutput_Key[42];

	//Commandline settings
	int		debug;				// -d		Debug level (0-5)
	int		verbose;			// -v		Verbose output level (0-5)
	int		archDays;			// -ad		Number of days back to get Archived DayData (0=disabled, 1=today, ...)
	int		archMonths;			// -am		Number of months back to get Archived MonthData (0=disabled, 1=current month, ...)
	int		upload;				// -u		Upload to online monitoring systems (PVOutput, ...)
	int		forceInq;			// -finq	Inquire inverter also during the night
	int		ACLoop;				// -ac          Loop for AC-Read
	int		ACLoopDelay;			// -acd		Delay for AC-Loop

} Config;


#pragma pack(1)
typedef struct PacketHeader
{
	unsigned char SOP;              // Start Of Packet (0x7E)
	unsigned short pkLength;
	unsigned char pkChecksum;
	unsigned char InvAddress[6];    // SMA Inverter Address
	unsigned char LocAddress[6];    // Local BT Address
	unsigned short command;
} pkHeader;

typedef struct ArchiveDataRec
{
	time_t datetime;
	unsigned long long totalWh;
} ArchDataRec;

typedef unsigned char BYTE;

#define INV_STATUS	0x00214801	// *08* Device Status (Status/Device Status)

#define SPOT_PDC1	0x00251E01	// *40* DC Power String 1
#define SPOT_PDC2	0x00251E02	// *40* DC Power String 2

#define SPOT_ETOTAL	0x00260101	// *00* Energy generated total
#define SPOT_ETODAY	0x00262201	// *00* Energy generated today

#define SPOT_PACTOT 0x00263F01  // *40* Actual AC Power Production

#define INV_PACMAX1	0x00411E01  // *00* Max AC Power phase 1
#define INV_PACMAX2	0x00411F01  // *00* Max AC Power phase 2
#define INV_PACMAX3	0x00412001  // *00* Max AC Power phase 3

#define INV_PACMAX1_2	0x00832A01  // *00* Max AC Power (Some inverters like SB3300/SB1200)

#define SPOT_UDC1	0x00451F01	// *40* DC Voltage String 1
#define SPOT_UDC2	0x00451F02	// *40* DC Voltage String 2
#define SPOT_IDC1	0x00452101	// *40* DC Current String 1
#define SPOT_IDC2	0x00452102	// *40* DC Current String 2

#define SPOT_OPERTM	0x00462E01	// *00* Operating Time (hours)
#define SPOT_FEEDTM	0x00462F01	// *00* Feed-In Time (hours)

#define SPOT_PAC1	0x00464001	// *40* AC Power Phase 1
#define SPOT_PAC2	0x00464101	// *40* AC Power Phase 2
#define SPOT_PAC3	0x00464201	// *40* AC Power Phase 3
#define SPOT_UAC1	0x00464801	// *00* AC Voltage Phase 1
#define SPOT_UAC2	0x00464901	// *00* AC Voltage Phase 2
#define SPOT_UAC3	0x00464A01	// *00* AC Voltage Phase 3
#define SPOT_IAC1	0x00465001	// *00* AC Current Phase 1
#define SPOT_IAC2	0x00465101	// *00* AC Current Phase 2
#define SPOT_IAC3	0x00465201	// *00* AC Current Phase 3

#define SPOT_FREQ	0x00465701  // *00* Grid frequency

//Type Label
#define INV_NAME	0x00821E01  // *10* Inverter Name  (Type Label/Device name)
#define INV_CLASS	0x00821F01  // *08* Inverter Class (Type Label/Device class)
#define INV_TYPE	0x00822001  // *08* Inverter Type  (Type Label/Device type)
#define INV_TBD		0x00822101  // *08* TBD - Unknown
#define INV_SWVER	0x00823401	// *00* Software Version

//#define INV_PASSW	0xBBBBAAAAL

#define NULLVALUE   0x80000000L	// NULLVALUE & NOVALUE will be converted to 0 by SMAspot
#define NOVALUE     0xFFFFFFFFL
#define NULLVALUE64 0x8000000000000000LL	// NULLVALUE64 & NOVALUE64 will be converted to 0 by SMAspot
#define NOVALUE64   0xFFFFFFFFFFFFFFFFLL

typedef enum ErrorCodes
{
	E_OK = 0,
	E_NODATA = -1,		// Bluetooth buffer empty
	E_BADARG = -2,		// Unknown command line argument
	E_CHKSUM = -3,		// Invalid Checksum
	E_BUFOFLW = -4,		// Buffer overflow
	E_ARCHNODATA = -5,	// No archived data found for given timespan
	E_INIT = -6,		// Unable to initialize
	E_INVPASSW = -7		// Invalid password
} E_SMASPOT;

//User Group
#define	UG_USER			0x07L
#define UG_INSTALLER	0x0AL

#define tokWh(value64) (double)(value64)/1000
#define tokW(value32) (float)(value32)/1000
#define toHour(value64) (double)(value64)/3600
#define toAmp(value32) (float)value32/1000
#define toVolt(value32) (float)value32/100
#define toHz(value32) (float)value32/100

//Function prototypes
E_SMASPOT	initialiseSMAConnection(InverterData *invData);
int			BT_Connect(char *BTAddress);
E_SMASPOT	logonSMAInverter(long userGroup, char *password);
E_SMASPOT	logoffSMAInverter(void);
int			BT_Send(unsigned char *btbuffer);
void		HexDump(unsigned char *buf, int count, int radix);
int			ArchiveData(unsigned long serial, time_t startTime, time_t endTime);
int			GetConfig(Config *cfg);

void DebugTN(char *msg);
int getSpotValues(InverterData *invData);

E_SMASPOT	ArchiveMonthData(tm *start_tm, MonthData monthData[], int numRecords);
E_SMASPOT	ArchiveDayData(time_t startTime, DayData dayData[], int numRecords);

int			getBT_SignalStrength(float *BT_SignalStrength);
int			DaysInMonth(int month, int year);
int			BT_Read(unsigned char *buf, unsigned int bufsize);
int			parseCmdline(int argc, char **argv, Config *cfg);
void		InvalidArg(char *arg);
void		SayHello(int ShowHelp);
void		CalcMissingSpot(InverterData *invData);
char		*dp2txt(char dp);
char		*delim2txt(const char delim);
int			get_tzOffset(void);
int			isCrcValid(unsigned char lb, unsigned char hb);
void		SynchInverterTime(void);
int			isValidSender(unsigned char address[]);
int			getInverterData(InverterData *invData, enum getInverterDataType type);
int     getInverterData(InverterData *invData, enum getInverterDataType type, int count);

E_SMASPOT	getPacket(int wait4Command);

const char *AttributeToText(int attribute);

void		printHexBytes(BYTE *buf, int num);

extern BYTE pcktBuf[maxpcktBufsize];
extern unsigned char smaBTInverterAddressArray[6];
extern unsigned char addr_unknown[6];
extern unsigned char pcktID;
extern int packetposition;

#endif
