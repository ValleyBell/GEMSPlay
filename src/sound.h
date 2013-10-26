// PLEASE NOTE: This header should ALWAYS remain system independent,
// (using #if/#else blocks is OK, as long as it'll compile unmodified
// on all target platforms), though sound.c is platform specific; 
// please try to keep this order as much as possible!!
#ifndef _S2RAS_SOUND_H
#define _S2RAS_SOUND_H

typedef struct _DAC_SAMPLE
{
	char* File;
	UINT8* sample;
	UINT16 size;
	UINT8 Flags;	// 0x10 - Looping, 0x80 - 4-bit PCM
	UINT8 UsageID;
} DAC_SAMPLE;
typedef struct _dac_table
{
	UINT8 Sample;
	UINT8 Rate;
} DAC_TABLE;


// Initialization function (do not export these)
int sound_init();
//void sound_update();
void sound_pause(unsigned char PauseOn);
void sound_update(unsigned short *stream_buf, unsigned int samples);
void sound_cleanup();

// for loader.c
void DumpDACSounds(void);
void SetDACUsage(UINT8 SampleID);


#endif
