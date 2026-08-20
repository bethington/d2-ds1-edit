#ifndef _WINDS1EDIT_TYPES_H_

#define _WINDS1EDIT_TYPES_H_

/* These name on-disk field widths (DS1/DT1/DCC/DC6/COF records), so they have
   to be exact. "unsigned long" was 32-bit under MSVC's LLP64 but is 64-bit
   under the LP64 model Linux and macOS use, which silently doubled the width
   of every UDWORD field. Pin them to stdint. */
#include <stdint.h>

typedef uint8_t   UBYTE;
typedef int16_t   WORD;
typedef uint16_t  UWORD;
typedef uint32_t  UDWORD;

// Disable warning message : warning C4996: 'sprintf': This function or variable may be unsafe. Consider using sprintf_s instead. To disable deprecation, use _CRT_SECURE_NO_WARNINGS. See online help for details.
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#endif
