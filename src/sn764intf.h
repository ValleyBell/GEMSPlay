#pragma once

/*WRITE8_DEVICE_HANDLER( ym2413_w );

WRITE8_DEVICE_HANDLER( ym2413_register_port_w );
WRITE8_DEVICE_HANDLER( ym2413_data_port_w );

DEVICE_GET_INFO( ym2413 );
#define SOUND_YM2413 DEVICE_GET_INFO_NAME( ym2413 )*/

void sn764xx_stream_update(UINT8 ChipID, stream_sample_t *outputs, int samples);

int device_start_sn764xx(UINT8 ChipID, int clock, int rate, int shiftregwidth, int noisetaps);
void device_stop_sn764xx(UINT8 ChipID);
void device_reset_sn764xx(UINT8 ChipID);

void sn764xx_w(UINT8 ChipID, offs_t offset, UINT8 data);

void sn764xx_set_mute_mask(UINT8 ChipID, UINT32 MuteMask);
