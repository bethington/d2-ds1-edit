/* Raw palette-index frames: the shared output of the sprite decoders.
 *
 * The DC6 and DCC decoders produce these buffers first; every bitmap or RGBA
 * consumer (the editor's animated objects, asset export, compose export)
 * converts from them. Tests and the reference probe read them directly, so
 * the indices are never reconstructed from colours.
 *
 * Transparency policy: palette index 0 is transparent. The formats have no
 * separate opacity mask -- a DC6 "skip" and a literal index 0 both leave 0 in
 * the buffer -- so nothing is discarded by representing a frame this way.
 *
 * Coordinates: (origin_x, origin_y) is the signed sprite-space position of
 * pixel (0, 0), the top-left of the buffer. Rows are stored top-down with
 * `stride` bytes per row. */

#ifndef _DS1EDIT_INDEX_FRAME_H_
#define _DS1EDIT_INDEX_FRAME_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DS1EDIT_INDEX_FRAME
{
   int32_t   width;
   int32_t   height;
   int32_t   stride;
   int32_t   origin_x;
   int32_t   origin_y;
   int32_t   frame;
   int32_t   direction;
   size_t    pixels_len;
   uint8_t * pixels;      /* owned; stride * height bytes */
} DS1EDIT_INDEX_FRAME;

/* Pass as `direction` to decode every direction, ordered direction-major. */
#define DS1EDIT_ALL_DIRECTIONS (-1)

/* Release every owned pixel buffer and the array itself. NULL-safe. */
void ds1edit_index_frames_free(DS1EDIT_INDEX_FRAME *frames, size_t count);

/* Union of the frames' sprite-space rectangles. Empty (0x0) frames do not
 * contribute. Returns 1 and fills the outputs when at least one frame is
 * non-empty, 0 otherwise. */
int ds1edit_index_frames_bounds(const DS1EDIT_INDEX_FRAME *frames, size_t count,
                                int32_t *x_min, int32_t *y_min,
                                int32_t *width, int32_t *height);

/* Copy `frame` into an index canvas whose top-left pixel sits at sprite-space
 * (canvas_x, canvas_y). Index 0 is transparent and never overwrites. Writes
 * are clipped to the canvas. */
void ds1edit_index_frame_place(const DS1EDIT_INDEX_FRAME *frame,
                               uint8_t *canvas, int32_t canvas_w,
                               int32_t canvas_h,
                               int32_t canvas_x, int32_t canvas_y);

/* Lay frames (typically one direction) onto canvases the size of their
 * union, top-left at sprite-space (*off_x, *off_y) -- how the editor and the
 * exports lay a DC6 direction out. Returns 0 on success; release with
 * ds1edit_index_canvases_free(*canvases, count). */
int  ds1edit_index_frames_to_canvases(const DS1EDIT_INDEX_FRAME *frames,
                                      size_t count, uint8_t ***canvases,
                                      int *w, int *h, int *off_x, int *off_y);
void ds1edit_index_canvases_free(uint8_t **canvases, size_t count);

/* Decode DC6 bytes into raw index frames.
 *
 * `direction` selects one direction, or DS1EDIT_ALL_DIRECTIONS. Frames are
 * returned in file order, each at its own size and origin
 * (origin_y = offsetY - height + 1, rows bottom-up in the file). The `flip`
 * dword is not interpreted. Returns 1 on success, 0 on failure with a message
 * in `error` (when error_cap > 0); *frames/*count are NULL/0 on failure. */
int dc6_decode_indices(const void *bytes, size_t len, int direction,
                       DS1EDIT_INDEX_FRAME **frames, size_t *count,
                       char *error, size_t error_cap);

/* Decode DCC bytes into raw index frames (implemented in dcc.c).
 *
 * Every frame of a direction is returned at the direction's bounding box --
 * the size the decoder's frame buffer has, and the one every consumer draws
 * -- with origin_x/origin_y = the box's minimum corner. Same conventions and
 * return contract as dc6_decode_indices. */
int dcc_decode_indices(const void *bytes, size_t len, int direction,
                       DS1EDIT_INDEX_FRAME **frames, size_t *count,
                       char *error, size_t error_cap);

#ifdef __cplusplus
}
#endif

#endif
