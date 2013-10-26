// GEMS Player
// -----------
// ported from the GEMS 2.5 ASM source to C by Valley Bell
//
// Note: The original port was based on GEMS 2.0 and was later updated to
//       include v2.5 changes.
//       There's also support for the GEMS 2.8 sequence pointer format.

#include <memory.h>
#include "stdtype.h"
#define NULL	(void*)0

extern int __cdecl printf(const char *, ...);

extern void YM2612_Write(UINT8 Addr, UINT8 Data);
extern void YM2612_WriteA(UINT8 Addr, UINT8 Data);
extern UINT8 YM2612_Read(UINT8 Addr);
extern void PSG_Write(UINT8 Data);

extern void DACPlay(UINT8 Sample, UINT8 SmpFlags);	// for VGM logging
extern void DACStop(UINT8 FinishLoop);	// for VGM logging

static void CheckForSongEnd(void);
extern void StartSignal(UINT8 SeqNum);
extern void StopSignal(void);

/*UINT8 ym2612_r(UINT8 ChipID, UINT32 offset);
void ym2612_w(UINT8 ChipID, UINT32 offset, UINT8 data);
void sn764xx_w(UINT8 ChipID, UINT32 offset, UINT8 data);
#define YM2612_Read(Addr)			ym2612_r(0, Addr)
#define YM2612_Write(Addr, Data)	ym2612_w(0, Addr, Data)
#define PSG_Write(Data)				sn764xx_w(0, 0, Data)*/

// You can enable the DACME commands they spammed everywhere by activating this line.
// But you don't need it and you don't want it. Really.
//#define DACxME()	DACME()
#define DACxME()
//static void DACxME(void)
//{}


static __inline UINT16 Read16Bit(const UINT8* Data);
static __inline void Write16Bit(UINT8* Data, const UINT16 Value);
static __inline UINT32 Read24Bit(const UINT8* Data);
static __inline void Write24Bit(UINT8* Data, const UINT32 Value);
static __inline void FMWrite(UINT8 Addr, UINT8 Reg, UINT8 Data);
static __inline void FMWr(UINT8 Addr, UINT8 Channel, UINT8 Reg, UINT8 Data);
static __inline void FMWrgl(UINT8 Reg, UINT8 Data);

void VBLINT(void);
static void CHECKTICK(void);
static void DOPSGENV(void);

static UINT8 GETCBYTE(void);
//static void XFER68K(UINT8* DstPtr, UINT32 SrcAddr, UINT8 BytCount);
static void XFER68K(UINT8* DstPtr, const UINT8* SrcData, UINT32 SrcAddr, UINT8 BytCount);
void DACME(void);
static void FILLDACFIFO(UINT8 ForceFill);

static UINT8 GETSBYTE(UINT8* ChnCCB, UINT8* CHBUFPTR);
static void UPDSEQ(void);
static void SEQUENCER(UINT8* ChnCCB, UINT8* CHBUFPTR, UINT8 CurChn);
static UINT8 seqcmd(UINT8* ChnCCB, UINT8* CHBUFPTR, UINT8 CurChn, UINT8 CmdByte);
static void VTIMER(void);
static void vtimerloop(UINT8 VoiceType, UINT8* VTblPtr);

static UINT8 GETCCBPTR(UINT8** RetCCB);
static UINT8* GETCCBPTR2(UINT8 Channel);
void gems_init(void);
void gems_loop(void);
static void RESUMEALL(void);
void STARTSEQ(UINT8 SeqNum);
static void STOPSEQ(UINT8 SeqNum);
static void PAUSESEQ(UINT8 SeqNum);
static void CLIPALL(void);
static void CLIPLOOP(UINT8* VTblPtr, UINT8 Mode);
static void SETTEMPO(UINT8 BPM);
static void TRIGENV(UINT8* ChnCCB, UINT8 MidChn, UINT8 EnvNum);
static void DOENVELOPE(void);
static void DOPITCHBEND(UINT8 CurChn);
static void APPLYBEND(void);
static UINT16 GETFREQ(UINT8 VType, UINT8 Channel, UINT8 Note, UINT8* PbPtr);
static UINT8* GETPATPTR(UINT8 Channel);
//static UINT16 MULTIPLY(UINT8 Val8, UINT16 Val16);
static void NOTEON(UINT8 MidChn, UINT8 Note, UINT8* ChnCCB);
static void noteonpsg(UINT8 MidChn, UINT8* ChnCCB, UINT8 Mode);
static void noteondig(UINT8 MidChn, UINT8* ChnCCB);
static void noteonfm(UINT8 MidChn, UINT8* ChnCCB);
static void WRITEFM(const UINT8* InsList, UINT8 Bank, UINT8 Channel);
static void VTANDET(UINT8* ChnCCB, UINT8* VTblPtr, UINT8 VFlags);
static void NOTEOFF(UINT8 MidChn, UINT8 NoteNum);
static void NOTEOFFDIG(void);
static void PCHANGE(void);
static void FETCHPATCH(UINT8* ChnCCB);
static void PATCHLOAD(UINT8 InsNum);

static UINT8 ALLOC(UINT8 MidChn, UINT8* ChnCCB, UINT8* VTblPtr, UINT8** RetAlloc);
static UINT8 ALLOCSPEC(UINT8* ChnCCB, UINT8* VTblPtr, UINT8** RetAlloc);
static UINT8 DEALLOC(UINT8 MidChn, UINT8 Note, UINT8* VTblPtr);

#pragma pack(1)
static struct
{
	UINT8 psgcom[4];		// [0009]  0 command 1 = key on, 2 = key off, 4 = stop snd
	UINT8 psglev[4];		// [000D]  4 output level attenuation (4 bit) [default: 0xFF]
	UINT8 psgatk[4];		// [0011]  8 attack rate
	UINT8 psgdec[4];		// [0015] 12 decay rate
	UINT8 psgslv[4];		// [0019] 16 sustain level attenuation
	UINT8 psgrrt[4];		// [001D] 20 release rate
	UINT8 psgenv[4];		// [0021] 24 envelope mode 0 = off, 1 = attack, 2 = decay, 3 = sustain, 4
	UINT8 psgdtl[4];		// [0025] 28 tone bottom 4 bits, noise bits
	UINT8 psgdth[4];		// [0029] 32 tone upper 6 bits
	UINT8 psgalv[4];		// [002D] 36 attack level attenuation
	UINT8 whdflg[4];		// [0031] 40 flags to indicate hardware should be updated
} pdata;
#pragma pack()
//static UINT8 unused;				// [0035]
static volatile UINT8 CMDWPTR;	// [0036] cmd fifo wptr @ $36
static UINT8 CMDRPTR;				// [0037] read pointer @ $37
static UINT8 NewCmdWPtr;

enum
{
	COM		= 0,
	LEV		= 4,
	ATK		= 8,
	DKY		= 12,
	SLV		= 16,
	RRT		= 20,
	MODE	= 24,
	DTL		= 28,
	DTH		= 32,
	ALV		= 36,
	FLG		= 40
};

// VBLINT Variables
static UINT8 TICKFLG[2];	// [003E] (TICKFLG+1) set by VBLINT
static UINT8 TICKCNT;		// [0040] tick accumulated by CHECKTICK

//static UINT16 DACMEJRINST;	// [02B3] for saving the DACME inst for slow sample rates
//static UINT16 DACME4BINST;	// [02B5] for saving the DACMEPROC inst for processing
// [1. We need only one byte here to decide what to do.
//  2. Both values are always constant (JR and an address from the code), so using defines works well here.]
#define DACMEJRINST	0x18	// 0x18 = JR
#define DACME4BINST	0x18	// 0x18 = JR

// FILLDACFIFO Variables
static UINT8 DACFIFOWPTR;	// [02FB]
static UINT8 SAMPLEPTR[3];	// [02FC]
static UINT16 SAMPLECTR;	// [02FF]
static UINT8 FDFSTATE;		// [0301]

/* CCB Entries:
*		2,1,0	tag addr of 1st byte in 32-byte channel buffer
*		5,4,3	addr of next byte to fetch
*						so: 0 <= addr-tag <= 31 means hit in buffer
*		6		flags
*		8,7		timer (contains 0-ticks to delay)
*		10,9	delay
*		12,11	duration */
enum
{
	CCBTAGL		=  0,	// lsb of addr of 1st byte in 32-byte sequence buffer
	CCBTAGM		=  1,	// mid of "
	CCBTAGH		=  2,	// msb of "
	CCBADDRL	=  3,	// lsb of addr of next byte to read from sequence
	CCBADDRM	=  4,	// mid of "
	CCBADDRH	=  5,	// msb of "
	CCBFLAGS	=  6,	// 80 = sustain
						// 40 = env retrigger
						// 20 = lock (for 68k based sfx)
						// 10 = running (not paused)
						// 08 = use sfx (150 bpm) timebase
						// 02 = muted (running, but not executing note ons)
						// 01 = in use
	CCBTIMERL	=  7,	// lsb of 2's comp, subbeat (1/24th) timer till next event
	CCBTIMERH	=  8,	// msb of "
	CCBDELL		=  9,	// lsb of registered subbeat delay value
	CCBDELH		= 10,	// msb of "
	CCBDURL		= 11,	// lsb of registered subbeat duration value
	CCBDURH		= 12,	// msb of "
	CCBPNUM		= 13,	// program number (patch)
	CCBSNUM		= 14,	// sequence number (in sequence bank)
	CCBVCHAN	= 15,	// MIDI channel number within sequence CCBSNUM
	CCBLOOP0	= 16,	// loop stack (counter, lsb of start addr, mid of start addr)
	CCBLOOP1	= 19,
	CCBLOOP2	= 22,
	CCBLOOP3	= 25,
	CCBPRIO		= 28,	// priority (0 lowest, 127 highest)
	CCBENV		= 29,	// envelope number
	CCBATN		= 30,	// channel attenuation (0=loud, 127=quiet)
	CCBy		= 31
};

// UPDSEQ Variables
static UINT8* CHPATPTR;	// [0456] pointer to current channel's patch buffer

// main Variables
static UINT16 SBPT;		// [08BE] sub beats per tick (8frac), default is 120bpm [default: 204]
static UINT16 SBPTACC;		// [08C0] accumulates ^^ each tick to track sub beats
static UINT8 TBASEFLAGS;	// [08C2]

#pragma pack(1)
struct
{
	UINT8 PTBL68K[3];		// [0AA5] 24-bit 68k space pointer to patch table
	UINT8 ETBL68K[3];		// [0AA8] 24-bit 68k space pointer to envelope table
	UINT8 STBL68K[3];		// [0AAB] 24-bit 68k space pointer to sequence table
	UINT8 DTBL68K[3];		// [0AAE] 24-bit 68k space pointer to digital sample table
} Tbls;
#pragma pack()

// DOENVELOPE Variables
static UINT8 ECB[29] =		// [0EAB]
{	0x40, 0x40, 0x40, 0x40,	// 4 envelopes worth of control blocks (ECB's)
	0xFF,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x80, 0xA0, 0xC0, 0xE0};
enum
{
	ECBCHAN		=  0,			// offset to 4 envelopes' channel numbers and flags
								// [7]=eot, [6]=free, [5]=sfx tempo
	ECBPTRL		=  5,			//		 "				  segment ptr LSBs
	ECBPTRH		=  9,			//		 "				  segment ptr MSBs
	ECBCTR		= 13,			// 		 "				  segment ctrs
	ECBDELL		= 17,			//		 "				  segment delta LSBs
	ECBDELH		= 21,			//		 "				  segment delta MSBs
	ECBBUFP		= 25,			// LSB of pointer to 32 byte envelope buffer
};

// DOPITCHBEND Variables
static UINT8 NEEDBEND;		// [0F75] set to 1 to trigger a need to bend
static UINT8 PBTBL[16*5];	// [0F76]	// pitch bend LSB
										// pitch bend MSB
										// env bend LSB
										// env bend MSB
										// [0]=apply bend - set by pbend/mod,
										// cleared by applybend
enum
{
	PBPBL		=  0,			// offset in PBTBL to 16 channels' pitchbend LSB
	PBPBH		= 16,			// offset in PBTBL to 16 channels' pitchbend MSB
	PBEBL		= 32,			// offset in PBTBL to 16 channels' envelopebend LSB
	PBEBH		= 48,			// offset in PBTBL to 16 channels' envelopebend MSB
	PBRETRIG	= 64,			// offset in PBTBL to 16 channels' retrigger flag
};

// NOTEON Variables
// fmftbl contains a 16 bit freq number for each half step in a single octave (C-C)
static UINT16 fmftbl[13] =		// [1168]
	{644, 682, 723, 766, 811, 859, 910, 965, 1022, 1083, 1147, 1215, 1288};
// psgftbl contains the 16 bit wavelength numbers for the notes A2 thru B7 (33-95)
static UINT16 psgftbl[64] =	// [1182]
{	        0x03F9, 0x03C0, 0x038A,	// A2 > B2
	
	0x0357, 0x0327, 0x02FA, 0x02CF,	// C3 > B3
	0x02A7, 0x0281, 0x025D, 0x023B,
	0x021B, 0x01FC, 0x01E0, 0x01C5,
	
	0x01AC, 0x0194, 0x017D, 0x0168,	// C4 > B4
	0x0153, 0x0140, 0x012E, 0x011D,
	0x010D, 0x00FE, 0x00F0, 0x00E2,
	
	0x00D6, 0x00CA, 0x00BE, 0x00B4,	// C5 > B5
	0x00AA, 0x00A0, 0x0097, 0x008F,
	0x0087, 0x007F, 0x0078, 0x0071,
	
	0x006B, 0x0065, 0x005F, 0x005A,	// C6 > B6
	0x0055, 0x0050, 0x004C, 0x0047,
	0x0043, 0x0040, 0x003C, 0x0039,
	
	0x0035, 0x0032, 0x002F, 0x002D,	// C7 > B7 (not very accurate!)
	0x002A, 0x0028, 0x0026, 0x0023,
	0x0021, 0x0020, 0x001E, 0x001C,

	0x001C};						// extra value for interpolation of B7
//	0x001B};	// this would be better, IMO

#pragma pack(1)
static struct
{
	UINT8 note;				// [1202] note on note (keep these together - stored as BC) [noteonnote]
	UINT8 ch;				// [1203] note on channel [noteonch]
	UINT8 voice;			// [1204] allocated voice [noteonvoice]
	UINT8 atten;			// [1205] allocated voice [noteonatten]
} noteon;

static struct
{
	// Note: XFER is used on these variables.
	UINT8 FLAGS;			// [1421]
	UINT8 PTR[3];			// [1422]
	UINT16 SKIP;			// [1425]
	UINT16 FIRST;			// [1427]
	UINT16 LOOP;			// [1429]
	UINT16 END;				// [142B]
} SAMP;
#pragma pack()
static UINT16 noteonffreq;	// [142D]

static UINT8 CARRIERTBL[8] =	// [15C6] 
{	0x08,	// alg 0, op 4 is carrier
	0x08,	// alg 1, op 4 is carrier
	0x08,	// alg 2, op 4 is carrier
	0x08,	// alg 3, op 4 is carrier
	0x0A,	// alg 4, op 2 and 4 are carriers
	0x0E,	// alg 5, op 2 and 3 and 4 are carriers
	0x0E,	// alg 6, op 2 and 3 and 4 are carriers
	0x0F};	// alg 7, all ops carriers

static UINT8 CARRIERS;		// [15CE] 

static UINT8 MASTERATN;	// [15CF] master attenuation is 7 frac bits (0 = full volume)

// WRITEFM Variables
static UINT8 FMADDRTBL[0x3D] =	// [161C]
{	0xB0,   2,	// set feedback, algorithm
	0xB4,   3,	// set output, ams, fms
	0x30,   4,	// operator 1 - set detune, mult
	0x40, 133,	//5+128		// set total level
	0x50,   6,	// set rate scaling, attack rate
	0x60,   7,	// set am enable, decay rate
	0x70,   8,	// set sustain decay rate
	0x80,   9,	// set sustain level, release rate
	0x90,   0,	// set proprietary register
	0x38,  16,	// operator 2 - set detune, mult
	0x48, 145,	//17+128	// set total level
	0x58,  18,	// set rate scaling, attack rate
	0x68,  19,	// set am enable, decay rate
	0x78,  20,	// set sustain decay rate
	0x88,  21,	// set sustain level, release rate
	0x98,   0,	// set proprietary register
	0x34,  10,	// operator 3 - set detune, mult
	0x44, 139,	//11+128	// set total level
	0x54,  12,	// set rate scaling, attack rate
	0x64,  13,	// set am enable, decay rate
	0x74,  14,	// set sustain decay rate
	0x84,  15,	// set sustain level, release rate
	0x94,   0,	// set proprietary register
	0x3C,  22,	// operator 4 - set detune, mult
	0x4C, 151,	//23+128	// set total level
	0x5C,  24,	// set rate scaling, attack rate
	0x6C,  25,	// set am enable, decay rate
	0x7C,  26,	// set sustain decay rate
	0x8C,  27,	// set sustain level, release rate
	0x9C,   0,	// set proprietary register
	0x00};

// Dynamic Voice Allocation
/*  FMVTBL - contains (6) 7-byte entires, one per voice:
*	byte 0: FRLxxVVV	flag byte, where F=free, R=release phase, L=locked, VVV=voice num
*						VVV is numbered (0,1,2,4,5,6) for writing directly to key on/off reg
*          [FRLTSVVV    T - self-timed note, S - SFX tempo]
*	byte 1: priority	only valid for in-use (F=0) voices
*	byte 2: notenum	    "
*	byte 3: channel	    "
*	byte 4: lsb of duration timer (for sequenced notes)
*	byte 5: msb of duration timer
*	byte 6: release timer */

#ifndef DUAL_SUPPORT
static UINT8 FMVTBL[43] =	// [1791]
{	0x80, 0, 0x50, 0, 0, 0, 0,		// fm voice 0
	0x81, 0, 0x50, 0, 0, 0, 0,		// fm voice 1
	0x84, 0, 0x50, 0, 0, 0, 0,		// fm voice 3
	0x85, 0, 0x50, 0, 0, 0, 0,		// fm voice 4
	0x86, 0, 0x50, 0, 0, 0, 0,		// [159D, FMVTBLCH6] fm voice 5 (supports digital)
	0x82, 0, 0x50, 0, 0, 0, 0,		// [15A4, FMVTBLCH3] fm voice 2 (supports CH3 poly mode)
	0xFF};
static UINT8* FMVTBLCH6 = &FMVTBL[4*7];
static UINT8* FMVTBLCH3 = &FMVTBL[5*7];
#else
static UINT8 FMVTBL[85] =		// [1791]
{	0x80, 0, 0x50, 0, 0, 0, 0,		// fm voice 0
	0x81, 0, 0x50, 0, 0, 0, 0,		// fm voice 1
	0x84, 0, 0x50, 0, 0, 0, 0,		// fm voice 3
	0x85, 0, 0x50, 0, 0, 0, 0,		// fm voice 4
	0x80, 0, 0x50, 0x80, 0, 0, 0,		// fm voice 1-0
	0x81, 0, 0x50, 0x80, 0, 0, 0,		// fm voice 1-1
	0x84, 0, 0x50, 0x80, 0, 0, 0,		// fm voice 1-3
	0x85, 0, 0x50, 0x80, 0, 0, 0,		// fm voice 1-4
	0x86, 0, 0x50, 0x80, 0, 0, 0,		// fm voice 1-5
	0x82, 0, 0x50, 0x80, 0, 0, 0,		// fm voice 1-2
	0x86, 0, 0x50, 0, 0, 0, 0,		// [159D, FMVTBLCH6] fm voice 5 (supports digital)
	0x82, 0, 0x50, 0, 0, 0, 0,		// [15A4, FMVTBLCH3] fm voice 2 (supports CH3 poly mode)
	0xFF};
static UINT8* FMVTBLCH6 = &FMVTBL[10*7];
static UINT8* FMVTBLCH3 = &FMVTBL[11*7];
#endif

static UINT8 PSGVTBL[22] =		// [17BC]
{	0x80, 0, 0x50, 0, 0, 0, 0,	// normal type voice, number 0
	0x81, 0, 0x50, 0, 0, 0, 0,	// normal type voice, number 1
	0x82, 0, 0x50, 0, 0, 0, 0,	// [15BA, PSGVTBLTG3] normal type voice, number 2
	0xFF};
static UINT8* PSGVTBLTG3 = &PSGVTBL[2*7];

static UINT8 PSGVTBLNG[8] =	// [17D2]
{	0x83, 0, 0x50, 0, 0, 0, 0,	// noise type voice, number 3
	0xFF};

enum
{
	VTBLFLAGS	= 0,
	VTBLPRIO	= 1,
	VTBLNOTE	= 2,	// [Note: The VTBLNOTE constant is missing in the original source code.]
	VTBLCH		= 3,
	VTBLDL		= 4,
	VTBLDH		= 5,
	VTBLRT		= 6
};
#define CHIP_BNK(x)	((x[VTBLCH] & 0x80) >> 5)


#define CCHSIZE		256			// [channel cache size, in bytes - makes resizing CH0BUF easier]

// DATA AREA
static UINT8 PATCHDATA[39*16];	// [1886]
static UINT8 MBOXES[32];		// [1B20] 32 bytes for mail boxes
       UINT8 CMDFIFO[64];		// [1B40] command fifo - 64 bytes
static UINT8 CCB[512];			// [1B80] CCB - 512 bytes
static UINT8 CH0BUF[CCHSIZE];	// [1D80] channel cache - 256 bytes [Note: 512 bytes in v2.0]
static UINT8 ENV0BUF[128];		// [1E80] envelope buffers - 128 bytes
static UINT8 DACFIFO[256];		// [1F00] DAC data FIFO - 256 bytes


static UINT8 DacMeEn;		// [02A3] is either RET (C9, disabled), EXX (D9, enabled) or JR (18, enable next time)
static UINT8 DacMeProc;		// [02C4] is either NOP (00, disabled) or JR (18, enabled)
static UINT8 DacMeRet;		// [02CF] is either RET (C9, normal) or NOP (00, extra delay)
static UINT8 Ch3ModeReg;	// register B, used by DACME
static UINT8 DacCtrlPat;	// register C, used by DACME
static UINT8 DacFifoReg;	// register DE, used by DACME (D is always 1F)
static UINT8 FillDacEn;		// [0302] is either RET (C9, disabled) or NOP (00, enabled)

static const UINT8* ROMData;
static const UINT8* PData;
static const UINT8* EData;
static const UINT8* SData;
static const UINT8* DData;
static const UINT8 NullData[0x10] =
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 SongAlmostEnd;
UINT8 Gems28Mode = 0x00;

static __inline UINT16 Read16Bit(const UINT8* Data)
{
	return	(Data[0x00] << 0) |
			(Data[0x01] << 8);
}

static __inline void Write16Bit(UINT8* Data, const UINT16 Value)
{
	Data[0x00] = (Value & 0x00FF) >> 0;
	Data[0x01] = (Value & 0xFF00) >> 8;
	
	return;
}

static __inline UINT32 Read24Bit(const UINT8* Data)
{
	return	(Data[0x00] <<  0) |
			(Data[0x01] <<  8) |
			(Data[0x02] << 16);
}

static __inline void Write24Bit(UINT8* Data, const UINT32 Value)
{
	Data[0x00] = (Value & 0x0000FF) >>  0;
	Data[0x01] = (Value & 0x00FF00) >>  8;
	Data[0x02] = (Value & 0xFF0000) >> 16;
	
	return;
}

static __inline void FMWrite(UINT8 Addr, UINT8 Reg, UINT8 Data)
{
	YM2612_Write(Addr | 0, Reg);
	YM2612_Write(Addr | 1, Data);
	
	return;
}

static __inline void FMWr(UINT8 Addr, UINT8 Channel, UINT8 Reg, UINT8 Data)
{
	YM2612_Write(Addr | 0, Reg | Channel);
	YM2612_Write(Addr | 1, Data);
	
	return;
}

static __inline void FMWrgl(UINT8 Reg, UINT8 Data)
{
	YM2612_Write(0, Reg);
	YM2612_Write(1, Data);
	
	return;
}

void VBLINT(void)
{
	Write16Bit(TICKFLG, 0x1A20);
}

static void CHECKTICK(void)
{
	UINT8 TempByt;
	
	// disable Interrupts
	// save AF/HL
	
	TempByt = TICKFLG[1];
	if (! TempByt)
		return;	//jr ctnotick
	
	TICKFLG[1] = 0x00;
	TICKCNT ++;
	
	DACxME();
	
	SBPTACC += SBPT;	// add sub beats per tick to its accumulator
						// this is all 8 frac bits, so (SBPTACC+1) is the # of subbeats gone by
	
	DACxME();
	
	//ctnotick:
	// restore AF/HL
	// enable Interrupts
	return;
}

static void DOPSGENV(void)
{
	UINT8 CurChn;
	UINT8 CmdMask;
	UINT8 CmdBits;
	UINT8 TempByt;
	INT16 CurVol;
	UINT8 NoteEnd;
	
	NoteEnd = 0x00;	// [not in actual code - for VGM logging]
	CmdMask = 0x80;	// load command mask
	for (CurChn = 0; CurChn < 4; CurChn ++, CmdMask += 0x20)
	{
		//vloop:
		DACxME();
		CmdBits = pdata.psgcom[CurChn];		// load command bits [IY+COM]
		pdata.psgcom[CurChn] = 0x00;		// clear command bits
		
		//stop:
		if (CmdBits & 0x04)					// test bit 2
		{
			pdata.psglev[CurChn] = 0xFF;	// reset output level [IY+LEV]
			pdata.whdflg[CurChn] = 0x01;	// flag hardware update [IY+FLG]
			pdata.psgenv[CurChn] = 0x00;	// shut off envelope processing [IY+MODE]
			if (CurChn == 3)				// was this TG4 (noise)
				PSGVTBLTG3[VTBLFLAGS] &= ~0x20;	// yes - clear locked bit in TG3
		}
		//ckof:
		if (CmdBits & 0x02)					// test bit 1
		{
			if (pdata.psgenv[CurChn])		// load envelope mode [IY+MODE], check for key on
			{
				pdata.whdflg[CurChn] = 0x01;	// flag hardware update [IY+FLG]
				pdata.psgenv[CurChn] = 0x04;	// switch to envelope release phase [IY+MODE]
			}
		}
		//ckon:
		if (CmdBits & 0x01)					// test bit 0
		{
			pdata.psglev[CurChn] = 0xFF;	// reset level [IY+LEV]
			TempByt = pdata.psgdtl[CurChn] | CmdMask;	// load tone lsb, mix with command stuff
			PSG_Write(TempByt);				// write tone lsb or noise data
			
			if (CurChn != 3)				// check for last channel ***BAS***
				PSG_Write(pdata.psgdth[CurChn]);	// load + write tone msb
			//nskip:
			pdata.whdflg[CurChn] = 0x01;	// flag hardware update [IY+FLG]
			pdata.psgenv[CurChn] = 0x01;	// shut off envelope processing [IY+MODE]
		}
		
		//envproc:
		DACxME();
		switch(pdata.psgenv[CurChn])		// [IY+MODE]
		{
		case 0:	// test for on/off
			break;	// off.
		case 1:	// attack mode?
			//mode1:
			pdata.whdflg[CurChn] = 0x01;							// flag hardware update [IY+FLG]
			CurVol = pdata.psglev[CurChn] - pdata.psgatk[CurChn];	// load level [IY+LEV], subtract attack rate [IY+ATK]
			// [Note: The original code just tests for Carry and Zero flags of the UINT8 subtraction.]
			if (CurVol > 0 && CurVol > pdata.psgalv[CurChn])		// attack finished, [IY+ALV]
			{
				pdata.psglev[CurChn] = (UINT8)CurVol;				// save new level [IY+LEV]
			}
			else
			{
				//atkend:
				pdata.psglev[CurChn] = pdata.psgalv[CurChn];		// save attack level as new level [IY+LEV]
				pdata.psgenv[CurChn] = 2;							// switch to decay mode [IY+MODE]
			}
			break;
		case 2:	// decay mode?
			//mode2:
			pdata.whdflg[CurChn] = 0x01;		// flag hardware update [IY+FLG]
			if (pdata.psglev[CurChn] < pdata.psgslv[CurChn])		// compare level [IY+LEV] and sustain level [IY+SLV]
			{
				// add to decay
				//dkadd:
				CurVol = pdata.psglev[CurChn] + pdata.psgdec[CurChn];		// add decay rate [IY+DKY]
				if (CurVol < 0x100 && CurVol < pdata.psgslv[CurChn])
				{
					//dksav:
					pdata.psglev[CurChn] = (UINT8)CurVol;			// save new level [IY+LEV]
				}
				else
				{
					// caused a wrap - we're done || decay finished
					//dkyend:
					pdata.psglev[CurChn] = pdata.psgslv[CurChn];	// save sustain level [IY+LEV]
					pdata.psgenv[CurChn] = 3;						// set sustain mode [IY+MODE]
				}
			}
			else if (pdata.psglev[CurChn] == pdata.psgslv[CurChn])
			{
				// decay finished
				//dkyend:
				pdata.psglev[CurChn] = pdata.psgslv[CurChn];		// save sustain level [IY+LEV]
				pdata.psgenv[CurChn] = 3;							// set sustain mode [IY+MODE]
			}
			else
			{
				CurVol = pdata.psglev[CurChn] - pdata.psgdec[CurChn];	// subtract decay rate [IY+DKY]
				if (CurVol >= 0x00 && CurVol >= pdata.psgslv[CurChn])
				{
					//dksav:
					pdata.psglev[CurChn] = (UINT8)CurVol;			// save new level [IY+LEV]
				}
				else
				{
					// caused a wrap - we're done || decay finished
					//dkyend:
					pdata.psglev[CurChn] = pdata.psgslv[CurChn];	// save sustain level [IY+LEV]
					pdata.psgenv[CurChn] = 3;						// set sustain mode [IY+MODE]
				}
			}
			break;
		case 3:
			// [Sustain phase, there's no code for it since the level is held.]
			break;
		case 4:	// check for sustain phase
			//mode4:
			pdata.whdflg[CurChn] = 0x01;							// flag hardware update [IY+FLG]
			CurVol = pdata.psglev[CurChn] + pdata.psgrrt[CurChn];	// load level [IY+LEV], add release rate [IY+RRT]
			// [Note: The original code just tests for the Carry flag of the UINT8 addition.]
			if (CurVol < 0x100)
			{
				pdata.psglev[CurChn] = (UINT8)CurVol;				// save new level [IY+LEV]
			}
			else
			{
				//killenv:
				pdata.psglev[CurChn] = 0xFF;						// reset level [IY+LEV]
				pdata.psgenv[CurChn] = 0;							// reset envelope mode [IY+MODE]
				if (CurChn == 3)									// was this TG4 we just killed?
					PSGVTBLTG3[VTBLFLAGS] &= ~0x20;					// yes - clear locked bit in TG3
				
				NoteEnd ++;
			}
			break;
		}
		
		//vedlp:
	}
	
	DACxME();
	
	CmdMask = 0x90;	// set command bits
	for (CurChn = 0; CurChn < 4; CurChn ++, CmdMask += 0x20)
	{
		if (! (pdata.whdflg[CurChn] & 0x01))	// test update flag
			continue;							// next channel
		pdata.whdflg[CurChn] = 0x00;			// clear update flag
		
		TempByt = pdata.psglev[CurChn] >> 4;	// load level [IY+LEV]
		TempByt |= CmdMask;						// set command bits
		PSG_Write(TempByt);						// write new level
	}
	
	//vquit:
	DACxME();
	if (NoteEnd && SongAlmostEnd)
		CheckForSongEnd();	// [not in actual code]
	return;
}


/***************************** Command FIFO (from 68000) ************************************/


// GETCBYTE - returns the next command byte in the fifo from the 68k. will wait
//   for one if the queue is empty when called.
static UINT8 GETCBYTE(void)
{
	UINT8 CurPtr;
	
	// getcbytel:
	/*do
	{
		DACxME();
		FILLDACFIFO(0x00);
		
	} while(CMDWPTR == CMDRPTR);	// compare read and write pointers, loop if equal
	
	DACxME();*/
	if (CMDWPTR == CMDRPTR)
		return 0x00;
	
	CurPtr = CMDRPTR;
	CMDRPTR ++;			// increment read ptr
	CMDRPTR &= 0x3F;	//  (mod 64)
	
	return CMDFIFO[CurPtr];
}

/*************************************  XFER68K  ****************************************/

// XFER68K - transfers 1 to 255 bytes from 68000 space to Z80. handles 32k block crossings.
//
//		parameters:		A	68k source address [23:16]
//						HL	68k source address [15:0]
//						DE	Z80 dest address
//						C	byte count (0 is illegal!)
//			[Note: I guess ByteCount == 0 behaves like 0x100 if this would cross a bank and like 0x00 else.]
static void XFER68K(UINT8* DstPtr, const UINT8* SrcData, UINT32 SrcAddr, UINT8 BytCount)
{
	// I can't be bothered to port this function, because it's large,
	// compilated and seems to wait for the 68k to do something.
	// So I'll just imitate its behaviour.
	const UINT8* SrcPtr;
	
	if (SrcAddr & 0x800000)	// [not in actual code] catch bad offsets
		return;
	
	DACxME();	// [they put this call EVERYWHERE]
	
	//SrcPtr = &ROMData[SrcAddr];
	SrcPtr = &SrcData[SrcAddr];
	while(BytCount)
	{
		*DstPtr = *SrcPtr;
		SrcPtr ++;	DstPtr ++;
		BytCount --;
	}
	
	return;
}

/*************************************  DIGITAL STUFF  ************************************/

// DACME - do that DAC thing. assumes the the alternate registers are set up as follows
//
//					B		15H (reset cmd to timer) + CH3 mode bits
//					C		control pattern for processing (compression, oversampling)
//					DE		pointing into DACFIFO (1F00-1FFF)
//					HL		4000H
void DACME(void)
{
	UINT8 CurSmpl;
	
	if (DacMeEn == DACMEJRINST)	// JR DACMEALT
	{
		//DACMEALT:
		// changes DACME back to working next time
		DacMeEn = 0xD9;
		return;
	}
	else if (DacMeEn == 0xC9)	// RET
	{
		return;
	}
	
	// dacmepoint:
	YM2612_WriteA(0, 0x27);			// 0x4000 = 0x27
	// dacmespin:
	//while(! (YM2612_Read(0) & 0x01))	// BIT 0, 0x4000
	//	;
	if (! (YM2612_Read(0) & 0x01))
		return;
	
	FILLDACFIFO(0x00);				// [not in actual code]
	
	YM2612_WriteA(1, Ch3ModeReg);	// reset timer (sets CH3 mode bits)
	
	CurSmpl = DACFIFO[DacFifoReg];	// get next byte from fifo
	
	//DACMEPROC:
	if (DacMeProc == 0x00)			// NOP
	{
		DacFifoReg ++;
	}
	else							// JR DACMEDSP
	{
		//DACMEDSP:
		DacCtrlPat = (DacCtrlPat >> 1) | (DacCtrlPat << 7);		// which sample (high nibble or low) [RRC]
		if (DacCtrlPat & 0x80)									// high
		{
			//DACME4BHI:			// here for hi nib - 2nd half - inc ptr
			DacFifoReg ++;
		}
		else
		{
			CurSmpl <<= 4;			// here for low nibble - 1st half
		}
		//DACME4BMSK:
		CurSmpl &= 0xF0;
	}
	
	//DACMEOUT:
	YM2612_WriteA(0, 0x2A);			// point FM chip at DAC data register
	YM2612_WriteA(1, CurSmpl);		// output sample
	
	//DACMERET:
	// change to ZNOP for DACME's every other call
	// for (slow sample rates)
	if (DacMeRet == 0x00)
	{
		// changes DACME to jump to DACMEALT next time
		DacMeEn = DACMEJRINST;
	}
	
	return;
}


// FILLDACFIFO - gets the next 128 bytes of sample from the 68000 into the DACFIFO
static void FILLDACFIFO(UINT8 ForceFill)
{
	UINT8 FillVal;
	INT16 SmpCntr;		// Reg HL
	UINT8 DstAddr;		// Reg E (of DE, D is always 0x1F)
	UINT32 SmplPtr;
	UINT8 SmplLeft;		// Reg BC (though B is always 0)
	
	if (! ForceFill)
	{
		if (FillDacEn == 0xC9)
			return;
		
		FillVal = DACFIFOWPTR ^ DacFifoReg;
		FillVal &= 0x80;
		if (! FillVal)
			return;
	}
	
	// FORCEFILLDF: [save AF]
	CHECKTICK();
	
	// FDFneeded:
	DACxME();
	
	if (FDFSTATE < 7)
	{
		// FDF4N5N6:	// states 4, 5, and 6
		SmpCntr = SAMPLECTR - 128;
		if (SmpCntr > 0)
		{
			// FDF4NORM:
			SAMPLECTR = SmpCntr;
			
			// xfer next 128 samples from (SAMPLEPTR)
			DstAddr = DACFIFOWPTR;
			DACFIFOWPTR += 128;		// increment dest addr for next time
			
			SmplPtr = Read24Bit(SAMPLEPTR);
			XFER68K(DACFIFO + DstAddr, DData, SmplPtr, 128);
			
			SmplPtr += 128;
			Write24Bit(SAMPLEPTR, SmplPtr);
			
			// jp FDFreturn
		}
		else
		{
			// FDF4DONE:	// for now, loop back
			// xfer the samples that are left
			SmplLeft = SmpCntr + 128;	// save # xfered here
			
			DstAddr = DACFIFOWPTR;
			DACFIFOWPTR += 128;			// increment dest addr for next time
			
			SmplPtr = Read24Bit(SAMPLEPTR);
			XFER68K(DACFIFO + DstAddr, DData, SmplPtr, SmplLeft);
			
			// needs to xfer the next few if needed, for now, just loop back
			if (FDFSTATE != 5)
			{
				// FDF7 [copy from below]
				DacMeEn = 0xC9;
				FillDacEn = 0xC9;
				YM2612_Write(0, 0x2B);
				YM2612_Write(1, 0x00);
				
				FMVTBLCH6[VTBLFLAGS] = 0xC6;
				FMVTBLCH6[VTBLDL] = 0x00;
				FMVTBLCH6[VTBLDH] = 0x00;
				return;
			}
			
			SmplPtr = Read24Bit(SAMPLEPTR);
			SmplPtr += SmplLeft;			// add to sample pointer
			SmplPtr -= SAMP.LOOP;			// then subtract loop length
			Write24Bit(SAMPLEPTR, SmplPtr);	// store new (beginning of loop ptr)
			
			SmplLeft = 128 - SmplLeft;		// BC <- number to complete this 128byte bank
			if (! SmplLeft)
				return;						// none to xfer
			
			SAMPLECTR -= SmplLeft;			// subtract these few samples from ctr
			SmplPtr = Read24Bit(SAMPLEPTR);
			XFER68K(DACFIFO + DstAddr, DData, SmplPtr, SmplLeft);	// reload FIFO
			
			SmplPtr += SmplLeft;			// SAMPLEPTR <- SAMPLEPTR + 128
			
			// jp FDFreturn
		}
	}
	else
	{
		// FDF7:
		DacMeEn = 0xC9;		// disable with RET opcode
		FillDacEn = 0xC9;	// disable with RET opcode
		YM2612_Write(0, 0x2B);
		YM2612_Write(1, 0x00);
		
		FMVTBLCH6[VTBLFLAGS] = 0xC6;	// mark voice free, unlocked, and releasing
		FMVTBLCH6[VTBLDL] = 0x00;		// clear any pending release timer value
		FMVTBLCH6[VTBLDH] = 0x00;
		// jp FDFreturn
	}
	
	// FDFreturn:
	return;
}


/************************************ SEQUENCER CODE **************************************/

// GETSBYTE - get the channel's sequence byte pointed to by the CCB
//#define BUFSIZE		16	// [actually commented out in the code, but it's nice to use]
#define BUFSIZE		(CCHSIZE / 16)	// [this is even better]
static UINT8 GETSBYTE(UINT8* ChnCCB, UINT8* CHBUFPTR)
{
	UINT32 CcbAddr;
	UINT32 CcbTag;
	UINT32 AddrDiff;
	//UINT8* BufPtr;
	UINT8 RetByte;
	
	DACxME();
	
	CcbAddr = Read24Bit(&ChnCCB[CCBADDRL]);
	CcbTag = Read24Bit(&ChnCCB[CCBTAGL]);
	AddrDiff = CcbAddr - CcbTag;
	if (AddrDiff >= BUFSIZE)	// NOT (if mid and msb ok, is lsb < 16?)
	{
		//gsbmiss:
		DACxME();
		CHECKTICK();
		
		// here to refill buffer w/ next 32 bytes in seq
		CcbTag = CcbAddr;
		Write24Bit(&ChnCCB[CCBTAGL], CcbTag);
		XFER68K(CHBUFPTR, SData, CcbTag, BUFSIZE);	// refill away
		AddrDiff = 0x00;
		// and hit on first byte (since we just refilled here)
		//jr gsbhit
	}
	
	//gsbhit:
	DACxME();
	
	// hit!
	RetByte = CHBUFPTR[AddrDiff & 0xFF];	// ptr to byte in buffer, byte from buffer
	CcbAddr ++;								// increment addr[23:0]
	Write24Bit(&ChnCCB[CCBADDRL], CcbAddr);
	
	//gsbincdone:
	DACxME();
	return RetByte;
}

// UPDSEQ - go through the CCB's, updating any enabled channels
static void UPDSEQ(void)
{
	UINT8* CHBUFPTR;	// [0454] pointer to current channel's sequence buffer
	UINT8* ChnCCB;
	UINT8 CurChn;
	UINT8 TestRes;
	
	ChnCCB = CCB;
	CHBUFPTR = CH0BUF;	// initialize seq buf ptr
	CHPATPTR = PATCHDATA;
	
	for (CurChn = 0; CurChn < 16; CurChn ++)
	{
		//updseqloop1:
		if (ChnCCB[CCBFLAGS] & 0x10)	// is channel running (vs. paused or free) [BIT #4]
		{
			if (! (ChnCCB[CCBFLAGS] & 0x08))	// is it sfx tempo based? [BIT #3]
				TestRes = (TBASEFLAGS & 0x02);	// music tempo based - beat gone by? [BIT #1]
			else
				//updseqsfx:
				TestRes = (TBASEFLAGS & 0x01);	// sfx tempo based - tick gone by? [BIT #0]
			if (TestRes)
			{
				//updseqdoit:
				DACxME();
				FILLDACFIFO(0x00);
				SEQUENCER(ChnCCB, CHBUFPTR, CurChn);
			}
		}
		
		//updseqloop2:
		// [CurChn ++; if !(CurChn < 16) break;]
		
		//updseqloop: [actually placed before updseqloop1]
		DACxME();
		ChnCCB += 32;	// go to next CCB
		CHBUFPTR += BUFSIZE;
		CHPATPTR += 39;
	}
	
	return;
}

// SEQUENCER - if the channel has timed out, then execute the next set of sequencer cmds
//
//		parameters:		IX	points to channel control block (CCB)
//						B	channel
//						C	timerbase flags ([0] = sfx, [1] = music)
static void SEQUENCER(UINT8* ChnCCB, UINT8* CHBUFPTR, UINT8 CurChn)
{
	UINT8 SkipGet;
	UINT8 CmdByte;
	UINT16 DelayVal;	// Register DE
	
	ChnCCB[CCBTIMERL] ++;		// increment channel timer
	if (ChnCCB[CCBTIMERL])
		return;
	ChnCCB[CCBTIMERH] ++;
	if (ChnCCB[CCBTIMERH])
		return;
	SkipGet = 0x00;
	while(1)
	{
		//seqcmdloop0:
		if (! SkipGet)	// [The actual code jumps to seqcmdloop0 or seqcmdloop instead.]
			CmdByte = GETSBYTE(ChnCCB, CHBUFPTR);	// timed out! - do the next sequence commands
		else
			SkipGet = 0x00;
		//seqcmdloop:
		if (CmdByte & 0x80)			// dispatch on cmd type
		{
			if (CmdByte & 0x40)
			{
				//seqdel:					// process delay commands
				DelayVal = CmdByte & 0x3F;	// get data bits into DE
				while(1)
				{
					//seqdelloop:
					CmdByte = GETSBYTE(ChnCCB, CHBUFPTR);	// is next command also a delay cmd?
					if ((CmdByte & 0xC0) != 0xC0)
						break;
					
					DelayVal <<= 6;							// yes, shift in its data as the new lsbs
					DelayVal |= (CmdByte & 0x3F);
				}
				//seqdeldone:
				DelayVal = -DelayVal;		// negate delay value before storing
				Write16Bit(&ChnCCB[CCBDELL], DelayVal);
				SkipGet = 0x01;				// [jr seqdelloop]
				continue;
			}
			else
			{
				//seqdur:					// process duration commands
				DelayVal = CmdByte & 0x3F;	// get data bits into DE
				while(1)
				{
					//seqdelloop:
					CmdByte = GETSBYTE(ChnCCB, CHBUFPTR);	// is next command also a duration cmd?
					if ((CmdByte & 0xC0) != 0x80)
						break;
					
					DelayVal <<= 6;							// yes, shift in its data as the new lsbs
					DelayVal |= (CmdByte & 0x3F);
				}
				//seqdeldone:
				DelayVal = -DelayVal;		// negate delay value before storing
				Write16Bit(&ChnCCB[CCBDURL], DelayVal);
				SkipGet = 0x01;				// [jr seqdelloop]
				continue;
			}
		}
		else						// process a note or command
		{
			//seqnote:
			if (CmdByte >= 96)		// commands are 96-127
			{
				SkipGet = seqcmd(ChnCCB, CHBUFPTR, CurChn, CmdByte);
				if (SkipGet & 0x80)
					break;
			}
			else
			{
				SkipGet = 0x01;
				if (! (ChnCCB[CCBFLAGS] & 0x02))		// is this channel muted? [BIT #1]
					NOTEON(CurChn, CmdByte, ChnCCB);
				//else ;			// yup - don't note on
			}
			if (SkipGet == 0x01)
			{
				//seqdelay:
				DACxME();
				
				DelayVal = Read16Bit(&ChnCCB[CCBDELL]);
				if (DelayVal != 0)
				{
					ChnCCB[CCBTIMERL] = ChnCCB[CCBDELL];	// non-zero delay - set channel timer
					ChnCCB[CCBTIMERH] = ChnCCB[CCBDELH];
					break;
				}
				//else ;			// zero delay - do another command
				//continue;
			}
			SkipGet = 0x00;
		}
	}
	
	return;
}

// Return Values:
//	00 - just continue with the next command
//	01 - process delay
//	80 - return instantly from SEQUENCER
static UINT8 seqcmd(UINT8* ChnCCB, UINT8* CHBUFPTR, UINT8 CurChn, UINT8 CmdByte)
{
	UINT8 CurArg;
	
	// [Note: The cases are sorted in order of the functions in the code.
	//        The actual jump block has them properly sorted from 96 to 113.]
	switch(CmdByte)
	{
	case  99:	// 99 = nop (triggers another delay)
		return 0x01;					// [jp seqdelay]
	case  98:	// 98 = env
		//seqenv:
		CurArg = GETSBYTE(ChnCCB, CHBUFPTR);
		ChnCCB[CCBENV] = CurArg;
		if (! (ChnCCB[CCBFLAGS] & 0x40))	// immediate mode envelopes? [BIT #6]
			TRIGENV(ChnCCB, CurChn, CurArg);
		return 0x01;	// [jp seqdelay]
	case 102:	// 102 = retrigger mode
		//seqretrig:
		CurArg = GETSBYTE(ChnCCB, CHBUFPTR);
		if (CurArg)						// retrigger on?
			//seqrton:
			ChnCCB[CCBFLAGS] |= 0x40;	// [SET #6]
		else
			ChnCCB[CCBFLAGS] &= ~0x40;	// [RES #6]
		return 0x01;	// [jp seqdelay]
	case 103:	// 103 = sustain
		//seqsus:
		CurArg = GETSBYTE(ChnCCB, CHBUFPTR);
		if (CurArg)						// retrigger on?
			//seqsuson:
			ChnCCB[CCBFLAGS] |= 0x80;	// [SET #7]
		else
			ChnCCB[CCBFLAGS] &= ~0x80;	// [RES #7]
		return 0x01;	// [jp seqdelay]
	case  96:	// 96 = eos
		//seqeos:
		ChnCCB[CCBFLAGS] = 0;			// end of sequence - disable CCB (free it)
		ChnCCB[CCBDURL] = 0;
		ChnCCB[CCBDURH] = 0;
		
		CheckForSongEnd();	// [not in actual code]
		return 0x80;	// [ret]
	case  97:	// 97 = pchange
		//seqpchange:
		ChnCCB[CCBPNUM] = GETSBYTE(ChnCCB, CHBUFPTR);
		FETCHPATCH(ChnCCB);
		return 0x01;	// [jp seqdelay]
	case 100:	// 100 = loop start
		//seqsloop:
		{
			UINT8* LoopCCB;
			
			LoopCCB = &ChnCCB[CCBLOOP0];	// first loop stack entry for this CCB
			//seqsllp:
			while(LoopCCB[0])				// is this stack entry free?
				LoopCCB += 3;				// no - try the next one
			//seqslfound:
			LoopCCB[0] = GETSBYTE(ChnCCB, CHBUFPTR);	// yes - store loop count and addr[15:0]
			LoopCCB[1] = ChnCCB[CCBADDRL];
			LoopCCB[2] = ChnCCB[CCBADDRM];
		}
		return 0x00;	// [jp seqcmdloop0]
	case 101:	// 101 = loopend
		//seqeloop:
		{
			UINT8* LoopCCB;
			UINT16 OldOfs;
			UINT16 NewOfs;
			
			LoopCCB = &ChnCCB[CCBLOOP3];	// last loop stack entry for this CCB
			//seqellp:
			while(! LoopCCB[0])				// is this stack entry free?
				LoopCCB -= 3;				// yes - try the previous one
			//seqelfound:
			if (LoopCCB[0] != 127)			// endless loop - go back
			{
				LoopCCB[0] --;
				if (! LoopCCB[0])
					return 0x00;			// end of finite loop - don't go back [jp seqcmdloop0]
			}
			//seqelgobk:
			OldOfs = Read16Bit(&LoopCCB[1]);
			NewOfs = Read16Bit(&ChnCCB[CCBADDRL]);
			ChnCCB[CCBADDRL] = LoopCCB[1];
			ChnCCB[CCBADDRM] = LoopCCB[2];
			if (OldOfs > NewOfs)
				ChnCCB[CCBADDRH] --;
		}
		return 0x00;	// [jp seqcmdloop0]
	case 104:	// 104 = tempo
		//seqtempo:
		CurArg = GETSBYTE(ChnCCB, CHBUFPTR);
		SETTEMPO(CurArg + 40);
		return 0x01;	// [jp seqdelay]
	case 105:	// 105 = mute
		//seqmute:
		CurArg = GETSBYTE(ChnCCB, CHBUFPTR);	// [4] is 1 for mute, [3:0] is midi channel
		{
			UINT8* MuteCCB;
			UINT8 MuteChn;
			const char* Str;
			
			Str = (CurArg & 0x10) ? "un" : "";
			printf("Channel %u %smutes channel %u\n", CurChn, Str, CurArg & 0x0F);
			MuteCCB = CCB;
			for (MuteChn = 0; MuteChn < 16; MuteChn ++, MuteCCB += 32)
			{
				//seqmutelp:
				if (! (MuteCCB[CCBFLAGS] & 0x01))			// channel in use? [BIT 0]
					continue;								// [jp seqmutenext]
				if (MuteCCB[CCBSNUM] == ChnCCB[CCBSNUM])	// running this sequence?
				{
					if ((CurArg & 0x0F) == MuteCCB[CCBVCHAN])	// and the desired channel?
					{
						//seqmuteit:
						if (CurArg & 0x10)					// mute or unmute?
							//sequnmute:
							ChnCCB[CCBFLAGS] &= ~0x02;		// [RES #1]
						else
							ChnCCB[CCBFLAGS] |= 0x02;		// [SET #1]
					}
				}
				//seqmutenext:								// try the next chan
			}
		}
		return 0x01;	// [jp seqdelay]
	case 106:	// 106 = priority
		//seqprio:
		ChnCCB[CCBPRIO] = GETSBYTE(ChnCCB, CHBUFPTR);
		return 0x01;	// [jp seqdelay]
	case 107:	// 107 = start song
		//seqssong:
		CurArg = GETSBYTE(ChnCCB, CHBUFPTR);
		printf("Channel %u starts sequence %02X\n", CurChn, CurArg);
		STARTSEQ(CurArg);
		return 0x01;	// [jp seqdelay]
	case 108:	// 108 = pitch bend
		//seqpbend:
		{
			UINT8* PbTblPtr;
			
			PbTblPtr = &PBTBL[CurChn];						// pointer to this ch's pitch bend data
			
			PbTblPtr[PBPBL] = GETSBYTE(ChnCCB, CHBUFPTR);	// 16 bit signed pitch bend (8 frac bits, semitones)
			PbTblPtr[PBPBH] = GETSBYTE(ChnCCB, CHBUFPTR);
			PbTblPtr[PBRETRIG] |= 0x01;						// [SET #0]
			NEEDBEND = 1;
		}
		return 0x01;	// [jp seqdelay]
	case 109:	// 109 = use sfx timebase
		//seqsfx:
		ChnCCB[CCBFLAGS] |= 0x08;		// set sfx timebase flag in CCB [SET #3]
		return 0x01;	// [jp seqdelay]
	case 110:	// 110 = set sample plbk rate
		//seqsamprate:
		CurArg = GETSBYTE(ChnCCB, CHBUFPTR);
		if (CHPATPTR[0] == 1)			// is this a digital patch?
			CHPATPTR[1] = CurArg;		// yes - update sample rate value
		//else ;						// no - no effect [jp seqdelay]
		return 0x01;	// [jp seqdelay]
	case 111:	// 111 = goto
		//seqgoto:
		{
			INT16 JmpOfs;
			UINT32 FinalAddr;
			
			CurArg = GETSBYTE(ChnCCB, CHBUFPTR);
			JmpOfs = (GETSBYTE(ChnCCB, CHBUFPTR) << 8) | (JmpOfs << 0);
			//seqbranch:					// jump to addr + 24 bit offset in DHL
			FinalAddr = Read24Bit(&ChnCCB[CCBADDRL]);
			FinalAddr += JmpOfs;
			Write24Bit(&ChnCCB[CCBADDRL], FinalAddr);
		}
		return 0x00;	// [jp seqcmdloop0]
	case 112:	// 112 = store
		//seqstore: [with inlined seqmboxstart]
		CurArg = GETSBYTE(ChnCCB, CHBUFPTR);				// get  mailbox num
		MBOXES[2 + CurArg] = GETSBYTE(ChnCCB, CHBUFPTR);	// store value in mbox
		return 0x01;	// [jp seqdelay]
	case 113:	// 113 = if
		//seqif:
		// [For now, eat 4 bytes up.]
		{
			UINT8 CurMBox;
			UINT8 CurRel;
			UINT8 Result;
			UINT32 FinalAddr;
			
			//seqif: [with inlined seqmboxstart]
			CurMBox = GETSBYTE(ChnCCB, CHBUFPTR);	// get  mailbox num
			// [Note: The actual code doesn't cache the value and uses (HL) instead.]
			CurMBox = MBOXES[2 + CurMBox];
			CurRel = GETSBYTE(ChnCCB, CHBUFPTR);	// save relation
			CurArg = GETSBYTE(ChnCCB, CHBUFPTR);	// value
			switch(CurRel)
			{
			case 1:
				//seqifne:
				Result = (CurArg != CurMBox);		// if M<>V then NZ
				break;
			case 2:
				//seqifgt:
				Result = (CurArg > CurMBox);		// if M>V then C
				break;
			case 3:
				//seqifgte:
				Result = (CurArg >= CurMBox);		// if M>=V then C|Z
				break;
			case 4:
				//seqiflt:
				Result = (CurArg < CurMBox);		// if M<V then NZ & NC
				break;
			case 5:
				//seqiflte:
				Result = (CurArg <= CurMBox);		// if M<=V then NC
				break;
			case 0:
			default:
				//seqifeq:
				Result = (CurArg == CurMBox);		// [if M==V then Z]
				break;
			}
			// [jump if the condition is false, continue if true]
			if (Result)
			{
				//seqifdoit:
				GETSBYTE(ChnCCB, CHBUFPTR);			// [eat one byte (jump offset) up]
			}
			else
			{
				//seqifpunt:
				CurArg = GETSBYTE(ChnCCB, CHBUFPTR);
				//seqbranch
				FinalAddr = Read24Bit(&ChnCCB[CCBADDRL]);
				FinalAddr += CurArg;
				Write24Bit(&ChnCCB[CCBADDRL], FinalAddr);
			}
		}
		return 0x00;	// [jp seqcmdloop0]
	case 114:	// 114 = seekrit codes [Is this supposed to be "secret codes"?]
		{
			UINT8 CurCode;	// Register D
			UINT8 CurVal;	// Register E
			
			CurCode = GETSBYTE(ChnCCB, CHBUFPTR);	// get code
			CurVal = GETSBYTE(ChnCCB, CHBUFPTR);	// get value
			switch(CurCode)						// dispatch on code
			{
			case 4:
				//seqatten:
				MASTERATN = CurVal;
				return 0x01;	// [jp seqdelay]
			case 5:
				//seqchatten:
				ChnCCB[CCBATN] = CurVal;
				return 0x01;	// [jp seqdelay]
			case 0:
				//seqstopseq:
				STOPSEQ(CurVal);
				return 0x01;	// [jp seqdelay]
			case 1:
				//seqpauseseq:
				//seqpausecom:
				PAUSESEQ(CurVal);
				return 0x01;	// [jp seqdelay]
			case 3:
				//seqpauselmusic:
				CurVal = MBOXES[2];
				//seqpausecom:
				PAUSESEQ(CurVal);
				return 0x01;	// [jp seqdelay]
			case 2:
				//seqresume:
				RESUMEALL();
				return 0x01;	// [jp seqdelay]
			default:
				printf("Invalid special command %u found on Channel %u!\n", CurCode, CurChn);
				break;
			}
		}
		return 0x01;	// [jp seqdelay]
	default:
		//* THIS COULD USE SOME FANCY ERROR DETECTION RIGHT ABOUT NOW
		printf("Invalid command %u found on Channel %u!\n", CmdByte, CurChn);
		return 0x00;	// [jp seqcmdloop0]
	}
	return 0x00;
}

// VTIMER - updates the voice timers - first note on and then release values
static void VTIMER(void)
{
	vtimerloop(0, FMVTBL);
	vtimerloop(1, PSGVTBL);
	vtimerloop(1, PSGVTBLNG);
	
	return;
}

static void vtimerloop(UINT8 VoiceType, UINT8* VTblPtr)
{
	UINT8 CurVoc;
	
	// [Using a for-loop here is soooo much more comfortable than a while.]
	//vtimerloop0:
	for ( ; VTblPtr[VTBLFLAGS] != 0xFF; VTblPtr += 7)
	{
		DACxME();
		if (VTblPtr[VTBLFLAGS] & 0x08)	// sfx tempo driven? [BIT #3]
		{
			//vtimersfx:
			if (! (TBASEFLAGS & 0x01))	// [BIT #0]
				continue;
		}
		else if (! (TBASEFLAGS & 0x02))	// no - music beat flag set? [BIT #1]
		{
			continue;
		}
		
		//vtimerdoit:
		if (VTblPtr[VTBLFLAGS] & 0x40)			// if in release [BIT #6]
		{
			VTblPtr[VTBLRT] --;					//   decrement release timer
			if (! VTblPtr[VTBLRT])				//   not at zero, loop
				VTblPtr[VTBLFLAGS] &= ~0x40;	//   turn off release flag [RES #6]
			//jr vtimerloop0
		}
		else
		{
			//vtimerloop2:
			if (! (VTblPtr[VTBLFLAGS] & 0x10))	// self timed note? [BIT #4]
				continue;
			
			CurVoc = VTblPtr[VTBLFLAGS] & 0x07;	// yes - save voice # in C
			VTblPtr[VTBLDL] ++;					// inc lsb of timer
			if (VTblPtr[VTBLDL])
				continue;
			VTblPtr[VTBLDH] ++;					// if zero (carry), inc msb
			if (VTblPtr[VTBLDH])
				continue;
			
			VTblPtr[VTBLFLAGS] &= ~0x18;				// timed out - clear self timer bit [RES #4] and clear sfx bit [RES #3]
			if ((VTblPtr[VTBLFLAGS] & 0x2F) == 0x26)	// is it voice 6 (must be FM) and locked?
			{
				NOTEOFFDIG();					//   yes - its a digital noteoff [else]
				// for now, note off don't effect digital
			}
			else
			{
				VTblPtr[VTBLFLAGS] |= 0xC0;		//   no - set release bit, set free bit [SET #6], set free bit [SET #7]
				
				//vtnoteoff:					// note off...
				if (VoiceType)					// voice type? [BIT #0]
				{
					pdata.psgcom[CurVoc] |= 0x02;	// set key off command [SET #1]
				}
				else
				{
					//vtnoteofffm:
#ifndef DUAL_SUPPORT
					FMWrite(0, 0x28, CurVoc);	// key off
#else
					FMWrite(CHIP_BNK(VTblPtr), 0x28, CurVoc);
#endif
				}
			}
			
			if (SongAlmostEnd)
				CheckForSongEnd();	// [not in actual code]
		}
	}
	
	return;
}

/*************************************  MAIN LOOP  ****************************************/

// GETCCBPTR - gets one byte from command queue for channel number, multiplies by 32,
//		and returns pointer to that channel's CCB in IX, as well as the channel #
//		in A
// GETCCBPTR2 - alternate entry point to providing channel # in A (skips GETCBYTE)
static UINT8 GETCCBPTR(UINT8** RetCCB)
{
	UINT8 CurChn;
	
	CurChn = GETCBYTE();
	*RetCCB = GETCCBPTR2(CurChn);
	return CurChn;
}

static UINT8* GETCCBPTR2(UINT8 Channel)
{
	return &CCB[Channel * 32];
}

void gems_init(void)
{
	UINT8 CurIns;
	
	SBPT = 204;			// sub beats per tick (8frac), default is 120bpm
	SBPTACC = 0;
	TBASEFLAGS = 0;
	
	DacMeEn = 0x18;
	DacMeProc = 0x18;
	FillDacEn = 0xC9;
	
	memset(pdata.psglev, 0xFF, 0x04);
	// TODO: memzero everything else
	
	//main:
	Ch3ModeReg = 0x15;	// timer reset command (also hold CH3 mode bits)
	// silence the psg voices
	PSG_Write(0x9F);
	PSG_Write(0xBF);
	PSG_Write(0xDF);
	PSG_Write(0xFF);
	
	// set all patch buffers to undefined
	for (CurIns = 0; CurIns < 16; CurIns ++)
		//pinitloop:
		PATCHDATA[CurIns * 39] = 0xFF;
	
	//DACMEJRINST = DacMeEn;	// save the jr in DACME to slow sample rate mode
	//DACME4BINST = DacMeProc;	// save the jr for enabling processing
	
	DacMeEn = 0xC9;				// opcode "RET" and disable for now
	
	SongAlmostEnd = 0x01;
	
	//loop:
	// [see gems_loop]
	return;
}

void gems_loop(void)
{
	// [Note: every "return;" in this function would actually loop back
	//        to the beginning of the function.
	//        But since we can't do busy-waiting here, we'll just return.]
	UINT8 TickID;	// Register B
	UINT8 CmdByte;
	UINT8 CurIdx;
	UINT8* ChnPtr;
	UINT8 CurChn;
	UINT8 CurNote;
	
	CHECKTICK();
	
	DACxME();
	
	FILLDACFIFO(0x00);
	
	DACxME();
	
	CHECKTICK();
	
	do	// [Note: This loop is not in the actual code. The actual code constantly loops this whole function instead.]
	{
	TickID = 0;
	
	if (TICKCNT > 0)	// check tick counter
	{
		TICKCNT --;		// a tick's gone by...
		DOPSGENV();		//   do PSG envs and set tick flag
		CHECKTICK();
		TickID = 1;		//   set tick flag
	}
	//noticks:
	DACxME();
	
	if (SBPTACC >= 0x100)	// check beat counter (scaled by tempo) [actually only checks the MSB byte of SBPTACC]
	{
		SBPTACC -= 0x100;	// a beat (1/24 beat) 's gone by...
		TickID |= 2;		//   set beat flag [SET #1]
	}
	//nobeats:
	if (TickID)
	{
		TBASEFLAGS = TickID;
		DOENVELOPE();		// call the envelope processor
		CHECKTICK();
		VTIMER();			// update voice timers
		CHECKTICK();
		UPDSEQ();			// update sequencers
		CHECKTICK();
	}
	//neithertick:
	APPLYBEND();			// check if bends need applying
	} while(SBPTACC >= 0x100 || TICKCNT);	// [not in actual code: Loop until all ticks are processed.]
	
	if (CMDWPTR == CMDRPTR)	// check for command bytes... compare read and write pointers
		return;				// loop if no command bytes waiting [jp loop]
	
	CmdByte = GETCBYTE();	// main loop
	if (CmdByte != 0xFF)	// start of command?
		return;				// no, wait for one [again, return instead of jp loop]
	
	CmdByte = GETCBYTE();	// get command
	// [Note: The cases are sorted in order of the functions in the code. (This affects only 1x commands though.)
	//        The actual jump block has them properly sorted from 0 to 29.]
	switch(CmdByte)
	{
	case 0:		// note on?
		//cmdnoteon:
		CurChn = GETCCBPTR(&ChnPtr);		// GETCBYTE for channel, IX <- CCB ptr, A <- channel
		CHPATPTR = GETPATPTR(CurChn);		// set pointer to this channel's patch buffer
		
		CurNote = GETCBYTE();				// note
		NOTEON(CurChn, CurNote, ChnPtr);
		return;
	case 1:
		//cmdnoteoff:
		CurChn = GETCBYTE();	// channel
		CurNote = GETCBYTE();	// note
		NOTEOFF(CurChn, CurNote);
		return;
	case 2:
		//cmdpchange:
		PCHANGE();
		return;
	case 3:
		//cmdpupdate:
		CurIdx = GETCBYTE();
		PATCHLOAD(CurIdx);
		return;
	case 4:
		//cmdpbend:
		CurChn = GETCBYTE();	// get midi channel
		DOPITCHBEND(CurChn);	// PITCHBEND gets its own data from the cmd queue [outdated comment]
		return;
	case 30:
		//cmdpbendvch:
		CurIdx = GETCBYTE();				// seq #
		CmdByte = GETCBYTE();				// midi ch #
		ChnPtr = CCB;						// gems ch # [0] (CCB num)
		for (CurChn = 0; CurChn < 16; CurChn ++, ChnPtr += 32)
		{
			//pbvchloop:
			if (! (ChnPtr[CCBFLAGS] & 0x01))	// is this channel in use? [BIT #0]
				continue;						// no - skip it
			if (ChnPtr[CCBSNUM] != CurIdx)		// yes - is it for this seq number?
				continue;
			if (ChnPtr[CCBVCHAN] != CmdByte)	// yes - for this channel ?
				continue;
			DOPITCHBEND(CurChn);				// yes - bend this channel
		}
		GETCBYTE();
		GETCBYTE();
		break;
	case 5:
		//cmdtempo:
		CurIdx = GETCBYTE();
		SETTEMPO(CurIdx);
		return;
	case 6:
		//cmdenv:
		CurChn = GETCCBPTR(&ChnPtr);		// GETCBYTE for channel, IX <- CCB ptr, A <- channel
		CurIdx = GETCBYTE();				// envelope number
		ChnPtr[CCBENV] = CurIdx;			// store new env number
		if (! (ChnPtr[CCBFLAGS] & 0x40))	// retrigger mode?
			TRIGENV(ChnPtr, CurChn, CurIdx);
		return;
	case 7:
		//cmdretrig:
		CurChn = GETCCBPTR(&ChnPtr);	// GETCBYTE for channel, IX <- CCB ptr, A <- channel
		CmdByte = GETCBYTE();			// 80h for retrigg, 0 for immediate
		if (CmdByte)					// set retrigger?
			ChnPtr[CCBFLAGS] |= 0x40;	// [SET #6]
		else
			//retrigclr:
			ChnPtr[CCBFLAGS] &= ~0x40;	// [RES #6]
		return;
	case 16:
		//cmdstartseq:		// start a sequence
		CurIdx = GETCBYTE();
		STARTSEQ(CurIdx);
		return;
	case 18:
		//cmdstopseq:
		CurIdx = GETCBYTE();			// get sequencer number to stop
		STOPSEQ(CurIdx);
		return;
	case 11:
		//cmdgetptrs:
		for (CurIdx = 0; CurIdx < 12; CurIdx ++)	// read 12 bytes into the pointer variables
			Tbls.PTBL68K[CurIdx] = GETCBYTE();
		return;
	case 12:
		//cmdpause:			// pause all CCB's current running
		ChnPtr = CCB;
		for (CurChn = 0; CurChn < 16; CurChn ++, ChnPtr += 32)	// only 16 CCB's to try
		{
			//cmdpsloop:				// go through CCB's
			ChnPtr[CCBFLAGS] &= ~0x10;	// shut off running flags [RES #4]
		}
		CLIPALL();
		return;
	case 13:
		//cmdresume:
		RESUMEALL();
		return;
	case 14:
		//cmdsussw:			// set sustain flag for this channel
		CurChn = GETCCBPTR(&ChnPtr);	// GETCBYTE for channel, IX <- CCB ptr, A <- channel
		CmdByte = GETCBYTE();
		if (CmdByte)					// switch on?
			ChnPtr[CCBFLAGS] |= 0x80;	// yes [SET #7]
		else
			//cmdsusoff:
			ChnPtr[CCBFLAGS] &= ~0x80;	// no [RES #7]
		return;
	case 20:
		//cmdsetprio:
		CurChn = GETCCBPTR(&ChnPtr);	// GETCBYTE for channel, IX <- CCB ptr, A <- channel
		ChnPtr[CCBPRIO] = GETCBYTE();	// set priority for this channel
		return;
	case 22:
		//cmdstopall:
		ChnPtr = CCB;				// start with CCB 0
		for (CurChn = 0; CurChn < 16; CurChn ++, ChnPtr += 32)	// only 16 CCB's to try
		{
			//stopallloop:
			ChnPtr[CCBFLAGS] = 0;	// yes - make it free, no retrig, no sustain
			ChnPtr[CCBDURL] = 0;	// clear duration to enable live play
			ChnPtr[CCBDURH] = 0;
		}
		CLIPALL();					// chop off all notes
		return;
	case 23:
		//cmdmute:
		CmdByte = GETCBYTE();	// seq #
		CurIdx = GETCBYTE();	// ch #
		CurNote = GETCBYTE();	// 1 to mute, 0 to unmute
		
		ChnPtr = CCB;				// start with CCB 0
		for (CurChn = 0; CurChn < 16; CurChn ++, ChnPtr += 32)	// only 16 CCB's to try
		{
			//muteseqloop:
			if (! (ChnPtr[CCBFLAGS] & 0x01))	//is this channel in use?
				continue;						// no - skip it
			if (ChnPtr[CCBSNUM] != CmdByte)		// yes - is it for this seq number?
				continue;
			if (ChnPtr[CCBVCHAN] != CurIdx)		// yes - for this channel ?
				continue;
			if (CurNote & 0x01)					// mute or unmute? [BIT #0]
				//muteit:
				ChnPtr[CCBFLAGS] |= 0x02;		// mute [SET #1]
			else
				ChnPtr[CCBFLAGS] &= ~0x02;		// mute [RES #1]
			//muteseqskip:
		}
		return;
	case 26:
		//cmdsamprate:
		CurChn = GETCBYTE();			// channel
		ChnPtr = GETPATPTR(CurChn);		// PATCHDATA + 39 * A
		
		CurIdx = GETCBYTE();			// new rate value
		
		if (ChnPtr[0] != 1)				// is this a digital patch?
			return;					// no - no effect
		ChnPtr[1] = CurIdx;				// yes - update sample rate value
		return;
	case 27:
		//cmdstore:
		CurIdx = GETCBYTE();
		MBOXES[2 + CurIdx] = GETCBYTE();
		return;
	case 28:
		//cmdlockch:
		CurChn = GETCCBPTR(&ChnPtr);	// GETCBYTE for channel, IX <- CCB ptr, A <- channel
		ChnPtr[CCBFLAGS] |= 0x20;		// [SET #5]
		return;
	case 29:
		//cmdunlockch:
		CurChn = GETCCBPTR(&ChnPtr);	// GETCBYTE for channel, IX <- CCB ptr, A <- channel
		ChnPtr[CCBFLAGS] &= ~0x20;		// [RES #5]
		return;
	
	case 31:
		//cmdvolume:
		CurChn = GETCCBPTR(&ChnPtr);	// GETCBYTE for channel, IX <- CCB ptr, A <- channel
		ChnPtr[CCBATN] = GETCBYTE();	// get attenuation value
		break;
	case 32:
		//cmdmasteratn:
		MASTERATN = GETCBYTE();
		break;
	}
	return;
}

static void RESUMEALL(void)	// resume all enabled CCB's
{
	UINT8 CurChn;
	UINT8* ChnPtr;	// Register IX
	
	ChnPtr = CCB;
	for (CurChn = 0; CurChn < 16; CurChn ++, ChnPtr += 32)	// only 16 CCB's to try
	{
		//cmdresloop:
		if (ChnPtr[CCBFLAGS] & 0x20)		// locked? then dont resume [BIT #5]
			continue;
		if (! (ChnPtr[CCBFLAGS] & 0x01))
			continue;
		ChnPtr[CCBFLAGS] |= 0x10;			// set any enabled CCB's running again [SET #4]
		//cmdresnext:
	}
	
	return;
}

// STARTSEQ - starts a multi channel sequence. a free CCB is allocated for each channel
//   in the sequence.
void STARTSEQ(UINT8 SeqNum)
{
	UINT8 stseqx[49];	// [0BBD] 33 byte scratch area for starting a sequence
	UINT8 stseqsnum;	// [0BDE]
	UINT32 STblPtr;
	UINT32 SeqPtr;
	UINT8* ChnCCB;		// register IX
	UINT8* ChnPB;		// register IY
	UINT8* StSqBuf;		// register HL
	UINT8 CurChn;		// register B (but counting up)
	UINT8 VirtChn;
	UINT8 DoGems28;
	
	StartSignal(SeqNum);
	SBPTACC = 0;		// [note in actual code] reset current tempo ticks to prevent bugs
	
	stseqsnum = SeqNum;
	STblPtr = Read24Bit(Tbls.STBL68K);		// [not in actual code] caching for later use
	SeqPtr = STblPtr + (SeqNum << 1);		// AHL <- pointer to this seq's offset [in 68k space]
	XFER68K(stseqx, SData, SeqPtr, 2);		// read 2 byte offset, into... scratch
	
	SeqPtr = STblPtr + Read16Bit(stseqx);	// AHL <- pointer to seq hdr data
	XFER68K(stseqx, SData, SeqPtr, 49);		// xfer the max 33 byte seq hdr into scratch
	
	// *** this should probably be something different!
	if (! stseqx[0])
	{
		CheckForSongEnd();	// [not in actual code]
		return;	// return if empty sequence
	}
	
	DoGems28 = Gems28Mode;
	if (stseqx[0] > 1)		// [extra code to detect GEMS 2.8]
	{
		if (stseqx[3] <= 1 && stseqx[6] <= 1)
			DoGems28 = 0x01;
	}
	
	ChnCCB = CCB;			// start with CCB 0
	ChnPB = PBTBL;
	StSqBuf = &stseqx[1];	// track pointers start at stseqx+1
	VirtChn = 0;			// C <- channel count
	for (CurChn = 0; CurChn < 16; CurChn ++, ChnCCB += 32, ChnPB ++)	// only 16 CCB's to try
	{
		//chkstseqloop:
		if (ChnCCB[CCBFLAGS] & 0x21)	// check in use and locked flags
			continue;					// if either set, skip ch
		ChnCCB[CCBFLAGS] = 0x11;		// yes - set enable and running bits
		
		if (! DoGems28)
		{
			SeqPtr = STblPtr + Read16Bit(StSqBuf);	// addr of this track is 24 bit base pointer, plus 16 bit offset in descriptor
			StSqBuf += 2;
		}
		else
		{
			SeqPtr = STblPtr + Read24Bit(StSqBuf);	// addr of this track is 24 bit base pointer, plus 24 bit offset in descriptor
			StSqBuf += 3;							// so, (IX+CCBADDR[H,M,L]) = (STBL68K[23:0]) + (HL[23:0])
		}
		Write24Bit(&ChnCCB[CCBADDRL], SeqPtr);
		
		ChnCCB[CCBTAGL] = 0xFF;			// invalidate tags
		ChnCCB[CCBTAGM] = 0xFF;
		ChnCCB[CCBTAGH] = 0xFF;
		ChnCCB[CCBTIMERL] = 0xFF;
		ChnCCB[CCBTIMERH] = 0xFF;		// set timer to -1 to trigger sequencer next tick
		ChnCCB[CCBSNUM] = stseqsnum;	// save sequence number
		ChnCCB[CCBVCHAN] = VirtChn;		// save virtual channel number
		ChnCCB[CCBLOOP0] = 0x00;		// clear loop stack
		ChnCCB[CCBLOOP1] = 0x00;
		ChnCCB[CCBLOOP2] = 0x00;
		ChnCCB[CCBLOOP3] = 0x00;
		ChnCCB[CCBENV] = 0x00;			// clear envelope
		ChnCCB[CCBPRIO] = 0x00;
		ChnCCB[CCBATN] = 0x00;			// clear channel attenuation
		ChnPB[PBPBL] = 0x00;			// clear pitchbend, envelope bend
		ChnPB[PBPBH] = 0x00;
		ChnPB[PBEBL] = 0x00;
		ChnPB[PBEBH] = 0x00;
		
		VirtChn ++;
		if (VirtChn >= stseqx[0])
			break;						// return if all tracks started [actually RET]
	}
	SongAlmostEnd = 0x00;
	
	return;
}

// STOPSEQ - stops a multi channel sequence (actually, all occurances of it)
static void STOPSEQ(UINT8 SeqNum)
{
	UINT8 CurChn;
	UINT8* ChnPtr;	// Register IX
	
	ChnPtr = CCB;
	for (CurChn = 0; CurChn < 16; CurChn ++, ChnPtr += 32)	// only 16 CCB's to try
	{
		//stopseqloop:
		if (! (ChnPtr[CCBFLAGS] & 0x01))	// is this channel in use? [BIT #0]
			continue;						// no - skip it
		if (ChnPtr[CCBFLAGS] & 0x20)		// is this channel in locked? [BIT #5]
			continue;						// yes - skip it
		if (SeqNum == 255 ||				// stop song 255 means stop all
			ChnPtr[CCBSNUM] == SeqNum)		// yes - is it for this seq number?
		{
			//stopseqstopit:
			ChnPtr[CCBFLAGS] = 0;			// yes - make it free, no retrig, no sustain
			ChnPtr[CCBDURL] = 0;			// clear duration to enable live play
			ChnPtr[CCBDURH] = 0;
		}
		//stopseqskip:
	}
	CLIPALL();
	
	return;
}

// PAUSESEQ - pause a multi channel sequence (actually, all occurances of it)
static void PAUSESEQ(UINT8 SeqNum)
{
	UINT8 CurChn;
	UINT8* ChnPtr;	// Register IX
	
	ChnPtr = CCB;
	for (CurChn = 0; CurChn < 16; CurChn ++, ChnPtr += 32)	// only 16 CCB's to try
	{
		//pauseseqloop:
		if (! (ChnPtr[CCBFLAGS] & 0x01))	// is this channel in use? [BIT #0]
			continue;						// no - skip it
		if (ChnPtr[CCBFLAGS] & 0x20)		// is this channel in locked? [BIT #5]
			continue;						// yes - skip it
		if (ChnPtr[CCBSNUM] == SeqNum)		// yes - is it for this seq number?
			ChnPtr[CCBFLAGS] &= ~0x10;		// shut off running flags [RES #4]
		//pauseseqskip:
	}
	CLIPALL();
	
	return;
}

// CLIPALL - called by STOPALL and PAUSEALL - cancels all envelopes, voices
//
//		scans voice tables, clipping off notes from inactive (not running channels)
static void CLIPALL(void)
{
	UINT8* CurECB;	// Register IY
	UINT8* ChnCCB;	// Register HL
	UINT8 EcbFlags;	// Register A
	
	CLIPLOOP(FMVTBL, 0);	// do fm voices
	CLIPLOOP(PSGVTBL, 1);
	CLIPLOOP(PSGVTBLNG, 1);
	
	//clipenvloop:
	for (CurECB = ECB; ; CurECB ++)
	{
		EcbFlags = CurECB[ECBCHAN];
		if (EcbFlags & 0x80)			// end of list? [BIT #7]
			break;
		if (EcbFlags & 0x40)			// in use? [BIT #6]
			continue;
		ChnCCB = &CCB[(EcbFlags & 0x0F) << 5];
		if (ChnCCB[CCBFLAGS] & 0x40)	// running? [BIT #4]
			continue;					// yes - don't clip this env
		
		CurECB[ECBCHAN] |= 0x40;		// [SET #6]
	}
	
	CheckForSongEnd();	// [not in actual code]
	
	return;
}

// IX <- voice table, E <- 0 for fm, 1 for psg
static void CLIPLOOP(UINT8* VTblPtr, UINT8 Mode)
{
	UINT8* ChnCCB;	// Register HL
	UINT8* PRTbl;	// PSG Register Table Pointer, Register IY
	UINT8 CLIPVNUM;	// [0CF8]
	UINT8 VFlags;	// Register A
	UINT8 CurBank;	// Register D
	
	for ( ; VTblPtr[VTBLFLAGS] != 0xFF; VTblPtr += 7)	//clipnxt:
	{
		ChnCCB = &CCB[(VTblPtr[VTBLCH] & 0x0F) << 5];
		if (ChnCCB[CCBFLAGS] & 0x10)	// running? [BIT #4]
			continue;					// yes - don't clip
		
		VFlags = VTblPtr[VTBLFLAGS];	// get vtbl entry
		VFlags &= 0x07;					// get voice num
		VFlags |= 0x80;					// add free flag
		VTblPtr[VTBLFLAGS] = VFlags;	// update table
		VTblPtr[VTBLDL] = 0x00;			// clear release and duration timers
		VTblPtr[VTBLDH] = 0x00;
		VTblPtr[VTBLRT] = 0x00;
		
		VFlags &= 0x07;					// get voice num back
		CLIPVNUM = VFlags;
		if (! (Mode & 0x01))			// fm or psg? [BIT #0]
		{
			if (VTblPtr[VTBLFLAGS] & 0x20)	// fm - digital mode? [BIT #5]
			{
				DacMeEn = 0xC9;				// disable DACME routine
				FillDacEn = 0xC9;			// disable FILLDACFIFO
				YM2612_Write(0, 0x2B);		// disable DAC mode
				YM2612_Write(1, 0x00);
			}
			else
			{
				//clipfm:
				CurBank = 0;				// point to bank 0
				if (VFlags > 3)				// is voice in bank 1 ?
				{
					VFlags -= 4;			// yes, subtract 4 (map 4-6 >> 0-2)
					CurBank = 2;			// point to bank 1
				}
#ifdef DUAL_SUPPORT
				CurBank |= CHIP_BNK(VTblPtr);
#endif
				//clpafm0:
				FMWr(CurBank, VFlags, 0x40, 0x7F);	// clamp all EGs
				FMWr(CurBank, VFlags, 0x44, 0x7F);
				FMWr(CurBank, VFlags, 0x48, 0x7F);
				FMWr(CurBank, VFlags, 0x4C, 0x7F);
				
#ifndef DUAL_SUPPORT
				FMWrite(0, 0x28, CLIPVNUM);	// key off
#else
				FMWrite(CurBank & 0x04, 0x28, CLIPVNUM);
#endif
			}
		}
		else
		{
			//clippsg:
			PRTbl = &pdata.psgcom[VFlags];	// load psg register table, point to correct register
			PRTbl[COM] = 4;					// set stop command
		}
	}
	
	return;
}

// SETTEMPO - sets the (1/24 beat) / (1/60 sec) ratio in SBPT (Sub Beat Per Tick)
//		SBPT is 16 bits, 8 of em fractional
static void SETTEMPO(UINT8 BPM)
{
	UINT16 TempoBPT;
	
	TempoBPT = BPM * 218;
	SBPT = TempoBPT >> 7;
	
	return;
}

// TRIGENV - initialize an envelope
//
//		parameters:	C	envelope number
//					E	midi channel
//					IX	pointer to CCB
static void TRIGENV(UINT8* ChnCCB, UINT8 MidChn, UINT8 EnvNum)
{
	UINT8* PBEnv;	// Register IY
	UINT8* EcbPtr;
	UINT32 EnvOfs;
	UINT8 EcbBufIdx;
	UINT8 fpoffset[2];	// [1726], shared with FETCHPATCH
	UINT16 fpoffset_s;
	
	PBEnv = &PBTBL[MidChn];
	EcbPtr = ECB;	// point at the envelope control blocks
	while(1)
	{
		//retrigloop:
		if (EcbPtr[ECBCHAN] & 0x80)	// first see if an ECB already exists for this channel [BIT #7]
		{
			//tryfree:
			EcbPtr = ECB;
			while(1)
			{
				//trigloop:					// then try to find a free ECB
				if (EcbPtr[ECBCHAN] & 0x80)	// channel number and flags, end of list? [BIT #7]
					return;					// yup - return
				if (EcbPtr[ECBCHAN] & 0x40)	// active ? [BIT #6]
					break;					// nope - go allocate [JR trigger]
				EcbPtr ++;
			}
			break;
		}
		else if (EcbPtr[ECBCHAN] == MidChn)
		{
			break;
		}
		EcbPtr ++;
	}
	//trigger:
	if (ChnCCB[CCBFLAGS] & 0x08)	// sfx flag set in CCB? [BIT #3]
		MidChn |= 0x20;				// yes - set sfx flag in ECB [SET #5]
	//trigger1:
	EcbPtr[ECBCHAN] = MidChn;		// set channel
	EcbPtr[ECBCTR] = 0;				// clear counter to trigger segment update
	
	EnvOfs = Read24Bit(Tbls.ETBL68K) + (EnvNum << 1);	// pointer to env table in 68k space
	XFER68K(fpoffset, EData, EnvOfs, 2);				// read 2 byte offset, into... local fpoffset (shared w/ fetchpatch)
	fpoffset_s = Read16Bit(fpoffset);
	
	EnvOfs = Read24Bit(Tbls.ETBL68K) + fpoffset_s;		// pointer to env data
	EcbBufIdx = EcbPtr[ECBBUFP] & 0x7F;
	XFER68K(&ENV0BUF[EcbBufIdx], EData, EnvOfs, 32);	// xfer the 32 byte env into this ECB's envelope buffer
	
	PBEnv[PBEBL] = ENV0BUF[EcbBufIdx];	EcbBufIdx ++;
	PBEnv[PBEBH] = ENV0BUF[EcbBufIdx];	EcbBufIdx ++;
	
	EcbPtr[ECBPTRL] = 0x80 + EcbBufIdx;					// point ECB at envelope after initial value
	EcbPtr[ECBPTRH] = 0x1E;
	
	return;
}

// DOENVELOPE - update the pitch envelope processor
static void DOENVELOPE(void)
{
	UINT8* CurECB;	// Register IX
	UINT8 TestRes;
	UINT32 SegPos;	// Register HL
	UINT8 SegCntr;	// Register A
	UINT16 SegVal;
	UINT8* CurPB;	// Register IY
	
	CurECB = ECB;	// point at the envelope control blocks
	//envloop:
	while(1)
	{
		DACxME();
		if (CurECB[ECBCHAN] & 0x80)		// end of list? [BIT #7]
			break;						// yup - return
		if (! (CurECB[ECBCHAN] & 0x40))	// active ?	[BIT #6]
		{
			//envactive:				// check if this envelope's timebase has ticked
			
			if (TBASEFLAGS & 0x20)		// sfx timebase? [BIT #5]
				//envsfx:
				TestRes = (TBASEFLAGS & 0x01);	// yes - check sfx tick flag [BIT #0]
			else
				TestRes = (TBASEFLAGS & 0x02);	// no - check music tick flag [BIT #1]
			if (TestRes)
			{
				//envticked:
				if (CurECB[ECBCTR] == 0)	// ctr at 0?
				{
					//envnextseg:		// yes - [Note: The comment is really cut here.]
					SegPos = (CurECB[ECBPTRH] << 8) | (CurECB[ECBPTRL] << 0);
					SegPos -= 0x1E80;	// [1E80 is subtracted to make up for ENV0BUF[]]
					SegCntr = ENV0BUF[SegPos];
					if (SegCntr == 0)
					{
						// jr envdone
						TestRes = 0x00;
					}
					else
					{
						SegPos ++;
						CurECB[ECBDELL] = ENV0BUF[SegPos];
						SegPos ++;
						CurECB[ECBDELH] = ENV0BUF[SegPos];	// ECB's delta <- this segment's delta
						SegPos ++;
						DACxME();
						SegPos += 0x1E80;					// [not in actual code, SegPos is relative to ENV0BUF here]
						CurECB[ECBPTRL] = (SegPos & 0x00FF) >> 0;
						CurECB[ECBPTRH] = (SegPos & 0xFF00) >> 8;
						// [fall through to envseg]
					}
				}
				else					// no - process segment
				{
					SegCntr = CurECB[ECBCTR] - 1;
				}
				if (TestRes)	// [need to check that, since envdone skips envseg]
				{
					//envseg:
					CurPB = &PBTBL[CurECB[ECBCHAN] & ~0x20];	// ptr to this channel's pitchbend entries [RES #5]
					CurECB[ECBCTR] = SegCntr;
					SegVal = (CurPB[PBEBH] << 8) | (CurPB[PBEBL] << 0);
					SegVal += (CurECB[ECBDELH] << 8) | (CurECB[ECBDELL] << 0);
					CurPB[PBEBL] = (SegVal & 0x00FF) >> 0;
					CurPB[PBEBH] = (SegVal & 0xFF00) >> 8;
					// [fall through to envneedupd]
				}
				else
				{
					//envdone:
					CurPB = &PBTBL[CurECB[ECBCHAN] & ~0x20];	// ptr to this channel's pitchbend entries [RES #5]
					CurPB[PBEBL] = 0;							// zero the envelope bend on this channel
					CurPB[PBEBH] = 0;
					CurECB[ECBCHAN] = 0x40;						// shut off this envelope
					//jr envneedupd
				}
				//envneedupd:
				CurPB[PBRETRIG] |= 0x01;	// [SET #0]
				NEEDBEND = 1;
				//jr envnext
			}
		}
		//envnext:						// nope - loop
		CurECB ++;
	}
	
	return;
}

// DOPITCHBEND- updates the (pitchbend) value for the gems channel (= MIDI channel during perf [another cut comment]
//
//		inputs:		A							CCB number (0-15)
//					(next 2 bytes in cmd queue)	pbend value
static void DOPITCHBEND(UINT8 CurChn)
{
	UINT8* PBEnv;	// Register IX
	
	PBEnv = &PBTBL[CurChn];		// ptr to this ch's bends
	PBEnv[PBPBL] = GETCBYTE();	// get pitch bend in half steps (8 fracs) into
	PBEnv[PBPBH] = GETCBYTE();
	PBEnv[PBRETRIG] |= 0x01;	// [SET #0]
	NEEDBEND = 1;
	
	return;
}

// APPLYBEND - if NEEDBEND is set, apply the pitch and envelope bends to all channels,
//	and reset NEEDBEND
static void APPLYBEND(void)
{
	UINT8* VTPtr;	// Register IY
	UINT8* PBPtr;	// Register IY
	UINT8 VocNum;	// Register B
	UINT8 NoteNum;	// Register C
	UINT8 ChnNum;	// Register E
	UINT8 CurBnk;	// Register D
	
	if (! NEEDBEND)
		return;		// return if no bend needed
	NEEDBEND = 0;	// clear the flag and go for it
	
	CHECKTICK();
	
	for (VTPtr = FMVTBL; ; VTPtr += 7)	// go through FM voice table
	{
		//pbfmloop:
		DACxME();
		if (VTPtr[VTBLFLAGS] == 0xFF)	// eot?
			break;						// yup - all done
		VocNum = VTPtr[VTBLFLAGS] & 7;	// voice number
		NoteNum = VTPtr[VTBLNOTE];		// note number
		ChnNum = VTPtr[VTBLCH];			// channel number
#ifdef DUAL_SUPPORT
		ChnNum &= 0x0F;
#endif
		
		PBPtr = &PBTBL[ChnNum];				// ptr to pitch/envelope bend for this ch
		if (! (PBPtr[PBRETRIG] & 0x01))		// check for change in bend on this channel [BIT #0]
			continue;
		
		noteonffreq = GETFREQ(0, ChnNum, NoteNum, PBPtr);	// get the new freq num for this voice
		
		if (VocNum <= 3)		// is voice in bank 1 ?
		{
			CurBnk = 0;			// indicates bank 0 to FMWr
		}
		else
		{
			VocNum -= 4;		// yes, subtract 4 (map 4-6 >> 0-2)
			CurBnk = 2;			// indicates bank 1 to FMWr
		}
#ifdef DUAL_SUPPORT
		CurBnk |= CHIP_BNK(VTPtr);
#endif
		//pbfmbank0:
		FMWr(CurBnk, VocNum, 0xA4, (noteonffreq & 0xFF00) >> 8);	// set frequency msb
		FMWr(CurBnk, VocNum, 0xA0, (noteonffreq & 0x00FF) >> 0);	// set frequency lsb
	}
	
	//pbpsg:
	for (VTPtr = PSGVTBL; ; VTPtr += 7)	// go through PSG voice table
	{
		//pbpsgloop:
		DACxME();
		if (VTPtr[VTBLFLAGS] == 0xFF)	// eot?
			break;						// yup - all done
		VocNum = VTPtr[VTBLFLAGS] & 7;	// voice number
		NoteNum = VTPtr[VTBLNOTE];		// note number
		ChnNum = VTPtr[VTBLCH];			// channel number
		
		PBPtr = &PBTBL[ChnNum];				// ptr to pitch/envelope bend for this ch
		if (! (PBPtr[PBRETRIG] & 0x01))		// check for change in bend on this channel [BIT #0]
			continue;
		
		noteonffreq = GETFREQ(1, ChnNum, NoteNum, PBPtr);	// get the new freq num for this voice
		
		DACxME();
		
		VocNum <<= 5;
		NoteNum = (noteonffreq & 0x00F) >> 0;
		PSG_Write(NoteNum | 0x80 | VocNum);
		
		NoteNum = (noteonffreq & 0xFF0) >> 4;
		PSG_Write(NoteNum);		// write tone msb
	}
	
	//pbdone:
	PBPtr = &PBTBL[PBRETRIG];
	for (ChnNum = 0; ChnNum < 16; ChnNum ++, PBPtr ++)
	{
		//pbdoneloop:
		PBPtr = 0;
	}
	
	return;
}

// GETFREQ - gets a frequency (for FM) or wavelength (for PSG) value from a note
//		number and a channel # (for adding pitch and envelope bends)
//
//		parameters:	A	0 for FM, 1 for PSG
//					C	note (0=C0, 95=B7)
//					E	channel
//					IX	pointer to this channel's PBTBL entry
//
//		returns:	DE	freq or wavelength value
static UINT16 GETFREQ(UINT8 VType, UINT8 Channel, UINT8 Note, UINT8* PbPtr)
{
	INT16 gfpbend;		// [10A6] local pitch bend
//	UINT8* PbPtr;		// Pitchbend Table Pointer, Register IX
	UINT16* FTPtr;		// Frequency Table Pointer, Register IX
	UINT8 TempByt;		// Register A
	UINT8 PbNote;		// Register C
	INT16 PBValue;		// Registers DE
	
	DACxME();
	
	gfpbend = (PbPtr[PBPBH] << 8) | (PbPtr[PBPBL] << 0);
	gfpbend += (PbPtr[PBEBH] << 8) | (PbPtr[PBEBL] << 0);// pitchbend(IX) + envelopebend(IX)
	
	DACxME();
	
	TempByt = Note + (gfpbend >> 8);	// semitone + semitone portion of bend
	if (TempByt >= 96)					// is it outside 0..95?
	{
		if (gfpbend < 0)				// yes - was bend up or down? [BIT #7]
		{
			gfpbend &= 0xFF00;			// down - peg at 0 (C0) [set gfpbend[0] = 00]
			TempByt = 0;
		}
		else
		{
			//gftoohi:
			gfpbend |= 0x00FF;			// [set gfpbend[0] = FF]
			TempByt = 95;				// up - peg at 95 (B7) and max frac pbend
		}
	}
	//gflookup:
	DACxME();
	if (! VType)					// voice type ? (dictates lookup method) [BIT #0]
	{
		//gfllufm:					// fm style lookup
		PbNote = TempByt / 12;
		TempByt %= 12;
		FTPtr = fmftbl;
	}
	else
	{
		//gflupsg:					// psg style lookup
		if (TempByt < 33)
		{
			TempByt = 0;
			gfpbend &= 0x00FF;		// [set gfpbend[0] = 00]
		}
		else
		{
			TempByt -= 33;			// lowest note for PSG is A2
		}
		//gflupsg1:
		FTPtr = psgftbl;
	}
	//gfinterp:						// interpolate up from value at (IX) by (gfpbend)
	FTPtr += TempByt;				// ptr in appropriate table
	
	DACxME();
	
	PBValue = FTPtr[1] - FTPtr[0];	// next table entry - this table entry
	
	PBValue = PBValue * (gfpbend & 0x00FF);	// (DE (table delta) * A (frac bend) ) * 256
	PBValue >>= 8;							// [of HL, set L = 0]
	// [Note: HL were used in the reverse way here with H as LSB and L as MSB.]
	
	DACxME();
	
	if (PBValue & 0x0080)
		PBValue |= 0xFF00;			// [of HL, set L = FF]
	//gfnoextnd:
	PBValue = FTPtr[0] + PBValue;	// this entry + (delta * frac);
	
	if (! VType)					// voice type ? [BIT #0]
	{
		// for FM, put octave in F number 13:11
		PBValue |= (PbNote << 3) << 8;
	}
	//gfdone:
	DACxME();
	return PBValue;
}

// MULTIPLY - unsigned 8 x 16 multiply: HL <- A * DE
//		MULADD entry point: for preloading HL with an offset
//		GETPATPTR entry point: HL <- PATCHDATA + 39 * A
static UINT8* GETPATPTR(UINT8 Channel)
{
#ifdef DUAL_SUPPORT
	if (Channel & 0x80)
		Channel &= 0x7F;
#endif
	return &PATCHDATA[Channel * 39];
}

//static UINT16 MULTIPLY(UINT8 Val8, UINT16 Val16)	// not needed in C


// NOTEON - note on (key on)
//		parameters:	B			midi channel
//					C			note number: 0..95 = C0..B7
//					IX			pointer to this channel's CCB
//					(CHPATPTR)	pointer to this channel's patch
static void NOTEON(UINT8 MidChn, UINT8 Note, UINT8* ChnCCB)
{
	DACxME();
	
	noteon.atten = MASTERATN + ChnCCB[CCBATN];	// sum channel and master attenuations, limit to 127
	if (noteon.atten >= 0x80)
		noteon.atten = 127;
	
	noteon.note = Note;	// save note and channel
	noteon.ch = MidChn;
	
	FILLDACFIFO(0x00);
	CHECKTICK();
	
	switch(*CHPATPTR)	// patch type (byte 0 of patch)
	{
	case 0:
		noteonfm(MidChn, ChnCCB);		// 0 for fm patches
		break;
	case 1:
		noteondig(MidChn, ChnCCB);		// 1 for digital patches
		break;
	case 2:
		noteonpsg(MidChn, ChnCCB, 0);	// [do noteontone + noteoneither]
		break;
	case 3:
		noteonpsg(MidChn, ChnCCB, 1);	// [do noteonnoise + noteoneither]
		break;
	}
	return;
}

// here to allocate a voice for a PSG patch
static void noteonpsg(UINT8 MidChn, UINT8* ChnCCB, UINT8 Mode)
{
	UINT8 AllocFlags;	// Register A
	UINT8* VTblPtr;		// Register IY/HL
	UINT16 ChnFreq;		// Register DE
	UINT8* ChnPat;		// Register IX
	UINT8* PSGCtrl;		// Register IY
	
	if (Mode)
	{
		//noteonnoise:
		AllocFlags = ALLOCSPEC(ChnCCB, PSGVTBLNG, &VTblPtr);	// try to get TG4 (noise ch)
	}
	else
	{
		//noteontone:
		AllocFlags = ALLOC(MidChn, ChnCCB, PSGVTBL, &VTblPtr);
	}
	//noteoneither:
	if (AllocFlags == 0xFF)
		return;		// return if unable to allocate a voice
	
	VTANDET(ChnCCB, VTblPtr, AllocFlags);	// call code shared by FM and PSG to update
											//   VoiceTable AND Envelope Trigger
	ChnFreq = GETFREQ(1, MidChn, noteon.note, &PBTBL[MidChn]);
	
	ChnPat = CHPATPTR + 1;					// patch pointer
	
	PSGCtrl = &pdata.psgcom[noteon.voice];	// psg control registers for this voice
	
	PSGCtrl[DTL] = (ChnFreq & 0x00F) >> 0;	// write tone lsb
	PSGCtrl[DTH] = (ChnFreq & 0xFF0) >> 4;	// write tone msb
	
	if (noteon.voice == 3)
	{
		PSGVTBLTG3[VTBLFLAGS] &= ~0x20;		// assume TG3 is not locked by this noise patch [RES #5]
		
		if ((ChnPat[-1] & 0x03) == 0x03)	// its TG4 - is it clocked by TG3?
		{
			// yes - move the frequency directly to TG3
			PSG_Write(PSGCtrl[DTL] | 0xC0);
			PSG_Write(PSGCtrl[DTH]);
			
			PSGVTBLTG3[VTBLFLAGS] = 0xA2;		// in the voice table... show TG3 free and locked
			PSGVTBLTG3[VTBLNOTE] = noteon.note;	// and store note and channel (for pitch mod)
			PSGVTBLTG3[VTBLCH] = noteon.ch;
			
			pdata.psgcom[2] = 4;				// and send a stop command to TG3 env processor
		}
		//psgnoise:
		PSGCtrl[DTL] = ChnPat[0];			// load and write noise data
	}
	//else [jr pskon]						// for TG1-TG3, go on to rest of control regs
	//pskon:
	PSGCtrl[ATK] = ChnPat[1];				// load and write attack rate
	PSGCtrl[SLV] = ChnPat[2] << 4;			// load and write sustain level, fix significance (<<4)
	PSGCtrl[ALV] = ChnPat[3] << 4;			// load and write attack level, fix significance (<<4)
	PSGCtrl[DKY] = ChnPat[4];				// load and write decay rate
	PSGCtrl[RRT] = ChnPat[5];				// load and write release rate
	PSGCtrl[COM] |= 0x01;					// key on command [SET #0]
	
	return;
}

// here for a digital patch note on
static void noteondig(UINT8 MidChn, UINT8* ChnCCB)
{
	UINT8 AllocFlags;	// Register A
	UINT8* VTblPtr;		// Register IY/HL
	UINT8 CurSmpl;
	UINT32 SmplPos;		// Register AHL
	INT8 TempByt;
	
	//noteonnoise:
	AllocFlags = ALLOCSPEC(ChnCCB, FMVTBLCH6, &VTblPtr);	// try to get FM voice 6 (DAC)
	if (AllocFlags == 0xFF)
		return;				// return if unable to allocate
	
	if (! (AllocFlags & 0x80))	// was it in use? [BIT #7]
	{
		DacMeEn = 0xC9;			// yes - disable DACME in case it was on to speed noteondig
		if (! (AllocFlags & 0x20))					// yes - was it FM? [BIT #5]
			FMWrite(0, 0x28, AllocFlags & 0x07);	// yes - do a keyoff
	}
	//noteondig2:
	VTANDET(ChnCCB, VTblPtr, AllocFlags);	// call code shared by FM and PSG to update
											//   VoiceTable AND Envelope Trigger
	FMVTBLCH6[VTBLFLAGS] |= 0x20;			// lock the voice from FM allocation [SET #5]
	
	// at this point, C is note number - C4 >> B7 equals samples  0 through 47 (for back compatibil
	//									 C0 >> B3 equals samples 48 through 96
	// trigger sample by reading sample bank table for header
	CurSmpl = noteon.note;	// [actually, it's read from Register C, which is unchanged from NOTEON]
	if (CurSmpl < 48)
		CurSmpl += 48;		// [The code actually does -48, and if negative, then +96]
	else
		CurSmpl -= 48;
	//noteondig21:
	SmplPos = Read24Bit(Tbls.DTBL68K);
	SmplPos += CurSmpl * 12;
	
	XFER68K(&SAMP.FLAGS, DData, SmplPos, 12);	// read 12 byte header, into ... sample header cache
	
	if (Read24Bit(SAMP.PTR) > 0x20000)			// [not in actual code - >512 KB]
		SAMP.FIRST = 0;
	if (SAMP.FIRST == 0)						// check for non-zero sample length
	{
		FMVTBLCH6[VTBLFLAGS] = 0xC6;			// empty sample - mark voice 6 free and releasing
		return;
	}
	
	//sampleok:
	// now check for sample playback rate override (2nd byte of patch != 4) - override rate in SAMP
	if (CHPATPTR[1] != 4)
	{
		SAMP.FLAGS &= 0xF0;					// replace counter value in flags (controls freq)
		SAMP.FLAGS |= CHPATPTR[1];
	}
	//sampleok1:
	DacFifoReg = 0;							// reset FIFO read ptr to start of buffer
	
	YM2612_Write(0, 0x24);					// set timer A msb
	TempByt = SAMP.FLAGS & 0x0F;
	TempByt = (-TempByt) >> 2;
	YM2612_Write(1, TempByt);
	
	YM2612_Write(0, 0x25);					// timer A lsb
	TempByt = SAMP.FLAGS & 0x0F;
	TempByt = (-TempByt) & 0x03;
	YM2612_Write(1, TempByt);
	
	YM2612_Write(0, 0x2B);					// enable the dac
	YM2612_Write(1, 0x80);
	
	YM2612_Write(0, 0x27);					// enable timer
	YM2612_Write(1, Ch3ModeReg);
	
	FMWrite(2, 0xB6, 0xC0);					// enable ch6 output to both R and L
	
	SmplPos = Read24Bit(Tbls.DTBL68K);		// pointer to sample table
	SmplPos += Read24Bit(SAMP.PTR);			// 24-bit sample start offset, add em up to get ptr to sample start
	SmplPos += SAMP.SKIP;					// add skip value to pointer for initial load
	Write24Bit(SAMPLEPTR, SmplPos);			// store read pointer for FILLDACFIFO
	
	SAMPLECTR = SAMP.FIRST;					// initialize counter
	
	FillDacEn = 0x00;						// enable full FILLDACFIFO routine
	DACFIFOWPTR = 0x00;						// start fill at 1F00
	
	if (SAMP.FLAGS & 0x10)					// looped? [BIT #4]
		FDFSTATE = 5;						// FDF=5 to run loop sample
	else
		FDFSTATE = 4;						// FDF=4 to run nonloop sample
	
	FILLDACFIFO(0x01);						// force the fill [calls FORCEFILLDF]
	
	DacMeEn = 0xD9;							// opcode "EXX", enable DACME routine
	
	if ((SAMP.FLAGS & 0x0F) < 10)			// check for slow dacme mode: samp rate = 5.2kHz, samples rate <= 5.2?
		DacMeRet = 0xC9;					// opcode "RET", to disable toggling
	else
		DacMeRet = 0x00;					// (if slow, put a NOP at DACMERET to enable toggling)
	// [Note: Toggling is done to make the Z80 do less busy-waiting for the YM2612.]
	
	if (SAMP.FLAGS & 0x80)
	{
		DacMeProc = DACME4BINST;			// jump to DACMEDSP for DACMEPROC
		DacCtrlPat = 0xAA;					// pattern to control nibble selection in DACMEDSP
	}
	else
	{
		DacMeProc = 0x00;					// (2 nops for 8 bit mode) [only 1 here]
	}
	
	DACPlay(CurSmpl, SAMP.FLAGS);
	
	return;
}

static void noteonfm(UINT8 MidChn, UINT8* ChnCCB)
{
	UINT8 AllocFlags;	// Register A
	UINT8* VTblPtr;		// Register IY/HL
	UINT8* ChnPat;		// Register IX
	UINT8 VocNum;		// Register B
	UINT8 CurBnk;		// Register D
	
	if (CHPATPTR[2] & 0x40)		// [BIT #6]
	{
		AllocFlags = ALLOCSPEC(ChnCCB, FMVTBLCH3, &VTblPtr);	// only CH3 will do for a CH3 mode patch
	}
	else
	{
		//noteonfm1:
		AllocFlags = ALLOC(MidChn, ChnCCB, FMVTBL, &VTblPtr);
	}
	//noteonfm15:
	DACxME();
	if (AllocFlags == 0xFF)
		return;				// return if unable to allocate a voice
	
#ifndef DUAL_SUPPORT
	if (! (AllocFlags & 0x80))					// was it in use? [BIT #7]
		FMWrite(0, 0x28, AllocFlags & 0x07);	// yes - do a keyoff
#else
	if (! (AllocFlags & 0x80))
		FMWrite(CHIP_BNK(VTblPtr), 0x28, AllocFlags & 0x07);
#endif
	//noteonfm2:
	VTANDET(ChnCCB, VTblPtr, AllocFlags);
	
	if (! (CHPATPTR[2] & 0x40))					// skip freq computation for CH3 mode [BIT #6]
	{
		noteonffreq = GETFREQ(0, MidChn, noteon.note, &PBTBL[MidChn]);
		DACxME();
	}
	
	//noteonfm3:
	ChnPat = CHPATPTR + 1;						// patch pointer + 1 (past type byte)
	if (noteon.voice == 2)						// channel 3 ?
	{
		Ch3ModeReg = ChnPat[1] | 0x15;			// yes - add CH3 mode bits to DACME's reset cmd
		FMWrgl(0x27, ChnPat[1] | 0x05);			// KEEP TIMER A ENABLED AND RUNNING, but not reset
	}
	//noteonfm4:
	VocNum = noteon.voice;
	if (VocNum <= 3)							// is voice in bank 1 ?
	{
		CurBnk = 0;								// indicates bank 0 to FMWr
	}
	else
	{
		VocNum -= 4;							// yes, subtract 4 (map 4-6 >> 0-2)
		CurBnk = 2;								// indicates bank 1 to FMWr
	}
#ifdef DUAL_SUPPORT
	CurBnk |= CHIP_BNK(VTblPtr);
#endif
	//fmbank0:
	if (ChnPat[0] & 0x08)						// only load if LFO on in this patch
	{
#ifndef DUAL_SUPPORT
		FMWrgl(0x22, ChnPat[0]);				// write lfo register
#else
		FMWrite(CurBnk & 0x04, 0x22, ChnPat[0]);
#endif
	}
	//fmlfodis:
	
	AllocFlags = ChnPat[2] & 7;					// lookup up carrier mask by alg number
	CARRIERS = CARRIERTBL[AllocFlags];			// bit 0 for op 1 carrier, bit 1 for op 2 carrier...
	
	WRITEFM(ChnPat, CurBnk, VocNum);
	
	if (! (ChnPat[1] & 0x40))					// check channel 3 mode [BIT #6]
	{
		FMWr(CurBnk, VocNum, 0xA4, (noteonffreq & 0xFF00) >> 8);	// set frequency msb
		FMWr(CurBnk, VocNum, 0xA0, (noteonffreq & 0x00FF) >> 0);	// set frequency lsb
		// jp fmkon								// go key on
	}
	else										// go set channel 3 frequency
	{
		//fmc3on:
		CurBnk = 0x00;
		FMWrite(CurBnk, 0xA6, ChnPat[28]);		// ch3 op1 msb
		FMWrite(CurBnk, 0xA2, ChnPat[29]);		// ch3 op1 lsb
		FMWrite(CurBnk, 0xAC, ChnPat[30]);		// ch3 op2 msb
		FMWrite(CurBnk, 0xA8, ChnPat[31]);		// ch3 op2 lsb
		FMWrite(CurBnk, 0xAD, ChnPat[32]);		// ch3 op3 msb
		FMWrite(CurBnk, 0xA9, ChnPat[33]);		// ch3 op3 lsb
		FMWrite(CurBnk, 0xAE, ChnPat[34]);		// ch3 op4 msb
		FMWrite(CurBnk, 0xAA, ChnPat[35]);		// ch3 op4 lsb
	}
	//fmkon:
#ifndef DUAL_SUPPORT
	FMWrite(0x00, 0x28, (ChnPat[36] << 4) | noteon.voice);
#else
	FMWrite(CurBnk & 0x04, 0x28, (ChnPat[36] << 4) | noteon.voice);
#endif
	
	return;
}

// WRITEFM - write a string of values to the FM chip. BC points to a null-terminated
//	list of reg/data pairs, where data is an offset off of IX. if data is 0, the
//	indirection is skipped and a 0 written (for the "proprietary register")
//	H <- 40H (MSB of FM chip register address)
//	D <- 0 for bank 0 (channels 0,1,2) or 2 for bank 1 (channels 3,4,5)
//	E <- channel within bank (0-2)
static void WRITEFM(const UINT8* InsList, UINT8 Bank, UINT8 Channel)
{
	const UINT8* RegList;	// Register BC
	UINT8 CurReg;
	UINT8 CurData;
	UINT8 NoteAtt;
	
	RegList = FMADDRTBL;
	while(*RegList)	// (0 = EOT)
	{
		CurReg = *RegList;
		RegList ++;
		
		//while(YM2612_Read(0) & 0x80)	// spin on busy bit
		//	;
		
		CurReg += Channel;				// add voice num to point at correct register
		YM2612_Write(Bank, CurReg);
		CurData = *RegList;				// get data offset
		if (CurData)					// if data offset 0, just write 0
		{
			if (CurData & 0x80)			// msb indicates total level values
			{
				CurData &= 0x7F;
				if (CARRIERS & 0x01)	// is this a carrier?
				{
					CurData = 127 - InsList[CurData];
					NoteAtt = ((CurData << 1) * noteon.atten) >> 8;
					CurData -= NoteAtt;	// reduce by attenuation amount
					CurData = 127 - CurData;
				}
				else					// no - normal output
				{
					//nottl0:
					//nottl:
					CurData = InsList[CurData];	// (IX+dataoffset)
				}
				CARRIERS >>= 1;			// [the best way to simulate RR + JP NC]
			}
			else
			{
				//nottl:
				CurData = InsList[CurData];	// (IX+dataoffset)
			}
		}
		
		//writefm0:
		YM2612_Write(Bank + 1, CurData);
		RegList ++;
	}
	
	return;
}

// VTANDET - code shared between FM and PSG note on routines for stuffing the
//   voice table entry and checking for envelope retrigger
static void VTANDET(UINT8* ChnCCB, UINT8* VTblPtr, UINT8 VFlags)
{
	// Parameters:
	//	 A - VTBL Flags
	//	HL - Voice Table Pointer [xxVTBL]
	//	IX - CCB Pointer
	UINT16 TempVal;
	
	VFlags &= 7;					// clear flags
	noteon.voice = VFlags;			// save allocated voice
	VTblPtr[VTBLFLAGS] = VFlags;
	
	TempVal = Read16Bit(&ChnCCB[CCBDURL]);
	if (TempVal)					// if non-zero duration, set self-time flag
		VTblPtr[VTBLFLAGS] |= 0x10;	// [SET #4]
	//noselftime:
	if (ChnCCB[CCBFLAGS] & 0x08)	// sfx tempo based? [BIT #3]
		VTblPtr[VTBLFLAGS] |= 0x08;	//  yes - set voice tbl sfx flag [SET #3]
	//nosfxtempo:
	VTblPtr[VTBLPRIO] = ChnCCB[CCBPRIO];
	VTblPtr[VTBLNOTE] = noteon.note;	// store note and channel in table
#ifndef DUAL_SUPPORT
	VTblPtr[VTBLCH] = noteon.ch;
#else
	VTblPtr[VTBLCH] &= ~0x0F;			// keep the high 4 bit
	VTblPtr[VTBLCH] |= noteon.ch;
#endif
	VTblPtr[VTBLDL] = ChnCCB[CCBDURL];
	VTblPtr[VTBLDH] = ChnCCB[CCBDURH];
	VTblPtr[VTBLRT] = 254;			// init release timer
	
	DACxME();
	
	if (ChnCCB[CCBFLAGS] & 0x20)	// envelope retrigger on?
	{
		// yes - trigger the envelope
		TRIGENV(ChnCCB, noteon.ch, ChnCCB[CCBENV]);
		DACxME();
	}
	
	return;
}

// NOTEOFF - note off (key off)
//
//		parameters:	B	midi channel
//					C	note number: bits 6:4 = octave, bits 3:0 = note (0-11)
static void NOTEOFF(UINT8 MidChn, UINT8 NoteNum)
{
//	UINT8 noteoffnote;	// [169C] [not needed, equals NoteNum]
//	UINT8 noteoffch;	// [169D] [not needed, equals MidChn]
	UINT8 VocFlags;
	
	DACxME();
	VocFlags = DEALLOC(MidChn, NoteNum, FMVTBL);
	
	if (VocFlags != 0xFF)	// was note found?
	{
		VocFlags &= 0x27;		// yes - locked channel six?
		if (VocFlags == 0x26)	//   yes - do digital note off
		{
			//digoff:
			NOTEOFFDIG();
		}
		else					//   no - get note number
		{
#ifndef DUAL_SUPPORT
			VocFlags &= 7;
			FMWrite(0, 0x28, VocFlags);	// key off
#else
			FMWrite((VocFlags & 0x08) >> 1, 0x28, VocFlags & 0x07);
#endif
		}
		return;
	}
	//trypsg:
	DACxME();
	VocFlags = DEALLOC(MidChn, NoteNum, PSGVTBL);
	
	if (VocFlags != 0xFF)	// was note found?
	{
		VocFlags &= 0x03;
		pdata.psgcom[VocFlags] |= 0x02;	// set key off command [SET #2]
		return;
	}
	//trynois:
	DACxME();
	VocFlags = DEALLOC(MidChn, NoteNum, PSGVTBLNG);
	
	if (VocFlags != 0xFF)	// was note found?
	{
		VocFlags &= 0x03;
		pdata.psgcom[VocFlags] |= 0x02;	// set key off command [SET #2]
		return;
	}
	
	return;
}

static void NOTEOFFDIG(void)
{
	if (SAMP.FLAGS & 0x20)		// is clip@noteoff set? [BIT #5]
	{
		//noteoffdig1:
		FDFSTATE = 7;			// yes - shut down digitial
		DACStop(0x00);
	}
	else if (SAMP.FLAGS & 0x10)	// is it looped? [BIT #4]
	{
		//noteoffdig2:
		FDFSTATE = 6;			// yes - indicate end of loop
		DACStop(0x01);
	}
	
	return;
}

// PCHANGE - program change
static void PCHANGE(void)
{
	UINT8 Channel;		// Register A
	UINT8* ChnCCB;		// Register IX
	
	Channel = GETCCBPTR(&ChnCCB);	// GETCBYTE for channel, IX <- CCB ptr, A <- channel
	CHPATPTR = GETPATPTR(Channel);	// PATCHDATA + 39 * A
	
	ChnCCB[CCBPNUM] = GETCBYTE();	// set program number in CCB
	
	// actually the code falls though to FETCHPATCH
	FETCHPATCH(ChnCCB);
	
	return;
}

static void FETCHPATCH(UINT8* ChnCCB)
{
	// Parameters:
	//	IX - CCB Pointer
	UINT32 PTblPtr;
	UINT16 TempOfs;
	UINT8 fpoffset[2];	// [1726], shared with TRIGENV
	UINT16 fpoffset_s;
	
	TempOfs = ChnCCB[CCBPNUM] << 1;					// pnum * 2
	PTblPtr = Read24Bit(Tbls.PTBL68K) + TempOfs;	// pointer to this patch's offset
	XFER68K(fpoffset, PData, PTblPtr, 2);			// read 2 byte offset, into local fpoffset
	fpoffset_s = Read16Bit(fpoffset);
	
	PTblPtr = Read24Bit(Tbls.PTBL68K) + fpoffset_s;	// pointer to patch data
	XFER68K(CHPATPTR, PData, PTblPtr, 39);			// xfer the 39 byte patch into this channel's patch buffer
	
	return;
}

static void PATCHLOAD(UINT8 InsNum)
{
	UINT8 CurChn;		// Register C, loop counter
	UINT8* ChnCCB;		// Register IX
	
	ChnCCB = CCB;
	CHPATPTR = PATCHDATA;
	for (CurChn = 0; CurChn < 16; CurChn ++, ChnCCB += 32, CHPATPTR += 39)
	{
		//plloop:
		if (ChnCCB[CCBPNUM] == InsNum)
		{
			FETCHPATCH(ChnCCB);
		}
		//plloop1:
		// [move to next channel]
	}
	
	return;
}

/**************************  Dynamic Voice Allocation **************************/

// ALLOC     - dynamic voice allocation routine
// ALLOCSPEC - special entry point for only allocating or not the single voice at (IY)
//
//		parameters:	B	channel
//					IX	pointer to this channel's CCB
//					IY	first entry in appropriate voice table
//
//		returns:	A	flags of voice allocated, or FF if none allocated
//					HL	pointer to entry allocated
static UINT8 ALLOC(UINT8 MidChn, UINT8* ChnCCB, UINT8* VTblPtr, UINT8** RetAlloc)
{
	UINT8* avlowestp;	// [17DA] pointer to lowest priority
	UINT8* avfreestp;	// [17DC] pointer to longest free
	UINT8 CurPrio;		// Register C
	UINT8 CurFree;		// Register L
	
	CurPrio = 0xFF;		// lowest prio so far (max actually 7FH)
	CurFree = 0xFF;		// freest so far (max actually 0FE)
	//avloop:
	for ( ; VTblPtr[VTBLFLAGS] != 0xFF; VTblPtr += 7)	// [Note: Actually there should be a DACxME() before the += 7]
	{
		//avstart:
		if (VTblPtr[VTBLFLAGS] & 0x20)		// channel locked? [BIT #5]
			continue;						// yup - skip it
		if (VTblPtr[VTBLFLAGS] & 0x80)		// check free/used [BIT #7]
		{
			//avfree:
			// its free - same channel is requester?, yes - sustain on? [BIT #7]
#ifndef DUAL_SUPPORT
			if (VTblPtr[VTBLCH] == MidChn && ! (ChnCCB[CCBFLAGS] & 0x80))
#else
			if ((VTblPtr[VTBLCH] & 0x0F) == MidChn && ! (ChnCCB[CCBFLAGS] & 0x80))
#endif
			{
				*RetAlloc = VTblPtr;
				return VTblPtr[VTBLFLAGS];
			}
			//avdiffch:
			if (VTblPtr[VTBLRT] < CurFree)		// freer than freest so far?
			{
				CurFree = VTblPtr[VTBLRT];		// yes - so make this the freest
				avfreestp = VTblPtr;
			}
		}
		else
		{
			if (VTblPtr[VTBLPRIO] < CurPrio)	// in use - check priority against lowest so far
			{
				CurPrio = VTblPtr[VTBLPRIO];	// yes - so make this lowest
				avlowestp = VTblPtr;
			}
		}
	}
	// end of table?
	//   yes - look into taking an in use voice
	//aveot:
	if (CurFree != 0xFF)		// any found free?
	{
		*RetAlloc = avfreestp;	// yes take freest
		return avfreestp[VTBLFLAGS];
	}
	//avtakeused:
									// no free ones - check lowest so far priority
	//avspecprio:
	if (CurPrio > ChnCCB[CCBPRIO])	// compare to priority of this channel, this channel >= lowest priority
		return 0xFF;				// failed to allocate
	//avtakeit:
	*RetAlloc = avlowestp;
	return avlowestp[VTBLFLAGS];
}

static UINT8 ALLOCSPEC(UINT8* ChnCCB, UINT8* VTblPtr, UINT8** RetAlloc)
{
	UINT8 VFlags;
	
	DACxME();
	
									// here to only try to allocate the voice at (IY)
	if (VTblPtr[VTBLFLAGS] & 0x80)	// free? [BIT #7]
	{
		*RetAlloc = VTblPtr;
		return VTblPtr[VTBLFLAGS];	// yes - take it
	}
	//avspecused:
	VFlags = VTblPtr[VTBLPRIO];		// no - have to check priority first
	//avspecprio:
	if (VFlags > ChnCCB[CCBPRIO])	// compare to priority of this channel, this channel >= lowest priority
		return 0xFF;				// failed to allocate
	//avtakeit:
	*RetAlloc = VTblPtr;
	return VTblPtr[VTBLFLAGS];
}

// DEALLOC - deallocate a voice by searching for a match on notenum and channel.
//   for now - for digital do nuthing
//   if release timer (byte 6) is zero, then set free bit immediately, otherwise
//   set release bit (a 60 Hz routine will count this down and set free when its zero)
//
//		parameters:	B	channel
//					C	note
//					IX	top of voice table
//
//		returns:	A	flags byte of deallocated voice, or
//						  0FFH if note not found
static UINT8 DEALLOC(UINT8 MidChn, UINT8 Note, UINT8* VTblPtr)
{
	UINT8 VFlags;
	
	//dvloop:
	for ( ; VTblPtr[VTBLFLAGS] != 0xFF; VTblPtr += 7)
	{
		//dvstart:
		if (VTblPtr[VTBLFLAGS] & 0x80)	// if if free skip this voice [BIT #7]
			continue;
#ifndef DUAL_SUPPORT
		if (VTblPtr[VTBLNOTE] == Note && VTblPtr[VTBLCH] == MidChn)
#else
		if (VTblPtr[VTBLNOTE] == Note && (VTblPtr[VTBLCH] & 0x0F) == MidChn)
#endif
		{
			VFlags = VTblPtr[VTBLFLAGS];
			if ((VFlags & 0x27) != 0x26)						// check for digital - locked and voice num=6
				VTblPtr[VTBLFLAGS] = (VFlags & 0x27) | 0xC0;	// set free and release
#ifdef DUAL_SUPPORT
			VFlags &= ~0x08;
			VFlags |= (VTblPtr[VTBLCH] & 0x80) >> 4;
#endif
			//deallocdig:
			return VFlags;										// [return old VTblPtr[VTBLFLAGS]]
		}
	}
	
	return 0xFF;	// eot - return FF in A for not found [actually returns VTblPtr[VTBLFLAGS]]
}


void SetROMFile(const UINT8* ROMPtr)
{
	ROMData = ROMPtr;
	PData = ROMPtr;
	Tbls.PTBL68K[0] = Tbls.PTBL68K[1] = Tbls.PTBL68K[2] = 0x00;
	EData = ROMPtr;
	Tbls.ETBL68K[0] = Tbls.ETBL68K[1] = Tbls.ETBL68K[2] = 0x00;
	SData = ROMPtr;
	Tbls.STBL68K[0] = Tbls.STBL68K[1] = Tbls.STBL68K[2] = 0x00;
	DData = ROMPtr;
	Tbls.DTBL68K[0] = Tbls.DTBL68K[1] = Tbls.DTBL68K[2] = 0x00;
	
	return;
}

void SetDataFiles(const UINT8* PatPtr, const UINT8* EnvPtr,
				  const UINT8* SeqPtr, const UINT8* SmpPtr)
{
	ROMData = NULL;
	PData = PatPtr;
	Tbls.PTBL68K[0] = Tbls.PTBL68K[1] = Tbls.PTBL68K[2] = 0x00;
	EData = EnvPtr;
	Tbls.ETBL68K[0] = Tbls.ETBL68K[1] = Tbls.ETBL68K[2] = 0x00;
	SData = SeqPtr;
	Tbls.STBL68K[0] = Tbls.STBL68K[1] = Tbls.STBL68K[2] = 0x00;
	DData = SmpPtr;
	Tbls.DTBL68K[0] = Tbls.DTBL68K[1] = Tbls.DTBL68K[2] = 0x00;
	
	// Catch NULL-pointers in a way similar to most games.
	if (DData == NULL)
		DData = NullData;
	if (SData == NULL)
		SData = DData;
	if (EData == NULL)
		EData = SData;
	if (PData == NULL)
		PData = EData;
	
	return;
}

const UINT8* GetDataPtr(UINT8 DataType)
{
	if (DataType == 0x00)
		return PData + Read24Bit(Tbls.PTBL68K);
	else if (DataType == 0x01)
		return EData + Read24Bit(Tbls.ETBL68K);
	else if (DataType == 0x02)
		return SData + Read24Bit(Tbls.STBL68K);
	else if (DataType == 0x03)
		return DData + Read24Bit(Tbls.DTBL68K);
	
	return NULL;
}

void WriteFIFOCommand(UINT8 CmdByte)
{
	CMDFIFO[NewCmdWPtr] = CmdByte;
	NewCmdWPtr ++;
	NewCmdWPtr &= 0x3F;
	
	return;
}

void FinishCommandFIFO(void)
{
	CMDWPTR = NewCmdWPtr;
	
	return;
}


static void CheckForSongEnd(void)
{
	UINT8 CurChn;
	UINT8 ChnMask;
	UINT8* ChnCCB;
	UINT8* VTblPtr;
	
	ChnMask = 0x00;
	for (CurChn = 0x00; CurChn < 0x10; CurChn ++)
	{
		ChnCCB = GETCCBPTR2(CurChn);
		if (ChnCCB[CCBFLAGS] & 0x01)	// channel in use?
			ChnMask |= (1 << CurChn);	// add to channel mask
	}
	
	if (ChnMask)
	{
		SongAlmostEnd = 0x00;
		return;
	}
	
	SongAlmostEnd = 0x01;
	
	VTblPtr = FMVTBL;
	for ( ; VTblPtr[VTBLFLAGS] != 0xFF; VTblPtr += 7)
	{
		if (VTblPtr[VTBLFLAGS] & 0x10)
			ChnMask ++;
	}
	
	VTblPtr = PSGVTBL;
	for ( ; VTblPtr[VTBLFLAGS] != 0xFF; VTblPtr += 7)
	{
		if (! (VTblPtr[VTBLFLAGS] & 0x80))
			ChnMask ++;
	}
	
	VTblPtr = PSGVTBLNG;
	for ( ; VTblPtr[VTBLFLAGS] != 0xFF; VTblPtr += 7)
	{
		if (! (VTblPtr[VTBLFLAGS] & 0x80))
			ChnMask ++;
	}
	
	for (CurChn = 0; CurChn < 4; CurChn ++)
	{
		if (pdata.psgenv[CurChn])
			ChnMask ++;
	}
	
	if (! ChnMask)
	{
		StopSignal();	// no channel running - signal sequence end
		SongAlmostEnd = 0x00;
	}
	
	return;
}
