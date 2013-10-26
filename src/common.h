#ifndef _COMMON_H
#define _COMMON_H

typedef signed char					s8;
typedef unsigned char				u8;
typedef signed short				s16;
typedef unsigned short				u16;
typedef signed int					s32;
typedef unsigned int				u32;
#define sysmem_alloc(x) (malloc(x))
#define sysmem_free(x)  (free(x))

#define DEBUGASSERT(x) assert(x)

// This defines an appropriate C-based function inline qualifier
#if (defined _MSC_VER)					// Microsoft Visual C++
#	define inlinefunc static __inline
#elif (defined __GNUC__)				// GNU GCC
#	define inlinefunc static __inline__
#else									// Unknown compiler; no inline marker!
#	define inlinefunc
#endif

#endif
