#ifndef _APNG_WRITER_H_
#define _APNG_WRITER_H_

// Minimal APNG (Animated PNG) encoder. Emits 8-bit RGBA frames with
// per-frame delays. The first frame doubles as the default still image
// (IDAT) so non-APNG viewers see frame 0 as a normal PNG. Subsequent
// frames are stored as fcTL + fdAT chunk pairs.
//
// The encoder uses the standard PNG/APNG chunk format (see APNG spec
// https://wiki.mozilla.org/APNG_Specification) and zlib (already a
// transitive dependency of Allegro 5's image addon) for image data
// compression and CRC32. No third-party APNG patches required.

typedef struct APNG_WRITER_S APNG_WRITER_S;

// Open an APNG file for writing. Returns NULL on failure (e.g. fopen
// failed, bad parameters).
//
//   path        Output file path.
//   width       Frame width in pixels (must be > 0).
//   height      Frame height in pixels (must be > 0).
//   num_frames  Total number of frames the writer will receive
//               (must be >= 1). The acTL chunk is written up front
//               and must match the actual count.
//   num_plays   Loop count. 0 means infinite loop.
APNG_WRITER_S * apng_writer_open(const char *path, int width, int height,
                                 int num_frames, int num_plays);

// Append one frame. Pixels is a row-major RGBA buffer of width*height*4
// bytes (R, G, B, A in that order; alpha 0 = fully transparent, 255 =
// fully opaque).
//
//   delay_num/delay_den  Per-frame display duration as a rational. For
//   example 40/1000 = 40 ms; 1/25 = 40 ms; 100/100 = 1 s. Per the APNG
//   spec, a delay_den of 0 is treated as 100.
//
// Returns 1 on success, 0 on failure.
int apng_writer_write_frame(APNG_WRITER_S *w,
                            const unsigned char *pixels,
                            int delay_num, int delay_den);

// Finalize and close the file. Returns 1 on success, 0 if any frame
// write failed or the underlying fclose failed. The writer pointer is
// freed regardless of return value.
int apng_writer_close(APNG_WRITER_S *w);

#endif
