/*
    vgmwrite.c

    VGM output module

  Ported from Gens/GS r7+ vgm mod.

*/

#include "vgmwrite.h"

// C includes.
#include <stdlib.h>
#include <stdio.h>

#if ! defined(_MSC_VER) || _MSC_VER >= 1600	// MS VC++ 2010 and higher

#include <stdint.h>

#else

typedef unsigned char	uint8_t;
typedef signed char 	int8_t;
typedef unsigned short	uint16_t;
typedef signed short	int16_t;
#ifndef _WINDOWS_H
typedef unsigned int	uint32_t;
typedef signed int		int32_t;
#endif

#endif

#include <string.h>
#include <malloc.h>


void ClearLine(void);			// from main.c
void RedrawStatusLine(void);	// from main.c


#define CLOCK_NTSC	53693175
#define CLOCK_PAL	53203424
 
#ifndef INLINE
#define INLINE	__inline
#endif


typedef struct _vgm_file_header VGM_HEADER;
struct _vgm_file_header
{
	uint32_t fccVGM;
	uint32_t lngEOFOffset;
	uint32_t lngVersion;
	uint32_t lngHzPSG;
	uint32_t lngHz2413;
	uint32_t lngGD3Offset;
	uint32_t lngTotalSamples;
	uint32_t lngLoopOffset;
	uint32_t lngLoopSamples;
	uint32_t lngRate;
	uint16_t shtPSG_Feedback;
	uint8_t bytPSG_STWidth;
	uint8_t bytPSG_Flags;
	uint32_t lngHz2612;
	uint32_t lngHz2151;
	uint32_t lngDataOffset;
	uint32_t lngHzSPCM;
	uint32_t lngSPCMIntf;
};	// -> 0x40 Bytes
typedef struct _vgm_gd3_tag GD3_TAG;
struct _vgm_gd3_tag
{
	uint32_t fccGD3;
	uint32_t lngVersion;
	uint32_t lngTagLength;
	wchar_t strTrackNameE[0x70];
	wchar_t strTrackNameJ[0x10];	// Japanese Names are not used
	wchar_t strGameNameE[0x70];
	wchar_t strGameNameJ[0x10];
	wchar_t strSystemNameE[0x30];
	wchar_t strSystemNameJ[0x10];
	wchar_t strAuthorNameE[0x30];
	wchar_t strAuthorNameJ[0x10];
	wchar_t strReleaseDate[0x10];
	wchar_t strCreator[0x20];
	wchar_t strNotes[0x50];
};	// -> 0x200 Bytes
typedef struct _vgm_file_inf VGM_INF;
struct _vgm_file_inf
{
	FILE* hFile;
	VGM_HEADER Header;
	uint32_t BytesWrt;
	uint32_t SmplsWrt;
	uint32_t EvtDelay;
};
typedef struct _vgm_chip VGM_CHIP;
struct _vgm_chip
{
	uint8_t ChipType;
	//uint16_t VgmID;
	uint8_t HadWrite;
};


static const uint8_t CHIP_LIST[0x02] = {VGMC_YM2612, VGMC_SN76496};

unsigned char Enable_VGMDumping = 0x00;
static uint8_t VGM_Dumping;
unsigned char VGM_IgnoreWrt = 0x00;
static VGM_INF VgmFile;
static VGM_CHIP VgmChip[0x02];
static GD3_TAG VgmTag;

// Function Prototypes
static INLINE size_t atwcpy(wchar_t* dststr, const char* srcstr);
static uint8_t ChipType2ID(uint8_t chip_type);
static void vgm_header_clear(void);
static void vgm_dump_init(void);
static void vgm_close(void);
static void write_vgm_tag(const wchar_t* TagStr, FILE* hFile);
static void vgm_write_delay(void);

static unsigned short int FrameSmpls;

static const char* MusFileName = NULL;
static int MusTrackID = -1;
static char* VGMFileName = NULL;

// ASCII to Wide-Char String Copy
static INLINE size_t atwcpy(wchar_t* dststr, const char* srcstr)
{
	return mbstowcs(dststr, srcstr, strlen(srcstr) + 0x01);
}

static uint8_t ChipType2ID(uint8_t chip_type)
{
	uint8_t curchip;
	uint8_t chip_id;
	
	chip_id = 0xFF;
	for (curchip = 0x00; curchip < 0x02; curchip ++)
	{
		if (CHIP_LIST[curchip] == chip_type)
		{
			chip_id = curchip;
			break;
		}
	}
	
	return chip_id;
}

void MakeVgmFileName(const char* FileName, int TrackID)
{
	size_t StrLen;
	char* TempPnt;
	
	TempPnt = strrchr(FileName, '\\');
	if (TempPnt == NULL)
		TempPnt = strrchr(FileName, '/');
	if (TempPnt == NULL)
		MusFileName = FileName;
	else
		MusFileName = TempPnt + 0x01;
	MusTrackID = TrackID;
	
	StrLen = 0x06 + strlen(MusFileName) + 0x05;
	VGMFileName = (char*)realloc(VGMFileName, StrLen);
	
	strcpy(VGMFileName, "dumps\\");
	strcat(VGMFileName, MusFileName);
	TempPnt = strrchr(VGMFileName, '.');
	if (TempPnt == NULL)
		TempPnt = VGMFileName + strlen(VGMFileName);
	
	if (MusTrackID == -1)
		strcpy(TempPnt, ".vgm");
	else
		sprintf(TempPnt, "_%02X.vgm", MusTrackID);
	
	return;
}

int vgm_dump_start(void)
{
	if (! Enable_VGMDumping)
		return 0;
	
	VgmFile.hFile = fopen(VGMFileName, "wb");
	if (VgmFile.hFile == NULL)
	{
		ClearLine();
		printf("Can't open file for VGM dumping!\n");
		RedrawStatusLine();
		return -3;
	}
	
	FrameSmpls = 735;

	VgmFile.BytesWrt = 0;
	VgmFile.SmplsWrt = 0;
	VgmFile.EvtDelay = 0;
	vgm_header_clear();
	
	VgmTag.fccGD3 = 0x20336447;	// 'Gd3 '
	VgmTag.lngVersion = 0x0100;
	if (MusTrackID == -1)
		atwcpy(VgmTag.strTrackNameE, MusFileName);
	else
#if ! defined(_MSC_VER) || _MSC_VER >= 1400	// MS VC++ 2005 and higher
		swprintf(VgmTag.strTrackNameE, 0x70, L"Music %02X", MusTrackID);
#else	// older MS VC++ versions like VC6
		swprintf(VgmTag.strTrackNameE, L"Music %02X", MusTrackID);
#endif
	wcscpy(VgmTag.strTrackNameJ, L"");
	if (MusTrackID == -1)
		wcscpy(VgmTag.strGameNameE, L"");
	else
		atwcpy(VgmTag.strGameNameE, MusFileName);
	wcscpy(VgmTag.strGameNameJ, L"");
	wcscpy(VgmTag.strSystemNameE, L"Sega Mega Drive / Genesis");
	wcscpy(VgmTag.strSystemNameJ, L"");
	wcscpy(VgmTag.strAuthorNameE, L"");
	wcscpy(VgmTag.strAuthorNameJ, L"");
	wcscpy(VgmTag.strReleaseDate, L"");
	wcscpy(VgmTag.strCreator, L"");
	
#if ! defined(_MSC_VER) || _MSC_VER >= 1400	// MS VC++ 2005 and higher
	swprintf(VgmTag.strNotes, 0x50, L"Generated by %s", L"GEMS Music Player");
#else	// older MS VC++ versions like VC6
	swprintf(VgmTag.strNotes, L"Generated by %s", L"GEMS Music Player");
#endif
	VgmTag.lngTagLength = (uint32_t)(	wcslen(VgmTag.strTrackNameE) + 0x01 +
										wcslen(VgmTag.strTrackNameJ) + 0x01 +
										wcslen(VgmTag.strGameNameE) + 0x01 +
										wcslen(VgmTag.strGameNameJ) + 0x01 +
										wcslen(VgmTag.strSystemNameE) + 0x01 +
										wcslen(VgmTag.strSystemNameJ) + 0x01 +
										wcslen(VgmTag.strAuthorNameE) + 0x01 +
										wcslen(VgmTag.strAuthorNameJ) + 0x01 +
										wcslen(VgmTag.strReleaseDate) + 0x01 +
										wcslen(VgmTag.strCreator) + 0x01 +
										wcslen(VgmTag.strNotes) + 0x01);
	VgmTag.lngTagLength *= 0x02;	// String Length -> Byte Length
	
	VGM_Dumping = 0x01;
	
	vgm_dump_init();
	
	return 0;
}

int vgm_dump_stop(void)
{
	uint8_t curchip;
	uint8_t chip_unused;
	
	if (! VGM_Dumping)
		return -1;
	
	chip_unused = 0x00;
	for (curchip = 0x00; curchip < 0x02; curchip ++)
	{
		if (! VgmChip[curchip].HadWrite)
		{
			chip_unused ++;
			switch(CHIP_LIST[curchip])
			{
			case VGMC_SN76496:
				VgmFile.Header.lngHzPSG = 0x00;
				VgmFile.Header.shtPSG_Feedback = 0x00;
				VgmFile.Header.bytPSG_STWidth = 0x00;
				VgmFile.Header.bytPSG_Flags = 0x00;
				break;
			case VGMC_YM2612:
				VgmFile.Header.lngHz2612 = 0x00;
				break;
			}
		}
	}
//	if (chip_unused)
//		printf("Header Data of %hu unused Chips removed.\n", chip_unused);
	
	vgm_close();
	
	VGM_Dumping = 0x00;
	/*if (ShowMsg)
	{
		ClearLine();
		printf("VGM Dump Stopped.\n");
		RedrawStatusLine();
	}*/
	
	return 0;
}

void vgm_update(void)
{
	if (! VGM_Dumping)
		return;
	
	VgmFile.EvtDelay += FrameSmpls;
	//VgmFile.EvtDelay ++;
	
	return;
}

static void vgm_header_clear(void)
{
	uint32_t CUR_CLOCK;
	VGM_HEADER* Header;
	
	if (! VgmFile.hFile)
		return;
	
	Header = &VgmFile.Header;
	memset(Header, 0x00, sizeof(VGM_HEADER));
	Header->fccVGM = 0x206D6756;	// 'Vgm '
	Header->lngEOFOffset = 0x00;
	Header->lngVersion = 0x0160;
	//Header->lngGD3Offset = 0x00;
	//Header->lngTotalSamples = 0;
	//Header->lngLoopOffset = 0x00;
	//Header->lngLoopSamples = 0;
	Header->lngRate = 60;
	Header->lngDataOffset = sizeof(VGM_HEADER);
	
	Header->lngDataOffset -= 0x34;	// moved here from vgm_close
	
	CUR_CLOCK = CLOCK_NTSC;
	VgmFile.Header.lngHz2612 = (CUR_CLOCK + 3) / 7;
#ifdef DUAL_SUPPORT
	VgmFile.Header.lngHz2612 |= 0x40000000;
#endif
	VgmFile.Header.lngHzPSG = (CUR_CLOCK + 7) / 15;
	VgmFile.Header.shtPSG_Feedback = 0x09;
	VgmFile.Header.bytPSG_STWidth = 0x10;
	VgmFile.Header.bytPSG_Flags = 0x06;
	
	fwrite(Header, sizeof(VGM_HEADER), 0x01, VgmFile.hFile);
	VgmFile.BytesWrt += sizeof(VGM_HEADER);
	
	return;
}

static void vgm_dump_init(void)
{
	uint8_t curchip;
	
	for (curchip = 0x00; curchip < 0x02; curchip ++)
	{
		VgmChip[curchip].ChipType = CHIP_LIST[curchip];
		VgmChip[curchip].HadWrite = 0x00;
	}
	
	return;
}

static void vgm_close(void)
{
	VGM_HEADER* Header;
	
	if (! VgmFile.hFile)
		return;
	
	Header = &VgmFile.Header;
	
	vgm_write_delay();
	fputc(0x66, VgmFile.hFile);	// Write EOF Command
	VgmFile.BytesWrt ++;
	
	// GD3 Tag
	Header->lngGD3Offset = VgmFile.BytesWrt - 0x14;
	fwrite(&VgmTag.fccGD3, 0x04, 0x01, VgmFile.hFile);
	fwrite(&VgmTag.lngVersion, 0x04, 0x01, VgmFile.hFile);
	fwrite(&VgmTag.lngTagLength, 0x04, 0x01, VgmFile.hFile);
	write_vgm_tag(VgmTag.strTrackNameE, VgmFile.hFile);
	write_vgm_tag(VgmTag.strTrackNameJ, VgmFile.hFile);
	write_vgm_tag(VgmTag.strGameNameE, VgmFile.hFile);
	write_vgm_tag(VgmTag.strGameNameJ, VgmFile.hFile);
	write_vgm_tag(VgmTag.strSystemNameE, VgmFile.hFile);
	write_vgm_tag(VgmTag.strSystemNameJ, VgmFile.hFile);
	write_vgm_tag(VgmTag.strAuthorNameE, VgmFile.hFile);
	write_vgm_tag(VgmTag.strAuthorNameJ, VgmFile.hFile);
	write_vgm_tag(VgmTag.strReleaseDate, VgmFile.hFile);
	write_vgm_tag(VgmTag.strCreator, VgmFile.hFile);
	write_vgm_tag(VgmTag.strNotes, VgmFile.hFile);
	VgmFile.BytesWrt += 0x0C + VgmTag.lngTagLength;
	
	// Rewrite Header
	Header->lngTotalSamples = VgmFile.SmplsWrt;
	if (Header->lngLoopOffset)
	{
		Header->lngLoopOffset -= 0x1C;
		Header->lngLoopSamples = VgmFile.SmplsWrt - Header->lngLoopSamples;
	}
	Header->lngEOFOffset = VgmFile.BytesWrt - 0x04;
	fseek(VgmFile.hFile, 0x00, SEEK_SET);
	fwrite(Header, sizeof(VGM_HEADER), 0x01, VgmFile.hFile);
	
	fclose(VgmFile.hFile);
	VgmFile.hFile = NULL;
	
	//logerror("VGM %02hX closed.\t%lu Bytes, %lu Samples written\n", vgm_id, VgmFile.BytesWrt, VgmFile.SmplsWrt);
	
	return;
}

static void write_vgm_tag(const wchar_t* TagStr, FILE* hFile)
{
	const wchar_t* CurStr;
	uint16_t UnicodeChr;
	
	// under Windows it also would be possible to use this line
	//fwrite(TagStr, 0x02, wcslen(TagStr) + 0x01, hFile);
	
	CurStr = TagStr;
	// Write Tag-Text
	while(*CurStr)
	{
		UnicodeChr = (unsigned short)*CurStr;
		fwrite(&UnicodeChr, 0x02, 0x01, hFile);
		CurStr ++;
	}
	// Write Null-Terminator
	UnicodeChr = (unsigned short)*CurStr;
	fwrite(&UnicodeChr, 0x02, 0x01, hFile);
	
	return;
}

static void vgm_write_delay(void)
{
	uint16_t delaywrite;
	
	while(VgmFile.EvtDelay)
	{
		if (VgmFile.EvtDelay > 0x0000FFFF)
			delaywrite = 0xFFFF;
		else
			delaywrite = (unsigned short)VgmFile.EvtDelay;
		
		if (delaywrite <= 0x0010)
		{
			fputc(0x6F + delaywrite, VgmFile.hFile);
			VgmFile.BytesWrt += 0x01;
		}
		else
		{
			if (delaywrite == 735)
			{
				fputc(0x62, VgmFile.hFile);
				VgmFile.BytesWrt += 0x01;
			}
			else if (delaywrite == 2 * 735)
			{
				fputc(0x62, VgmFile.hFile);
				fputc(0x62, VgmFile.hFile);
				VgmFile.BytesWrt += 0x02;
			}
			else
			{
				fputc(0x61, VgmFile.hFile);
				fwrite(&delaywrite, 0x02, 0x01, VgmFile.hFile);
				VgmFile.BytesWrt += 0x03;
			}
		}
		VgmFile.SmplsWrt += delaywrite;
		
		VgmFile.EvtDelay -= delaywrite;
	}
	
	return;
}

void vgm_write(unsigned char chip_type, unsigned char port, unsigned short int r, unsigned char v)
{
	uint8_t chip_id;
	uint8_t TempByt;
	
	if (! VGM_Dumping || VGM_IgnoreWrt)
		return;
	chip_id = ChipType2ID(chip_type);
	if (chip_id == 0xFF)
		return;
	if (! VgmFile.hFile)
		return;
	
	if (! VgmChip[chip_id].HadWrite)
		VgmChip[chip_id].HadWrite = 0x01;
	vgm_write_delay();
	
	switch(chip_type)	// Write the data
	{
	case VGMC_SN76496:
		switch(port)
		{
		case 0x00:	// standard PSG register
			fputc(0x50, VgmFile.hFile);
			fputc(r, VgmFile.hFile);
			VgmFile.BytesWrt += 0x02;
			break;
		case 0x01:	// GG Stereo
			fputc(0x4F, VgmFile.hFile);
			fputc(r, VgmFile.hFile);
			VgmFile.BytesWrt += 0x02;
			break;
		}
		break;
	case VGMC_YM2612:
		TempByt = 0x52 + (port & 0x01);
#ifdef DUAL_SUPPORT
		if (port & 0x02)
			TempByt += 0x50;
#endif
		fputc(TempByt, VgmFile.hFile);
		fputc(r, VgmFile.hFile);
		fputc(v, VgmFile.hFile);
		VgmFile.BytesWrt += 0x03;
		break;
	default:
		fputc(0x01, VgmFile.hFile);	// write invalid data - for debugging purposes
		break;
	}
	
	return;
}

void vgm_write_large_data(unsigned char chip_type, unsigned char type, unsigned int datasize, unsigned int value1, unsigned int value2, const void* data)
{
	uint8_t chip_id;
	uint32_t finalsize;
	uint16_t TempSht;
	char write_it;
	
	if (! VGM_Dumping)
		return;
	chip_id = ChipType2ID(chip_type);
	if (chip_id == 0xFF)
		return;
	
	if (! VgmFile.hFile)
		return;
	
	vgm_write_delay();
	
	write_it = 0x00;
	switch(chip_type)	// Write the data
	{
	case VGMC_SN76496:
		break;
	case VGMC_YM2612:
		switch(type)
		{
		case 0x00:	// uncompressed
			type = 0x00;
			write_it = 0x01;
			break;
		case 0x01:	// comrpessed
			type = 0x40;
			write_it = 0x01;
			break;
		}
		break;
	}
	
	if (write_it)
	{
		fputc(0x67, VgmFile.hFile);
		fputc(0x66, VgmFile.hFile);
		fputc(type, VgmFile.hFile);
		switch(type & 0xC0)
		{
		case 0x80:
			// Value 1 & 2 are used to write parts of the image (and save space)
			if (! value2)
				value2 = datasize - value1;
			if (data == NULL)
			{
				value1 = 0x00;
				value2 = 0x00;
			}
			finalsize = 0x08 + value2;
			
			fwrite(&finalsize, 0x04, 0x01, VgmFile.hFile);	// Data Block Size
			fwrite(&datasize, 0x04, 0x01, VgmFile.hFile);	// ROM Size
			fwrite(&value1, 0x04, 0x01, VgmFile.hFile);	// Data Base Address
			// Data Length is useless - is equal to finalsize - 0x08
			//fwrite(&value2, 0x04, 0x01, VgmFile.hFile);	// Data Length
			fwrite(data, 0x01, value2, VgmFile.hFile);
			VgmFile.BytesWrt += 0x07 + finalsize;
			break;
		case 0xC0:
			finalsize = datasize + 0x02;
			fwrite(&finalsize, 0x04, 0x01, VgmFile.hFile);	// Data Block Size
			fwrite(&value1, 0x02, 0x01, VgmFile.hFile);	// Data Address
			fwrite(data, 0x01, datasize, VgmFile.hFile);
			VgmFile.BytesWrt += 0x07 + finalsize;
			break;
		case 0x00:
			finalsize = datasize + 0x04;
			fwrite(&datasize, 0x04, 0x01, VgmFile.hFile);	// Data Block Size
			fwrite(data, 0x01, datasize, VgmFile.hFile);
			VgmFile.BytesWrt += 0x07 + datasize;
			break;
		case 0x40:
			finalsize = datasize + 0x0A;
			fwrite(&finalsize, 0x04, 0x01, VgmFile.hFile);	// Data Block Size
			
			fputc(0x00, VgmFile.hFile);						// Compression Type: n-Bit Compression
			fwrite(&value1, 0x04, 0x01, VgmFile.hFile);		// uncompressed size
			
			fputc(8, VgmFile.hFile);						// Bits decompressed
			fputc(4, VgmFile.hFile);						// Bits compressed
			fputc(0x01, VgmFile.hFile);						// Compression Sub-Type: Shift Left
			TempSht = (uint16_t)value2;
			fwrite(&TempSht, 0x02, 0x01, VgmFile.hFile);	// start value
			fwrite(data, 0x01, datasize, VgmFile.hFile);
			VgmFile.BytesWrt += 0x07 + finalsize;
			break;
		}
	}
	else
	{
		fputc(0x01, VgmFile.hFile);	// write invalid data
	}
	
	return;
}

void vgm_write_stream_data_command(unsigned char stream, unsigned char type, unsigned int data, unsigned char flags)
{
	if (! VGM_Dumping)
		return;
	
	vgm_write_delay();
	
	switch(type)
	{
	case 0x00:	// chip setup
		fputc(0x90, VgmFile.hFile);
		fputc(stream, VgmFile.hFile);
		fputc((data & 0x00FF0000) >> 16, VgmFile.hFile);
		fputc((data & 0x0000FF00) >>  8, VgmFile.hFile);
		fputc((data & 0x000000FF) >>  0, VgmFile.hFile);
		VgmFile.BytesWrt += 0x05;
		break;
	case 0x01:	// data block setup
		fputc(0x91, VgmFile.hFile);
		fputc(stream, VgmFile.hFile);
		fputc(data & 0xFF, VgmFile.hFile);
		fputc(0x01, VgmFile.hFile);
		fputc(0x00, VgmFile.hFile);
		VgmFile.BytesWrt += 0x05;
		break;
	case 0x02:	// frequency setup
		fputc(0x92, VgmFile.hFile);
		fputc(stream, VgmFile.hFile);
		fwrite(&data, 0x04, 0x01, VgmFile.hFile);
		VgmFile.BytesWrt += 0x06;
		break;
	case 0x04:	// stop sample
		fputc(0x94, VgmFile.hFile);
		fputc(stream, VgmFile.hFile);
		VgmFile.BytesWrt += 0x02;
		break;
	case 0x05:	// start sample
		fputc(0x95, VgmFile.hFile);
		fputc(stream, VgmFile.hFile);
		fputc((data & 0x00FF) >> 0, VgmFile.hFile);
		fputc((data & 0xFF00) >> 8, VgmFile.hFile);
		fputc(flags, VgmFile.hFile);
		VgmFile.BytesWrt += 0x05;
		break;
	}
	
	return;
}

void vgm_set_loop(void)
{
	if (! VGM_Dumping || ! VgmFile.hFile)
		return;
	
	vgm_write_delay();
	
	VgmFile.Header.lngLoopOffset = VgmFile.BytesWrt;
	VgmFile.Header.lngLoopSamples = VgmFile.SmplsWrt;
	
	return;
}
