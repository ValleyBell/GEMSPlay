/****************************************************************

    MAME / MESS functions

****************************************************************/

#include "mamedef.h"
//#include "sndintrf.h"
//#include "streams.h"
#include "sn76489.h"
#include "sn764intf.h"


#define NULL	((void *)0)

/* for stream system */
typedef struct _sn764xx_state sn764xx_state;
struct _sn764xx_state
{
	void *chip;
};

extern UINT32 SampleRate;
#define MAX_CHIPS	0x10
static sn764xx_state SN764xxData[MAX_CHIPS];

void sn764xx_stream_update(UINT8 ChipID, stream_sample_t *outputs, int samples)
{
	sn764xx_state *info = &SN764xxData[ChipID];
	SN76489_Update((SN76489_Context*)info->chip, outputs, samples);
}

int device_start_sn764xx(UINT8 ChipID, int clock, int rate, int shiftregwidth, int noisetaps)
{
	sn764xx_state *info;
	//int rate;
	
	if (ChipID >= MAX_CHIPS)
		return -1;
	
	info = &SN764xxData[ChipID];
	/* emulator create */
	//rate = SampleRate;
	info->chip = SN76489_Init(clock, rate);
	if (info->chip == NULL)
		return 1;
	SN76489_Config((SN76489_Context*)info->chip, noisetaps, shiftregwidth, 0);
 
	return 0;
}

void device_stop_sn764xx(UINT8 ChipID)
{
	sn764xx_state *info = &SN764xxData[ChipID];
	SN76489_Shutdown((SN76489_Context*)info->chip);
}

void device_reset_sn764xx(UINT8 ChipID)
{
	sn764xx_state *info = &SN764xxData[ChipID];
	SN76489_Reset((SN76489_Context*)info->chip);
}


void sn764xx_w(UINT8 ChipID, offs_t offset, UINT8 data)
{
	sn764xx_state *info = &SN764xxData[ChipID];
	switch(offset)
	{
	case 0x00:
		SN76489_Write((SN76489_Context*)info->chip, data);
		break;
	case 0x01:
		SN76489_GGStereoWrite((SN76489_Context*)info->chip, data);
		break;
	}
}


void sn764xx_set_mute_mask(UINT8 ChipID, UINT32 MuteMask)
{
	sn764xx_state *info = &SN764xxData[ChipID];
	SN76489_SetMute(info->chip, ~MuteMask & 0x0F);
	
	return;
}
