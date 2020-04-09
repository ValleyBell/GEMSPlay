#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include "stdtype.h"
//#include "common.h"
#include "sound.h"
#include "../GemsPlay.h"

/*3456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456
0000000001111111111222222222233333333334444444444555555555566666666667777777777888888888899999*/


#define INSTYPE_FM			0x00
#define INSTYPE_DAC			0x01
#define INSTYPE_PSG_TONE	0x02
#define INSTYPE_PSG_NOISE	0x03
typedef struct instrument_data
{
	UINT8 InsType;
	const UINT8* Data;
} INS_DATA;


#pragma pack(1)
typedef struct dac_data
{
	UINT8 V00_Flags;
	UINT16 V01_StartLSB;
	UINT8 V03_StartMSB;
	UINT16 V04_Skip;
	UINT16 V06_DataLen;
	UINT16 V08_Loop;
	UINT16 V0A_End;
	UINT8* Data;
} DAC_DATA;
#pragma pack()



extern unsigned char Enable_VGMDumping;
extern UINT8 Gems28Mode;
extern UINT8 DACSmplCount;
extern DAC_SAMPLE DACSmpls[0x80];

UINT16 InsCount;
INS_DATA InsData[0x100];

static __inline UINT16 Read16Bit(const UINT8* Data)
{
	return	(Data[0x00] << 0) |
			(Data[0x01] << 8);
}

static __inline UINT32 Read24Bit(const UINT8* Data)
{
	return	(Data[0x00] <<  0) |
			(Data[0x01] <<  8) |
			(Data[0x02] << 16);
}

void DetectSeqType(void)
{
	// detect GEMS Sequence type (2-byte or 3-byte pointers)
	const UINT8* SeqData;
	UINT16 BasePos;
	UINT16 MinPos1;
	UINT16 MinPos2;
	UINT16 CurPos;
	UINT8 TrkCnt;
	
	SeqData = GetDataPtr(0x02);
	if (SeqData == NULL)
		return;
	
	MinPos1 = MinPos2 = 0xFFFF;
	for (BasePos = 0x0000; BasePos < 0x200; BasePos += 0x02)
	{
		if (BasePos >= MinPos1)
			break;
		CurPos = Read16Bit(&SeqData[BasePos]);
		if (! SeqData[CurPos])	// skip empty songs
			continue;
		
		if (MinPos1 > CurPos)
			MinPos1 = CurPos;
		else if (MinPos2 > CurPos && CurPos > MinPos1)
			MinPos2 = CurPos;
	}
	
	TrkCnt = SeqData[MinPos1];
	MinPos1 ++;
	if (MinPos2 == 0xFFFF)
		MinPos2 = Read16Bit(&SeqData[MinPos1]);
	
	if (MinPos1 + 0x03 * TrkCnt == MinPos2)
		Gems28Mode = 0x01;
	else if (MinPos1 + 0x02 * TrkCnt == MinPos2)
		Gems28Mode = 0x00;
	else
	{
		Gems28Mode = 0x00;
		printf("Warning! GEMS Version Autodetection failed!\nPlease report!\n");
	}
	
	return;
}

void PreparseInstruments(void)
{
	const UINT8* PtchData;
	UINT32 BasePos;
	UINT32 EndPos;
	UINT32 CurPos;
	UINT16 CurIns;
	
	PtchData = GetDataPtr(0x00);
	if (PtchData == NULL)
	{
		InsCount = 0x00;
		return;
	}
	
	BasePos = 0x00;
	EndPos = (UINT32)-1;
	// Note: This reads to the offset of the first actual instrument data.
	for (CurIns = 0x00; CurIns < 0x100; CurIns ++, BasePos += 0x02)
	{
		if (BasePos >= EndPos)
			break;
		
		CurPos = Read16Bit(&PtchData[BasePos]);
		if (EndPos > CurPos)
			EndPos = CurPos;
		
		InsData[CurIns].InsType = PtchData[CurPos + 0x00];
		InsData[CurIns].Data = &PtchData[CurPos + 0x01];
	}
	InsCount = CurIns;
	
	return;
}

void PreparseSamples(void)
{
	const UINT8* DACData;
	UINT32 CurPos;
	UINT32 EndPos;
	UINT32 SmplPos;
	const DAC_DATA* TempDAC;
	UINT8 CurSnd;
	DAC_SAMPLE* TempSmpl;
	
	DACData = GetDataPtr(0x03);
	if (DACData == NULL)
		return;
	
	TempDAC = (const DAC_DATA*)&DACData[0x00];
	if (! TempDAC->V01_StartLSB && ! TempDAC->V03_StartMSB)
		return;	// Digital Sample data empty
	
	EndPos = (UINT32)-1;
	for (CurSnd = 0x00, CurPos = 0x00; CurSnd < 0x80; CurSnd ++, CurPos += 0x0C)
	{
		if (CurPos >= EndPos)
			break;
		
		TempDAC = (const DAC_DATA*)&DACData[CurPos];
		
		SmplPos = (TempDAC->V03_StartMSB << 16) | (TempDAC->V01_StartLSB << 0);
		if (TempDAC->V06_DataLen && EndPos > SmplPos)
			EndPos = SmplPos;
		
		TempSmpl = &DACSmpls[CurSnd];
		TempSmpl->File = NULL;
		TempSmpl->sample = (UINT8*)DACData + SmplPos;
		TempSmpl->size = TempDAC->V06_DataLen;
		TempSmpl->Flags = TempDAC->V00_Flags;
		TempSmpl->UsageID = CurSnd;
	}
	DACSmplCount = CurSnd;
	
	return;
}

void PreparseGemsSeq(UINT8 SongNo)
{
	const UINT8* SeqData;
#define SQEUE_LIMIT	0x10
	UINT8 SeqQueue[SQEUE_LIMIT];
	UINT8 SeqQEnd;
	UINT8 SeqQPos;
	UINT32 CurPos;
	UINT32 BasePos;
	UINT8 TrkCnt;
	UINT8 CurTrk;
	UINT8 CurCmd;
	UINT8 TrkEnd;
	UINT8 ChnMode;
	UINT8 TempByt;
	UINT8 LoopID;
	UINT8 LoopCount[0x10];
	
	if (! Enable_VGMDumping)
		return;
	
	SetDACUsage(0xFF);
	
	SeqData = GetDataPtr(0x02);
	if (SeqData == NULL)
		return;
	
	SeqQEnd = 0x00;
	SeqQueue[SeqQEnd] = SongNo;
	SeqQEnd ++;
	
	for (SeqQPos = 0x00; SeqQPos < SeqQEnd; SeqQPos ++)
	{
		CurPos = Read16Bit(&SeqData[SeqQueue[SeqQPos] * 0x02]);
		TrkCnt = SeqData[CurPos];
		CurPos ++;
		
		BasePos = CurPos;
		for (CurTrk = 0x00; CurTrk < TrkCnt; CurTrk ++)
		{
			if (! Gems28Mode)
			{
				CurPos = Read16Bit(&SeqData[BasePos]);
				BasePos += 0x02;
			}
			else
			{
				CurPos = Read24Bit(&SeqData[BasePos]);
				BasePos += 0x03;
			}
			
			ChnMode = 0x00;
			TrkEnd = 0x00;
			memset(LoopCount, 0x00, 0x10);
			LoopID = 0xFF;
			
			while(! TrkEnd)
			{
				CurCmd = SeqData[CurPos];
				CurPos ++;
				if (CurCmd < 0x60)	// 00-5F - Note
				{
					TempByt = CurCmd;
					if (ChnMode == INSTYPE_DAC)
					{
						TempByt = (TempByt + 0x30) % 0x60;
						SetDACUsage(TempByt);
					}
				}
				else if (CurCmd >= 0x80)
				{
					if (CurCmd < 0xC0)	// 80-BF - Note Length
					{
						CurCmd = SeqData[CurPos];
						while((CurCmd & 0xC0) == 0x80)
						{
							CurPos ++;
							CurCmd = SeqData[CurPos];
						}
					}
					else				// C0-FF - Note Delay
					{
						CurCmd = SeqData[CurPos];
						while((CurCmd & 0xC0) == 0xC0)
						{
							CurPos ++;
							CurCmd = SeqData[CurPos];
						}
					}
				}
				else	// 60-7F - Commands
				{
					switch(CurCmd)
					{
					case 0x60:	// Track End
						TrkEnd = 0x01;
						break;
					case 0x61:	// Instrument Change
						TempByt = SeqData[CurPos];
						CurPos += 0x01;
						
						if (TempByt < InsCount)
							ChnMode = InsData[TempByt].InsType;
						break;
					case 0x62:	// Pitch Envelope
						CurPos += 0x01;
						break;
					case 0x63:	// no operation
						break;
					case 0x64:	// Loop Start
						LoopID ++;
						LoopCount[LoopID] = SeqData[CurPos];
						CurPos += 0x01;
						break;
					case 0x65:	// Loop End
						if (LoopID == 0xFF)
						{
							printf("Warning! Invalid Loop End found!\n");
							break;
						}
						
						if (LoopCount[LoopID] == 0x7F)
							TrkEnd = 0x01;
						LoopID --;
						break;
					case 0x66:	// Toggle retrigger mode. ?
						CurPos += 0x01;
						break;
					case 0x67:	// Sustain mode
						CurPos += 0x01;
						break;
					case 0x68:	// Set Tempo
						CurPos += 0x01;
						break;
					case 0x69:	// Mute
						CurPos += 0x01;
						break;
					case 0x6A:	// Set Channel Priority
						CurPos += 0x01;
						break;
					case 0x6B:	// Play another song
						// queue song, but don't queue already played ones
						for (TempByt = 0x00; TempByt < SeqQEnd; TempByt ++)
						{
							if (SeqData[CurPos] == SeqQueue[TempByt])
								break;
						}
						if (TempByt >= SeqQEnd && SeqQEnd < SQEUE_LIMIT)
						{
							// not yet in queue
							SeqQueue[SeqQEnd] = SeqData[CurPos];
							SeqQEnd ++;
						}
						CurPos += 0x01;
						break;
					case 0x6C:	// Pitch Bend
						CurPos += 0x02;
						break;
					case 0x6D:	// Set song to use SFX timebase - 150 BPM
						break;
					case 0x6E:	// Set DAC sample playback rate
						CurPos += 0x01;
						break;
					case 0x6F:	// Jump
						CurPos += (INT16)Read16Bit(&SeqData[CurPos]);
						break;
					case 0x70:	// Store Value
						CurPos += 0x02;
						break;
					case 0x71:	// Conditional Jump
						CurPos += 0x05;
						break;
					case 0x72:	// More functions
						// 2 parameter (function type + function parameter)
						CurPos += 0x02;
						break;
					default:
						printf("Unknown event %02X on track %X\n", CurCmd, CurTrk);
						TrkEnd = 0x01;
						break;
					}	// end switch(CurCmd)
				}	// end if (CurCmd)
			}	// end while(! TrkEnd)
		}	// end for (CurTrk)
	}	// end for (SeqQPos)
	
	DumpDACSounds();
	
	return;
}

void SetDACUsage(UINT8 SampleID)
{
	UINT8 CurSmpl;
	
	if (SampleID == 0xFF)
	{
		for (CurSmpl = 0x00; CurSmpl < DACSmplCount; CurSmpl ++)
			DACSmpls[CurSmpl].UsageID = 0xFF;
	}
	else
	{
		DACSmpls[SampleID].UsageID = 0x80;
	}
	
	return;
}
