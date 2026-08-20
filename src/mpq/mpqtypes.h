/* Stormless MPQ sample code by Tom Amigo (http://www.angelfire.com/sc/mpq/, 2000),
 * as adapted by Paul Siramy for win_ds1edit. See NOTICE at the repository root
 * for attribution and license status. */

#ifndef _MPQTYPES_H_

#define _MPQTYPES_H_

#include <stdio.h>
#include <stdint.h>

/* The MPQ format is 32-bit on disk everywhere, so say so explicitly rather
 * than borrowing the host's `long`. The original code defined these as
 * `unsigned long`, which is only 32-bit under the Windows data model; on LP64
 * hosts (macOS, Linux) it is 64-bit, which doubles the width of every field in
 * the header and both tables. The header scan uses sizeof(DWORD) against
 * 4-byte constants, so a perfectly valid archive is rejected as "not a valid
 * MPQ archive" before anything else gets a chance to go wrong. */
typedef uint8_t  UInt8;
typedef uint16_t UInt16;
typedef int16_t  SInt16;
typedef uint32_t UInt32;
typedef int32_t  SInt32;

#if defined(_WIN32)
/* Match the Windows SDK's own DWORD exactly. It is already 32-bit there, and
 * using the identical type avoids a redefinition clash in any translation unit
 * that also pulls in windows.h. */
typedef unsigned long DWORD;
#else
typedef uint32_t DWORD;
#endif

#define MPQTYPES_MAX_PATH   256


// global datas for reading mpq
typedef struct GLB_MPQ_S
{
   int   is_open;          // FALSE / TRUE

   DWORD	offset_mpq;			// Offset to MPQ file data
   DWORD	offset_htbl;		// Offset to hash_table of MPQ
   DWORD	offset_btbl;		// Offset to block_table of MPQ
   DWORD	lenght_mpq_part;	// Lenght of MPQ file data
   DWORD	lenght_htbl;		// Lenght of hash table
   DWORD	lenght_btbl;		// Lenght of block table
   DWORD	*hash_table;		// Hash table
   DWORD	*block_table;		// Block table
   DWORD	count_files;		// Number of files in MPQ (calculated from size of block_table)
   DWORD	massive_base[0x500];// This massive is used to calculate crc and decode files
   char	*filename_table;	// Array of MPQ filenames
   char	*identify_table;	// Bitmap table of MPQ filenames 1 - if file name for this entry is known, 0 - if is not

   char	file_name[257];		// Name of archive
   char	work_dir[MPQTYPES_MAX_PATH];	// Work directory
   char	prnbuf[MPQTYPES_MAX_PATH+100];	// Buffer
   char	default_list[MPQTYPES_MAX_PATH];// Path to list file
   FILE	*fpMpq;

   // This is used to decompress DCL-compressed and WAVE files
   DWORD	 avail_metods[4];//={0x08,0x01,0x40,0x80};
   DWORD	 lenght_write;
   UInt8  * global_buffer, * read_buffer_start, * write_buffer_start, * explode_buffer;
   UInt32 * file_header;
} GLB_MPQ_S;

extern GLB_MPQ_S * glb_mpq;

#endif

