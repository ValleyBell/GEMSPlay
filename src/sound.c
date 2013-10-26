#ifdef _MSC_VER
#pragma warning (disable : 4312)
#endif

#include <assert.h>
#include <stdio.h>
#include "common.h"

#include <windows.h>
#include "stdtype.h"
#include "Stream.h"

#include "stdtype.h"
#include "mamedef.h"
#include "../GemsPlay.h"
#include "2612intf.h"
#include "sn764intf.h"
#include "sound.h"
#include "vgmwrite.h"

#define TRUE	1
#define FALSE	0

// Virtual clock rates for the YM2612 / SN76496
#define CLOCK_YM2612	7670454
#define CLOCK_SN76496	3579545

// Audio stream for YM2612 / SN76496 audio output
static u8	is_stereo;
static s16	*ym_buffer[4], *sn_buffer;
static u32	rateFMUpdate, rateDACUpdate;		// Different rate divisions; rateFMUpdate is how often YM2612 and SN76496 update, and rateDACUpdate is the DAC

u8 DACSmplCount;
DAC_SAMPLE DACSmpls[0x80];	// 0x7F different samples are maximum
//DAC_TABLE DACMasterPlaylist[0x80];	// 0x7F = Sounds from 0x01 to 0x7F

unsigned int SampleRate;

unsigned int PlayingTimer;
signed int LoopCntr;
signed int WaitCntr;

int sound_init()
{
	int init_OK = TRUE;
	u32 rate;
	u8 RetVal;

	//rate = SampleRate = 48000;
	rate = SampleRate = 44100;

	rateFMUpdate  = rate / 30;
	rateDACUpdate = rate / 60;
	is_stereo = TRUE;


	// Allocate buffers for the audio output of the different devices:
	ym_buffer[0]	= (s16 *)malloc(rateFMUpdate);	// YM2612 LEFT
	ym_buffer[1]	= (s16 *)malloc(rateFMUpdate);	// YM2612 RIGHT
#ifdef DUAL_SUPPORT
	ym_buffer[2]	= (s16 *)malloc(rateFMUpdate);	// YM2612 #2 LEFT
	ym_buffer[3]	= (s16 *)malloc(rateFMUpdate);	// YM2612 #2 RIGHT
#endif
	sn_buffer		= (s16 *)malloc(rateFMUpdate);	// SN76496

	DEBUGASSERT(ym_buffer[0] && ym_buffer[1] && sn_buffer);
	init_OK &= (ym_buffer[0] && ym_buffer[1] && sn_buffer);


	init_OK &= (device_start_ym2612(0, CLOCK_YM2612, rate) == 0);
#ifdef DUAL_SUPPORT
	init_OK &= (device_start_ym2612(1, CLOCK_YM2612, rate) == 0);
#endif
	init_OK &= (device_start_sn764xx(0, CLOCK_SN76496, rate, 0x10, 0x09) == 0);

	gems_init();

	RetVal = StartStream(0x00);
	if (RetVal)
	{
		printf("Error 0x%02X initialiting Stream!\n", RetVal);
		init_OK = FALSE;
	}
	
	PlayingTimer = 0;
	LoopCntr = -1;
	WaitCntr = -1;

	return init_OK;
}


void DumpDACSounds(void)
{
	UINT8 CurSnd;
	DAC_SAMPLE* TempSmpl;
	UINT8 SmplID;
	UINT8* SmplData;
	UINT32 CurPos;
	
	if (! Enable_VGMDumping)
		return;
	
	SmplID = 0x00;
	for (CurSnd = 0; CurSnd < DACSmplCount; CurSnd ++)
	{
		TempSmpl = &DACSmpls[CurSnd];
		if (! TempSmpl->size || TempSmpl->UsageID == 0xFF)
			continue;
		
		if (TempSmpl->UsageID == 0x80)
		{
			TempSmpl->UsageID = SmplID;
			SmplID ++;
		}
		
		if (! (TempSmpl->Flags & 0x80))
		{
			vgm_write_large_data(VGMC_YM2612, 0x00, TempSmpl->size, 0, 0, TempSmpl->sample);
		}
		else
		{
			// GEMS nibble order is Low-High, VGMs use High-Low
			SmplData = (UINT8*)malloc(TempSmpl->size);
			for (CurPos = 0x00; CurPos < TempSmpl->size; CurPos ++)
			{
				SmplData[CurPos] =	((TempSmpl->sample[CurPos] & 0x0F) << 4) |
									((TempSmpl->sample[CurPos] & 0xF0) >> 4);	
			}
			vgm_write_large_data(VGMC_YM2612, 0x01, TempSmpl->size, TempSmpl->size << 1, 0x00, SmplData);
			free(SmplData);	SmplData = NULL;
		}
	}
	if (! SmplID)
		return;	// no samples written = no DAC used
	
	vgm_write_stream_data_command(0x00, 0x00, 0x02002A, 0x00);
	vgm_write_stream_data_command(0x00, 0x01, 0x00, 0x00);
	
	return;
}

void DACPlay(UINT8 Sample, UINT8 SmpFlags)
{
	// Note: This command is used only for VGM logging.
	UINT32 DacFreq;
	UINT8 SmplID;
	
	if (! Enable_VGMDumping)
		return;
	
	SmplID = DACSmpls[Sample].UsageID;
	if (SmplID == 0xFF)
		return;
	
	// SmpFlags:
	//	Bits 0-3:	Sample Rate (based on YM2612 Timer A)
	//	 Bit  4:	Looping on/off
	//	 Bit  7:	4-bit PCM mode (already handled by DumpDACSounds)
	DacFreq = 144 * (SmpFlags & 0x0F);
	DacFreq = (7670454 + DacFreq / 2) / DacFreq;
	vgm_write_stream_data_command(0x00, 0x02, DacFreq, 0x00);
	vgm_write_stream_data_command(0x00, 0x05, SmplID, (SmpFlags & 0x10) >> 4);
	
	return;
}

void DACStop(UINT8 FinishLoop)
{
	// Note: This command is used only for VGM logging.
	if (! Enable_VGMDumping)
		return;
	
	if (! FinishLoop)
		vgm_write_stream_data_command(0x00, 0x04, 0x00, 0x00);
	else
		vgm_write_stream_data_command(0x00, 0x05, 0xFFFF, 0x00);
	
	return;
}

void FillBuffer(WAVE_16BS* Buffer, u32 BufferSize)
{
	sound_update((unsigned short*)Buffer, BufferSize);
}

void sound_pause(unsigned char PauseOn)
{
	PauseStream(PauseOn);
}

void FinishedSongSignal(void);
void TrackChangeSignal(UINT8 SeqNum);

void sound_update(unsigned short *stream_buf, unsigned int samples)
{
	u32 loop, loop2;
	s32 sample;
	s16 *stream_ptr[2];

	// Update Music Playback
	VBLINT();
	gems_loop();
	vgm_update();
	if (WaitCntr != -1)
	{
		if (WaitCntr == 2*60)
			FinishedSongSignal();
		WaitCntr ++;
	}
	else
	{
		PlayingTimer ++;
	}

	{
		for(loop = 0; loop < rateDACUpdate; loop++) 
		{
			DACME();
			//vgm_update();
			
			// update stream 
			stream_ptr[0] = &ym_buffer[0][loop];
			stream_ptr[1] = &ym_buffer[1][loop];
			ym2612_stream_update(0, stream_ptr, 1);
#ifdef DUAL_SUPPORT
			stream_ptr[0] = &ym_buffer[2][loop];
			stream_ptr[1] = &ym_buffer[3][loop];
			ym2612_stream_update(1, stream_ptr, 1);
#endif
		}
		
		sn764xx_stream_update(0, sn_buffer, rateDACUpdate);

		if (rateDACUpdate < samples)
			samples = rateDACUpdate;
		if(is_stereo) 
		{
			// Update loop for stereo sound
			loop2 = 0;
			for (loop = 0; loop < samples; loop ++)
			{
				// Left/Right interleve
				sample = sn_buffer[loop]/2 + ym_buffer[0][loop];	// Left
#ifdef DUAL_SUPPORT
				sample += ym_buffer[2][loop];
#endif
				if (sample < -0x7FFF)
					sample = -0x7FFF;
				else if (sample > 0x7FFF)
					sample = 0x7FFF;
				stream_buf[loop2++] = sample;
				
				sample = sn_buffer[loop]/2 + ym_buffer[1][loop];	// Right
#ifdef DUAL_SUPPORT
				sample += ym_buffer[3][loop];
#endif
				if (sample < -0x7FFF)
					sample = -0x7FFF;
				else if (sample > 0x7FFF)
					sample = 0x7FFF;
				stream_buf[loop2++] = sample;
			}
		}
		else 
		{
			// Update loop for monaural sound 
			for (loop = 0; loop < samples; loop ++)
				stream_buf[loop] = sn_buffer[loop]/4 + (ym_buffer[0][loop] + ym_buffer[1][loop]);
		}
	}
}



void sound_cleanup()
{
	// Destroy the audio stream
	StopStream();

	// Cleanup the YM2612 emulator
	device_stop_ym2612(0);
#ifdef DUAL_SUPPORT
	device_stop_ym2612(1);
#endif
	device_stop_sn764xx(0);

	// Free the buffers
	free(ym_buffer[0]);
	free(ym_buffer[1]);
#ifdef DUAL_SUPPORT
	free(ym_buffer[2]);
	free(ym_buffer[3]);
#endif
	free(sn_buffer);
}


// The following functions are not to be exported,
// but must be linked to GemsPlay.c
// Emulation of YM2612(/SN76496) writes.

static u8 YM2612_Reg;
// Use to write to 0xA04000/2 and then 0xA04001/3
void YM2612_Write(u8 Addr, u8 Data)
{
	if (Addr & 0x01)
		vgm_write(VGMC_YM2612, Addr >> 1, YM2612_Reg, Data);
	else
		YM2612_Reg = Data;
	
#ifndef DUAL_SUPPORT
	ym2612_w(0, Addr, Data);
#else
	ym2612_w(Addr >> 2, Addr & 0x03, Data);
#endif
	
	return;
}

// This doesn't get logged to VGM. Use it for DAC writes,
void YM2612_WriteA(u8 Addr, u8 Data)
{
#ifndef DUAL_SUPPORT
	ym2612_w(0, Addr, Data);
#else
	ym2612_w(Addr >> 2, Addr & 0x03, Data);
#endif
	
	return;
}

// Use to read from 0xA04000 (for timing)
u8 YM2612_Read(u8 Addr)
{
#ifndef DUAL_SUPPORT
	return ym2612_r(0, Addr);
#else
	return ym2612_r(Addr >> 2, Addr & 0x03);
#endif
}


// Use to write to 0xC00011
void PSG_Write(u8 data)
{
	vgm_write(VGMC_SN76496, 0, data, 0);
	
	sn764xx_w(0, 0, data);
	
	return;
}


// called when the last channel received an 95 flag
void StopSignal(void)
{
	vgm_dump_stop();
	LoopCntr = -1;
	WaitCntr = 0;
	
	return;
}

void LoopStartSignal(void)
{
	vgm_set_loop();
	LoopCntr = 1;
	
	return;
}

void LoopEndSignal(void)
{
	vgm_dump_stop();
	if (LoopCntr >= 2)
		FinishedSongSignal();
	LoopCntr ++;
	
	return;
}

void StartSignal(UINT8 SeqNum)
{
	if (LoopCntr < 0)
		PlayingTimer = 0;
	LoopCntr = 0;
	WaitCntr = -1;
	TrackChangeSignal(SeqNum);
	
	return;
}
