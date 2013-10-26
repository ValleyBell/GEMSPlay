#pragma once

#ifndef __VGMWRITE_H__
#define __VGMWRITE_H__

void MakeVgmFileName(const char* FileName, int TrackID);
int vgm_dump_start(void);
int vgm_dump_stop(void);
void vgm_update(void);
void vgm_write(unsigned char chip_type, unsigned char port, unsigned short int r, unsigned char v);
void vgm_write_large_data(unsigned char chip_type, unsigned char type, unsigned int datasize, unsigned int value1, unsigned int value2, const void* data);
void vgm_write_stream_data_command(unsigned char stream, unsigned char type, unsigned int data, unsigned char flags);
void vgm_set_loop(void);

// VGM Chip Constants
#define VGMC_SN76496	0x00
#define VGMC_YM2612		0x02

extern unsigned char Enable_VGMDumping;
extern unsigned char VGM_IgnoreWrt;

#endif /* __VGMWRITE_H__ */
