// typedefs to use MAME's (U)INTxx types (copied from MAME\src\ods\odscomm.h)
#include "stdtype.h"

/* offsets and addresses are 32-bit (for now...) */
typedef UINT32	offs_t;

/* stream_sample_t is used to represent a single sample in a sound stream */
typedef INT16 stream_sample_t;

#if defined(_MSC_VER)
//#define INLINE	static __forceinline
#define INLINE	static __inline
#elif defined(__GNUC__)
#define INLINE	static __inline__
#else
#define INLINE	static inline
#endif
#define M_PI	3.14159265358979323846

#ifdef _DEBUG
#define logerror	printf
#else
#define logerror
#endif
