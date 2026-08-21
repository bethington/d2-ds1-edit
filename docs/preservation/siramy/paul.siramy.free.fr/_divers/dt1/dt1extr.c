#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mem.h>
#include <dir.h>

#include <allegro.h>

#define WORD  short int
#define UBYTE unsigned char

#ifndef FALSE
   #define FALSE 0
#endif
#ifndef TRUE
   #define TRUE -1
#endif

#define set_floor(x)     ( (x) |= 2)
#define set_wall(x)      ( (x) |= 4)
#define set_static(x)    ( (x) |= 8)
#define set_animated(x)  ( (x) |= 16)
#define set_wall_up(x)   ( (x) |= 32)
#define set_wall_down(x) ( (x) |= 64)

#define is_floor(x)      ( (x) & 2)
#define is_wall(x)       ( (x) & 4)
#define is_static(x)     ( (x) & 8)
#define is_animated(x)   ( (x) & 16)
#define is_wall_up(x)    ( (x) & 32)
#define is_wall_down(x)  ( (x) & 64)


// reducing size of executable, for the allegro 3.9.34 (wip) lib

BEGIN_COLOR_DEPTH_LIST
   COLOR_DEPTH_8
END_COLOR_DEPTH_LIST

BEGIN_MIDI_DRIVER_LIST
   MIDI_DRIVER_SB_OUT
END_MIDI_DRIVER_LIST

BEGIN_DIGI_DRIVER_LIST
   DIGI_DRIVER_SB
END_DIGI_DRIVER_LIST

typedef struct
{
   WORD  xpos;
   WORD  ypos;
   WORD  unknown1;
   int   xgrid;
   int   ygrid;
   WORD  format;
   long  data_length;
   WORD  unknown2;
   long  data_offset;
   UBYTE * data;
} SUB_TILE_S;

typedef struct
{
   long       direction;
   WORD       y_delta;
   UBYTE      tile_sound;
   UBYTE      animated;
   long       ysize;
   long       xsize;
   long       zeros1;
   long       orientation;
   long       main_index;
   long       sub_index;
   long       frame;
   long       unknown;
   int        sub_tiles_flags[32];
   long       tiles_ptr;
   long       tiles_length;
   long       tiles_number;
   int        zeros2[12];
   SUB_TILE_S * tile;
   int        f;
   int        pcx_n;
   long       pcx_x;
   long       pcx_y;
} BLOCK_S;

typedef enum {TD_NORM, TD_SPLIT, TD_INDEX, TD_FLAGS} ENUM_TDRAW;

// global datas
PALETTE the_pal;
BLOCK_S * glb_block = NULL;
long    glb_block_number = 0;
#define MAX_SAVED_INFOS 1024
struct
{
   long direction;
   long orientation;
   long main_index;
   long sub_index;
   long x;
   long next_y;
} saved_infos[MAX_SAVED_INFOS];
int num_saved_infos = 0;


// ==========================================================================
// atexit() function, when the program end, to free the allocated memory
void dt1con_exit(void)
{
   BLOCK_S     * block_ptr;
   SUB_TILE_S  * tile_ptr;
   int         b, t;

   if (glb_block == NULL)
      return;
   for (b=0; b<glb_block_number; b++)
   {
      block_ptr = glb_block + b;
      if (block_ptr->tile != NULL)
      {
         for (t=0; t<block_ptr->tiles_number; t++)
         {
            tile_ptr = block_ptr->tile + t;
            if (tile_ptr->data != NULL)
               free(tile_ptr->data);
         }
         free(block_ptr->tile);
      }
   }
   free(glb_block);
}


// ==========================================================================
// load a diablo 2 palette
void load_palette(char * d2pal)
{
   FILE * in;
   int  i;
   
   in = fopen(d2pal, "rb");
   if (in == NULL)
   {
      set_gfx_mode(GFX_TEXT, 80, 25, 0, 0);
      printf("\ncan't open palette : %s\n", d2pal);
      exit(1);
   }
   for (i=0; i<256; i++)
   {
      // order is BGR, not RGB
      the_pal[i].b = fgetc(in) >> 2;
      the_pal[i].g = fgetc(in) >> 2;
      the_pal[i].r = fgetc(in) >> 2;
      /*
         >> 2 is needed because the internal range of the RGB componants
         in allegro (8 bpp mode) is from 0 to 63 (because of the mode 13h),
         not 0 to 255. Since this program only output bitmaps for the purpose
         of tests, and don't create dt1 files, it's ok.
      */
   }

   // make a grey background
   the_pal[0].r = 24;
   the_pal[0].g = 24;
   the_pal[0].b = 24;
   fclose(in);
}


// ==========================================================================
// read the dt1 main header
void read_dt1_header(FILE * in)
{
   long x1, x2, fsize;
   int  i, c, is_dt1 = TRUE;

   // file size
   fseek(in, 0, SEEK_END);
   fsize = ftell(in);
   fseek(in, 0, SEEK_SET);
   
   // signature
   fread(&x1, 1, 4, in);
   fread(&x2, 1, 4, in);
   if ((x1 != 7) || (x2 != 6))
      is_dt1 = FALSE;
      
   // zeros
   for (i=0; i<260; i++)
   {
      c = fgetc(in);
      if (c != 0)
         is_dt1 = FALSE;
   }
   
   // # of blocks
   fread(&glb_block_number, 1, 4, in);
   if (glb_block_number < 0)
   {
      fclose(in);
      printf("error : number of blocks is negative (%li)\n", glb_block_number);
      exit(1);
   }

   // start of block header, always 0x114
   fread(&x1, 1, 4, in);
   if (x1 != 0x114)
      is_dt1 = FALSE;
   if (x1 >= fsize)
   {
      fclose(in);
      printf("error : start of blocks (%li) >= size of file (%li)\n", x1, fsize);
      exit(1);
   }

   // summary
   if (is_dt1 != TRUE)
{
      printf("possibly not a dt1 file, stop\n");
      exit(1);
}

   // get mem for block headers
   glb_block = (BLOCK_S *) malloc(sizeof(BLOCK_S) * glb_block_number);
   if (glb_block == NULL)
   {
      fclose(in);
      printf("not enough memory for %li blocks header\n", glb_block_number);
      exit(1);
   }
   memset(glb_block, 0, sizeof(BLOCK_S) * glb_block_number);

   // ready to read block headers
   fseek(in, x1, SEEK_SET);
}


// ==========================================================================
// read the blocks headers
void read_blocks_headers(FILE * in)
{
   BLOCK_S * block_ptr;
   int     h, i;

   block_ptr = glb_block;
   for (h=0; h<glb_block_number; h++)
   {
//fprintf(stderr, "block header %i\n", h);
      // direction
      fread(&block_ptr->direction, 1, 4, in);

      // flags
      fread(&block_ptr->y_delta, 1, 2, in);
      fread(&block_ptr->tile_sound, 1, 1, in);
      fread(&block_ptr->animated, 1, 1, in);
      
      // ysize and xsize
      // negate the ysize : it is negative and I love when they are positive
      fread(&block_ptr->ysize, 1, 4, in);
      fread(&block_ptr->xsize, 1, 4, in);

      // zeros ?
      fread(&block_ptr->zeros1, 1, 4, in);
      
      // seems to be that
      fread(&block_ptr->orientation, 1, 4, in);
      fread(&block_ptr->main_index,  1, 4, in);
      fread(&block_ptr->sub_index,   1, 4, in);
      fread(&block_ptr->frame,       1, 4, in);

      // unknown 2 (always FF 00 FF 00 ?)
      fread(&block_ptr->unknown, 1, 4, in);

      // sub-tiles flags (space for 32 max, but 25 in floor tiles)
      for (i=0; i<32; i++)
          block_ptr->sub_tiles_flags[i] = fgetc(in);

      // pointer to sub-tiles headers
      fread(&block_ptr->tiles_ptr,     1, 4, in);

      // sub-tiles length
      fread(&block_ptr->tiles_length,  1, 4, in);

      // # of sub-tiles (32 max ?)
      fread(&block_ptr->tiles_number,  1, 4, in);

      // zeros ?
      for (i=0; i<12; i++)
          block_ptr->zeros2[i] = fgetc(in);

      // ready to read next block header
      block_ptr++;
   }
}


// ==========================================================================
// read the sub-tiles of 1 block
void read_tiles_of_block(FILE * in, long b)
{
   BLOCK_S    * block_ptr;
   SUB_TILE_S * tile_ptr;
   UBYTE      * ptr;
   long       nb_tiles, t, f_pos;
   int        i;

//fprintf(stderr, "block gfx %i\n", b);

   // get ready
   block_ptr = glb_block + b;
   nb_tiles = block_ptr->tiles_number;

   // reserve memory
   block_ptr->tile = (SUB_TILE_S *) malloc(sizeof(SUB_TILE_S) * nb_tiles);
   if (block_ptr->tile == NULL)
   {
      fclose(in);
      printf("in block %li, not enough memory for %li tiles\n", b, nb_tiles);
      exit(1);
   }
   memset(block_ptr->tile, 0, sizeof(SUB_TILE_S) * nb_tiles);

   // ready to read tiles header
   fseek(in, block_ptr->tiles_ptr, SEEK_SET);
   tile_ptr = block_ptr->tile;

   // read each of them
   for (t=0; t<nb_tiles; t++)
   {
      // xpos & ypos
      fread(&tile_ptr->xpos, 1, 2, in);
      fread(&tile_ptr->ypos, 1, 2, in);

      // unknwon 1
      fread(&tile_ptr->unknown1, 1, 2, in);

      // xgrid & ygrid
      tile_ptr->xgrid = fgetc(in);
      tile_ptr->ygrid = fgetc(in);

      // sub-tile format
      fread(&tile_ptr->format, 1, 2, in);

      // sub-tile data length
      fread(&tile_ptr->data_length, 1, 4, in);

      // unknown 2
      fread(&tile_ptr->unknown2, 1, 2, in);
      
      // sub-til data offset
      fread(&tile_ptr->data_offset, 1, 4, in);

      // ready to read next sub-tile
      tile_ptr++;
   }

   // after the sub-tiles headers, their datas
   
   // read data of each sub-tiles
   tile_ptr = block_ptr->tile;
   for (t=0; t<nb_tiles; t++)
   {
      // file pointer = start of sub-tiles header + sub-tile data offset
      f_pos = block_ptr->tiles_ptr + tile_ptr->data_offset;
      fseek(in, f_pos, SEEK_SET);

      // reserved memory for data
      tile_ptr->data = (UBYTE *) malloc(tile_ptr->data_length);
      if (tile_ptr->data == NULL)
      {
         fclose(in);
         printf("in block %li, sub-tile %li, not enough memory for %li bytes\n",
            b, t, tile_ptr->data_length);
         exit(1);
      }

      // read data
      ptr = tile_ptr->data;
      for (i=0; i<tile_ptr->data_length; i++)
      {
         * ptr = fgetc(in);
         ptr++;
      }

      // next sub-tile data
      tile_ptr++;
   }
}


// ==========================================================================
// draw a 3d-isometric sub-tile
void draw_sub_tile_isometric (BITMAP * dst, int x0, int y0, const UBYTE * data, int length)
{
   UBYTE * ptr = data;
   int   x, y=0, n,
         xjump[15] = {14, 12, 10, 8, 6, 4, 2, 0, 2, 4, 6, 8, 10, 12, 14},
         nbpix[15] = {4, 8, 12, 16, 20, 24, 28, 32, 28, 24, 20, 16, 12, 8, 4};

   // 3d-isometric subtile is 256 bytes, no more, no less
   if (length != 256)
      return;

   // draw
   while (length > 0)
   {
      x = xjump[y];
      n = nbpix[y];
      length -= n;
      while (n)
      {
         putpixel(dst, x0+x, y0+y, * ptr);
         ptr++;
         x++;
         n--;
      }
      y++;
   }
}


// ==========================================================================
// draw a normal sub-tile (can be transparent, so there are "jump" coded)
void draw_sub_tile_normal (BITMAP * dst, int x0, int y0, const UBYTE * data, int length)
{
   UBYTE * ptr = data, b1, b2;
   int   x=0, y=0;

   // draw
   while (length > 0)
   {
      b1 = * ptr;
      b2 = * (ptr + 1);
      ptr += 2;
      length -= 2;
      if (b1 || b2)
      {
         x += b1;
         length -= b2;
         while (b2)
         {
            putpixel(dst, x0+x, y0+y, * ptr);
            ptr++;
            x++;
            b2--;
         }
      }
      else
      {
         x = 0;
         y++;
      }
   }
}


// ==========================================================================
void draw_block(BITMAP * dst, int x0, int y0, int b)
{
   BLOCK_S    * block_ptr;
   SUB_TILE_S * tile_ptr;
   UBYTE      * ptr;
   int        t, x, y, length;

   block_ptr = glb_block + b;
   tile_ptr  = block_ptr->tile;

   for(t=0; t<block_ptr->tiles_number; t++)
   {
      length = tile_ptr->data_length;
      ptr    = tile_ptr->data;
      x      = tile_ptr->xpos;
      y      = tile_ptr->ypos;
      if (tile_ptr->format == 0x0001)
         draw_sub_tile_isometric(dst, x0+x, y0+y, ptr, length);
      else
         draw_sub_tile_normal(dst, x0+x, y0+y, ptr, length);
      tile_ptr++;
   }
}


// ==========================================================================
int main(int argc, char * argv[])
{
   BLOCK_S * block_ptr=NULL;
   BITMAP  * pcxbmp = NULL;
   FILE    * in, * out;
   char    strtmp[256], pcxfilename[256], * ptr;
   long    b, t, w, h;
   enum    {PT_FA, PT_FS, PT_WU, PT_WD, PT_MAX};
   int     pcx_idx, i;
   struct
   {
      char * name;
      long w;
      long h;
      int  col;
   } pcx[PT_MAX];
   
   // header & syntaxe
   printf("DT1EXTR v 0.97 beta, by Paul Siramy, Freeware\n"
          "=============================================\n");
   if (argc == 1)
   {
      printf("syntaxe : dt1extr <file.dt1> [-pal <file.dat>]\n");
      exit(0);
   }

   // palette & atexit()
   if (strcmp(argv[2], "-pal") == 0)
      load_palette(argv[3]);
   else
   {
      printf("opening default palette\n   ");
      load_palette("d2pal\\act1.dat");
   }

   // dt1 file
   in = fopen(argv[1], "rb");
   if (in == NULL)
   {
      printf("can't open %s\n", argv[1]);
      exit(1);
   }
   else
      printf("opening %s\n", argv[1]);
   read_dt1_header(in);
   read_blocks_headers(in);
   
   for (t=0; t<glb_block_number; t++)
      read_tiles_of_block(in, t);
   fclose(in);

   allegro_init();
   set_color_depth(8);
   atexit(dt1con_exit);
   
   memset(pcx, 0, sizeof(pcx));
   pcx[PT_FA].name = "fa"; // floor animated
   pcx[PT_FS].name = "fs"; // floor static
   pcx[PT_WU].name = "wu"; // walls up
   pcx[PT_WD].name = "wd"; // walls down
   ptr = argv[1];
   i=strlen(ptr) - 1;
   while ((i >= 0) && ( * (ptr+i) != '\\') && ( * (ptr+i) != ':'))
      i--;
   if (i != 0)
      i++;
   strcpy(strtmp, ptr+i);
   strtmp[strlen(strtmp) - 4] = 0;
   strcat(strtmp, ".ini");
   out = fopen(strtmp, "wt");
   printf("saving %s\n", strtmp);
   strtmp[strlen(strtmp) - 4] = 0;
   
   // start of ini
   fprintf(out, "nb_blocks   = %li\n\n", glb_block_number);
   
   // check the size of each pcx
   for (b=0; b<glb_block_number; b++)
   {
      block_ptr = glb_block + b;
      // set some flags
      if ( ((block_ptr->direction == 3) && (block_ptr->orientation == 0)) ||
           ((block_ptr->direction == 5) && (block_ptr->orientation == 15)) )
      {
         // floor
         set_floor(block_ptr->f);
         if (block_ptr->animated)
         {
            set_animated(block_ptr->f);
            
         }
         else
            set_static(block_ptr->f);
      }
      else
      {
         // wall
         set_wall(block_ptr->f);
         if (block_ptr->orientation & 0xF0)
            set_wall_down(block_ptr->f);
         else
            set_wall_up(block_ptr->f);
      }

      // boxsize
      h = block_ptr->ysize;
      if (h < 0)
         h = - h;
      if (is_wall(block_ptr->f))
         h += 32; // 1 worldstone down wall need more border
      w = block_ptr->xsize;

      // place the block in the pcx
      pcx_idx = -1;
      if (is_floor(block_ptr->f))
      {
         h = 80;
         if (is_static(block_ptr->f))
            pcx_idx = PT_FS;
         else if (is_animated(block_ptr->f))
            pcx_idx = PT_FA;
      }
      else if (is_wall(block_ptr->f))
      {
         if (is_wall_up(block_ptr->f))
            pcx_idx = PT_WU;
         else if (is_wall_down(block_ptr->f))
            pcx_idx = PT_WD;
      }
      if (pcx_idx != -1)
      {
         block_ptr->pcx_n = pcx_idx;
         block_ptr->pcx_x = pcx[pcx_idx].w;
         if ( (pcx_idx == PT_FA) || (pcx_idx == PT_FS) )
         {
            block_ptr->pcx_y = 80 * (block_ptr->pcx_x / 800);
            block_ptr->pcx_x = block_ptr->pcx_x % 800;
         }
         pcx[pcx_idx].w += 160;
         if (pcx[pcx_idx].h < h)
             pcx[pcx_idx].h = h;
         pcx[pcx_idx].col++;
         // ini
         fprintf(out, "block       = %li\n",   b);
         fprintf(out, "pcx_file    = %s\n",    pcx[pcx_idx].name);
         fprintf(out, "pcx_x       = %i\n",    block_ptr->pcx_x);
         fprintf(out, "pcx_y       = %i\n",    block_ptr->pcx_y);
         fprintf(out, "direction   = %08lX\n", block_ptr->direction);
//         fprintf(out, "flags       = %08lX\n", block_ptr->flags);
         fprintf(out, "y_delta     = %i\n",    block_ptr->y_delta);
         fprintf(out, "tile_sound  = %i\n",    block_ptr->tile_sound);
         fprintf(out, "animated    = %i\n",    block_ptr->animated);
         fprintf(out, "orientation = %08lX\n", block_ptr->orientation);
         fprintf(out, "main_index  = %08lX\n", block_ptr->main_index);
         fprintf(out, "sub_index   = %08lX\n", block_ptr->sub_index);
         fprintf(out, "frame       = %08lX\n", block_ptr->frame);
         fprintf(out, "unknown     = %08lX\n", block_ptr->unknown);

         fprintf(out, "floor_flag1 = %02X %02X %02X %02X %02X\n",
            block_ptr->sub_tiles_flags[20],
            block_ptr->sub_tiles_flags[21],
            block_ptr->sub_tiles_flags[22],
            block_ptr->sub_tiles_flags[23],
            block_ptr->sub_tiles_flags[24]
         );
         fprintf(out, "floor_flag2 = %02X %02X %02X %02X %02X\n",
         block_ptr->sub_tiles_flags[15],
         block_ptr->sub_tiles_flags[16],
         block_ptr->sub_tiles_flags[17],
         block_ptr->sub_tiles_flags[18],
         block_ptr->sub_tiles_flags[19]
         );
         fprintf(out, "floor_flag3 = %02X %02X %02X %02X %02X\n",
            block_ptr->sub_tiles_flags[10],
            block_ptr->sub_tiles_flags[11],
            block_ptr->sub_tiles_flags[12],
            block_ptr->sub_tiles_flags[13],
            block_ptr->sub_tiles_flags[14]
         );
         fprintf(out, "floor_flag4 = %02X %02X %02X %02X %02X\n",
            block_ptr->sub_tiles_flags[5],
            block_ptr->sub_tiles_flags[6],
            block_ptr->sub_tiles_flags[7],
            block_ptr->sub_tiles_flags[8],
            block_ptr->sub_tiles_flags[9]
         );
         fprintf(out, "floor_flag5 = %02X %02X %02X %02X %02X\n",
            block_ptr->sub_tiles_flags[0],
            block_ptr->sub_tiles_flags[1],
            block_ptr->sub_tiles_flags[2],
            block_ptr->sub_tiles_flags[3],
            block_ptr->sub_tiles_flags[4]
         );
         fprintf(out, "\n");
      }
   }
   for (i=0; i<PT_MAX; i++)
   {
      if (pcx[i].w > 0)
      {
         if ((i == PT_FA) || (i == PT_FS))
         {
            if (pcx[i].w > 800)
            {
               pcx[i].h = 80 * (1 + pcx[i].w / 800);
               pcx[i].w = 800;
            }
         }
         pcxbmp = create_bitmap(pcx[i].w, pcx[i].h);
         clear(pcxbmp);
         if (pcxbmp != NULL)
         {
            for (b=0; b<glb_block_number; b++)
            {
               block_ptr = glb_block + b;
               if (block_ptr->pcx_n == i)
               {
                  if (is_floor(block_ptr->f))
                     draw_block(pcxbmp, block_ptr->pcx_x, block_ptr->pcx_y, b);
                  else
                  {
                     if (is_wall_up(block_ptr->f))
                        draw_block(pcxbmp, block_ptr->pcx_x, pcx[i].h, b);
                     else
                        draw_block(pcxbmp, block_ptr->pcx_x, 96, b);
                  }
               }
            }
         }
         sprintf(pcxfilename, "%s-%s.pcx", strtmp, pcx[i].name);
         printf("saving %s\n", pcxfilename);
         save_pcx(pcxfilename, pcxbmp, & the_pal);
         destroy_bitmap(pcxbmp);
      }
   }
   fclose(out);

   // end
   printf("done\n");
   return 0;
}

