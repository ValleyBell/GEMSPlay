/***************************************************************************

  2612intf.c

  The YM2612 emulator supports up to 2 chips.
  Each chip has the following connections:
  - Status Read / Control Write A
  - Port Read / Data Write A
  - Control Write B
  - Data Write B

***************************************************************************/

#include "mamedef.h"
//#include "sndintrf.h"
//#include "streams.h"
//#include "sound/fm.h"
//#include "sound/2612intf.h"
#include "fm.h"
#include "2612intf.h"
#include <malloc.h>

#define NULL	((void *)0)

typedef struct _ym2612_state ym2612_state;
struct _ym2612_state
{
	//sound_stream *	stream;
	//emu_timer *		timer[2];
	void *			chip;
	const ym2612_interface *intf;
	//const device_config *device;
};

static stream_sample_t* DUMMYBUF[2] = {NULL, NULL};

#define MAX_CHIPS	0x02
static ym2612_state YM2612Data[MAX_CHIPS];

/*INLINE ym2612_state *get_safe_token(const device_config *device)
{
	assert(device != NULL);
	assert(device->token != NULL);
	assert(device->type == SOUND);
	assert(sound_get_type(device) == SOUND_YM2612 || sound_get_type(device) == SOUND_YM3438);
	return (ym2612_state *)device->token;
}*/

/*------------------------- TM2612 -------------------------------*/
/* IRQ Handler */
static void IRQHandler(void *param,int irq)
{
	ym2612_state *info = (ym2612_state *)param;
	//if(info->intf->handler) info->intf->handler(info->device, irq);
	if(info->intf->handler) info->intf->handler(irq);
}

/* Timer overflow callback from timer.c */
//static TIMER_CALLBACK( timer_callback_2612_0 )
/*void timer_callback_2612_0(void *ptr, int param)
{
	ym2612_state *info = (ym2612_state *)ptr;
	ym2612_timer_over(info->chip,0);
}

//static TIMER_CALLBACK( timer_callback_2612_1 )
void timer_callback_2612_1(void *ptr, int param)
{
	ym2612_state *info = (ym2612_state *)ptr;
	ym2612_timer_over(info->chip,1);
}*/

static void timer_handler(void *param,int c,int count,int clock)
{
	ym2612_state *info = (ym2612_state *)param;
	if( count == 0 )
	{	// Reset FM Timer
		//timer_enable(info->timer[c], 0);
	}
	else
	{	// Start FM Timer
		//attotime period = attotime_mul(ATTOTIME_IN_HZ(clock), count);
		//if (!timer_enable(info->timer[c], 1))
		//	timer_adjust_oneshot(info->timer[c], period, 0);
	}
}

/* update request from fm.c */
void ym2612_update_request(void *param)
{
	ym2612_state *info = (ym2612_state *)param;
	//stream_update(info->stream);
	
	ym2612_update_one(info->chip, DUMMYBUF, 0);
}

/***********************************************************/
/*    YM2612                                               */
/***********************************************************/

//static STREAM_UPDATE( ym2612_stream_update )
void ym2612_stream_update(UINT8 ChipID, stream_sample_t **outputs, int samples)
{
	//ym2612_state *info = (ym2612_state *)param;
	ym2612_state *info = &YM2612Data[ChipID];
	
	ym2612_update_one(info->chip, outputs, samples);
}


//static STATE_POSTLOAD( ym2612_intf_postload )
/*static void ym2612_intf_postload(UINT8 ChipID)
{
	//ym2612_state *info = (ym2612_state *)param;
	ym2612_state *info = &YM2612Data[ChipID];
	ym2612_postload(info->chip);
}*/


//static DEVICE_START( ym2612 )
int device_start_ym2612(UINT8 ChipID, int clock, int rate)
{
	static const ym2612_interface dummy = { 0 };
	//ym2612_state *info = get_safe_token(device);
	ym2612_state *info;
	//int rate;

	if (ChipID >= MAX_CHIPS)
		return -1;
	
	info = &YM2612Data[ChipID];
	//rate = clock/72;
	//info->intf = device->static_config ? (const ym2612_interface *)device->static_config : &dummy;
	info->intf = &dummy;
	//info->device = device;

	/* FM init */
	/* Timer Handler set */
	//info->timer[0] = timer_alloc(device->machine, timer_callback_2612_0, info);
	//info->timer[1] = timer_alloc(device->machine, timer_callback_2612_1, info);

	/* stream system initialize */
	//info->stream = stream_create(device,0,2,rate,info,ym2612_stream_update);

	/**** initialize YM2612 ****/
	//info->chip = ym2612_init(info,clock,rate,timer_handler,IRQHandler);
	info->chip = ym2612_init(info,clock,rate,NULL,NULL);
	if (info->chip == NULL)
		return 1;
	//assert_always(info->chip != NULL, "Error creating YM2612 chip");
	//ym2612_postload(info->chip);

	//state_save_register_postload(device->machine, ym2612_intf_postload, info);
	//ym2612_intf_postload();
	device_reset_ym2612(ChipID);
	
	return 0;
}


//static DEVICE_STOP( ym2612 )
void device_stop_ym2612(UINT8 ChipID)
{
	//ym2612_state *info = get_safe_token(device);
	ym2612_state *info = &YM2612Data[ChipID];
	ym2612_shutdown(info->chip);
}

//static DEVICE_RESET( ym2612 )
void device_reset_ym2612(UINT8 ChipID)
{
	//ym2612_state *info = get_safe_token(device);
	ym2612_state *info = &YM2612Data[ChipID];
	ym2612_reset_chip(info->chip);
}


//READ8_DEVICE_HANDLER( ym2612_r )
UINT8 ym2612_r(UINT8 ChipID, offs_t offset)
{
	//ym2612_state *info = get_safe_token(device);
	ym2612_state *info = &YM2612Data[ChipID];
	return ym2612_read(info->chip, offset & 3);
}

//WRITE8_DEVICE_HANDLER( ym2612_w )
void ym2612_w(UINT8 ChipID, offs_t offset, UINT8 data)
{
	//ym2612_state *info = get_safe_token(device);
	ym2612_state *info = &YM2612Data[ChipID];
	ym2612_write(info->chip, offset & 3, data);
}


/*READ8_DEVICE_HANDLER( ym2612_status_port_a_r ) { return ym2612_r(device, 0); }
READ8_DEVICE_HANDLER( ym2612_status_port_b_r ) { return ym2612_r(device, 2); }
READ8_DEVICE_HANDLER( ym2612_data_port_a_r ) { return ym2612_r(device, 1); }
READ8_DEVICE_HANDLER( ym2612_data_port_b_r ) { return ym2612_r(device, 3); }

WRITE8_DEVICE_HANDLER( ym2612_control_port_a_w ) { ym2612_w(device, 0, data); }
WRITE8_DEVICE_HANDLER( ym2612_control_port_b_w ) { ym2612_w(device, 2, data); }
WRITE8_DEVICE_HANDLER( ym2612_data_port_a_w ) { ym2612_w(device, 1, data); }
WRITE8_DEVICE_HANDLER( ym2612_data_port_b_w ) { ym2612_w(device, 3, data); }*/
UINT8 ym2612_status_port_a_r(UINT8 ChipID, offs_t offset)
{
	return ym2612_r(ChipID, 0);
}
UINT8 ym2612_status_port_b_r(UINT8 ChipID, offs_t offset)
{
	return ym2612_r(ChipID, 2);
}
UINT8 ym2612_data_port_a_r(UINT8 ChipID, offs_t offset)
{
	return ym2612_r(ChipID, 1);
}
UINT8 ym2612_data_port_b_r(UINT8 ChipID, offs_t offset)
{
	return ym2612_r(ChipID, 3);
}

void ym2612_control_port_a_w(UINT8 ChipID, offs_t offset, UINT8 data)
{
	ym2612_w(ChipID, 0, data);
}
void ym2612_control_port_b_w(UINT8 ChipID, offs_t offset, UINT8 data)
{
	ym2612_w(ChipID, 2, data);
}
void ym2612_data_port_a_w(UINT8 ChipID, offs_t offset, UINT8 data)
{
	ym2612_w(ChipID, 1, data);
}
void ym2612_data_port_b_w(UINT8 ChipID, offs_t offset, UINT8 data)
{
	ym2612_w(ChipID, 3, data);
}


void ym2612_set_mute_mask(UINT8 ChipID, UINT32 MuteMask)
{
	ym2612_state *info = &YM2612Data[ChipID];
	ym2612_set_mutemask(info->chip, MuteMask);
}



/**************************************************************************
 * Generic get_info
 **************************************************************************/

/*DEVICE_GET_INFO( ym2612 )
{
	switch (state)
	{
		// --- the following bits of info are returned as 64-bit signed integers ---
		case DEVINFO_INT_TOKEN_BYTES:					info->i = sizeof(ym2612_state);				break;

		// --- the following bits of info are returned as pointers to data or functions ---
		case DEVINFO_FCT_START:							info->start = DEVICE_START_NAME( ym2612 );	break;
		case DEVINFO_FCT_STOP:							info->stop = DEVICE_STOP_NAME( ym2612 );	break;
		case DEVINFO_FCT_RESET:							info->reset = DEVICE_RESET_NAME( ym2612 );	break;

		// --- the following bits of info are returned as NULL-terminated strings ---
		case DEVINFO_STR_NAME:							strcpy(info->s, "YM2612");					break;
		case DEVINFO_STR_FAMILY:					strcpy(info->s, "Yamaha FM");				break;
		case DEVINFO_STR_VERSION:					strcpy(info->s, "1.0");						break;
		case DEVINFO_STR_SOURCE_FILE:						strcpy(info->s, __FILE__);					break;
		case DEVINFO_STR_CREDITS:					strcpy(info->s, "Copyright Nicola Salmoria and the MAME Team"); break;
	}
}


DEVICE_GET_INFO( ym3438 )
{
	switch (state)
	{
		// --- the following bits of info are returned as NULL-terminated strings ---
		case DEVINFO_STR_NAME:							strcpy(info->s, "YM3438");							break;

		default:										DEVICE_GET_INFO_CALL(ym2612);						break;
	}
}*/
