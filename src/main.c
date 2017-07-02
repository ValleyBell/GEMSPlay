#ifdef _MSC_VER
#pragma warning (disable : 4312)
#endif

#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <conio.h>
#include <windows.h>

#include "common.h"
#include "dirent.h"
#include "stdtype.h"
#include "../GemsPlay.h"
#include "sound.h"
#include "loader.h"
#include "vgmwrite.h"
#include "stdbool.h"


#define MAX_FILES	56
static s16 seq_count = 0;
static s16 cursor = 0;
//static char files[MAX_FILES][256];
static char IniPath[0x04][256];
static s16 seq_playing;
static bool GoToNextSong;

extern bool PauseThread;
bool PauseMode;
static bool AutoProgress;
static bool AutoStop;
static bool FollowTrack;

#define FMODE_ROM	0x00
#define FMODE_FILES	0x01
bool FileMode;

u32 ROMLength;
u8* ROMData;

u32 PatLength;
u8* PatData;
u32 EnvLength;
u8* EnvData;
u32 SeqLength;
u8* SeqData;
u32 SmpLength;
u8* SmpData;

u16 PointerLst[0x100];

extern unsigned int PlayingTimer;
extern signed int LoopCntr;
bool ThreadPauseConfrm;

extern UINT8 Gems28Mode;

/*static void get_file_list()
{
	DIR *dir;
	struct dirent *d;
	struct stat statbuf;
	char tempfile[256];

	dir = opendir("music");

	while( (d = readdir(dir)) != NULL )
	{
		if (seq_count >= MAX_FILES)
		{
			printf("Too many files! Stopped reading directory!\n");
			break;
		}
		
		sprintf(tempfile, "music\\%s", d->d_name);
		strcpy(files[seq_count], d->d_name);

		if(stat(tempfile, &statbuf) != -1)
		{
			if(!(statbuf.st_mode & _S_IFDIR))
			{
				seq_count++;
				printf("%2u %.75s\n", seq_count, d->d_name);
			}
		}
	}

	closedir(dir);
}*/


void LoadINIFile(const char* INIFileName)
{
	FILE* hFile;
	char BasePath[0x100];
	char TempStr[0x100];
	size_t TempInt;
	char* TempPnt;
	
	strcpy(IniPath[0x00], "");
	strcpy(IniPath[0x01], "");
	strcpy(IniPath[0x02], "");
	strcpy(IniPath[0x03], "");
	
	strcpy(BasePath, INIFileName);
	TempPnt = strrchr(BasePath, '\\');
	if (TempPnt != NULL)
		TempPnt ++;
	else
		TempPnt = BasePath;
	*TempPnt = '\0';
	
	hFile = fopen(INIFileName, "rt");
	if (hFile == NULL)
	{
		//printf("Error opening %s\n", INIFileName);
		return;
	}
	
	while(! feof(hFile))
	{
		TempPnt = fgets(TempStr, 0x100, hFile);
		if (TempPnt == NULL)
			break;
		if (TempStr[0x00] == '\n' || TempStr[0x00] == '\0')
			continue;
		if (TempStr[0x00] == ';')
		{
			// skip comment lines
			// fgets has set a null-terminator at char 0xFF
			while(TempStr[strlen(TempStr) - 1] != '\n')
			{
				fgets(TempStr, 0x100, hFile);
				if (TempStr[0x00] == '\0')
					break;
			}
			continue;
		}
		
		TempPnt = strchr(TempStr, '=');
		if (TempPnt == NULL || TempPnt == TempStr)
			continue;	// invalid line
		
		TempInt = strlen(TempPnt) - 1;
		if (TempPnt[TempInt] == '\n')
			TempPnt[TempInt] = '\0';
		
		*TempPnt = '\0';
		TempPnt ++;
		while(*TempPnt == ' ')
			TempPnt ++;
		
		TempInt = strlen(TempStr) - 1;
		while(TempInt > 0 && TempStr[TempInt] == ' ')
		{
			TempStr[TempInt] = '\0';
			TempInt --;
		}
		
		if (! _stricmp(TempStr, "PatchData"))
		{
			strcpy(IniPath[0x00], BasePath);
			strcat(IniPath[0x00], TempPnt);
		}
		else if (! _stricmp(TempStr, "EnvData"))
		{
			strcpy(IniPath[0x01], BasePath);
			strcat(IniPath[0x01], TempPnt);
		}
		else if (! _stricmp(TempStr, "SeqData"))
		{
			strcpy(IniPath[0x02], BasePath);
			strcat(IniPath[0x02], TempPnt);
		}
		else if (! _stricmp(TempStr, "SampleData"))
		{
			strcpy(IniPath[0x03], BasePath);
			strcat(IniPath[0x03], TempPnt);
		}
	}
	
	fclose(hFile);
	
	return;
}

UINT8 LoadDataFile(const char* FileName, UINT32* RetLength, UINT8** RetData)
{
	FILE* hFile;
	
	*RetLength = 0x00;
	*RetData = NULL;
	
	hFile = fopen(FileName, "rb");
	if (hFile == NULL)
	{
		printf("Error opening %s!\n", FileName);
		return 0xFF;
	}
	
	fseek(hFile, 0, SEEK_END);
	*RetLength = ftell(hFile);
	if (! *RetLength)
	{
		fclose(hFile);
		return 0x01;
	}
	
	fseek(hFile, 0, SEEK_SET);
	*RetData = (UINT8*)malloc(*RetLength);
	fread(*RetData, 0x01, *RetLength, hFile);
	
	fclose(hFile);
	
	return 0x00;
}

void LoadROM(const char* FileName)
{
	UINT8 RetVal;
	const char* TempPnt;
	
	RetVal = LoadDataFile(FileName, &ROMLength, &ROMData);
	if (RetVal)
		return;
	
	TempPnt = strrchr(FileName, '\\');
	if (TempPnt == NULL)
		TempPnt = FileName;
	else
		TempPnt ++;
	printf("ROM loaded: %s\n", TempPnt);
	
	return;
}

void LoadSongList(void)
{
	const UINT8* SeqData;
	UINT16 MusPtr;
	UINT16 MinPtr;
	UINT16 CurPos;
	
	seq_count = 0;
	SeqData = GetDataPtr(0x02);
	if (SeqData == NULL)
		return;
	MinPtr = 0x200;	// allow a maximum of 0x100 songs (GEMS takes only 00-FF anyway)
	for (CurPos = 0x00; CurPos < MinPtr; CurPos += 0x02)
	{
		MusPtr =	(SeqData[CurPos + 1] << 8) |
					(SeqData[CurPos + 0] << 0);
		if (MinPtr > MusPtr)
			MinPtr = MusPtr;
		
		PointerLst[seq_count] = MusPtr;
		seq_count ++;
	}
	
	return;
}

/*void DisplayLine(const char* Text, ...)
{
	va_list args;
	static char TempText[0x50];
	
	va_start(args, Text);
	vsnprintf(TempText, 0x4D, Text, args);
	TempText[0x4D] = '\0';
	strcat("\r");
	printf("%s", TempText);
	
	return;
}*/

void ClearLine(void)
{
	printf("%78s", "\r");
	return;
}

void ReDisplayFileID(s16 FileID);

void DisplayFileID(s16 FileID)
{
	ClearLine();
	
	ReDisplayFileID(FileID);
	
	return;
}

void ReDisplayFileID(s16 FileID)
{
	char TempBuf[0x20];	// maximum is 0x18 chars (for 2 loop digits) + \0
	char* TempPnt;
	
	TempBuf[0x00] = '\0';
	if (FileID == seq_playing)
	{
		UINT16 Min;
		UINT8 Sec;
		UINT8 Frame;
		
		TempPnt = TempBuf;
		strcpy(TempPnt, " (");	TempPnt += 0x02;
		
		Frame = PlayingTimer % 60;
		Sec = (PlayingTimer / 60) % 60;
		Min = PlayingTimer / 3600;
		TempPnt += sprintf(TempPnt, "%02u:%02u.%02u", Min, Sec, (Frame * 100 + 30) / 60);
		if (LoopCntr == -1)
		{
			TempPnt += sprintf(TempPnt, " %s", "finished");
		}
		else
		{
			TempPnt += sprintf(TempPnt, " %s", PauseMode ? "paused" : "playing");
			if (LoopCntr > 0)
				TempPnt += sprintf(TempPnt, " L %d", LoopCntr);
		}
		strcpy(TempPnt, ")");	TempPnt += 0x01;
	}
	
	printf("%02X%s\r", FileID, TempBuf);
	
	return;
}

void WaitTimeForKey(unsigned int MSec)
{
	DWORD CurTime;
	
	CurTime = timeGetTime() + MSec;
	while(timeGetTime() < CurTime)
	{
		Sleep(20);
		if (_kbhit())
			break;
	}
	
	return;
}

static void WaitForKey(void)
{
	while(! GoToNextSong && ! PauseMode)
	{
		Sleep(20);
		if (cursor == seq_playing)
			ReDisplayFileID(cursor);
		if (_kbhit())
			break;
	}
	
	return;
}

void FinishedSongSignal(void)
{
	if (AutoProgress && seq_playing < seq_count - 1)
		GoToNextSong = true;
	
	return;
}

void TrackChangeSignal(UINT8 SeqNum)
{
	if (! FollowTrack)
		return;
	
	if (seq_playing == cursor)
	{
		cursor = SeqNum;
		seq_playing = cursor;
		//PlayingTimer = 0;
	}
	
	return;
}


static void PlaySong(u8 SongNo)
{
	WriteFIFOCommand(0xFF);
	WriteFIFOCommand(0x10);	// start sequence
	WriteFIFOCommand(SongNo);
	FinishCommandFIFO();
	
	return;
}

static void WriteGEMSPtrs(u32 PatPtr, u32 EnvPtr, u32 SeqPtr, u32 SmpPtr)
{
	WriteFIFOCommand(0xFF);
	WriteFIFOCommand(0x0B);	// get pointers
	
	WriteFIFOCommand((PatPtr & 0x0000FF) >>  0);
	WriteFIFOCommand((PatPtr & 0x00FF00) >>  8);
	WriteFIFOCommand((PatPtr & 0xFF0000) >> 16);
	
	WriteFIFOCommand((EnvPtr & 0x0000FF) >>  0);
	WriteFIFOCommand((EnvPtr & 0x00FF00) >>  8);
	WriteFIFOCommand((EnvPtr & 0xFF0000) >> 16);
	
	WriteFIFOCommand((SeqPtr & 0x0000FF) >>  0);
	WriteFIFOCommand((SeqPtr & 0x00FF00) >>  8);
	WriteFIFOCommand((SeqPtr & 0xFF0000) >> 16);
	
	WriteFIFOCommand((SmpPtr & 0x0000FF) >>  0);
	WriteFIFOCommand((SmpPtr & 0x00FF00) >>  8);
	WriteFIFOCommand((SmpPtr & 0xFF0000) >> 16);
	FinishCommandFIFO();
	
	return;
}

int main(int argc, char *argv[])
{
	const char* ROMName;
	const char* TempPtr;
	UINT32 ROMPtrs[4];
	
	printf("GEMS Music Player\n");
	printf("-----------------\n");
	printf("by Valley Bell (based on Rob Jinnai's SMPS Player)\n");
#ifdef DUAL_SUPPORT
	printf("2xYM2612 support enabled.\n");
#endif
	printf("\n");
	if (argc == 2 && ! _stricmp(argv[1], "-help"))
	{
		printf("Usage:\n");
		printf("    GEMSPlay.exe [SeqData.bin]\n");
		printf("    GEMSPlay.exe InsData.bin EnvData.bin SeqData.bin SmpData.bin\n");
		printf("    GEMSPlay.exe ROM.bin SeqPtr\n");
		printf("    GEMSPlay.exe ROM.bin InsPtr EnvPtr SeqPtr SmpPtr\n");
		return 0;
	}
	
	Enable_VGMDumping = 0x00;
	/*if (argc >= 2)
	{
		if (argv[0x01][0x00] == '-')
		{
			if (toupper(argv[0x01][0x01] == 'V'))
				Enable_VGMDumping = 0x01;
		}
	}*/
	
	ROMLength = 0x00;	ROMData = NULL;	ROMName = NULL;
	
	PatLength = 0x00;	PatData = NULL;	ROMPtrs[0x00] = 0x00;
	EnvLength = 0x00;	EnvData = NULL;	ROMPtrs[0x01] = 0x00;
	SeqLength = 0x00;	SeqData = NULL;	ROMPtrs[0x02] = 0x00;
	SmpLength = 0x00;	SmpData = NULL;	ROMPtrs[0x03] = 0x00;
	
	//LoadINIFile("data\\config.ini");
	LoadINIFile("config.ini");
	
	if (argc == 2)
	{
		LoadDataFile(argv[1], &SeqLength, &SeqData);
		printf("Sequence File: %s\n", argv[1]);
		ROMName = argv[1];
		printf("\n");
	}
	else if (argc == 5)
	{
		LoadDataFile(argv[1], &PatLength, &PatData);
		printf("Patch File:    %s\n", argv[1]);
		LoadDataFile(argv[2], &EnvLength, &EnvData);
		printf("Envelope File: %s\n", argv[2]);
		LoadDataFile(argv[3], &SeqLength, &SeqData);
		printf("Sequence File: %s\n", argv[3]);
		LoadDataFile(argv[4], &SmpLength, &SmpData);
		printf("Sample File:   %s\n", argv[4]);
		ROMName = argv[3];
		printf("\n");
	}
	else if (argc >= 3)
	{
		ROMName = argv[1];
		LoadROM(ROMName);
		if (ROMData == NULL)
		{
			printf("Can't open ROM file!\n");
			return 1;
		}
		
		SetROMFile(ROMData);
		
		//LoadSongList(ROMData + 0x0AD284);
		//WriteGEMSPtrs(0x0ABFFA, 0x0AD22D, 0x0AD284, 0x0B62AB);
		if (argc == 3)
		{
			// Sequence pointer only
			ROMPtrs[0x02] = (UINT32)strtoul(argv[2], NULL, 0);
		}
		else
		{
			// all pointers
			if (argc < 6)
			{
				printf("Not enough arguments! Use GEMSPlay.exe -help\n");
				return 0;
			}
			ROMPtrs[0x00] = (UINT32)strtoul(argv[2], NULL, 0);
			ROMPtrs[0x01] = (UINT32)strtoul(argv[3], NULL, 0);
			ROMPtrs[0x02] = (UINT32)strtoul(argv[4], NULL, 0);
			ROMPtrs[0x03] = (UINT32)strtoul(argv[5], NULL, 0);
		}
		
		if (ROMPtrs[0x00])
		{
			printf("Patch Pointer:    %06X\n", ROMPtrs[0x00]);
			PatLength = 0x00;
			PatData = ROMData + ROMPtrs[0x00];
		}
		if (ROMPtrs[0x01])
		{
			printf("Envelope Pointer: %06X\n", ROMPtrs[0x01]);
			EnvLength = 0x00;
			EnvData = ROMData + ROMPtrs[0x01];
		}
		if (ROMPtrs[0x02])
		{
			printf("Sequence Pointer: %06X\n", ROMPtrs[0x02]);
			SeqLength = 0x00;
			SeqData = ROMData + ROMPtrs[0x02];
		}
		if (ROMPtrs[0x03])
		{
			printf("Sample Pointer:   %06X\n", ROMPtrs[0x03]);
			SmpLength = 0x00;
			SmpData = ROMData + ROMPtrs[0x03];
		}
		printf("\n");
	}
	if (PatData == NULL && IniPath[0x00][0] != '\0')
		LoadDataFile(IniPath[0x00], &PatLength, &PatData);
	if (EnvData == NULL && IniPath[0x01][0] != '\0')
		LoadDataFile(IniPath[0x01], &EnvLength, &EnvData);
	if (SeqData == NULL && IniPath[0x02][0] != '\0')
	{
		LoadDataFile(IniPath[0x02], &SeqLength, &SeqData);
		ROMName = IniPath[0x02];
	}
	if (SmpData == NULL && IniPath[0x03][0] != '\0')
		LoadDataFile(IniPath[0x03], &SmpLength, &SmpData);
	
	if (ROMPtrs[0x00] && ROMPtrs[0x01] && ROMPtrs[0x02] && ROMPtrs[0x03])
		// simulate real ROM (nice for testing)
		WriteGEMSPtrs(ROMPtrs[0x00], ROMPtrs[0x01], ROMPtrs[0x02], ROMPtrs[0x03]);
	else
		SetDataFiles(PatData, EnvData, SeqData, SmpData);
	gems_loop();	// process the command
	
	DetectSeqType();
	PreparseInstruments();
	PreparseSamples();
	
	if (ROMName == NULL)
		ROMName = "GEMS";
	TempPtr = strrchr(ROMName, '\\');
	if (TempPtr != NULL)
		ROMName = TempPtr + 1;
	LoadSongList();
	
	printf("Songs: %02X", seq_count);
	if (Gems28Mode)
		printf(" [GEMS 2.8]");
	printf("\n");
	if (seq_count && sound_init())
	{
		int inkey;
		
		AutoProgress = false;
		AutoStop = true;
		PauseMode = false;
		FollowTrack = true;
		GoToNextSong = false;
		seq_playing = -1;
		DisplayFileID(cursor);
		
		inkey = 0x00;
		while(inkey != 0x1B)
		{
			switch(inkey)
			{
			case 0xE0:
				inkey = _getch();
				switch(inkey)
				{
				case 0x48:	// Cursor Up
					if (cursor > 0)
					{
						cursor --;
						DisplayFileID(cursor);
					}
					break;
				case 0x50:	// Cursor Down
					if (cursor < seq_count - 1)
					{
						cursor ++;
						DisplayFileID(cursor);
					}
					break;
				}
				break;
			case 'n':
				if (cursor >= seq_count - 1)
					break;
				
				cursor ++;
				// fall through
			case 0x0D:
				ThreadPauseConfrm = false;
				PauseThread = true;
				while(! ThreadPauseConfrm)
					Sleep(1);
				vgm_dump_stop();
				
				if (AutoStop)
				{
					WriteFIFOCommand(0xFF);
					WriteFIFOCommand(0x16);	// stop all sequences
					FinishCommandFIFO();
				}
				gems_loop();	// process the command
				
				seq_playing = cursor;
				MakeVgmFileName(ROMName, seq_playing);
				vgm_dump_start();
				
				PreparseGemsSeq((u8)seq_playing);
				LoopCntr = -2;
				PlaySong((u8)seq_playing);
				
				PauseMode = false;
				sound_pause(PauseMode);
				DisplayFileID(cursor);	// erase line and redraw text
				
				break;
			case 'S':
				WriteFIFOCommand(0xFF);
				WriteFIFOCommand(0x12);	// stop this sequence
				WriteFIFOCommand((u8)cursor);
				FinishCommandFIFO();
				//vgm_dump_stop();
				break;
			case 'L':
				WriteFIFOCommand(0xFF);
				WriteFIFOCommand(0x16);	// stop all sequences
				FinishCommandFIFO();
				vgm_dump_stop();
				break;
			case 'T':
				AutoStop = ! AutoStop;
				ClearLine();
				printf("Track Auto-Stop %s.\r", AutoStop ? "enabled" : "disabled");
				WaitTimeForKey(1000);
				DisplayFileID(cursor);
				break;
			case 'F':
				FollowTrack = ! FollowTrack;
				ClearLine();
				printf("Follow Sequences %s.\r", FollowTrack ? "enabled" : "disabled");
				WaitTimeForKey(1000);
				DisplayFileID(cursor);
				break;
			case 'V':
				Enable_VGMDumping = ! Enable_VGMDumping;
				ClearLine();
				printf("VGM Logging %s.\r", Enable_VGMDumping ? "enabled" : "disabled");
				WaitTimeForKey(1000);
				DisplayFileID(cursor);
				break;
			case 'P':
			case ' ':
				PauseMode = ! PauseMode;
				sound_pause(PauseMode);
				DisplayFileID(cursor);
				break;
			case 'A':
				AutoProgress = ! AutoProgress;
				ClearLine();
				printf("Automatic Progressing %s.\r", AutoProgress ? "enabled" : "disabled");
				WaitTimeForKey(1000);
				DisplayFileID(cursor);
				break;
			}
			
			WaitForKey();
			if (GoToNextSong)
			{
				GoToNextSong = false;
				inkey = 'n';
			}
			else
			{
				inkey = toupper(_getch());
			}
		}
		
		vgm_dump_stop();
		
		sound_cleanup();
	}
	
	if (ROMData != NULL)
		free(ROMData);
	
	if (PatLength && PatData != NULL)
		free(PatData);
	if (EnvLength && EnvData != NULL)
		free(EnvData);
	if (SeqLength && SeqData != NULL)
		free(SeqData);
	if (SmpLength && SmpData != NULL)
		free(SmpData);
	
	ClearLine();
	printf("Quit.\n");
#ifdef _DEBUG
	_getch();
#endif
	
	return 0;
}

void RedrawStatusLine(void)
{
	DisplayFileID(cursor);
	
	return;
}
