#ifndef _S2RAS_TYPES_H
#define _S2RAS_TYPES_H

// This defines an appropriate C-based function inline qualifier
#if (defined _MSC_VER)					// Microsoft Visual C++
#	define inlinefunc static __inline
#elif (defined __GNUC__)				// GNU GCC
#	define inlinefunc static __inline__
#else									// Unknown compiler; no inline marker!
#	define inlinefunc
#endif

#define DEBUGASSERT(x)		// assert(x)

#ifndef NULL
#define NULL 0
#endif
#define TRUE 1
#define FALSE 0

typedef signed char					s8;
typedef unsigned char				u8;
typedef signed short				s16;
typedef unsigned short				u16;
typedef signed long					s32;
typedef unsigned long				u32;

#ifndef ALLEGRO_H		// Allegro defines the same thing basically the same way...
typedef s32							fixed;		// 16.16 fixed point type
#endif

typedef	u8							input;		// Size of controller input data

typedef u8							animscript;	// Animation script data size
typedef const animscript * const	animtable;	// Table of animation scripts

typedef struct coll_rect
{
	s16 x;	// X position of object (WHOLE part ONLY)
	s16 y;	// Y position of object (WHOLE part ONLY)
	u16 w;	// Width of object
	u16 h;	// Height of object 
	// (Height is actually usually only a byte in Sega's world, but might as well make this EVEN since most compilers will anyway)
} coll_rect;	// Collision rectangle

#define fixToInt(x)		((s16)  ((x) >> 16))			// 16.16 fixed --> 16bit int
#define intToFix(x)		((fixed)(((s32)(x)) << 16))		// 16bit int --> 16.16 fixed

// 8.8 fixed (used on velocities)
#define fix8ToInt(x)	((s8)   ((x) >> 8))				// 8.8 fixed --> 8bit int
#define intToFix8(x)	((s16)  (((s16)(x)) << 8))		// 8bit int --> 8.8 fixed
#define fix8ToFix(x)	((fixed)(((s32)(x)) << 8))		// 8.8 fixed --> 16.16 fixed

// Multiplies two 8.8FP values together
#define mulFix8(x, y)	((s16)(((s32)(x) * (s32)(y)) >> 8))

// These must be forward-declared for GCC to not freak out about them being used
// in function prototypes before their actual definition
struct object;
struct player;

#endif
