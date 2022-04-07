/*
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

#include "CSVexport.h"

char *FormatFloat(char *str, float value, int width, int precision, char decimalpoint)
{
	sprintf(str, "%*.*f", width, precision, value);
	char *dppos = strrchr(str, '.');
	if (dppos != NULL) *dppos = decimalpoint;
	return str;
}

char *FormatDouble(char *str, double value, int width, int precision, char decimalpoint)
{
	sprintf(str, "%*.*f", width, precision, value);
	char *dppos = strrchr(str, '.');
	if (dppos != NULL) *dppos = decimalpoint;
	return str;
}

int ExportMonthDataToCSV(Config *cfg, InverterData *invdata, MonthData mData[], int arraysize)
{
	if (cfg->CSV_Export == 1)
	{
		if (VERBOSE_NORMAL) puts("ExportMonthDataToCSV()");

		if (mData[0].datetime <= 0)	//invalid date?
			puts("There is no data to export!"); //First day of the month?
		else
		{
			FILE *csv;

			//Expand date specifiers in config::outputPath
			char exportPath[MAX_PATH];
			snprintf(exportPath, sizeof(exportPath), "%s", strftime_t(cfg->outputPath, mData[0].datetime));
			CreatePath(exportPath);

			char csvpath[MAX_PATH];
			sprintf(csvpath, "%s/%s-%s.csv", exportPath, cfg->plantname, strfgmtime_t("%Y%m", mData[0].datetime));

			if ((csv = fopen(csvpath, "w+")) == NULL)
			{
				fprintf(stderr, "Unable to open output file %s\n", csvpath);
				return -1;
			}
			else
			{
				if (cfg->CSV_ExtendedHeader == 1)
				{
					fprintf(csv, "sep=%c\n", cfg->delimiter);
					fprintf(csv, "Version CSV1|Tool SMAspot%s|Linebreaks CR/LF|Delimiter %s|Decimalpoint %s|Precision %d\n\n", cfg->prgVersion, delim2txt(cfg->delimiter), dp2txt(cfg->decimalpoint), cfg->precision);
					fprintf(csv, "%c%s%c%s\n", cfg->delimiter, invdata->DeviceName, cfg->delimiter, invdata->DeviceName);
					fprintf(csv, "%c%s%c%s\n", cfg->delimiter, invdata->DeviceType, cfg->delimiter, invdata->DeviceType);
					fprintf(csv, "%c%ld%c%ld\n", cfg->delimiter, invdata->Serial, cfg->delimiter, invdata->Serial);
					fprintf(csv, "%cTotal yield%cDay yield\n", cfg->delimiter, cfg->delimiter);
					fprintf(csv, "%cCounter%cAnalog\n", cfg->delimiter, cfg->delimiter);
				}
				if (cfg->CSV_Header == 1)
					fprintf(csv, "dd/MM/yyyy%ckWh%ckWh\n", cfg->delimiter, cfg->delimiter);
			}

			char FormattedFloat[16];

			int idx;
			for (idx = 0; idx < arraysize; idx++)
			{
				if (mData[idx].datetime > 0)
				{
					fprintf(csv, "%s%c", strfgmtime_t(cfg->DateFormat, mData[idx].datetime), cfg->delimiter);
					fprintf(csv, "%s%c", FormatFloat(FormattedFloat, mData[idx].totalkWh, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
					fprintf(csv, "%s\n", FormatFloat(FormattedFloat, mData[idx].daykWh, 0, cfg->precision, cfg->decimalpoint));
				}
			}
			fclose(csv);
		}
	}
    return 0;
}

int ExportDayDataToCSV(Config *cfg, InverterData *invdata, DayData dData[], int arraysize)
{
	if (cfg->CSV_Export == 1)
		{
		if (VERBOSE_NORMAL) puts("ExportDayDataToCSV()");

		FILE *csv;

		//fix 1.3.1 for inverters with BT piggyback (missing interval data in the dark)
		//need to find first valid date in array
		time_t date;
		int idx = 0;
		do
		{
			date = dData[idx++].datetime;
		} while ((idx < arraysize) && (date == 0));

		//Expand date specifiers in config::outputPath
		char exportPath[MAX_PATH];
		snprintf(exportPath, sizeof(exportPath), "%s", strftime_t(cfg->outputPath, date));
		CreatePath(exportPath);

		char csvpath[MAX_PATH];
		snprintf(csvpath, sizeof(csvpath), "%s/%s-%s.csv", exportPath, cfg->plantname, strftime_t("%Y%m%d", date));

		if ((csv = fopen(csvpath, "w+")) == NULL)
		{
			fprintf(stderr, "Unable to open output file %s\n", csvpath);
			return -1;
		}
		else
		{
			if (cfg->CSV_ExtendedHeader == 1)
			{
				fprintf(csv, "sep=%c\n", cfg->delimiter);
				fprintf(csv, "Version CSV1|Tool SMAspot%s|Linebreaks CR/LF|Delimiter %s|Decimalpoint %s|Precision %d\n\n", cfg->prgVersion, delim2txt(cfg->delimiter), dp2txt(cfg->decimalpoint), cfg->precision);
				fprintf(csv, "%c%s%c%s\n", cfg->delimiter, invdata->DeviceName, cfg->delimiter, invdata->DeviceName);
				fprintf(csv, "%c%s%c%s\n", cfg->delimiter, invdata->DeviceType, cfg->delimiter, invdata->DeviceType);
				fprintf(csv, "%c%ld%c%ld\n", cfg->delimiter, invdata->Serial, cfg->delimiter, invdata->Serial);
				fprintf(csv, "%cTotal yield%cPower\n", cfg->delimiter, cfg->delimiter);
				fprintf(csv, "%cCounter%cAnalog\n", cfg->delimiter, cfg->delimiter);
			}
			if (cfg->CSV_Header == 1)
				fprintf(csv, "dd/MM/yyyy HH:mm:ss%ckWh%ckW\n", cfg->delimiter, cfg->delimiter);
		}

		char FormattedFloat[16];

		for (int idx = 0; idx < arraysize; idx++)
		{
			if (dData[idx].datetime > 0)
			{
				if ((cfg->CSV_SaveZeroPower == 1) || (dData[idx].watt > 0))
				{
					fprintf(csv, "%s%c", strftime_t(cfg->DateTimeFormat, dData[idx].datetime), cfg->delimiter);
					fprintf(csv, "%s%c", FormatFloat(FormattedFloat, dData[idx].totalkWh, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
					fprintf(csv, "%s\n", FormatFloat(FormattedFloat, dData[idx].watt / 1000.0f, 0, cfg->precision, cfg->decimalpoint));
				}
			}
		}
		fclose(csv);
	}
    return 0;
}


int ExportSpotDataToCSV(Config *cfg, InverterData *invdata)
{
	if (cfg->CSV_Export == 1)
	{
		if (VERBOSE_NORMAL) puts("ExportSpotDataToCSV()");

		FILE *csv;

		//Expand date specifiers in config::outputPath
		char exportPath[MAX_PATH];
		snprintf(exportPath, sizeof(exportPath), "%s", strftime_t(cfg->outputPath, invdata->InverterDatetime));
		CreatePath(exportPath);

		char csvpath[MAX_PATH];
		snprintf(csvpath, sizeof(csvpath), "%s/%s-Spot-%s.csv", exportPath, cfg->plantname, strftime_t("%Y%m%d", invdata->InverterDatetime));

		if ((csv = fopen(csvpath, "a+")) == NULL)
		{
			printf("Unable to open output file %s\n", csvpath);
			return -1;
		}
		else
		{
			//Write header when new file has been created
			#ifdef WIN32
			if (filelength(fileno(csv)) == 0)
			#else
			struct stat fStat;
			stat(csvpath, &fStat);
			if (fStat.st_size == 0)
			#endif
			{
				if (cfg->CSV_ExtendedHeader == 1)
				{
					fprintf(csv, "sep=%c\n", cfg->delimiter);
					fprintf(csv, "Version CSV1|Tool SMAspot%s|Linebreaks CR/LF|Delimiter %s|Decimalpoint %s|Precision %d\n\n", cfg->prgVersion, delim2txt(cfg->delimiter), dp2txt(cfg->decimalpoint), cfg->precision);
					fprintf(csv, "%c%s\n", cfg->delimiter, invdata->DeviceName);
					fprintf(csv, "%c%s\n", cfg->delimiter, invdata->DeviceType);
					fprintf(csv, "%c%ld\n", cfg->delimiter, invdata->Serial);
					char Header1[] = "|Watt|Watt|Amp|Amp|Volt|Volt|Watt|Watt|Watt|Amp|Amp|Amp|Volt|Volt|Volt|Watt|Watt|%|kWh|kWh|Hz|Hours|Hours|%\n";
					for (int i=0; Header1[i]!=0; i++)
						if (Header1[i]=='|') Header1[i]=cfg->delimiter;
					fputs(Header1, csv);
				}
				if (cfg->CSV_Header == 1)
				{
					char Header2[] = "DateTime|Pdc1|Pdc2|Idc1|Idc2|Udc1|Udc2|Pac1|Pac2|Pac3|Iac1|Iac2|Iac3|Uac1|Uac2|Uac3|PdcTot|PacTot|Efficiency|EToday|ETotal|Frequency|OperatingTime|FeedInTime|BT_Signal\n";
					for (int i=0; Header2[i]!=0; i++)
						if (Header2[i]=='|') Header2[i]=cfg->delimiter;
					fputs(Header2, csv);
				}
			}

			char FormattedFloat[32];
			const char *strout = "%s%c";

			//Calculated Power Values (Sum of Power per string)
			float calPdcTot = (float)(invdata->Pdc1 + invdata->Pdc2);
			float calPacTot = (float)(invdata->Pac1 + invdata->Pac2 + invdata->Pac3);
			float calEfficiency = calPdcTot == 0 ? 0 : calPacTot / calPdcTot * 100;
			//Calculated Power Values (Sum of U*I per string)
			//float calPdcTotUxI = invdata->Idc1 * invdata->Udc1 + invdata->Idc2 * invdata->Udc2;
			//float calPacTotUxI = invdata->Iac1 * invdata->Uac1 + invdata->Iac2 * invdata->Uac2 + invdata->Iac3 + invdata->Uac3;
			//float calEfficiencyUxI = calPdcTotUxI == 0.0 ? 0.0f : calPacTotUxI / calPdcTotUxI * 100.0f;

			fprintf(csv, strout, strftime_t(cfg->DateTimeFormat, invdata->CurrentTime), cfg->delimiter);
			fprintf(csv, strout, FormatFloat(FormattedFloat, (float)invdata->Pdc1, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
			fprintf(csv, strout, FormatFloat(FormattedFloat, (float)invdata->Pdc2, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
			fprintf(csv, strout, FormatFloat(FormattedFloat, (float)invdata->Idc1/1000, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
			fprintf(csv, strout, FormatFloat(FormattedFloat, (float)invdata->Idc2/1000, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
			fprintf(csv, strout, FormatFloat(FormattedFloat, (float)invdata->Udc1/100, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
			fprintf(csv, strout, FormatFloat(FormattedFloat, (float)invdata->Udc2/100, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
			fprintf(csv, strout, FormatFloat(FormattedFloat, (float)invdata->Pac1, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
			fprintf(csv, strout, FormatFloat(FormattedFloat, (float)invdata->Pac2, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
			fprintf(csv, strout, FormatFloat(FormattedFloat, (float)invdata->Pac3, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
			fprintf(csv, strout, FormatFloat(FormattedFloat, (float)invdata->Iac1/1000, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
			fprintf(csv, strout, FormatFloat(FormattedFloat, (float)invdata->Iac2/1000, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
			fprintf(csv, strout, FormatFloat(FormattedFloat, (float)invdata->Iac3/1000, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
			fprintf(csv, strout, FormatFloat(FormattedFloat, (float)invdata->Uac1/100, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
			fprintf(csv, strout, FormatFloat(FormattedFloat, (float)invdata->Uac2/100, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
			fprintf(csv, strout, FormatFloat(FormattedFloat, (float)invdata->Uac3/100, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
			fprintf(csv, strout, FormatFloat(FormattedFloat, calPdcTot, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
			fprintf(csv, strout, FormatFloat(FormattedFloat, (float)invdata->TotalPac, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
			fprintf(csv, strout, FormatFloat(FormattedFloat, calEfficiency, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
			fprintf(csv, strout, FormatDouble(FormattedFloat, (double)invdata->EToday/1000, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
			fprintf(csv, strout, FormatDouble(FormattedFloat, (double)invdata->ETotal/1000, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
			fprintf(csv, strout, FormatFloat(FormattedFloat, (float)invdata->GridFreq/100, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
			fprintf(csv, strout, FormatDouble(FormattedFloat, (double)invdata->OperationTime/3600, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
			fprintf(csv, strout, FormatDouble(FormattedFloat, (double)invdata->FeedInTime/3600, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
			// Last item needs newline instead of delimiter
			fprintf(csv, "%s\n", FormatFloat(FormattedFloat, invdata->BT_Signal, 0, cfg->precision, cfg->decimalpoint));
			fclose(csv);
		}
	}
	return 0;
}
