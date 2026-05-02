#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <zlib.h>

#include "core/apng_writer.h"

struct APNG_WRITER_S
{
   FILE     *fp;
   int       width;
   int       height;
   int       num_frames;
   int       frames_written;
   uint32_t  sequence_number;
   int       had_error;
};

static const unsigned char PNG_SIG[8] = {
   0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
};

static void put_u32_be(unsigned char *dst, uint32_t v)
{
   dst[0] = (unsigned char) ((v >> 24) & 0xFF);
   dst[1] = (unsigned char) ((v >> 16) & 0xFF);
   dst[2] = (unsigned char) ((v >>  8) & 0xFF);
   dst[3] = (unsigned char) ( v        & 0xFF);
}

static void put_u16_be(unsigned char *dst, uint16_t v)
{
   dst[0] = (unsigned char) ((v >> 8) & 0xFF);
   dst[1] = (unsigned char) ( v       & 0xFF);
}

// Write a PNG chunk: length (4 BE) | type (4) | data | crc32 (4 BE).
// CRC is computed over (type || data) per the PNG spec.
static int write_chunk(FILE *fp, const char type[4],
                       const unsigned char *data, uint32_t len)
{
   unsigned char hdr[8];
   unsigned char crc_buf[4];
   uLong crc;

   put_u32_be(hdr, len);
   memcpy(hdr + 4, type, 4);
   if (fwrite(hdr, 1, 8, fp) != 8)
      return 0;

   if (data != NULL && len > 0)
   {
      if (fwrite(data, 1, len, fp) != len)
         return 0;
   }

   crc = crc32(0L, Z_NULL, 0);
   crc = crc32(crc, (const Bytef *) (type), 4);
   if (data != NULL && len > 0)
      crc = crc32(crc, (const Bytef *) data, len);
   put_u32_be(crc_buf, (uint32_t) crc);

   if (fwrite(crc_buf, 1, 4, fp) != 4)
      return 0;
   return 1;
}

// Build the per-row filtered-image stream the PNG IDAT/fdAT format
// expects: one filter-type byte (0 = None) prepended to each row,
// followed by the row's RGBA bytes.
//
// On success, *out_len is set to (1 + width*4) * height and the caller
// owns the returned buffer (free()).
static unsigned char *build_filtered_rows(const unsigned char *pixels,
                                          int width, int height,
                                          size_t *out_len)
{
   size_t row_bytes = (size_t) width * 4;
   size_t total = (1 + row_bytes) * (size_t) height;
   unsigned char *out;
   int y;

   if (out_len != NULL)
      *out_len = 0;

   out = (unsigned char *) malloc(total);
   if (out == NULL)
      return NULL;

   for (y = 0; y < height; y++)
   {
      out[y * (1 + row_bytes)] = 0;  /* filter type 0 = None */
      memcpy(out + y * (1 + row_bytes) + 1,
             pixels + y * row_bytes,
             row_bytes);
   }

   if (out_len != NULL)
      *out_len = total;
   return out;
}

// zlib-deflate a buffer. Caller owns the returned buffer.
static unsigned char *deflate_buffer(const unsigned char *src, size_t src_len,
                                     size_t *out_len)
{
   uLongf dest_len;
   unsigned char *dest;

   if (out_len != NULL)
      *out_len = 0;
   if (src == NULL || src_len == 0)
      return NULL;

   dest_len = compressBound((uLong) src_len);
   dest = (unsigned char *) malloc(dest_len);
   if (dest == NULL)
      return NULL;

   if (compress2(dest, &dest_len, src, (uLong) src_len, Z_BEST_SPEED) != Z_OK)
   {
      free(dest);
      return NULL;
   }

   if (out_len != NULL)
      *out_len = (size_t) dest_len;
   return dest;
}

APNG_WRITER_S *apng_writer_open(const char *path, int width, int height,
                                int num_frames, int num_plays)
{
   APNG_WRITER_S *w;
   unsigned char ihdr[13];
   unsigned char actl[8];

   if (path == NULL || width <= 0 || height <= 0 || num_frames < 1)
      return NULL;

   w = (APNG_WRITER_S *) calloc(1, sizeof(*w));
   if (w == NULL)
      return NULL;

   w->fp = fopen(path, "wb");
   if (w->fp == NULL)
   {
      free(w);
      return NULL;
   }

   w->width = width;
   w->height = height;
   w->num_frames = num_frames;
   w->frames_written = 0;
   w->sequence_number = 0;
   w->had_error = 0;

   /* PNG signature */
   if (fwrite(PNG_SIG, 1, 8, w->fp) != 8)
   {
      w->had_error = 1;
      return w;
   }

   /* IHDR: width(4) height(4) bitdepth(1) colortype(1) compression(1)
    * filter(1) interlace(1). 8-bit RGBA = colortype 6, bitdepth 8. */
   put_u32_be(ihdr + 0, (uint32_t) width);
   put_u32_be(ihdr + 4, (uint32_t) height);
   ihdr[8]  = 8;  /* bit depth */
   ihdr[9]  = 6;  /* color type: RGBA */
   ihdr[10] = 0;  /* compression: deflate */
   ihdr[11] = 0;  /* filter method: 0 */
   ihdr[12] = 0;  /* interlace: none */
   if (!write_chunk(w->fp, "IHDR", ihdr, 13))
      w->had_error = 1;

   /* acTL: num_frames(4) num_plays(4). MUST appear before IDAT. */
   put_u32_be(actl + 0, (uint32_t) num_frames);
   put_u32_be(actl + 4, (uint32_t) num_plays);
   if (!write_chunk(w->fp, "acTL", actl, 8))
      w->had_error = 1;

   return w;
}

int apng_writer_write_frame(APNG_WRITER_S *w,
                            const unsigned char *pixels,
                            int delay_num, int delay_den)
{
   unsigned char fctl[26];
   unsigned char *raw = NULL;
   size_t raw_len = 0;
   unsigned char *comp = NULL;
   size_t comp_len = 0;
   uint32_t fctl_seq;

   if (w == NULL || w->fp == NULL || pixels == NULL || w->had_error)
      return 0;
   if (w->frames_written >= w->num_frames)
      return 0;
   if (delay_num < 0)
      delay_num = 0;
   if (delay_den < 0)
      delay_den = 0;

   /* fcTL: sequence_number(4) width(4) height(4) x_offset(4) y_offset(4)
    * delay_num(2) delay_den(2) dispose_op(1) blend_op(1) */
   fctl_seq = w->sequence_number++;
   put_u32_be(fctl +  0, fctl_seq);
   put_u32_be(fctl +  4, (uint32_t) w->width);
   put_u32_be(fctl +  8, (uint32_t) w->height);
   put_u32_be(fctl + 12, 0);                  /* x_offset */
   put_u32_be(fctl + 16, 0);                  /* y_offset */
   put_u16_be(fctl + 20, (uint16_t) delay_num);
   put_u16_be(fctl + 22, (uint16_t) delay_den);
   fctl[24] = 0;  /* dispose_op = APNG_DISPOSE_OP_NONE */
   fctl[25] = 0;  /* blend_op   = APNG_BLEND_OP_SOURCE (replace) */

   if (!write_chunk(w->fp, "fcTL", fctl, 26))
   {
      w->had_error = 1;
      return 0;
   }

   raw = build_filtered_rows(pixels, w->width, w->height, &raw_len);
   if (raw == NULL)
   {
      w->had_error = 1;
      return 0;
   }

   comp = deflate_buffer(raw, raw_len, &comp_len);
   free(raw);
   if (comp == NULL)
   {
      w->had_error = 1;
      return 0;
   }

   if (w->frames_written == 0)
   {
      /* First frame goes into IDAT (also serves as the default still
       * image for non-APNG viewers). IDAT does NOT carry a sequence
       * number. */
      if (!write_chunk(w->fp, "IDAT", comp, (uint32_t) comp_len))
      {
         free(comp);
         w->had_error = 1;
         return 0;
      }
   }
   else
   {
      /* Subsequent frames are fdAT: sequence_number(4) || image_data */
      uint32_t fdat_seq = w->sequence_number++;
      size_t fdat_len = 4 + comp_len;
      unsigned char *fdat = (unsigned char *) malloc(fdat_len);
      if (fdat == NULL)
      {
         free(comp);
         w->had_error = 1;
         return 0;
      }
      put_u32_be(fdat, fdat_seq);
      memcpy(fdat + 4, comp, comp_len);
      if (!write_chunk(w->fp, "fdAT", fdat, (uint32_t) fdat_len))
      {
         free(fdat);
         free(comp);
         w->had_error = 1;
         return 0;
      }
      free(fdat);
   }

   free(comp);
   w->frames_written++;
   return 1;
}

int apng_writer_close(APNG_WRITER_S *w)
{
   int ok = 1;

   if (w == NULL)
      return 0;

   if (w->fp != NULL)
   {
      if (!w->had_error)
      {
         if (!write_chunk(w->fp, "IEND", NULL, 0))
            ok = 0;
      }
      else
      {
         ok = 0;
      }

      if (fclose(w->fp) != 0)
         ok = 0;
   }
   else
   {
      ok = 0;
   }

   if (w->frames_written != w->num_frames)
      ok = 0;

   free(w);
   return ok;
}
