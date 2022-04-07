#define VERSION "2.0.4"
/************************************************************************************************
SMAspot - Yet another tool to read power production of SMA solar inverters
(c)2012-2013, SBF (mailto:s.b.f@skynet.be)

Latest version found at http://code.google.com/p/sma-spot/

This software is based on Stuart Pittaway's "NANODE SMA PV MONITOR" - Thanks a lot Stuart!
https://github.com/stuartpittaway/nanodesmapvmonitor

Special Thanks to Wim Simons and Gerd Schnuff for helping me to better understand the
SMA Bluetooth Protocol

The Windows version of this project is developed using Visual C++ 2010 Express
=> Use SMAspot.sln / SMAspot.vcxproj
For Linux, project can be built using Code::Blocks or Make tool
=> Use SMAspot.cbp for Code::Blocks
=> Use makefile for make tool (converted from .cbp project file)

I invite everyone to have a look at another project:
SMA-rt - SMA Reader for Tablets (and other Android sma-rt phones)
You can find it at http://code.google.com/p/sma-rt/

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

#include "osselect.h"

#include "SMAspot.h"
#include "misc.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <time.h>
#include "bluetooth.h"
#include "SMANet.h"
#include "CSVexport.h"

int MAX_BT_Buf = 0;
int MAX_pcktBuf = 0;

//Public vars
int debug = 0;
int verbose = 0;
char DateTimeFormat[32];
char DateFormat[32];

int main(int argc, char **argv)
{
	int rc = 0;
	unsigned int idx;

	SayHello(0);

	Config cfg;

	//----------------------------------------------------------------------------------------------------------------------
	//Read the command line and store settings in config struct
	rc = parseCmdline(argc, argv, &cfg);
	if (rc == -1) return 1;	//Invalid commandline - Quit, error
	if (rc == 1) return 0;	//Nothing to do - Quit, no error

	//----------------------------------------------------------------------------------------------------------------------
	//Read config file and store settings in config struct
	rc = GetConfig(&cfg);	//Config struct contains fullpath to config file
	if (rc != 0) return rc;

	//----------------------------------------------------------------------------------------------------------------------
	//Copy some config settings to public variables
	debug = cfg.debug;
	verbose = cfg.verbose;
	strncpy(DateTimeFormat, cfg.DateTimeFormat, sizeof(DateTimeFormat));
	strncpy(DateFormat, cfg.DateFormat, sizeof(DateFormat));

	//----------------------------------------------------------------------------------------------------------------------
	DebugTN("connect...");
	int attempts = 1;
	do
	{
		printf("Connecting to %s (%d/%d)\n", cfg.BT_Address, attempts, cfg.BT_ConnectRetries);
		rc = BT_Connect(cfg.BT_Address);
		attempts++;
	} while ((attempts <= cfg.BT_ConnectRetries) && (rc != 0));

	if (rc != 0)
	{
		fprintf(stderr, "BT_Connect() returned %d\n", rc);
		return rc;
	}
	DebugTN("connect...done");

	//----------------------------------------------------------------------------------------------------------------------
	//Convert BT_Address (00:00:00:00:00:00) to smaBTInverterAddressArray
	//scanf reads %02X as int, but we need unsigned char
	unsigned int tmp[6];
	sscanf(cfg.BT_Address, "%02X:%02X:%02X:%02X:%02X:%02X", &tmp[5], &tmp[4], &tmp[3], &tmp[2], &tmp[1], &tmp[0]);
	for (int i=0; i<6; i++)
		smaBTInverterAddressArray[i] = (unsigned char)tmp[i];

	//----------------------------------------------------------------------------------------------------------------------
	InverterData invData; memset(&invData, 0, sizeof(InverterData));

	//----------------------------------------------------------------------------------------------------------------------
	rc = initialiseSMAConnection(&invData);
	if (rc != E_OK)
	{
		fprintf(stderr, "Failed to initialize communication with inverter.\n");
		return rc;
	}

	//----------------------------------------------------------------------------------------------------------------------
	rc = getBT_SignalStrength(&invData.BT_Signal);
	if (VERBOSE_NORMAL)
		printf("BT Signal=%0.f%%\n", invData.BT_Signal);

	if (logonSMAInverter(cfg.userGroup, cfg.SMA_Password) != E_OK)
	{
		fprintf(stderr, "Logon failed. Check '%s' Password\n", cfg.userGroup == UG_USER? "USER":"INSTALLER");
		return 1;
	}

	if (VERBOSE_NORMAL)
		puts("Logon OK");

	//----------------------------------------------------------------------------------------------------------------------
	// Synchronize inverter time with system time only if enabled in config and difference is more than 1 minute
	if (cfg.synchTime == 1) SynchInverterTime();

	//----------------------------------------------------------------------------------------------------------------------
	if ((rc = getInverterData(&invData, SoftwareVersion, 3)) != 0)
		printf("getSoftwareVersion returned an error: %d\n", rc);

	if ((rc = getInverterData(&invData, TypeLabel, 3)) != 0)
		printf("getTypeLabel returned an error: %d\n", rc);
	else if (VERBOSE_NORMAL)
	{
		printf("Device Name:      %s\n", invData.DeviceName);
		printf("Device Class:     %s\n", invData.DeviceClass);
		printf("Device Type:      %s\n", invData.DeviceType);
		printf("Software Version: %s\n", invData.SWVersion);
		printf("Serial number:    %lu\n", invData.Serial);
	}

	if ((rc = getInverterData(&invData, DeviceStatus, 3)) != 0)
		printf("getDeviceStatus returned an error: %d\n", rc);
	else if (VERBOSE_NORMAL)
		printf("Device Status:      %s\n", AttributeToText(invData.DeviceStatus));

	//Get current time
	time(&cfg.archdata_from);

	//----------------------------------------------------------------------------------------------------------------------
	// Get Day Data 
	time_t arch_time;
	time(&arch_time);
	for (int count=0; count<cfg.archDays; count++)
	{
		DayData dData[288];
		if ((rc = ArchiveDayData(arch_time, dData, sizeof(dData)/sizeof(DayData))) != E_OK)
		{
			if (rc != E_ARCHNODATA) printf("ArchiveDayData returned an error: %d\n", rc);
		}
		else
		{
			if (VERBOSE_HIGH)
			{
				for (idx=0; idx<sizeof(dData)/sizeof(DayData); idx++)
				{
					if (dData[idx].datetime > 0)
						printf("%s : %.3fkWh - %3.3fW\n", strftime_t(cfg.DateTimeFormat, dData[idx].datetime), dData[idx].totalkWh, dData[idx].watt);
				}

				puts("======");
			}

			ExportDayDataToCSV(&cfg, &invData, dData, sizeof(dData)/sizeof(DayData));
		}

		//Goto previous day
		arch_time -= 86400;
	}

	//----------------------------------------------------------------------------------------------------------------------
	// Get Month Data 
	time(&arch_time);
	struct tm arch_tm;
	memcpy(&arch_tm, gmtime(&arch_time), sizeof(arch_tm));

	for (int count=0; count<cfg.archMonths; count++)
	{
		MonthData mData[31];
		ArchiveMonthData(&arch_tm, mData, sizeof(mData)/sizeof(MonthData));

		if (VERBOSE_HIGH)
		{
			for (unsigned int idx = 0; idx < sizeof(mData)/sizeof(MonthData); idx++)
			{
				if (mData[idx].datetime > 0)
					printf("%s : %.3fkWh - %3.3fkWh\n", strfgmtime_t(cfg.DateTimeFormat, mData[idx].datetime), mData[idx].totalkWh, mData[idx].daykWh);
			}
			puts("======");
		}

		ExportMonthDataToCSV(&cfg, &invData, mData, sizeof(mData)/sizeof(MonthData));

		//Go to previous month
		if (--arch_tm.tm_mon < 0)
		{
			arch_tm.tm_mon = 11;
			arch_tm.tm_year--;
		}
	}

	//----------------------------------------------------------------------------------------------------------------------
	// Spot Data
	if ( (rc=getSpotValues(&invData)) == 0 )
  {
	  if (cfg.calcMissingSpot == 1) 
      CalcMissingSpot(&invData);
	  ExportSpotDataToCSV(&cfg, &invData);
    if (VERBOSE_NORMAL)
    {
      if (invData.InverterDatetime > 0)
        printf("Current Inverter Time: %s\n", strftime_t(cfg.DateTimeFormat, invData.InverterDatetime));

      if (invData.WakeupTime > 0)
        printf("Inverter Wake-Up Time: %s\n", strftime_t(cfg.DateTimeFormat, invData.WakeupTime));

      if (invData.SleepTime > 0)
        printf("Inverter Sleep Time  : %s\n", strftime_t(cfg.DateTimeFormat, invData.SleepTime));
    }
  }
	
	//----------------------------------------------------------------------------------------------------------------------
	// AC-Power
  invData.Pdc1 = 0;
	invData.Pdc2 = 0;
	invData.Udc1 = 0;
	invData.Udc2 = 0;
	invData.Idc1 = 0;
	invData.Idc2 = 0;
	invData.Pmax1 = 0;
	invData.Pmax2 = 0;
	invData.Pmax3 = 0;
	invData.TotalPac = 0;
	invData.Pac1 = 0;
	invData.Pac2 = 0;
	invData.Pac3 = 0;
	invData.Uac1 = 0;
	invData.Uac2 = 0;
	invData.Uac3 = 0;
	invData.Iac1 = 0;
	invData.Iac2 = 0;
	invData.Iac3 = 0;
	invData.GridFreq = 0;
	invData.OperationTime = 0;
	invData.FeedInTime = 0;
	invData.EToday = 0;
	invData.ETotal = 0;
	for (int loop = 1; loop < cfg.ACLoop; loop++ )
	{
		usleep(cfg.ACLoopDelay*1000);

	  	time(&invData.CurrentTime);
		if ((rc = getInverterData(&invData, SpotACPower, 3)) == 0 &&
		    (rc = getInverterData(&invData, SpotDCPower, 3)) == 0   )
    		{
		    	if (cfg.calcMissingSpot == 1) 
        			CalcMissingSpot(&invData);
			ExportSpotDataToCSV(&cfg, &invData);
    		}
		else
			printf("getSpotXXPower returned an error: %d\n", rc);

	}

	//----------------------------------------------------------------------------------------------------------------------
	logoffSMAInverter();

	puts("Done.");

	return 0;
}

//---------------------------------------------------------------------------------------------------------------------------------------------
int getSpotValues(InverterData *invData)
{
	int rc, errcnt=0;

	time(&invData->CurrentTime);

	if (((invData->flags & MaxACPower) != 0) && ((invData->flags & MaxACPower2) != 0))
	{
		if ((rc = getInverterData(invData, MaxACPower, 3)) != 0)
			printf("getMaxACPower returned an error: %d\n", rc);
		else
		{
			if ((invData->Pmax1 == 0) && (rc = getInverterData(invData, MaxACPower2, 3)) != 0)
				printf("getMaxACPower2 returned an error: %d\n", rc);
		}
	}

	if ((rc = getInverterData(invData, EnergyProduction, 3)) != 0)
		printf("getEnergyProduction returned an error: %d\n", rc) && errcnt++;

	if ((rc = getInverterData(invData, OperationTime, 3)) != 0)
		printf("getOperationTime returned an error: %d\n", rc);

	if ((rc = getInverterData(invData, SpotDCPower, 3)) != 0)
		printf("getSpotDCPower returned an error: %d\n", rc);

	if ((rc = getInverterData(invData, SpotDCVoltage, 3)) != 0)
		printf("getSpotDCVoltage returned an error: %d\n", rc);

	if ((rc = getInverterData(invData, SpotACPower, 3)) != 0)
		printf("getSpotACPower returned an error: %d\n", rc);

	if ((rc = getInverterData(invData, SpotACVoltage, 3)) != 0)
		printf("getSpotACVoltage returned an error: %d\n", rc);

	if ((rc = getInverterData(invData, SpotACTotalPower, 3)) != 0)
		printf("getSpotACTotalPower returned an error: %d\n", rc);

	if ((rc = getInverterData(invData, SpotGridFrequency, 3)) != 0)
		printf("getSpotGridFrequency returned an error: %d\n", rc);

	if (VERBOSE_NORMAL)
	{
		puts("DC Spot Data:");
		printf("Pac max phase 1: %luW\n", invData->Pmax1);
		printf("Pac max phase 2: %luW\n", invData->Pmax2);
		printf("Pac max phase 3: %luW\n", invData->Pmax3);
		printf("\tEToday: %.3fkWh\n", tokWh(invData->EToday));
		printf("\tETotal: %.3fkWh\n", tokWh(invData->ETotal));
		printf("\tOperation Time: %.2fh\n", toHour(invData->OperationTime));
		printf("\tFeed-In Time  : %.2fh\n", toHour(invData->FeedInTime));
		printf("\tString 1 Pdc: %7.3fkW - Udc: %6.2fV - Idc: %6.3fA\n", tokW(invData->Pdc1), toVolt(invData->Udc1), toAmp(invData->Idc1));
		printf("\tString 2 Pdc: %7.3fkW - Udc: %6.2fV - Idc: %6.3fA\n", tokW(invData->Pdc2), toVolt(invData->Udc2), toAmp(invData->Idc2));
		puts("AC Spot Data:");
		printf("\tPhase 1 Pac : %7.3fkW - Uac: %6.2fV - Iac: %6.3fA\n", tokW(invData->Pac1), toVolt(invData->Uac1), toAmp(invData->Iac1));
		printf("\tPhase 2 Pac : %7.3fkW - Uac: %6.2fV - Iac: %6.3fA\n", tokW(invData->Pac2), toVolt(invData->Uac2), toAmp(invData->Iac2));
		printf("\tPhase 3 Pac : %7.3fkW - Uac: %6.2fV - Iac: %6.3fA\n", tokW(invData->Pac3), toVolt(invData->Uac3), toAmp(invData->Iac3));
		printf("\tTotal Pac   : %7.3fkW\n", tokW(invData->TotalPac));
		printf("Grid Freq. : %.2fHz\n", toHz(invData->GridFreq));
	}

  return rc;
}

//DecimalPoint To Text
char *dp2txt(const char dp)
{
	if (dp == '.') return (char *)"point";
	if (dp == ',') return (char *)"comma";
	return (char *)"?";
}

//Delimiter To Text
char *delim2txt(const char delim)
{
	if (delim == ';') return (char *)"semicolon";
	if (delim == ',') return (char *)"comma";
	return (char *)"?";
}

E_SMASPOT ArchiveDayData(time_t startTime, DayData dayData[], int arraySize)
{
	if (VERBOSE_NORMAL)
	{
		puts("********************");
		puts("* ArchiveDayData() *");
		puts("********************");
	}

		printf("startTime = %08lX -> %s\n", startTime, strftime_t("%d/%m/%Y %H:%M:%S", startTime));

	E_SMASPOT rc = E_OK;
	struct tm start_tm;
	//memcpy(&start_tm, localtime(&startTime), sizeof(start_tm));
	memcpy(&start_tm, gmtime(&startTime), sizeof(start_tm));

		printf("startTime = %08lX -> %s\n", startTime, strftime_t("%d/%m/%Y %H:%M:%S", startTime));

	start_tm.tm_hour = 0;
	start_tm.tm_min = 0;
	start_tm.tm_sec = 0;
	startTime = mktime(&start_tm);

	if (VERBOSE_NORMAL)
		printf("startTime = %08lX -> %s\n", startTime, strftime_t("%d/%m/%Y %H:%M:%S", startTime));

	int idx;
	for (idx = 0; idx < arraySize; idx++)
		dayData[idx].datetime = 0;

	int packetcount = 0;
	int validPcktID = 0;

	E_SMASPOT hasData = E_ARCHNODATA;

	do
	{
		do
		{
			pcktID++;
			writePacketHeader(pcktBuf, 0x01, smaBTInverterAddressArray);
			writePacket(pcktBuf, 0x09, 0xE0, pcktID, 0, 0, 0);
			writeByte(pcktBuf, 0x80);
			writeLong(pcktBuf, 0x70000200);
			writeLong(pcktBuf, startTime - 300);
			writeLong(pcktBuf, startTime + 86100);
			writePacketTrailer(pcktBuf);
			writePacketLength(pcktBuf);
		} while (!isCrcValid(pcktBuf[packetposition-3], pcktBuf[packetposition-2]));

		BT_Send(pcktBuf);

		unsigned long long totalWh = 0;
		unsigned long long totalWh_prev = 0;
		float watt = 0;
		float kWh = 0;
		time_t datetime;
		const int recordsize = 12;

		do
		{
			if ((rc = getPacket(1)) != E_OK)
				return rc;

			packetcount = pcktBuf[25];
			//When packetcount < 0 there was no data available...
			if ((validateChecksum() == false) || (packetcount < 0)) break;

			if ((validPcktID == 1) || (pcktID == pcktBuf[27]))
			{
				validPcktID = 1;
				for(int x = 41; x < (packetposition - 3); x += recordsize)
				{
					datetime = (time_t)get_long(pcktBuf + x);
					totalWh = (unsigned long long)get_longlong(pcktBuf + x + 4);
					if (totalWh == NOVALUE64) totalWh = 0;
					if (totalWh > 0) hasData = E_OK;
					kWh = (float)totalWh / 1000.0f;
					watt = (float)(totalWh - totalWh_prev) * 12;    // 60:5
					if (totalWh_prev != 0)
					{
						struct tm timeinfo;
						//memcpy(&timeinfo, localtime(&datetime), sizeof(timeinfo));
						memcpy(&timeinfo, gmtime(&datetime), sizeof(timeinfo));
						if (start_tm.tm_mday == timeinfo.tm_mday)
						{
							int idx = (timeinfo.tm_hour * 12) + (timeinfo.tm_min / 5);
							if (idx < arraySize)
							{
								dayData[idx].datetime = datetime;
								dayData[idx].totalkWh = kWh;
								dayData[idx].watt = watt;
							}
						}
					}
					totalWh_prev = totalWh;
				} //for
			}
			else
			{
				if (DEBUG_HIGHEST) printf("Packet ID mismatch. Expected %d, received %d\n", pcktID, pcktBuf[27]);
				validPcktID = 0;
				packetcount = 0;
				pcktID--;
			}
		} while (packetcount > 0);
	} while (validPcktID == 0);

	return hasData;
}


E_SMASPOT ArchiveMonthData(tm *start_tm, MonthData monthData[], int arraySize)
{
	if (VERBOSE_NORMAL)
	{
		puts("**********************");
		puts("* ArchiveMonthData() *");
		puts("**********************");
	}

	E_SMASPOT rc = E_OK;
	static time_t offset = -1;

	// Set time to 1st of the month at 12:00:00
	start_tm->tm_hour = 12;
	start_tm->tm_min = 0;
	start_tm->tm_sec = 0;
	start_tm->tm_mday = 1;
	time_t startTime = mktime(start_tm);

	if (VERBOSE_NORMAL)
		printf("startTime = %08lX -> %s\n", startTime, strftime_t("%d/%m/%Y %H:%M:%S", startTime));

	for (int idx = 0; idx < arraySize; idx++)
		monthData[idx].datetime = 0;

	int packetcount = 0;
	int validPcktID = 0;

	do
	{
		do
		{
			pcktID++;
			writePacketHeader(pcktBuf, 0x01, smaBTInverterAddressArray);
			writePacket(pcktBuf, 0x09, 0xE0, pcktID, 0, 0, 0);
			writeByte(pcktBuf, 0x80);
			writeLong(pcktBuf, 0x70200200);
			writeLong(pcktBuf, startTime - 86400 - 86400);
			writeLong(pcktBuf, startTime + 86400 * (arraySize +1));
			writePacketTrailer(pcktBuf);
			writePacketLength(pcktBuf);
		} while (!isCrcValid(pcktBuf[packetposition-3], pcktBuf[packetposition-2]));

		BT_Send(pcktBuf);

		unsigned long long totalWh = 0;
		unsigned long long totalWh_prev = 0;
		float totalkWh = 0;
		float daykWh = 0;
		const int recordsize = 12;
		time_t datetime;

		int idx = 0;
		do
		{
			if ((rc = getPacket(1)) != E_OK)
				return rc;

			packetcount = pcktBuf[25];
			//When packetcount < 0 there was no data available...
			if ((validateChecksum() == false) || (packetcount < 0)) break;

			if ((validPcktID == 1) || (pcktID == pcktBuf[27]))
			{
				validPcktID = 1;

				if (offset == -1)
				{
					time_t curtime;
					time(&curtime);
					struct tm curtime_tm;
					memcpy(&curtime_tm, gmtime(&curtime), sizeof(curtime_tm));
					datetime = (time_t)get_long(pcktBuf + packetposition - 15);
					struct tm datetime_tm;
					memcpy(&datetime_tm, gmtime(&datetime), sizeof(datetime_tm));
					if (datetime_tm.tm_yday == curtime_tm.tm_yday) offset = -86400;
					else if (datetime_tm.tm_yday < curtime_tm.tm_yday) offset = 0;
					if (debug > 5)
						printf("curtime_tm.tm_yday=%d datetime_tm.tm_yday=%d -> offset=%ld\n", curtime_tm.tm_yday, datetime_tm.tm_yday, offset);
				}

				for(int x = 41; x < (packetposition - 3); x += recordsize)
				{
					datetime = (time_t)get_long(pcktBuf + x);
					datetime += offset;
					totalWh = get_longlong(pcktBuf + x + 4);
					if (totalWh != MAXULONGLONG)
					{
						totalkWh = (float)totalWh / 1000.0f;
						daykWh = (float)(totalWh - totalWh_prev) / 1000.0f;
						if (totalWh_prev != 0)
						{
							struct tm local_tm;
							//					memcpy(&local_tm, localtime(&datetime), sizeof(local_tm));
							memcpy(&local_tm, gmtime(&datetime), sizeof(local_tm));
							if (local_tm.tm_mon == start_tm->tm_mon)
							{
								if (idx < arraySize)
								{
									monthData[idx].datetime = datetime;
									monthData[idx].totalkWh = totalkWh;
									monthData[idx].daykWh = daykWh;
									idx++;
								}
							}
						}
						totalWh_prev = totalWh;
					}
				} //for
			}
			else
			{
				if (DEBUG_HIGHEST) printf("Packet ID mismatch. Expected %d, received %d\n", pcktID, pcktBuf[27]);
				validPcktID = 0;
				packetcount = 0;
				pcktID--;
			}
		} while (packetcount > 0);
	} while (validPcktID == 0);

	return E_OK;
}

E_SMASPOT getPacket(int wait4Command)
{
	if (DEBUG_NORMAL) printf("getPacket(%d)\n", wait4Command);
	int index = 0;
	int hasL2pckt  = 0;
	pkHeader *pkHdr = (pkHeader *)BT_buf;
	do
	{
		int bib = BT_Read(BT_buf, sizeof(pkHeader));
		if (bib < 0)
		{
			if (DEBUG_NORMAL) printf("No data!\n");
			return E_NODATA;
		}

		//More data after header?
		if (pkHdr->pkLength > sizeof(pkHeader))
		{
			bib += BT_Read(BT_buf + sizeof(pkHeader), pkHdr->pkLength - sizeof(pkHeader));

			if (DEBUG_HIGH) HexDump(BT_buf, bib, 10);

			//In a multi-inverter plant, we need to check
			//if data is coming from the main inverter?
			if (isValidSender(pkHdr->InvAddress) == 1)
			{
				if (DEBUG_NORMAL) printf("cmd=%d\n", pkHdr->command);

				if ((hasL2pckt == 0) && (BT_buf[18] == 0x7E) && (get_long(BT_buf+19) == 0x656003FF))
				{
					hasL2pckt = 1;
				}

				if (hasL2pckt == 1)
				{
					//Copy BT_buf to packetbuffer
					int bufptr = sizeof(pkHeader);
					bool escNext = false;

					if (DEBUG_NORMAL) printf("PacketLength=%d\n", pkHdr->pkLength);

					for (int i = sizeof(pkHeader); i < pkHdr->pkLength; i++)
					{
						pcktBuf[index] = BT_buf[bufptr++];
						//Keep 1st byte raw unescaped 0x7E
						if (escNext == true)
						{
							pcktBuf[index] ^= 0x20;
							escNext = false;
							index++;
						}
						else
						{
							if (pcktBuf[index] == 0x7D)
								escNext = true; //Throw away the 0x7d byte
							else
								index++;
						}
						if (index >= maxpcktBufsize)
						{
							printf("Warning: pcktBuf buffer overflow! (%d)\n", index);
							return E_BUFOFLW;
						}
					}
					packetposition = index;
				}
				else
				{
					memcpy(pcktBuf, BT_buf, bib);
					packetposition = bib;
				}
			} // isValidSender()
			else if (DEBUG_NORMAL)
				printf("Wrong sender\n");
		}
	}
	while (pkHdr->command != wait4Command);

	if (DEBUG_HIGH)
	{
		printf("<<<====== Content of pcktBuf =======>>>\n");
		HexDump(pcktBuf, packetposition, 10);
		printf("<<<=================================>>>\n");
	}

	if (packetposition > MAX_pcktBuf)
	{
		MAX_pcktBuf = packetposition;
		if (DEBUG_HIGH)
		{
			printf("MAX_pcktBuf is now %d bytes\n", MAX_pcktBuf);
		}
	}

	return E_OK;
}

E_SMASPOT initialiseSMAConnection(InverterData *invData)
{
	if (VERBOSE_NORMAL) puts("Initializing...");

	//Wait for announcement/broadcast message from PV inverter
	if (getPacket(2) != E_OK)
		return E_INIT;

	invData->NetID = pcktBuf[22];
	if (VERBOSE_NORMAL) printf("SMA netID=%02X\n", invData->NetID);

	writePacketHeader(pcktBuf, 0x02, smaBTInverterAddressArray);
	writeLong(pcktBuf, 0x00700400);
	writeByte(pcktBuf, invData->NetID);
	writeLong(pcktBuf, 0);
	writeLong(pcktBuf, 1);
	writePacketLength(pcktBuf);
	BT_Send(pcktBuf);

	if (getPacket(5) != E_OK)
		return E_INIT;

	do
	{
		pcktID++;
		writePacketHeader(pcktBuf, 0x01, addr_unknown);
		writePacket(pcktBuf, 0x09, 0xA0, pcktID, 0, 0, 0);
		writeByte(pcktBuf, 0x80);
		writeLong(pcktBuf, 0x00000200);
		writeLong(pcktBuf, 0);
		writeLong(pcktBuf, 0);
		writePacketTrailer(pcktBuf);
		writePacketLength(pcktBuf);
	} while (!isCrcValid(pcktBuf[packetposition-3], pcktBuf[packetposition-2]));

	BT_Send(pcktBuf);

	if (getPacket(1) != E_OK)
		return E_INIT;

	if (!validateChecksum())
		return E_CHKSUM;

	invData->Serial = get_long(pcktBuf + 57);
	if (VERBOSE_NORMAL) printf("Serial Nr: %08lX (%lu)\n", invData->Serial, invData->Serial);

	logoffSMAInverter();

	return E_OK;
}

E_SMASPOT logonSMAInverter(long userGroup, char *password)
{
#define MAX_PWLENGTH 12
	unsigned char pw[MAX_PWLENGTH] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	if (DEBUG_NORMAL) puts("logonSMAInverter()");

	char encChar = (userGroup == UG_USER)? 0x88:0xBB;
	//Encode password
	unsigned int idx;
	for (idx = 0; (password[idx] != 0) && (idx < sizeof(pw)); idx++)
		pw[idx] = password[idx] + encChar;
	for (; idx < MAX_PWLENGTH; idx++)
		pw[idx] = encChar;

	time_t now;
	time(&now);

	E_SMASPOT rc = E_OK;
	int validPcktID = 0;

	do	//while (validPcktID == 0);
	{
		do
		{
			pcktID++;
			writePacketHeader(pcktBuf, 0x01, addr_unknown);
			writePacket(pcktBuf, 0x0e, 0xa0, pcktID, 0x00, 0x01, 0x01);
			writeByte(pcktBuf, 0x80);
			writeLong(pcktBuf, 0xFFFD040C);
			writeLong(pcktBuf, userGroup);	// User / Installer
			writeLong(pcktBuf, 0x00000384); // Timeout = 900sec ?
			//			writeLong(pcktBuf, INV_PASSW);
			writeLong(pcktBuf, now);
			writeLong(pcktBuf, 0);
			writeArray(pcktBuf, pw, sizeof(pw));
			writePacketTrailer(pcktBuf);
			writePacketLength(pcktBuf);
		} while (!isCrcValid(pcktBuf[packetposition-3], pcktBuf[packetposition-2]));

		BT_Send(pcktBuf);

		if ((rc = getPacket(1)) != E_OK)
			return rc;

		if (!validateChecksum())
			return E_CHKSUM;
		else
		{
			//			if ((pcktID == pcktBuf[27]) && (get_long(pcktBuf + 41) == (long)INV_PASSW))
			if ((pcktID == pcktBuf[27]) && (get_long(pcktBuf + 41) == now))
			{
				validPcktID = 1;
				rc = (pcktBuf[24] == 0)	? E_OK : E_INVPASSW;
			}
			else
			{
				if (DEBUG_HIGHEST) puts("Unexpected response from inverter. Let's retry...");
				pcktID--;
			}
		}
	} while (validPcktID == 0);

	return rc;
}

E_SMASPOT logoffSMAInverter()
{
	if (DEBUG_NORMAL) puts("logoffSMAInverter()");
	do
	{
		pcktID++;
		writePacketHeader(pcktBuf, 0x01, addr_unknown);
		writePacket(pcktBuf, 0x08, 0xA0, pcktID, 0, 3, 3);
		writeByte(pcktBuf, 0x80);
		writeLong(pcktBuf, 0xFFFD010E);
		writeLong(pcktBuf, 0xFFFFFFFF);
		writePacketTrailer(pcktBuf);
		writePacketLength(pcktBuf);
	} while (!isCrcValid(pcktBuf[packetposition-3], pcktBuf[packetposition-2]));

	BT_Send(pcktBuf);

	return E_OK;
}

void DebugTN(char *msg)
{
	time_t localtime;
	time(&localtime);
	printf("%s: %s\n", strftime_t(DateTimeFormat, localtime), msg);
}

//Added V1.4.3 -> fixed V1.4.4
//Issue 12 - http://code.google.com/p/sma-spot/issues/detail?id=12
void SynchInverterTime()
{
	if (DEBUG_NORMAL) puts("SynchInverterTime()");

	//Local InverterData struct only to get current inverter time
	InverterData invData; memset(&invData, 0, sizeof(InverterData));

	//Energyproduction will give us the current invertertime
	int rc = getInverterData(&invData, EnergyProduction);

	if ((rc == 0) && (invData.InverterDatetime != 0))
	{
		time_t localtime;
		time(&localtime);
		long tzOffset = get_tzOffset();
		time_t timediff = localtime - invData.InverterDatetime;

		if (VERBOSE_NORMAL)
		{
			printf("Local PC Time: %s\n", strftime_t(DateTimeFormat, localtime));
			printf("Inverter Time: %s\n", strftime_t(DateTimeFormat, invData.InverterDatetime));
			printf("Time diff (s): %ld\n", timediff);
			printf("TZ offset (s): %ld\n", tzOffset);
		}

		//Set inverter time only if more than 60 sec difference
		//Each time change is logged as event
		if (abs(timediff) > 60)
		{
			if (VERBOSE_NORMAL) printf("Setting inverter time to %s\n", strftime_t(DateTimeFormat, localtime));
			do
			{
				pcktID++;
				writePacketHeader(pcktBuf, 0x01, smaBTInverterAddressArray);
				writePacket(pcktBuf, 0x10, 0xA0, pcktID, 0, 0, 0);
				writeByte(pcktBuf, 0x80);
				writeLong(pcktBuf, 0xF000020A);
				writeLong(pcktBuf, 0x00236D00);
				writeLong(pcktBuf, 0x00236D00);
				writeLong(pcktBuf, 0x00236D00);
				writeLong(pcktBuf, localtime);
				writeLong(pcktBuf, localtime);
				writeLong(pcktBuf, localtime);
				writeLong(pcktBuf, tzOffset);
				writeLong(pcktBuf, localtime);
				writeLong(pcktBuf, 1);
				writePacketTrailer(pcktBuf);
				writePacketLength(pcktBuf);
			} while (!isCrcValid(pcktBuf[packetposition-3], pcktBuf[packetposition-2]));

			BT_Send(pcktBuf);
		}
	}
}

#define MAX_CFG_AD 90
#define MAX_CFG_AM 60
int parseCmdline(int argc, char **argv, Config *cfg)
{
	cfg->debug = 0;			// debug level - 0=none, 5=highest
	cfg->verbose = 0;		// verbose level - 0=none, 5=highest
	cfg->archDays = 1;		// today only
	cfg->archMonths = 1;	// this month only
	cfg->upload = 0;		// upload to PVoutput and others (See config file)
	cfg->forceInq = 0;		// Inquire inverter also during the night
	cfg->userGroup = UG_USER;
	cfg->ACLoop = 0;
	cfg->ACLoopDelay = 250;

	//Build fullpath to config file (SMAspot.cfg should be in same folder as SMAspot.exe)
	strncpy(cfg->ConfigFile, argv[0], sizeof(cfg->ConfigFile));
	int len = strlen(cfg->ConfigFile);
	if (stricmp(&cfg->ConfigFile[len-4], ".exe") == 0)
		cfg->ConfigFile[len-4] = 0; //Cut extension
	strcat(cfg->ConfigFile, ".cfg");

	char *pEnd = NULL;
	long lValue = 0;

	printf("Commandline Args:");
	for (int i = 1; i < argc; i++)
		printf(" %s", argv[i]);
	printf("\n");

	for (int i = 1; i < argc; i++)
	{
		if (*argv[i] == '/')    // to please windows users ;-)
			*argv[i] = '-';

		//Set #days (archived daydata)
		if (strnicmp(argv[i], "-ad", 3) == 0)
		{
			if (strlen(argv[i]) > 5)
			{
				InvalidArg(argv[i]);
				return -1;
			}
			lValue = strtol(argv[i]+3, &pEnd, 10);
			if ((lValue < 0) || (lValue > MAX_CFG_AD) || (*pEnd != 0))
			{
				InvalidArg(argv[i]);
				return -1;
			}
			else
				cfg->archDays = (int)lValue;

		}

		//Set #months (archived monthdata)
		else if (strnicmp(argv[i], "-am", 3) == 0)
		{
			if (strlen(argv[i]) > 5)
			{
				InvalidArg(argv[i]);
				return -1;
			}
			lValue = strtol(argv[i]+3, &pEnd, 10);
			if ((lValue < 0) || (lValue > MAX_CFG_AM) || (*pEnd != 0))
			{
				InvalidArg(argv[i]);
				return -1;
			}
			else
				cfg->archMonths = (int)lValue;
		}

		//Set debug level
		else if(strnicmp(argv[i], "-d", 2) == 0)
		{
			lValue = strtol(argv[i]+2, &pEnd, 10);
			if (strlen(argv[i]) == 2) lValue = 2;	// only -d sets medium debug level
			if ((lValue < 0) || (lValue > 5) || (*pEnd != 0))
			{
				InvalidArg(argv[i]);
				return -1;
			}
			else
				cfg->debug = (int)lValue;
		}

		//Set verbose level
		else if (strnicmp(argv[i], "-v", 2) == 0)
		{
			lValue = strtol(argv[i]+2, &pEnd, 10);
			if (strlen(argv[i]) == 2) lValue = 2;	// only -v sets medium verbose level
			if ((lValue < 0) || (lValue > 5) || (*pEnd != 0))
			{
				InvalidArg(argv[i]);
				return -1;
			}
			else
				cfg->verbose = (int)lValue;
		}

		//Set upload flag
		else if (stricmp(argv[i], "-u") == 0)
			cfg->upload = 1;

		//Set inquiryDark flag
		else if (stricmp(argv[i], "-finq") == 0)
			cfg->forceInq= 1;

		//look for alternative config file
		else if (strnicmp(argv[i], "-cfg", 4) == 0)
		{
			if (strlen(argv[i]) == 4)
			{
				InvalidArg(argv[i]);
				return -1;
			}
			else
				strncpy(cfg->ConfigFile, argv[i]+4, sizeof(cfg->ConfigFile));
		}
		
		//Set AC Loop-Read Delay
		else if (strnicmp(argv[i], "-acd", 4) == 0)
		{
			DebugTN("-acd...");
			lValue = strtol(argv[i]+4, &pEnd, 10);
			if ((lValue < 0) || (*pEnd != 0))
			{
				InvalidArg(argv[i]);
				return -1;
			}
			else
				cfg->ACLoopDelay = (int)lValue;
		}

		//Set AC Loop-Read
		else if (strnicmp(argv[i], "-ac", 3) == 0)
		{
			lValue = strtol(argv[i]+3, &pEnd, 10);
			if ((lValue < 0) || (*pEnd != 0))
			{
				InvalidArg(argv[i]);
				return -1;
			}
			else
				cfg->ACLoop = (int)lValue;
		}

		//Scan for bluetooth devices
		else if (stricmp(argv[i], "-scan") == 0)
		{
#ifdef WIN32
			BT_SearchDevices();
#else
			puts("On LINUX systems, use hcitool scan");
#endif
			return 1;	// Caller should terminate, no error
		}

		//Show Help
		else if (stricmp(argv[i], "-?") == 0)
		{
			SayHello(1);
			return 1;	// Caller should terminate, no error
		}

		else
		{
			InvalidArg(argv[i]);
			return -1;
		}

	}

	return 0;
}

void InvalidArg(char *arg)
{
	printf("Invalid argument: %s\nUse -? for help\n", arg);
}

void SayHello(int ShowHelp)
{
	printf("SMAspot V%s\n", VERSION);
	puts("Yet another tool to read power production of SMA solar inverters");
	puts("(c) 2012-2013, SBF (http://code.google.com/p/sma-spot)\n");
	if (ShowHelp != 0)
	{
		puts(  "SMAspot [-scan] [-d#] [-v#] [-ad#] [-am#] [-cfgX.Y] [-u]");
		puts(  " -scan   Scan for bluetooth enabled SMA inverters.");
		puts(  " -d#     Set debug level: 0-5 (0=none, default=2)");
		puts(  " -v#     Set verbose output level: 0-5 (0=none, default=2)");
		printf(" -ad#    Set #days for archived daydata: 0-%d\n", MAX_CFG_AD);
		puts(  "         0=disabled, 1=today (default), ...");
		printf(" -am#    Set #months for archived monthdata: 0-%d\n", MAX_CFG_AM);
		puts(  "         0=disabled, 1=current month (default), ...");
		puts(  " -cfgX.Y Set alternative config file to X.Y (multiple inverters)");
		puts(  " -u      Upload to online monitoring system (see config file)");
		puts(  " -finq   Force Inquiry (Inquire inverter also during the night)");
	}
}

//month: January = 0, February = 1...
int DaysInMonth(int month, int year)
{
	const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

	if ((month < 0) || (month > 11)) return 0;  //Error - Invalid month
	// If febuary, check for leap year
	if ((month == 1) && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)))
		return 29;
	else
		return days[month];
}

/* read Config from file */
int GetConfig(Config *cfg)
{
	//Initialise config structure and set default values
	strncpy(cfg->prgVersion, VERSION, sizeof(cfg->prgVersion));
	cfg->BT_Address[0] = 0;
	cfg->outputPath[0] = 0;
	if (cfg->userGroup == UG_USER) cfg->SMA_Password[0] = 0;
	cfg->plantname[0] = 0;
	cfg->latitude = 0.0;
	cfg->longitude = 0.0;
	cfg->archdata_from = 0;
	cfg->archdata_to = 0;
	cfg->delimiter = ';';
	cfg->precision = 3;
	cfg->decimalpoint = ',';
	cfg->BT_Timeout = 5;
	cfg->BT_ConnectRetries = 10;

	cfg->calcMissingSpot = 0;
	strcpy(cfg->DateTimeFormat, "%d/%m/%Y %H:%M:%S");
	strcpy(cfg->DateFormat, "%d/%m/%Y");
	strcpy(cfg->TimeFormat, "%H:%M:%S");
	cfg->synchTime = 0;
	cfg->CSV_Export = 1;
	cfg->CSV_ExtendedHeader = 1;
	cfg->CSV_Header = 1;
	cfg->CSV_SaveZeroPower = 1;
	cfg->SunRSOffset = 900;
	cfg->PVoutput = 0;

	const char *CFG_Boolean = "(0-1)";
	const char *CFG_InvalidValue = "Invalid value for '%s' %s\n";

	FILE *fp;

	if ((fp = fopen(cfg->ConfigFile, "r")) == NULL)
	{
		printf("Error! Could not open file %s\n", cfg->ConfigFile);
		return -1;
	}

	char *pEnd = NULL;
	long lValue = 0;
	char line[200];
	int rc = 0;

	while ((rc == 0) && (fgets(line, sizeof(line), fp) != NULL))
	{
		if (line[0] != '#' && line[0] != 0 && line[0] != 10)
		{
			char *variable = strtok(line,"=");
			char *value = strtok(NULL,"=");

			if ((value != NULL) && (*rtrim(value) != 0))
			{
				if(strnicmp(variable, "BTaddress", 9) == 0) strncpy(cfg->BT_Address, value, sizeof(cfg->BT_Address));
				else if((cfg->userGroup == UG_USER) && (strnicmp(variable, "Password", 8) == 0)) strncpy(cfg->SMA_Password, value, sizeof(cfg->SMA_Password));
				else if(strnicmp(variable, "OutputPath", 10) == 0) strncpy(cfg->outputPath, value, sizeof(cfg->outputPath));
				else if(strnicmp(variable, "Latitude", 8) == 0) cfg->latitude = (float)atof(value);
				else if(strnicmp(variable, "Longitude", 9) == 0) cfg->longitude = (float)atof(value);
				else if(strnicmp(variable, "Plantname", 9) == 0) strncpy(cfg->plantname, value, sizeof(cfg->plantname));
				else if(strnicmp(variable, "CalculateMissingSpotValues", 26) == 0)
				{
					lValue = strtol(value, &pEnd, 10);
					if (((lValue == 0) || (lValue == 1)) && (*pEnd == 0))
						cfg->calcMissingSpot = (int)lValue;
					else
					{
						fprintf(stderr, CFG_InvalidValue, variable, CFG_Boolean);
						rc = -2;
					}
				}
				else if(strnicmp(variable, "DatetimeFormat", 14) == 0) strncpy(cfg->DateTimeFormat, value, sizeof(cfg->DateTimeFormat));
				else if(strnicmp(variable, "DateFormat", 10) == 0) strncpy(cfg->DateFormat, value, sizeof(cfg->DateFormat));
				else if(strnicmp(variable, "TimeFormat", 10) == 0) strncpy(cfg->TimeFormat, value, sizeof(cfg->TimeFormat));
				else if(strnicmp(variable, "DecimalPoint", 12) == 0)
				{
					if (strnicmp(value, "comma", 5) == 0) cfg->decimalpoint = ',';
					else if (strnicmp(value, "point", 5) == 0) cfg->decimalpoint = '.';
					else
					{
						fprintf(stderr, CFG_InvalidValue, variable, "(comma|point)");
						rc = -2;
					}
				}
				else if(strnicmp(variable, "CSV_Delimiter", 13) == 0)
				{
					if (strnicmp(value, "comma", 5) == 0) cfg->delimiter = ',';
					else if (strnicmp(value, "semicolon", 9) == 0) cfg->delimiter = ';';
					else
					{
						fprintf(stderr, CFG_InvalidValue, variable, "(comma|semicolon)");
						rc = -2;
					}
				}
				else if(strnicmp(variable, "SynchTime", 9) == 0)
				{
					lValue = strtol(value, &pEnd, 10);
					if (((lValue == 0) || (lValue == 1)) && (*pEnd == 0))
						cfg->synchTime = (int)lValue;
					else
					{
						fprintf(stderr, CFG_InvalidValue, variable, CFG_Boolean);
						rc = -2;
					}
				}
				else if(strnicmp(variable, "CSV_Export", 10) == 0)
				{
					lValue = strtol(value, &pEnd, 10);
					if (((lValue == 0) || (lValue == 1)) && (*pEnd == 0))
						cfg->CSV_Export = (int)lValue;
					else
					{
						fprintf(stderr, CFG_InvalidValue, variable, CFG_Boolean);
						rc = -2;
					}
				}
				else if(strnicmp(variable, "CSV_ExtendedHeader", 18) == 0)
				{
					lValue = strtol(value, &pEnd, 10);
					if (((lValue == 0) || (lValue == 1)) && (*pEnd == 0))
						cfg->CSV_ExtendedHeader = (int)lValue;
					else
					{
						fprintf(stderr, CFG_InvalidValue, variable, CFG_Boolean);
						rc = -2;
					}
				}
				else if(strnicmp(variable, "CSV_Header", 10) == 0)
				{
					lValue = strtol(value, &pEnd, 10);
					if (((lValue == 0) || (lValue == 1)) && (*pEnd == 0))
						cfg->CSV_Header = (int)lValue;
					else
					{
						fprintf(stderr, CFG_InvalidValue, variable, CFG_Boolean);
						rc = -2;
					}
				}
				else if(strnicmp(variable, "CSV_SaveZeroPower", 17) == 0)
				{
					lValue = strtol(value, &pEnd, 10);
					if (((lValue == 0) || (lValue == 1)) && (*pEnd == 0))
						cfg->CSV_SaveZeroPower = (int)lValue;
					else
					{
						fprintf(stderr, CFG_InvalidValue, variable, CFG_Boolean);
						rc = -2;
					}
				}
				else if(strnicmp(variable, "PVoutput_SID", 12) == 0)
				{
					lValue = strtol(value, &pEnd, 10);
					if ((lValue > 0) && (*pEnd == 0))
						cfg->PVoutput_SID = (int)lValue;
					else
					{
						fprintf(stderr, CFG_InvalidValue, variable, "(>0)");
						rc = -2;
					}
				}
				else if(strnicmp(variable, "PVoutput_Key", 12) == 0)
					strncpy(cfg->PVoutput_Key, value, sizeof(cfg->PVoutput_Key));

				// Order is important! PVoutput must be last
				else if(strnicmp(variable, "PVoutput", 8) == 0)
				{
					lValue = strtol(value, &pEnd, 10);
					if (((lValue == 0) || (lValue == 1)) && (*pEnd == 0))
						cfg->PVoutput = (int)lValue;
					else
					{
						fprintf(stderr, CFG_InvalidValue, variable, CFG_Boolean);
						rc = -2;
					}
				}
				else if(strnicmp(variable, "SunRSOffset", 11) == 0)
				{
					lValue = strtol(value, &pEnd, 10);
					if ((lValue >= 0) && (lValue <= 3600) && (*pEnd == 0))
						cfg->SunRSOffset = (int)lValue;
					else
					{
						fprintf(stderr, CFG_InvalidValue, variable, "(0-3600)");
						rc = -2;
					}
				}

			}
		}
	}
	fclose(fp);

	//Some basic checks must be done
	if (strlen(cfg->BT_Address) == 0)
	{
		fprintf(stderr, "Missing Bluetooth Address.\n");
		rc = -2;
	}

	if (strlen(cfg->SMA_Password) == 0)
	{
		fprintf(stderr, "Missing USER Password.\n");
		rc = -2;
	}

	if (cfg->decimalpoint == cfg->delimiter)
	{
		fprintf(stderr, "'CSV_Delimiter' and 'DecimalPoint' must be different character.\n");
		rc = -2;
	}

	//Silently enable CSV_Header when CSV_ExtendedHeader is enabled
	if (cfg->CSV_ExtendedHeader == 1)
		cfg ->CSV_Header = 1;

	if (strlen(cfg->outputPath) == 0)
	{
#ifdef WIN32
		strncpy(cfg->outputPath, "C:\\Temp\\sma", sizeof(cfg->outputPath));
#else
		strncpy(cfg->outputPath, "/home/sbf/Documents/sma", sizeof(cfg->outputPath));
#endif
	}

	if (strlen(cfg->plantname) == 0)
	{
		strncpy(cfg->plantname, "MyPlant", sizeof(cfg->plantname));
	}

	return rc;
}

int isCrcValid(unsigned char lb, unsigned char hb)
{
	if ((lb == 0x7E) || (hb == 0x7E) ||
		(lb == 0x7D) || (hb == 0x7D))
		return 0;
	else
		return 1;
}

//Power Values are missing on some inverters
void CalcMissingSpot(InverterData *invData)
{
  if ((invData->flags & SpotDCPower) && (invData->flags & SpotDCVoltage))
	{
		if (invData->Pdc1 == 0) invData->Pdc1 = invData->Idc1 * invData->Udc1;
		if (invData->Pdc2 == 0) invData->Pdc2 = invData->Idc2 * invData->Udc2;
	}

	if ((invData->flags & SpotACPower) && (invData->flags & SpotACVoltage))
	{
		if (invData->Pac1 == 0) invData->Pac1 = invData->Iac1 * invData->Uac1;
		if (invData->Pac2 == 0) invData->Pac2 = invData->Iac2 * invData->Uac2;
		if (invData->Pac3 == 0) invData->Pac3 = invData->Iac3 * invData->Uac3;
	}

	if ((invData->flags & SpotACPower) && (invData->flags & SpotACTotalPower))
	{
		/*if (invData->TotalPac == 0)*/ invData->TotalPac = invData->Pac1 + invData->Pac2 + invData->Pac3;
	}
}

/*
* isValidSender() compares 6-byte senderaddress with our inverter BT address
*/
int isValidSender(unsigned char address[])
{
	for (int i = 0; i < 6; i++)
		if (smaBTInverterAddressArray[i] != address[i])
			return 0;

	return 1;
}

int getInverterData(InverterData *invData, enum getInverterDataType type, int count)
{
	int rc = 0;
	for (int retries = 0; retries < count; retries++)
	{
		if ((rc = getInverterData(invData, type)) != 0)
			printf("getInverterData returned an error: %d\n", rc);
		else
			return 0;
	}
	return rc;
}

int getInverterData(InverterData *invData, enum getInverterDataType type)
{
	if (DEBUG_NORMAL) printf("getInverterData(%d)\n", type);
	const char *strWatt = "%-12s: %ld (W) %s";
	const char *strVolt = "%-12s: %.2f (V) %s";
	const char *strAmp = "%-12s: %.3f (A) %s";
	const char *strkWh = "%-12s: %.3f (kWh) %s";
	const char *strHour = "%-12s: %.3f (h) %s";

	int rc = E_OK;

	BT_Clear();

	int recordsize = 0;
	int validPcktID = 0;

	unsigned char code1;
	unsigned char code2;
	unsigned long code3;
	unsigned long first;
	unsigned long last;

	switch(type)
	{
	case EnergyProduction:
		// SPOT_ETODAY, SPOT_ETOTAL
		code1 = 0xA0;
		code2 = 0x80;
		code3 = 0x54000200;
		first = 0x00260100;
		last = 0x002622FF;
		break;

	case SpotDCPower:
		// SPOT_PDC1, SPOT_PDC2
		code1 = 0xA0;
		code2 = 0x83;
		code3 = 0x53800200;
		first = 0x00251E00;
		last = 0x00251EFF;
		break;

	case SpotDCVoltage:
		// SPOT_UDC1, SPOT_UDC2, SPOT_IDC1, SPOT_IDC2
		code1 = 0xA0;
		code2 = 0x83;
		code3 = 0x53800200;
		first = 0x00451F00;
		last = 0x004521FF;
		break;

	case SpotACPower:
		// SPOT_PAC1, SPOT_PAC2, SPOT_PAC3
		code1 = 0xA1;
		code2 = 0x80;
		code3 = 0x51000200;
		first = 0x00464000;
		last = 0x004642FF;
		break;

	case SpotACVoltage:
		// SPOT_UAC1, SPOT_UAC2, SPOT_UAC3, SPOT_IAC1, SPOT_IAC2, SPOT_IAC3
		code1 = 0xA1;
		code2 = 0x80;
		code3 = 0x51000200;
		first = 0x00464800;
		last = 0x004652FF;
		break;

	case SpotGridFrequency:
		// SPOT_FREQ
		code1 = 0xA1;
		code2 = 0x80;
		code3 = 0x51000200;
		first = 0x00465700;
		last = 0x004657FF;
		break;

	case MaxACPower:
		// INV_PACMAX1, INV_PACMAX2, INV_PACMAX3
		code1 = 0xA0;
		code2 = 0x80;
		code3 = 0x51000200;
		first = 0x00411E00;
		last = 0x004120FF;
		break;

	case MaxACPower2:
		// INV_PACMAX1_2
		code1 = 0xA0;
		code2 = 0x80;
		code3 = 0x51000200;
		first = 0x00832A00;
		last = 0x00832AFF;
		break;

	case SpotACTotalPower:
		// SPOT_PACTOT
		code1 = 0xA1;
		code2 = 0x80;
		code3 = 0x51000200;
		first = 0x00263F00;
		last = 0x00263FFF;
		break;

	case TypeLabel:
		// INV_NAME, INV_TYPE, INV_CLASS
		code1 = 0xA0;
		code2 = 0x80;
		code3 = 0x58000200;
		first = 0x00821E00;
		last = 0x008220FF;
		break;

	case SoftwareVersion:
		// INV_SWVERSION
		code1 = 0xA0;
		code2 = 0x80;
		code3 = 0x58000200;
		first = 0x00823400;
		last = 0x008234FF;
		break;

	case DeviceStatus:
		// INV_SWVERSION
		code1 = 0xA0;
		code2 = 0x80;
		code3 = 0x51800200;
		first = 0x00214800;
		last = 0x002148FF;
		break;

	case OperationTime:
		// SPOT_OPERTM, SPOT_FEEDTM
		code1 = 0xA0;
		code2 = 0x80;
		code3 = 0x54000200;
		first = 0x00462E00;
		last = 0x00462FFF;
		break;

	default:
		return E_BADARG;
	};

	do
	{
		validPcktID = 0;
		do
		{
			pcktID++;
			writePacketHeader(pcktBuf, 0x01, smaBTInverterAddressArray);
			writePacket(pcktBuf, 0x09, code1, pcktID, 0, 0, 0);
			writeByte(pcktBuf, code2);
			writeLong(pcktBuf, code3);
			writeLong(pcktBuf, first);
			writeLong(pcktBuf, last);
			writePacketTrailer(pcktBuf);
			writePacketLength(pcktBuf);
		} while (!isCrcValid(pcktBuf[packetposition-3], pcktBuf[packetposition-2]));

		BT_Send(pcktBuf);

		do
		{
			if ((rc = getPacket(1)) != E_OK)
				return rc;

			if (!validateChecksum())
				return E_CHKSUM;
			else
			{
				if (pcktID == pcktBuf[27])
				{
					validPcktID = 1;
					long value = 0;
					long long value64 = 0;
					unsigned char Vtype = 0;
					unsigned char Vbuild = 0;
					unsigned char Vminor = 0;
					unsigned char Vmajor = 0;
					for(int i = 41; i < packetposition - 4; i += recordsize)
					{
						unsigned long code = ((unsigned long)get_long(pcktBuf + i)) & 0x00FFFFFF;
						//unsigned char dataType = pcktBuf[i + 3];
						time_t datetime = (time_t)get_long(pcktBuf + i + 4);
						if (code != INV_NAME)
						{
							if ((type == EnergyProduction) || (type == OperationTime))
							{
								value64 = get_longlong(pcktBuf + i + 8);
								if ((value64 == (long long)NULLVALUE64) || (value64 == (long long)NOVALUE64)) value64 = 0;
							}
							else
							{
								value = (unsigned long)get_long(pcktBuf + i + 8);
								if ((value == (long)NULLVALUE) || (value == (long)NOVALUE)) value = 0;
							}
						}
						switch (code)
						{
						case SPOT_PACTOT:
							if (recordsize == 0) recordsize = 28;
							//This function gives us the time when the inverter was switched off
							invData->SleepTime = datetime;
							invData->TotalPac = value;
							invData->flags |= type;
							if (DEBUG_NORMAL) printf(strWatt, "SPOT_PACTOT", value, ctime(&datetime));
							break;

						case INV_PACMAX1:
							if (recordsize == 0) recordsize = 28;
							invData->Pmax1 = value;
							invData->flags |= type;
							if (DEBUG_NORMAL) printf(strWatt, "INV_PACMAX1", value, ctime(&datetime));
							break;

						case INV_PACMAX2:
							if (recordsize == 0) recordsize = 28;
							invData->Pmax2 = value;
							invData->flags |= type;
							if (DEBUG_NORMAL) printf(strWatt, "INV_PACMAX2", value, ctime(&datetime));
							break;

						case INV_PACMAX3:
							if (recordsize == 0) recordsize = 28;
							invData->Pmax3 = value;
							invData->flags |= type;
							if (DEBUG_NORMAL) printf(strWatt, "INV_PACMAX3", value, ctime(&datetime));
							break;

						case SPOT_PAC1:
							if (recordsize == 0) recordsize = 28;
							invData->Pac1 = value;
							invData->flags |= type;
							if (DEBUG_NORMAL) printf(strWatt, "SPOT_PAC1", value, ctime(&datetime));
							break;

						case SPOT_PAC2:
							if (recordsize == 0) recordsize = 28;
							invData->Pac2 = value;
							invData->flags |= type;
							if (DEBUG_NORMAL) printf(strWatt, "SPOT_PAC2", value, ctime(&datetime));
							break;

						case SPOT_PAC3:
							if (recordsize == 0) recordsize = 28;
							invData->Pac3 = value;
							invData->flags |= type;
							if (DEBUG_NORMAL) printf(strWatt, "SPOT_PAC3", value, ctime(&datetime));
							break;

						case SPOT_UAC1:
							if (recordsize == 0) recordsize = 28;
							invData->Uac1 = value;
							invData->flags |= type;
							if (DEBUG_NORMAL) printf(strVolt, "SPOT_UAC1", toVolt(value), ctime(&datetime));
							break;

						case SPOT_UAC2:
							if (recordsize == 0) recordsize = 28;
							invData->Uac2 = value;
							invData->flags |= type;
							if (DEBUG_NORMAL) printf(strVolt, "SPOT_UAC2", toVolt(value), ctime(&datetime));
							break;

						case SPOT_UAC3:
							if (recordsize == 0) recordsize = 28;
							invData->Uac3 = value;
							invData->flags |= type;
							if (DEBUG_NORMAL) printf(strVolt, "SPOT_UAC3", toVolt(value), ctime(&datetime));
							break;

						case SPOT_IAC1:
							if (recordsize == 0) recordsize = 28;
							invData->Iac1 = value;
							invData->flags |= type;
							if (DEBUG_NORMAL) printf(strAmp, "SPOT_IAC1", toAmp(value), ctime(&datetime));
							break;

						case SPOT_IAC2:
							if (recordsize == 0) recordsize = 28;
							invData->Iac2 = value;
							invData->flags |= type;
							if (DEBUG_NORMAL) printf(strAmp, "SPOT_IAC2", toAmp(value), ctime(&datetime));
							break;

						case SPOT_IAC3:
							if (recordsize == 0) recordsize = 28;
							invData->Iac3 = value;
							invData->flags |= type;
							if (DEBUG_NORMAL) printf(strAmp, "SPOT_IAC3", toAmp(value), ctime(&datetime));
							break;

						case SPOT_FREQ:
							if (recordsize == 0) recordsize = 28;
							invData->GridFreq = value;
							invData->flags |= type;
							if (DEBUG_NORMAL) printf("%-12s: %.2f (Hz) %s", "SPOT_FREQ", toHz(value), ctime(&datetime));
							break;

						case SPOT_PDC1:
							if (recordsize == 0) recordsize = 28;
							invData->Pdc1 = value;
							invData->flags |= type;
							if (DEBUG_NORMAL) printf(strWatt, "SPOT_PDC1", value, ctime(&datetime));
							break;

						case SPOT_PDC2:
							if (recordsize == 0) recordsize = 28;
							invData->Pdc2 = value;
							invData->flags |= type;
							if (DEBUG_NORMAL) printf(strWatt, "SPOT_PDC2", value, ctime(&datetime));
							break;

						case SPOT_UDC1:
							if (recordsize == 0) recordsize = 28;
							invData->Udc1 = value;
							invData->flags |= type;
							if (DEBUG_NORMAL) printf(strVolt, "SPOT_UDC1", toVolt(value), ctime(&datetime));
							break;

						case SPOT_UDC2:
							if (recordsize == 0) recordsize = 28;
							invData->Udc2 = value;
							invData->flags |= type;
							if (DEBUG_NORMAL) printf(strVolt, "SPOT_UDC2", toVolt(value), ctime(&datetime));
							break;

						case SPOT_IDC1:
							if (recordsize == 0) recordsize = 28;
							invData->Idc1 = value;
							invData->flags |= type;
							if (DEBUG_NORMAL) printf(strAmp, "SPOT_IDC1", toAmp(value), ctime(&datetime));
							break;

						case SPOT_IDC2:
							if (recordsize == 0) recordsize = 28;
							invData->Idc2 = value;
							invData->flags |= type;
							if (DEBUG_NORMAL) printf(strAmp, "SPOT_IDC1", toAmp(value), ctime(&datetime));
							break;

						case SPOT_ETOTAL:
							if (recordsize == 0) recordsize = 16;
							invData->ETotal = value64;
							invData->flags |= type;
							if (DEBUG_NORMAL) printf(strkWh, "SPOT_ETOTAL", tokWh(value64), ctime(&datetime));
							break;

						case SPOT_ETODAY:
							if (recordsize == 0) recordsize = 16;
							//This function gives us the current inverter time
							invData->InverterDatetime = datetime;
							invData->EToday = value64;
							invData->flags |= type;
							if (DEBUG_NORMAL) printf(strkWh, "SPOT_ETODAY", tokWh(value64), ctime(&datetime));
							break;

						case SPOT_OPERTM:
							if (recordsize == 0) recordsize = 16;
							invData->OperationTime = value64;
							invData->flags |= type;
							if (DEBUG_NORMAL) printf(strHour, "SPOT_OPERTM", toHour(value64), ctime(&datetime));
							break;

						case SPOT_FEEDTM:
							if (recordsize == 0) recordsize = 16;
							invData->FeedInTime = value64;
							invData->flags |= type;
							if (DEBUG_NORMAL) printf(strHour, "SPOT_FEEDTM", toHour(value64), ctime(&datetime));
							break;

						case INV_NAME:
							if (recordsize == 0) recordsize = 40;
							//This function gives us the time when the inverter was switched on
							invData->WakeupTime = datetime;
							strncpy(invData->DeviceName, (char *)pcktBuf + i + 8, sizeof(invData->DeviceName)-1);
							invData->flags |= type;
							if (DEBUG_NORMAL) printf("%-12s: '%s' %s", "INV_NAME", invData->DeviceName, ctime(&datetime));
							break;

						case INV_SWVER:	// Thanks to Wim Simons
							if (recordsize == 0) recordsize = 40;
							Vtype = pcktBuf[i + 24];
							char ReleaseType[4];
							if (Vtype > 5)
								sprintf(ReleaseType, "%d", Vtype);
							else
								sprintf(ReleaseType, "%c", "NEABRS"[Vtype]); //NOREV-EXPERIMENTAL-ALPHA-BETA-RELEASE-SPECIAL
							Vbuild = pcktBuf[i + 25];
							Vminor = pcktBuf[i + 26];
							Vmajor = pcktBuf[i + 27];
							//Vmajor and Vminor = 0x12 should be printed as '12' and not '18' (BCD)
							snprintf(invData->SWVersion, sizeof(invData->SWVersion), "%c%c.%c%c.%02d.%s", '0'+(Vmajor >> 4), '0'+(Vmajor & 0x0F), '0'+(Vminor >> 4), '0'+(Vminor & 0x0F), Vbuild, ReleaseType);
							invData->flags |= type;
							if (DEBUG_NORMAL) printf("%-12s: '%s' %s", "INV_SWVER", invData->SWVersion, ctime(&datetime));
							break;

						case INV_TYPE: // Thanks to Gerd Schnuff
							if (recordsize == 0) recordsize = 40;
							for (int idx = 8; idx < recordsize; idx += 4)
							{
								unsigned long attribute = ((unsigned long)get_long(pcktBuf + i + idx)) & 0x00FFFFFF;
								unsigned char status = pcktBuf[i + idx + 3];
								if (attribute == 0xFFFFFE) break;	//End of attributes
								if (status == 1)
								{
									switch (attribute)
									{
									case 0x233F: /*9023*/ strncpy(invData->DeviceType, "SB2500", sizeof(invData->DeviceType)); break;
									case 0x2342: /*9026*/ strncpy(invData->DeviceType, "SB3000", sizeof(invData->DeviceType)); break;
									case 0x2344: /*9028*/ strncpy(invData->DeviceType, "SB3300", sizeof(invData->DeviceType)); break;

									case 0x2395: /*9109*/ strncpy(invData->DeviceType, "SB1600TL", sizeof(invData->DeviceType)); break;
									case 0x233E: /*9022*/ strncpy(invData->DeviceType, "SB2100TL", sizeof(invData->DeviceType)); break;
									case 0x23C8: /*9160*/ strncpy(invData->DeviceType, "SB3600TL", sizeof(invData->DeviceType)); break;

									case 0x17DD: /*6109*/ strncpy(invData->DeviceType, "SB1600TL-10", sizeof(invData->DeviceType)); break;

									case 0x022E: /* 558*/ strncpy(invData->DeviceType, "SB3000TL-20", sizeof(invData->DeviceType)); break;
									case 0x0166: /* 358*/ strncpy(invData->DeviceType, "SB4000TL-20", sizeof(invData->DeviceType)); break;
									case 0x0167: /* 359*/ strncpy(invData->DeviceType, "SB5000TL-20", sizeof(invData->DeviceType)); break;

									case 0x2372: /*9074*/ strncpy(invData->DeviceType, "SB3000TL-21", sizeof(invData->DeviceType)); break;
									case 0x23CD: /*9165*/ strncpy(invData->DeviceType, "SB3600TL-21", sizeof(invData->DeviceType)); break;
									case 0x2373: /*9075*/ strncpy(invData->DeviceType, "SB4000TL-21", sizeof(invData->DeviceType)); break;
									case 0x2374: /*9076*/ strncpy(invData->DeviceType, "SB5000TL-21", sizeof(invData->DeviceType)); break;

										//Single Tracker
									case 0x23E0: /*9184*/ strncpy(invData->DeviceType, "SB2500TLST-21", sizeof(invData->DeviceType)); break;
									case 0x23E1: /*9185*/ strncpy(invData->DeviceType, "SB3000TLST-21", sizeof(invData->DeviceType)); break;

									case 0x2347: /*9031*/ strncpy(invData->DeviceType, "SB3300TL HC", sizeof(invData->DeviceType)); break;

									case 0x236F: /*9071*/ strncpy(invData->DeviceType, "SB2000HF-30", sizeof(invData->DeviceType)); break;
									case 0x2370: /*9072*/ strncpy(invData->DeviceType, "SB2500HF-30", sizeof(invData->DeviceType)); break;
									case 0x2371: /*9073*/ strncpy(invData->DeviceType, "SB3000HF-30", sizeof(invData->DeviceType)); break;

										//MiniCentral
									case 0x2353: /*9043*/ strncpy(invData->DeviceType, "SMC5000A", sizeof(invData->DeviceType)); break;
									case 0x2356: /*9046*/ strncpy(invData->DeviceType, "SMC6000A", sizeof(invData->DeviceType)); break;

									case 0x238D: /*9101*/ strncpy(invData->DeviceType, "STP8000TL-10", sizeof(invData->DeviceType)); break;
									case 0x236B: /*9067*/ strncpy(invData->DeviceType, "STP10000TL-10", sizeof(invData->DeviceType)); break;
									case 0x236C: /*9068*/ strncpy(invData->DeviceType, "STP12000TL-10", sizeof(invData->DeviceType)); break;
									case 0x236D: /*9069*/ strncpy(invData->DeviceType, "STP15000TL-10", sizeof(invData->DeviceType)); break;
									case 0x236E: /*9070*/ strncpy(invData->DeviceType, "STP17000TL-10", sizeof(invData->DeviceType)); break;

										//TriPower High Efficiency
									case 0x23B4: /*9140*/ strncpy(invData->DeviceType, "STP15000TLHE-10", sizeof(invData->DeviceType)); break;
									case 0x23B3: /*9139*/ strncpy(invData->DeviceType, "STP20000TLHE-10", sizeof(invData->DeviceType)); break;

										//Tripower Economic Excellence
									case 0x23DE: /*9182*/ strncpy(invData->DeviceType, "STP15000TLEE-10", sizeof(invData->DeviceType)); break;
									case 0x23DD: /*9181*/ strncpy(invData->DeviceType, "STP20000TLEE-10", sizeof(invData->DeviceType)); break;

										//US Versions
									case 0x234A: /*9034*/ strncpy(invData->DeviceType, "SB4000US", sizeof(invData->DeviceType)); break;
									case 0x2354: /*9044*/ strncpy(invData->DeviceType, "SB5000US", sizeof(invData->DeviceType)); break;
									case 0x2357: /*9047*/ strncpy(invData->DeviceType, "SB6000US", sizeof(invData->DeviceType)); break;
									case 0x235D: /*9053*/ strncpy(invData->DeviceType, "SB7000US", sizeof(invData->DeviceType)); break;
									case 0x237B: /*9083*/ strncpy(invData->DeviceType, "SB8000US", sizeof(invData->DeviceType)); break;

									case 0x23C1: /*9153*/ strncpy(invData->DeviceType, "SB6000TL-US", sizeof(invData->DeviceType)); break;
									case 0x23C0: /*9152*/ strncpy(invData->DeviceType, "SB7000TL-US", sizeof(invData->DeviceType)); break;
									case 0x23BC: /*9148*/ strncpy(invData->DeviceType, "SB8000TL-US", sizeof(invData->DeviceType)); break;
									case 0x23BD: /*9149*/ strncpy(invData->DeviceType, "SB9000TL-US", sizeof(invData->DeviceType)); break;
									case 0x23BE: /*9150*/ strncpy(invData->DeviceType, "SB10000TL-US", sizeof(invData->DeviceType)); break;
									case 0x23BF: /*9151*/ strncpy(invData->DeviceType, "SB11000TL-US", sizeof(invData->DeviceType)); break;

									default:
										strncpy(invData->DeviceType, "UNKNOWN TYPE", sizeof(invData->DeviceType));
										puts("Unknown Inverter Type. Report this issue at http://code.google.com/p/sma-spot/");
										printf("with following info: 0x%08lX and Inverter Type=...\n", attribute);
									}
								}
							}
							invData->flags |= type;
							if (DEBUG_NORMAL) printf("%-12s: '%s' %s", "INV_TYPE", invData->DeviceType, ctime(&datetime));
							break;

						case INV_CLASS: // Thanks to Gerd Schnuff
							if (recordsize == 0) recordsize = 40;
							for (int idx = 8; idx < recordsize; idx += 4)
							{
								unsigned long attribute = ((unsigned long)get_long(pcktBuf + i + idx)) & 0x00FFFFFF;
								unsigned char attValue = pcktBuf[i + idx + 3];
								if (attribute == 0xFFFFFE) break;	//End of attributes
								if (attValue == 1)
								{
									switch (attribute)
									{
									case 0x1F41: strncpy(invData->DeviceClass, "Solar Inverters", sizeof(invData->DeviceClass)); break;
									default:
										strncpy(invData->DeviceType, "UNKNOWN CLASS", sizeof(invData->DeviceType));
										printf("Unknown Inverter Type. Report this issue at http://code.google.com/p/sma-spot/ with following info:\n");
										printf("0x%08lX and Device Class=...\n", attribute);
									}
								}
							}
							invData->flags |= type;
							if (DEBUG_NORMAL) printf("%-12s: '%s' %s", "INV_CLASS", invData->DeviceClass, ctime(&datetime));
							break;

						case INV_STATUS: // Thanks to Gerd Schnuff
							if (recordsize == 0) recordsize = 40;
							for (int idx = 8; idx < recordsize; idx += 4)
							{
								unsigned long attribute = ((unsigned long)get_long(pcktBuf + i + idx)) & 0x00FFFFFF;
								unsigned char attValue = pcktBuf[i + idx + 3];
								if (attribute == 0xFFFFFE) break;	//End of attributes
								if (attValue == 1)
								{
									invData->DeviceStatus = attribute;
								}
							}
							invData->flags |= type;
							if (DEBUG_NORMAL) printf("%-12s: '%s' %s", "INV_STATUS", AttributeToText(invData->DeviceStatus), ctime(&datetime));
							break;

						case 0x00414901: //0xA1 0x80 0x51800200 0x00000000 0x00FFFFFF
						case 0x00414A01: //0xA1 0x80 0x51800200 0x00000000 0x00FFFFFF
						case 0x00414B01: //0xA1 0x80 0x51800200 0x00000000 0x00FFFFFF
						case 0x00416401: //0xA1 0x80 0x51800200 0x00000000 0x00FFFFFF
						case 0x00416501: //0xA1 0x80 0x51800200 0x00000000 0x00FFFFFF
						case 0x00671E01: //0xA1 0x80 0x51800200 0x00000000 0x00FFFFFF
						case 0x00822101: //0xA0 0x80 0x58000200 0x00821E00 0x008234FF;
							if (recordsize == 0) recordsize = 40;
							if (DEBUG_HIGH)
							{
								printHexBytes(pcktBuf+i, 4);
								printf("       type: 08-attr  fnt:  <%02X %02X %02X> ???\n", pcktBuf[i+2], pcktBuf[i+1], pcktBuf[i]);
								printHexBytes(pcktBuf+i+4, 4);
								printf("       time: %s", ctime(&datetime));
							}

							for (int idx = 8; idx < recordsize; idx += 4)
							{
								unsigned long attribute = ((unsigned long)get_long(pcktBuf + i + idx)) & 0x00FFFFFF;
								unsigned char attValue = pcktBuf[i + idx + 3];
								if (attribute == 0xFFFFFD)
								{
									if (DEBUG_HIGH)
									{
										printHexBytes(pcktBuf+i+idx, 4);
										printf("       attr:     ???\n");
									}
									break;
								}
								if (attribute == 0xFFFFFE)
								{
									if (DEBUG_HIGH)
									{
										printHexBytes(pcktBuf+i+idx, 4);
										printf("       attr:     END\n");
									}
									break;
								}
								if (DEBUG_HIGH)
								{
									printHexBytes(pcktBuf+i+idx, 4);
									printf("       attr:   %5lu    %s\n", attribute, attValue == 1? "YES":"NO");
								}
							}
							break;

						default:
							switch(code)
							{
								//cmd:51000200
							case 0x00416601:
							case 0x00417F01:
								//case 0x00665F01: see cmd:52000200
								//case 0x00666701: see cmd:52000200
								//cmd:52000200
							case 0x00237701:
							case 0x00267501:
							case 0x00458A01:
							case 0x00458B01:
							case 0x00467B01:
							case 0x00467C01:
							case 0x00467D01:
							case 0x00468301:
							case 0x00468401:
							case 0x00468501:
							case 0x00468B01:
							case 0x00665F01:
							case 0x00666701:
								//cmd:53000200
							case 0x00256601:
							case 0x00256602:
							case 0x00456701:
							case 0x00456702:
							case 0x00456801:
							case 0x00456802:
								if (recordsize == 0) recordsize = 28;
								break;

								//cmd:54000200
							case 0x00618C01:
								if (recordsize == 0) recordsize = 16;
								break;
							default:
								if (recordsize == 0) recordsize = 12;
							}
							if (DEBUG_NORMAL) printf("%-12s: %08lX=%08lX (%lu) %s", "UNKNOWN", code, value, value, ctime(&datetime));
						} //switch (code)
					} //for(int i = 41; i < packetposition - 4; i += recordsize)
				} //if (pcktID == pcktBuf[27])
				else
				{
					if (DEBUG_HIGHEST) printf("Packet ID mismatch. Expected %d, received %d\n", pcktID, pcktBuf[27]);
					pcktID--;
				}
			}
		}
		while (pcktBuf[25] != 0);	//moreRecords?
	} while (validPcktID == 0);

	return E_OK;
}

const char *AttributeToText(int attribute)
{
	switch (attribute)
	{
	case 0x0023: return "Fault";
	case 0x012f: return "Off";
	case 0x0133: return "OK";
	case 0x0134: return "On";
	case 0x01c7: return "Warning";
	default: return "?";
	}
}


void printHexBytes(BYTE *buf, int num)
{
	for(int idx=0; idx<num; idx++)
		printf("%02X ", buf[idx]);
}
