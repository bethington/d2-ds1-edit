/* Raw palette-index frames and the DC6 decoder that produces them.
 * See index_frame.h for the conventions. No Allegro dependency. */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/index_frame.h"

#define DC6_HEADER_SIZE        24
#define DC6_FRAME_HEADER_SIZE  32
/* Largest DC6 frame side in vanilla is 319; this only stops absurd
 * allocations from corrupt headers. */
#define DC6_MAX_SIDE           4096

static void set_error(char *error, size_t cap, const char *fmt, ...)
{
   va_list ap;

   if (error == NULL || cap == 0)
      return;
   va_start(ap, fmt);
   vsnprintf(error, cap, fmt, ap);
   va_end(ap);
}

static int32_t read_i32(const uint8_t *p)
{
   return (int32_t) ((uint32_t) p[0]
                   | ((uint32_t) p[1] << 8)
                   | ((uint32_t) p[2] << 16)
                   | ((uint32_t) p[3] << 24));
}

void ds1edit_index_frames_free(DS1EDIT_INDEX_FRAME *frames, size_t count)
{
   size_t i;

   if (frames == NULL)
      return;
   for (i = 0; i < count; i++)
      free(frames[i].pixels);
   free(frames);
}

int ds1edit_index_frames_bounds(const DS1EDIT_INDEX_FRAME *frames, size_t count,
                                int32_t *x_min, int32_t *y_min,
                                int32_t *width, int32_t *height)
{
   size_t i;
   int    found = 0;
   int64_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;

   if (frames == NULL)
      return 0;

   for (i = 0; i < count; i++)
   {
      const DS1EDIT_INDEX_FRAME *f = &frames[i];
      int64_t fx1, fy1, fx2, fy2;

      if (f->width <= 0 || f->height <= 0)
         continue;
      fx1 = f->origin_x;
      fy1 = f->origin_y;
      fx2 = fx1 + f->width - 1;
      fy2 = fy1 + f->height - 1;
      if (!found)
      {
         x1 = fx1; y1 = fy1; x2 = fx2; y2 = fy2;
         found = 1;
      }
      else
      {
         if (fx1 < x1) x1 = fx1;
         if (fy1 < y1) y1 = fy1;
         if (fx2 > x2) x2 = fx2;
         if (fy2 > y2) y2 = fy2;
      }
   }
   if (!found)
      return 0;

   if (x_min)  *x_min  = (int32_t) x1;
   if (y_min)  *y_min  = (int32_t) y1;
   if (width)  *width  = (int32_t) (x2 - x1 + 1);
   if (height) *height = (int32_t) (y2 - y1 + 1);
   return 1;
}

void ds1edit_index_frame_place(const DS1EDIT_INDEX_FRAME *frame,
                               uint8_t *canvas, int32_t canvas_w,
                               int32_t canvas_h,
                               int32_t canvas_x, int32_t canvas_y)
{
   int32_t x, y;

   if (frame == NULL || canvas == NULL || frame->pixels == NULL)
      return;

   for (y = 0; y < frame->height; y++)
   {
      int64_t cy = (int64_t) frame->origin_y + y - canvas_y;
      const uint8_t *row = frame->pixels + (size_t) y * frame->stride;

      if (cy < 0 || cy >= canvas_h)
         continue;
      for (x = 0; x < frame->width; x++)
      {
         int64_t cx = (int64_t) frame->origin_x + x - canvas_x;

         if (row[x] == 0 || cx < 0 || cx >= canvas_w)
            continue;
         canvas[(size_t) cy * canvas_w + (size_t) cx] = row[x];
      }
   }
}

/* ======================================================================== */
int ds1edit_index_frames_to_canvases(const DS1EDIT_INDEX_FRAME * frames, size_t count,
                           uint8_t *** canvases, int * w, int * h,
                           int * off_x, int * off_y)
{
   int32_t x1, y1, cw, ch;
   size_t  i;
   uint8_t   ** out;

   if ((canvases == NULL) || (w == NULL) || (h == NULL)
       || (off_x == NULL) || (off_y == NULL))
      return 1;
   *canvases = NULL;
   if (! ds1edit_index_frames_bounds(frames, count, & x1, & y1, & cw, & ch))
      return 1;

   out = (uint8_t **) calloc(count, sizeof(uint8_t *));
   if (out == NULL)
      return 1;
   for (i = 0; i < count; i++)
   {
      out[i] = (uint8_t *) calloc((size_t) cw * (size_t) ch, 1);
      if (out[i] == NULL)
      {
         ds1edit_index_canvases_free(out, count);
         return 1;
      }
      ds1edit_index_frame_place(& frames[i], out[i], cw, ch, x1, y1);
   }

   *canvases = out;
   *w        = cw;
   *h        = ch;
   *off_x    = x1;
   *off_y    = y1;
   return 0;
}


/* ======================================================================== */
void ds1edit_index_canvases_free(uint8_t ** canvases, size_t count)
{
   size_t i;

   if (canvases == NULL)
      return;
   for (i = 0; i < count; i++)
      free(canvases[i]);
   free(canvases);
}


/* Decode one DC6 scanline stream into a width x height buffer.
 *
 * Opcodes (BlitSpriteRLECopyFull): 0x80 ends the row, 0x81..0xFF skips
 * n & 0x7F pixels, 0x00..0x7F copies the next n bytes. Rows are bottom-up.
 * Pixels that land outside the frame are dropped, never written; a copy that
 * runs past the stream is an error. */
static int dc6_decode_stream(const uint8_t *data, size_t data_len,
                             uint8_t *pixels, int32_t width, int32_t height,
                             char *error, size_t error_cap)
{
   size_t  i = 0;
   int64_t x = 0;
   int64_t y = (int64_t) height - 1;

   while (i < data_len)
   {
      uint8_t c = data[i++];

      if (c == 0x80)
      {
         x = 0;
         y--;
      }
      else if (c & 0x80)
         x += c & 0x7F;
      else
      {
         uint8_t n;

         if ((size_t) c > data_len - i)
         {
            set_error(error, error_cap,
                      "dc6: copy of %u bytes runs past the frame data", c);
            return 0;
         }
         for (n = 0; n < c; n++, x++)
         {
            uint8_t v = data[i++];

            if (x >= 0 && x < width && y >= 0 && y < height)
               pixels[(size_t) y * width + (size_t) x] = v;
         }
      }
   }
   return 1;
}

int dc6_decode_indices(const void *bytes, size_t len, int direction,
                       DS1EDIT_INDEX_FRAME **frames, size_t *count,
                       char *error, size_t error_cap)
{
   const uint8_t       *buf = (const uint8_t *) bytes;
   int32_t              ver, unk1, unk2, dirs, fpd;
   int32_t              d_first, d_last, d, f;
   size_t               table_end, n_out, k = 0;
   DS1EDIT_INDEX_FRAME *out;

   if (frames != NULL) *frames = NULL;
   if (count  != NULL) *count  = 0;
   if (error != NULL && error_cap > 0) error[0] = 0;

   if (buf == NULL || frames == NULL || count == NULL)
   {
      set_error(error, error_cap, "dc6: invalid arguments");
      return 0;
   }
   if (len < DC6_HEADER_SIZE)
   {
      set_error(error, error_cap, "dc6: %lu bytes is shorter than the header",
                (unsigned long) len);
      return 0;
   }

   ver  = read_i32(buf + 0);
   unk1 = read_i32(buf + 4);
   unk2 = read_i32(buf + 8);
   dirs = read_i32(buf + 16);
   fpd  = read_i32(buf + 20);
   if (ver != 6 || unk1 != 1 || unk2 != 0)
   {
      set_error(error, error_cap, "dc6: unsupported header %ld/%ld/%ld",
                (long) ver, (long) unk1, (long) unk2);
      return 0;
   }
   if (dirs <= 0 || fpd <= 0 || dirs > 64 || fpd > 65536)
   {
      set_error(error, error_cap, "dc6: bad counts dirs=%ld fpd=%ld",
                (long) dirs, (long) fpd);
      return 0;
   }
   table_end = DC6_HEADER_SIZE + 4u * (size_t) dirs * (size_t) fpd;
   if (table_end > len)
   {
      set_error(error, error_cap, "dc6: frame table runs past the file");
      return 0;
   }

   if (direction == DS1EDIT_ALL_DIRECTIONS)
   {
      d_first = 0;
      d_last  = dirs - 1;
   }
   else if (direction >= 0 && direction < dirs)
      d_first = d_last = direction;
   else
   {
      set_error(error, error_cap, "dc6: direction %d not in [0, %ld)",
                direction, (long) dirs);
      return 0;
   }

   n_out = (size_t) (d_last - d_first + 1) * (size_t) fpd;
   out = (DS1EDIT_INDEX_FRAME *) calloc(n_out, sizeof(*out));
   if (out == NULL)
   {
      set_error(error, error_cap, "dc6: out of memory");
      return 0;
   }

   for (d = d_first; d <= d_last; d++)
   {
      for (f = 0; f < fpd; f++, k++)
      {
         DS1EDIT_INDEX_FRAME *fr = &out[k];
         int32_t  off = read_i32(buf + DC6_HEADER_SIZE
                                 + 4u * ((size_t) d * fpd + f));
         const uint8_t *fh;
         int32_t  w, h, ox, oy, flen;

         fr->frame = f;
         fr->direction = d;

         if (off < 0 || (size_t) off > len
             || len - (size_t) off < DC6_FRAME_HEADER_SIZE)
         {
            set_error(error, error_cap,
                      "dc6: direction %ld frame %ld offset %ld out of range",
                      (long) d, (long) f, (long) off);
            goto fail;
         }
         fh   = buf + off;
         w    = read_i32(fh + 4);
         h    = read_i32(fh + 8);
         ox   = read_i32(fh + 12);
         oy   = read_i32(fh + 16);
         flen = read_i32(fh + 28);
         if (w < 0 || h < 0 || w > DC6_MAX_SIDE || h > DC6_MAX_SIDE)
         {
            set_error(error, error_cap,
                      "dc6: direction %ld frame %ld has bad size %ldx%ld",
                      (long) d, (long) f, (long) w, (long) h);
            goto fail;
         }
         if (flen < 0
             || (size_t) flen > len - (size_t) off - DC6_FRAME_HEADER_SIZE)
         {
            set_error(error, error_cap,
                      "dc6: direction %ld frame %ld data length %ld runs past "
                      "the file", (long) d, (long) f, (long) flen);
            goto fail;
         }

         fr->width      = w;
         fr->height     = h;
         fr->stride     = w;
         fr->origin_x   = ox;
         fr->origin_y   = (int32_t) ((int64_t) oy - h + 1);
         fr->pixels_len = (size_t) w * (size_t) h;
         if (fr->pixels_len > 0)
         {
            fr->pixels = (uint8_t *) calloc(fr->pixels_len, 1);
            if (fr->pixels == NULL)
            {
               set_error(error, error_cap, "dc6: out of memory");
               goto fail;
            }
         }
         if (!dc6_decode_stream(fh + DC6_FRAME_HEADER_SIZE, (size_t) flen,
                                fr->pixels, w, h, error, error_cap))
            goto fail;
      }
   }

   *frames = out;
   *count  = n_out;
   return 1;

fail:
   ds1edit_index_frames_free(out, n_out);
   return 0;
}
