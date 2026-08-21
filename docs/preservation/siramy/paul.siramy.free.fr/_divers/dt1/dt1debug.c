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
   UBYTE      unknown_a;
   UBYTE      unknown_b;
   UBYTE      unknown_c;
   UBYTE      unknown_d;
   int        sub_tiles_flags[32];
   long       tiles_ptr;
   long       tiles_length;
   long       tiles_number;
   int        zeros2[12];
   SUB_TILE_S * tile;
} BLOCK_S;

typedef enum {TD_NORM, TD_SPLIT, TD_INDEX, TD_FLAGS} ENUM_TDRAW;

// global datas
PALETTE the_pal;
BLOCK_S * glb_block = NULL;
long    glb_block_number = 0;


// ==========================================================================
// atexit() function, when the program end, to free the allocated memory
void dt1debug_exit(void)
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

      // y_delta
      fread(&block_ptr->y_delta, 1, 2, in);
      
      // tile sound
      fread(&block_ptr->tile_sound, 1, 1, in);
      
      // animated
      fread(&block_ptr->animated, 1, 1, in);
      
      // ysize and xsize
      // negate the ysize : it is negative and I love when they are positive
      fread(&block_ptr->ysize, 1, 4, in);
      fread(&block_ptr->xsize, 1, 4, in);

      // zeros ?
      fread(&block_ptr->zeros1, 1, 4, in);
      
      // num[0] to num[3] (still unknown)
      fread(&block_ptr->orientation, 1, 4, in);
      fread(&block_ptr->main_index,  1, 4, in);
      fread(&block_ptr->sub_index,   1, 4, in);
      fread(&block_ptr->frame,       1, 4, in);

      // 4 unknown datas
      fread(&block_ptr->unknown_a, 1, 1, in);
      fread(&block_ptr->unknown_b, 1, 1, in);
      fread(&block_ptr->unknown_c, 1, 1, in);
      fread(&block_ptr->unknown_d, 1, 1, in);

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
void draw_block(BITMAP * dst, int x0, int y0, int b, ENUM_TDRAW methode)
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
      y     -= block_ptr->y_delta;
      if (tile_ptr->format == 0x0001)
         draw_sub_tile_isometric(dst, x0+x, y0+y, ptr, length);
      else
         draw_sub_tile_normal(dst, x0+x, y0+y, ptr, length);
      tile_ptr++;
   }
}


// ==========================================================================
void new_gfx_mode(int x, int y)
{
   if (set_gfx_mode(GFX_AUTODETECT, x, y, 0, 0) < 0)
   {
      set_gfx_mode(GFX_TEXT, 80, 25, 0, 0);
      printf("can't initialise the gfx mode %i %i 8 bpp\n", x, y);
      printf("%s\n", allegro_error);
      exit(1);
   }
}

// ==========================================================================
int main(int argc, char * argv[])
{
   BLOCK_S    * block_ptr=NULL;
   SUB_TILE_S * tile_ptr=NULL;
   FILE       * in;
   long       t, b;
   char       tmp[80];
   int        k, i, done = FALSE, scr=0, plotx=150, ploty=400,
              screen_x = 640, screen_y = 480, gridvalue=2, layer=0, box=1;
   BITMAP     * savebmp, * tmpbmp;
   char       d2palname[5][80] = {
                 {"d2pal\\act1.dat"},
                 {"d2pal\\act2.dat"},
                 {"d2pal\\act3.dat"},
                 {"d2pal\\act4.dat"},
                 {"d2pal\\act5.dat"}
              };
   char       * pal_name = NULL;;
   
   // header & syntaxe
   printf("DT1DEBUG v 0.97, by Paul Siramy, Freeware\n"
          "=========================================\n");
   if (argc == 1)
   {
      printf("syntaxe : dt1debug <file.dt1> [-pal <file.dat>]\n");
      exit(0);
   }

   // palette
   if (strcmp(argv[2], "-pal") == 0)
   {
      load_palette(argv[3]);
      pal_name = argv[3];
   }
   else
   {
      printf("opening default palette\n   ");
      load_palette("d2pal\\act1.dat");
      pal_name = d2palname[0];
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

   // inits
   allegro_init();
   set_color_depth(8);
   atexit(dt1debug_exit);
   install_keyboard();
   new_gfx_mode(screen_x, screen_y);
   set_palette(the_pal);
   b = 0;
   tmpbmp = create_bitmap(screen_x, screen_y);

   // main loop
   while ( ! done)
   {
      block_ptr = glb_block + b;
      clear(tmpbmp);

      // ploty adjustment
      if (block_ptr->orientation & 0xF0)
         ploty = 125; // walls bellow floor
      else if ((block_ptr->direction == 3) && (block_ptr->orientation == 0))
         ploty = (screen_y / 2); // only floor
      else if ((block_ptr->direction == 5) && (block_ptr->orientation == 15))
         ploty = screen_y - 15 - 80; // only roof
      else
         ploty = screen_y - 15; // walls to the up


      // grid on top of tile
      if (layer == 1)
         draw_block(tmpbmp, plotx, ploty, b, TD_NORM);

         
      // grid : adjustement
      if ((block_ptr->direction == 3) && (block_ptr->orientation == 0))
         ploty += 80; // only floor
      else if ((block_ptr->direction == 5) && (block_ptr->orientation == 15))
         ploty += 80; // only roof


      // x & y boxsize grid ("box" of blocks of 32*32 pixels)
      if (box)
      {
         ploty -= block_ptr->y_delta;
         if (block_ptr->orientation & 0xF0)
         {
            for (i=0; i<= - block_ptr->ysize; i+=32)
               hline(tmpbmp, plotx, ploty+i-80, plotx+block_ptr->xsize, 99);
            for (i=0; i<=block_ptr->xsize; i+=32)
               vline(tmpbmp, plotx+i, ploty-80, ploty-block_ptr->ysize-80, 99);
         }
         else
         {
            for (i=0; i >= block_ptr->ysize; i-=32)
               hline(tmpbmp, plotx, ploty+i, plotx+block_ptr->xsize, 99);
            for (i=0; i<=block_ptr->xsize; i+=32)
               vline(tmpbmp, plotx+i, ploty, ploty+block_ptr->ysize, 99);
         }
         ploty += block_ptr->y_delta;
               
      }

      if (gridvalue>0)
      {
         line(tmpbmp, plotx-1,  ploty-41, plotx+79,  ploty-81, 132);
         line(tmpbmp, plotx-1,  ploty-41, plotx+79,  ploty-1, 132);
         line(tmpbmp, plotx+80, ploty-81, plotx+161, ploty-41, 132);
         line(tmpbmp, plotx+80, ploty-1,  plotx+161, ploty-41, 132);
         putpixel(tmpbmp, plotx+161, ploty-41, 0);
      }

      // grid 2
      if (gridvalue>1)
      {
         line(tmpbmp, plotx-1+16, ploty-49, plotx+16+80, ploty-49+40, 133);
         line(tmpbmp, plotx-1+32, ploty-57, plotx+32+80, ploty-57+40, 133);
         line(tmpbmp, plotx-1+48, ploty-65, plotx+48+80, ploty-65+40, 133);
         line(tmpbmp, plotx-1+64, ploty-73, plotx+64+80, ploty-73+40, 133);

         line(tmpbmp, plotx-1+16, ploty-33, plotx+16+80, ploty-33-40, 133);
         line(tmpbmp, plotx-1+32, ploty-25, plotx+32+80, ploty-25-40, 133);
         line(tmpbmp, plotx-1+48, ploty-17, plotx+48+80, ploty-17-40, 133);
         line(tmpbmp, plotx-1+64, ploty- 9, plotx+64+80, ploty- 9-40, 133);
      }

      // invert the grid adjustement
      if ((block_ptr->direction == 3) && (block_ptr->orientation == 0))
         ploty -= 80;
      else if ((block_ptr->direction == 5) && (block_ptr->orientation == 15))
         ploty -= 80;
         
      // grid below tiles
      if (layer == 0)
         draw_block(tmpbmp, plotx, ploty, b, TD_NORM);

      // debug info
      sprintf(tmp, "file : %s", argv[1]);
      textout(tmpbmp, font, tmp, 10, 10, 255);

      sprintf(tmp, "block %li on %li", b, glb_block_number-1);
      textout(tmpbmp, font, tmp, 10, 20, 255);
      
      sprintf(tmp, "palette : % ", pal_name);
      textout(tmpbmp, font, tmp, 10, 30, 255);
      
      sprintf(tmp, "direction : %08lX", block_ptr->direction);
      textout(tmpbmp, font, tmp, 324, 10, 255);
      
      sprintf(tmp, "roof Y    : %04lX ( = %i)", block_ptr->y_delta, block_ptr->y_delta);
      textout(tmpbmp, font, tmp, 324, 20, 255);
      
      sprintf(tmp, "blk sound : %02X ( = %i)", block_ptr->tile_sound, block_ptr->tile_sound);
      textout(tmpbmp, font, tmp, 324, 30, 255);
      
      sprintf(tmp, "animated  : %02X", block_ptr->animated);
      textout(tmpbmp, font, tmp, 324, 40, 255);
      
      sprintf(tmp, "y boxsize : %08lX ( = %li)", block_ptr->ysize, block_ptr->ysize);
      textout(tmpbmp, font, tmp, 324, 50, 255);
      
      sprintf(tmp, "x boxsize : %08lX ( = %li)", block_ptr->xsize, block_ptr->xsize);
      textout(tmpbmp, font, tmp, 324, 60, 255);
      
      sprintf(tmp, "zeros     : %08lX", block_ptr->zeros1);
      textout(tmpbmp, font, tmp, 324, 70, 255);
      
      sprintf(tmp, "   orientation  : %08lX ( = %li)", block_ptr->orientation, block_ptr->orientation);
      textout(tmpbmp, font, tmp, 324, 80, 255);
      
      sprintf(tmp, "   main index ? : %08lX ( = %li)", block_ptr->main_index, block_ptr->main_index);
      textout(tmpbmp, font, tmp, 324, 90, 255);
      
      sprintf(tmp, "   sub-index  ? : %08lX ( = %li)", block_ptr->sub_index, block_ptr->sub_index);
      textout(tmpbmp, font, tmp, 324, 100, 255);
      
      sprintf(tmp, "   frame number : %08lX ( = %li)", block_ptr->frame, block_ptr->frame);
      textout(tmpbmp, font, tmp, 324, 110, 255);
      
      sprintf(tmp, "unknown a : %02X ( = %i)", block_ptr->unknown_a, block_ptr->unknown_a);
      textout(tmpbmp, font, tmp, 324, 120, 255);

      sprintf(tmp, "unknown b : %02X ( = %i)", block_ptr->unknown_b, block_ptr->unknown_b);
      textout(tmpbmp, font, tmp, 324, 130, 255);

      sprintf(tmp, "unknown c : %02X ( = %i)", block_ptr->unknown_c, block_ptr->unknown_c);
      textout(tmpbmp, font, tmp, 324, 140, 255);

      sprintf(tmp, "unknown d : %02X ( = %i)", block_ptr->unknown_d, block_ptr->unknown_d);
      textout(tmpbmp, font, tmp, 324, 150, 255);

      sprintf(tmp, "#subtiles : %li", block_ptr->tiles_number);
      textout(tmpbmp, font, tmp, 324, 160, 255);


      sprintf(tmp, "some sub-tiles data :");
      textout(tmpbmp, font, tmp, 0, 50, 255);

      sprintf(tmp, " # : xpos / ypos /");
      textout(tmpbmp, font, tmp, 0, 60, 255);
      
      sprintf(tmp, "    xgrid / ygrid");
      textout(tmpbmp, font, tmp, 0, 70, 255);
      
      for(i=0; i<block_ptr->tiles_number; i++)
      {
         tile_ptr = block_ptr->tile + i;
         sprintf(tmp, "%2i : %3i %3i", i, tile_ptr->xpos, tile_ptr->ypos);
         textout(tmpbmp, font, tmp, 0, 90 + (8 * i), 255);
         sprintf(tmp, "%i %i", tile_ptr->xgrid, tile_ptr->ygrid);
         textout(tmpbmp, font, tmp, 115, 90 + (8 * i), 255);
/*
         sprintf(tmp, "%04X", tile_ptr->format);
         textout(tmpbmp, font, tmp, 155, 90 + (8 * i), 255);
*/
      }

      sprintf(tmp, "FloorFlag : %02X %02X %02X %02X %02X",
         block_ptr->sub_tiles_flags[20],
         block_ptr->sub_tiles_flags[21],
         block_ptr->sub_tiles_flags[22],
         block_ptr->sub_tiles_flags[23],
         block_ptr->sub_tiles_flags[24]);
      textout(tmpbmp, font, tmp, 324, 180, 255);

      sprintf(tmp, "            %02X %02X %02X %02X %02X",
         block_ptr->sub_tiles_flags[15],
         block_ptr->sub_tiles_flags[16],
         block_ptr->sub_tiles_flags[17],
         block_ptr->sub_tiles_flags[18],
         block_ptr->sub_tiles_flags[19]);
      textout(tmpbmp, font, tmp, 324, 190, 255);

      sprintf(tmp, "            %02X %02X %02X %02X %02X",
         block_ptr->sub_tiles_flags[10],
         block_ptr->sub_tiles_flags[11],
         block_ptr->sub_tiles_flags[12],
         block_ptr->sub_tiles_flags[13],
         block_ptr->sub_tiles_flags[14]);
      textout(tmpbmp, font, tmp, 324, 200, 255);

      sprintf(tmp, "            %02X %02X %02X %02X %02X",
         block_ptr->sub_tiles_flags[5],
         block_ptr->sub_tiles_flags[6],
         block_ptr->sub_tiles_flags[7],
         block_ptr->sub_tiles_flags[8],
         block_ptr->sub_tiles_flags[9]);
      textout(tmpbmp, font, tmp, 324, 210, 255);

      sprintf(tmp, "            %02X %02X %02X %02X %02X",
         block_ptr->sub_tiles_flags[0],
         block_ptr->sub_tiles_flags[1],
         block_ptr->sub_tiles_flags[2],
         block_ptr->sub_tiles_flags[3],
         block_ptr->sub_tiles_flags[4]);
      textout(tmpbmp, font, tmp, 324, 220, 255);

      sprintf(tmp, "zeros     : %02X %02X %02X %02X %02X %02X %02X",
         block_ptr->sub_tiles_flags[25],
         block_ptr->sub_tiles_flags[26],
         block_ptr->sub_tiles_flags[27],
         block_ptr->sub_tiles_flags[28],
         block_ptr->sub_tiles_flags[29],
         block_ptr->sub_tiles_flags[30],
         block_ptr->sub_tiles_flags[31]);
      textout(tmpbmp, font, tmp, 324, 240, 255);

      textout(tmpbmp, font, "'Esc' = quit",                          324, 280, 255);
      textout(tmpbmp, font, "'B'   = change boxsize mode",           324, 290, 255);
      textout(tmpbmp, font, "'G'   = change grid mode",              324, 300, 255);
      textout(tmpbmp, font, "'L'   = layer vs grid priority",        324, 310, 255);
      textout(tmpbmp, font, "'P'   = print screen (screenshot)",     324, 320, 255);
      textout(tmpbmp, font, "move  : Up / Down / Left / Right / ",   324, 330, 255);
      textout(tmpbmp, font, "        Page Up / Page Down / Space /", 324, 340, 255);
      textout(tmpbmp, font, "        Backspace / Home / End",        324, 350, 255);
      textout(tmpbmp, font, "'1'   = palette act 1",                 324, 370, 255);
      textout(tmpbmp, font, "'2'   = palette act 2",                 324, 380, 255);
      textout(tmpbmp, font, "'3'   = palette act 3",                 324, 390, 255);
      textout(tmpbmp, font, "'4'   = palette act 4",                 324, 400, 255);
      textout(tmpbmp, font, "'5'   = palette act 5",                 324, 410, 255);
      textout(tmpbmp, font, "'+' (keypad) = higher screen resolution",      324, 430, 255);
      textout(tmpbmp, font, "'-' (keypad) = lower screen resolution",       324, 440, 255);
      sprintf(tmp, "current resolution : %i * %i", screen_x, screen_y);
      textout(tmpbmp, font, tmp,                                     324, 460, 255);
      
      vsync();
      blit(tmpbmp, screen, 0, 0, 0, 0, screen_x, screen_y);
      
      k = readkey();
      switch(k >> 8)
      {
         case KEY_ESC : done = TRUE; break;

         case KEY_RIGHT :
         case KEY_UP    :
         case KEY_SPACE : if (b < glb_block_number - 1)
                             b++;
                          break;

         case KEY_PGUP  : if (b < glb_block_number - 11)
                             b += 10;
                          else
                             b = glb_block_number - 1;
                          break;

         case KEY_PGDN  : if (b > 10)
                             b -= 10;
                          else
                             b = 0;
                          break;
                          
         case KEY_LEFT      :
         case KEY_DOWN      :
         case KEY_BACKSPACE : if (b > 0)
                                 b--;
                              break;

         case KEY_HOME : b=0;
                         break;

         case KEY_END  : b=glb_block_number - 1;
                         break;

         case KEY_G    : gridvalue++;
                         if (gridvalue>2)
                            gridvalue=0;
                         break;

         case KEY_L    : layer = 1 - layer;
                         break;

         case KEY_B    : box = 1 - box;
                         break;

         case KEY_1    : load_palette("d2pal\\act1.dat");
                         set_palette(the_pal);
                         pal_name = d2palname[0];
                         break;
                         
         case KEY_2    : load_palette("d2pal\\act2.dat");
                         set_palette(the_pal);
                         pal_name = d2palname[1];
                         break;
                         
         case KEY_3    : load_palette("d2pal\\act3.dat");
                         set_palette(the_pal);
                         pal_name = d2palname[2];
                         break;
                         
         case KEY_4    : load_palette("d2pal\\act4.dat");
                         set_palette(the_pal);
                         pal_name = d2palname[3];
                         break;
                         
         case KEY_5    : load_palette("d2pal\\act5.dat");
                         set_palette(the_pal);
                         pal_name = d2palname[4];
                         break;

         case KEY_P    : sprintf(tmp, "screenshot-%05i.pcx", scr);
                         while (__file_exists(tmp))
                         {
                            scr++;
                            sprintf(tmp, "screenshot-%05i.pcx", scr);
                         }
                         save_pcx(tmp, tmpbmp, &the_pal);
                         scr++;
                         break;

         case KEY_MINUS_PAD :
            switch(screen_x)
            {
               case 640  : break;
               case 800  : screen_x = 640;  screen_y = 480; break;
               case 1024 : screen_x = 800;  screen_y = 600; break;
               case 1280 : screen_x = 1024; screen_y = 768; break;
            }
            new_gfx_mode(screen_x, screen_y);
            set_palette(the_pal);
            destroy_bitmap(tmpbmp);
            tmpbmp = create_bitmap(screen_x, screen_y);
            break;

         case KEY_PLUS_PAD :
            switch(screen_x)
            {
               case 640  : screen_x = 800;  screen_y = 600;  break;
               case 800  : screen_x = 1024; screen_y = 768;  break;
               case 1024 : screen_x = 1280; screen_y = 1024; break;
               case 1280 : break;
            }
            new_gfx_mode(screen_x, screen_y);
            set_palette(the_pal);
            destroy_bitmap(tmpbmp);
            tmpbmp = create_bitmap(screen_x, screen_y);
            break;
      }
   }

   // end
   destroy_bitmap(tmpbmp);
   printf("done\n");
   return 0;
}

