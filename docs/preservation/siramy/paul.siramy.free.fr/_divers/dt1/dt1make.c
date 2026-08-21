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

#define dt1_head_size 276
#define block_size     96
#define sub_tile_size  20

// reducing size of executable, for the allegro 3.9.34 (wip) lib

BEGIN_GFX_DRIVER_LIST
   GFX_DRIVER_VGA
END_GFX_DRIVER_LIST

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
   WORD xpos;
   WORD ypos;
   int  xgrid;
   int  ygrid;
   WORD format;
   long data_length;
   long data_offset;
} SUB_TILE_S;

typedef struct
{
   long  direction;
   WORD  y_delta;
   UBYTE tile_sound;
   UBYTE animated;
   long  ysize;
   long  xsize;
   long  orientation;
   long  main_index;
   long  sub_index;
   long  frame;
   long  unknown;
   int   sub_tiles_flags[25];
   long  tiles_ptr;
   long  tiles_length;
   long  tiles_number;
} BLOCK_S;


// global datas
typedef    enum {PT_FA, PT_FS, PT_WU, PT_WD, PT_MAX} PCX_TYPE;
FILE       * ini = NULL, * dt1 = NULL;
BITMAP     * pcx[4] = {NULL, NULL, NULL, NULL};
PALETTE    the_pal;
BLOCK_S    block;
SUB_TILE_S sub_tile;
long       block_number = 0, ini_line_num = 1;
typedef    enum {LT_BN, // block number
                 LT_ZE, // zero char : empty line
                 LT_BH, // block header
                 LT_PF, // pcx file
                 LT_PX, // pcx x
                 LT_PY, // pcx y
                 LT_DI, // direction
                 LT_YD, // y_delta
                 LT_TS, // tile sound
                 LT_AN, // animated
                 LT_OR, // orientation
                 LT_MI, // main index
                 LT_SI, // sub index
                 LT_FR, // frame
                 LT_UN, // unknown
                 LT_F1, // floor_flag 1
                 LT_F2, // floor_flag 2
                 LT_F3, // floor_flag 3
                 LT_F4, // floor_flag 4
                 LT_F5, // floor_flag 5
                 LT_MAX} LINE_TYPE;
struct
{
   char lvalue[80];
   char format[10];
} line_info[LT_MAX] = {
     {"nb_blocks",   "%li"},   // LT_BN
     {"",            ""},      // LT_ZE
     {"block",       "%li"},   // LT_BH
     {"pcx_file",    "%s"},    // LT_PF
     {"pcx_x",       "%li"},   // LT_PX
     {"pcx_y",       "%li"},   // LT_PY
     {"direction",   "%08lX"}, // LT_DI
     {"y_delta",     "%i"},    // LT_YD
     {"tile_sound",  "%i"},    // LT_TS
     {"animated",    "%i"},    // LT_AN
     {"orientation", "%08lX"}, // LT_OR
     {"main_index",  "%08lX"}, // LT_MI
     {"sub_index",   "%08lX"}, // LT_SI
     {"frame",       "%08lX"}, // LT_FR
     {"unknown",     "%08lX"}, // LT_UN
     {"floor_flag1", ""},      // LT_F1
     {"floor_flag2", ""},      // LT_F2
     {"floor_flag3", ""},      // LT_F3
     {"floor_flag4", ""},      // LT_F4
     {"floor_flag5", ""}       // LT_F5
  };


// ==========================================================================
// atexit() function, when the program end, to close the files
void dt1make_exit(void)
{
   int i;
   
   if (ini != NULL) fclose(ini);
   if (dt1 != NULL) fclose(dt1);
   for (i=0; i<PT_MAX; i++)
      if (pcx[i] != NULL)
         destroy_bitmap(pcx[i]);
}


// ==========================================================================
// write the dt1 main header
void write_dt1_header(void)
{
   long x1 = 7, x2 = 6, head_start = dt1_head_size;
   int  i;

   // signature
   fwrite(&x1, 1, 4, dt1);
   fwrite(&x2, 1, 4, dt1);
   
   // zeros
   for (i=0; i<260; i++)
      fputc(0, dt1);
   
   // # of blocks
   fwrite(&block_number, 1, 4, dt1);

   // start of block header, always 0x114
   fwrite(&head_start, 1, 4, dt1);
}


// ==========================================================================
// write fake blocks headers
// will be set later
void write_fake_blocks_headers(void)
{
   int i, b;

   for (b=0; b<block_number; b++)
      for (i=0; i<block_size; i++)
         fputc(0, dt1);
}


// ==========================================================================
// write / update 1 block header
void write_block_header(long b)
{
   long old_pos, new_pos;
   int  h, i;

   old_pos = ftell(dt1);
   new_pos = dt1_head_size + (block_size * b);
   fseek(dt1, new_pos, SEEK_SET);
   
   // direction
   fwrite(&block.direction, 1, 4, dt1);

   // flags
   fwrite(&block.y_delta,    1, 2, dt1);
   fwrite(&block.tile_sound, 1, 1, dt1);
   fwrite(&block.animated,   1, 1, dt1);
   
   // ysize and xsize. ysize must be negative
   if (block.ysize > 0)
      block.ysize = - block.ysize;
   fwrite(&block.ysize, 1, 4, dt1);
   fwrite(&block.xsize, 1, 4, dt1);

   // zeros
   for (i=0; i<4; i++)
      fputc(0, dt1);
   
   // seems to be that
   fwrite(&block.orientation, 1, 4, dt1);
   fwrite(&block.main_index,  1, 4, dt1);
   fwrite(&block.sub_index,   1, 4, dt1);
   fwrite(&block.frame,       1, 4, dt1);

   // unknown (like 0xFF00FF00)
   fwrite(&block.unknown, 1, 4, dt1);

   // sub-tiles flags
   for (i=0; i<25; i++)
       fputc(block.sub_tiles_flags[i], dt1);

   // zeros
   for (i=0; i<7; i++)
      fputc(0, dt1);
      
   // pointer to sub-tiles headers
   fwrite(&block.tiles_ptr,    1, 4, dt1);

   // sub-tiles length
   fwrite(&block.tiles_length, 1, 4, dt1);

   // # of sub-tiles
   fwrite(&block.tiles_number, 1, 4, dt1);

   // zeros
   for (i=0; i<12; i++)
      fputc(0, dt1);

   // end, get back
   fseek(dt1, old_pos, SEEK_SET);
}


// ==========================================================================
// write fake sub-tiles headers of 1 block
// will be set later
void write_fake_sub_tiles_headers(long n)
{
   long i, t;

   for (t=0; t<n; t++)
      for (i=0; i<sub_tile_size; i++)
         fputc(0, dt1);
}


// ==========================================================================
// update the sub-tile header of 1 block
void update_sub_tile_header(void)
{
   int i;
   
   // xpos & ypos
   fwrite(&sub_tile.xpos, 1, 2, dt1);
   fwrite(&sub_tile.ypos, 1, 2, dt1);

   // zero
   for (i=0; i<2; i++)
      fputc(0, dt1);

   // xgrid & ygrid
   fputc(sub_tile.xgrid, dt1);
   fputc(sub_tile.ygrid, dt1);

   // format
   fwrite(&sub_tile.format, 1, 2, dt1);

   // data length
   fwrite(&sub_tile.data_length, 1, 4, dt1);

   // zero
   for (i=0; i<2; i++)
      fputc(0, dt1);

   // data offset
   fwrite(&sub_tile.data_offset, 1, 4, dt1);
}


// ==========================================================================
// check if it can be a 3d-isometric
// if at least 1 trans pixel then it's a trans isometric
int is_solid_isometric(BITMAP * src, int x0, int y0)
{
   int   x, y=0, n,
         xjump[15] = {14, 12, 10, 8, 6, 4, 2, 0, 2, 4, 6, 8, 10, 12, 14},
         nbpix[15] = {4, 8, 12, 16, 20, 24, 28, 32, 28, 24, 20, 16, 12, 8, 4},
         length=256;

   while (length > 0)
   {
      x = xjump[y];
      n = nbpix[y];
      length -= n;
      while (n)
      {
         if (getpixel(src, x0+x, y0+y) == 0)
            return FALSE;
         x++;
         n--;
      }
      y++;
   }
   return TRUE;
}


// ==========================================================================
// check if it is an empty 3d-isometric tile
// if at least 1 solid pixel then it's a non-empty isometric
int is_all_empty_isometric(BITMAP * src, int x0, int y0)
{
   int   x, y=0, n,
         xjump[15] = {14, 12, 10, 8, 6, 4, 2, 0, 2, 4, 6, 8, 10, 12, 14},
         nbpix[15] = {4, 8, 12, 16, 20, 24, 28, 32, 28, 24, 20, 16, 12, 8, 4},
         length=256;

   while (length > 0)
   {
      x = xjump[y];
      n = nbpix[y];
      length -= n;
      while (n)
      {
         if (getpixel(src, x0+x, y0+y) != 0)
            return FALSE;
         x++;
         n--;
      }
      y++;
   }
   return TRUE;
}


// ==========================================================================
// check if it is an empty tile (rectangular)
// if at least 1 solid pixel then it's a non-empty sub-tile of wall
// btw, wall's sub-tiles are always coded in the format with transparency
int is_all_empty_wall(BITMAP * src, int x0, int y0)
{
   int x, y;

   for (y=0; y<32; y++)
      for (x=0; x<32; x++)
         if (getpixel(src, x0+x, y0+y) != 0)
            return FALSE;
   return TRUE;
}


// ==========================================================================
// write a 3d-isometric sub-tile datas
void write_isometric(BITMAP * src, int x0, int y0)
{
   int   x, y=0, n,
         xjump[15] = {14, 12, 10, 8, 6, 4, 2, 0, 2, 4, 6, 8, 10, 12, 14},
         nbpix[15] = {4, 8, 12, 16, 20, 24, 28, 32, 28, 24, 20, 16, 12, 8, 4},
         length=256;

   while (length > 0)
   {
      x = xjump[y];
      n = nbpix[y];
      length -= n;
      while (n)
      {
         fputc(getpixel(src, x0+x, y0+y), dt1);
         x++;
         n--;
      }
      y++;
   }
}


// ==========================================================================
// write sub-tile datas with transparency
// pass TRUE to is_3diso if it is a transparent FLOOR
void write_tile_with_transparency(BITMAP * src, int x0, int y0, int is_3diso)
{
   BITMAP * tmp;
   int    x, y=0, n, c, old_c,
          xjump[15] = {14, 12, 10, 8, 6, 4, 2, 0, 2, 4, 6, 8, 10, 12, 14},
          nbpix[15] = {4, 8, 12, 16, 20, 24, 28, 32, 28, 24, 20, 16, 12, 8, 4},
          length=256,
          nb_trans, nb_solid, max_y;

   tmp = create_bitmap(32, 32);
   if (tmp == NULL)
      return;
   clear(tmp);
   if (is_3diso == TRUE)
   {
      max_y = 15;
      while (length > 0)
      {
         x = xjump[y];
         n = nbpix[y];
         length -= n;
         while (n)
         {
            c = getpixel(src, x0+x, y0+y);
            putpixel(tmp, x, y, c);
            x++;
            n--;
         }
         y++;
      }
   }
   else
   {
      max_y = 32;
      blit(src, tmp, x0, y0, 0, 0, 32, 32);
   }


   // encode
   x = y = 0;
   while (y < max_y)
   {
      // nb_trans
      c = getpixel(tmp, x, y);
      nb_trans = 0;
      if (c == 0)
      {
         x++;
         nb_trans = 1;
         while ( (x < 32) && (getpixel(tmp, x, y) == 0) )
         {
            x++;
            nb_trans++;
         }
      }
      if (x < 32)
      {
         fputc(nb_trans, dt1);
         // nb_solid
//         c = getpixel(tmp, x, y);
         nb_solid = 1;
         x++;
         while ( (x < 32) && (getpixel(tmp, x, y) != 0 ) )
         {
            x++;
            nb_solid++;
         }
         fputc(nb_solid, dt1);
         x -= nb_solid;
         while (nb_solid)
         {
            fputc(getpixel(tmp, x, y), dt1);
            x++;
            nb_solid--;
         }
      }
      // EOL
      if (x >= 32)
      {
         fputc(0, dt1);
         fputc(0, dt1);
         x = 0;
         y++;
      }
   }

   // end
   destroy_bitmap(tmp);
   return;
}


// ==========================================================================
// write each sub-tile of a floor
void write_floor_loop(BITMAP * src, int x0, int y0)
{
   struct
   {
      int x;
      int y;
   } pos[25] = { { 64, 64}, { 48, 56}, {32, 48}, {16, 40}, { 0, 32},
                 { 80, 56}, { 64, 48}, {48, 40}, {32, 32}, {16, 24},
                 { 96, 48}, { 80, 40}, {64, 32}, {48, 24}, {32, 16},
                 {112, 40}, { 96, 32}, {80, 24}, {64, 16}, {48,  8},
                 {128, 32}, {112, 24}, {96, 16}, {80,  8}, {64,  0}
               };
   long x, y, n=0, start_header, start_data, end_data, length;
   int  i, present[25];

   // for each sub-tiles, count the present ones
   for (i=0; i<25; i++)
      present[i] = FALSE;
      
   for (i=0; i<25; i++)
   {
      x = pos[i].x;
      y = pos[i].y;
      if (is_all_empty_isometric(src, x0+x, y0+y) == FALSE)
      {
         present[i] = TRUE;
         n++;
      }
   }

   // write sub-tiles headers
//   start_header = ftell(dt1);
   end_data = start_header = ftell(dt1);
   write_fake_sub_tiles_headers(n);

   // write sub-tiles datas
   n=0;
   for (i=0; i<25; i++)
   {
      if (present[i])
      {
         start_data = ftell(dt1);
         x = pos[i].x;
         y = pos[i].y;
         if (is_solid_isometric(src, x0+x, y0+y))
         {
            write_isometric(src, x0+x, y0+y);
            sub_tile.format = 0x0001;
         }
         else
         {
            write_tile_with_transparency(src, x0+x, y0+y, TRUE);
            sub_tile.format = 0x2005;
         }
         // sub-tile header update
         end_data = ftell(dt1);
         sub_tile.xpos = pos[i].x;
         sub_tile.ypos = pos[i].y;
         sub_tile.xgrid = 4 - (i % 5);
         sub_tile.ygrid = 4 - (i / 5);
         sub_tile.data_length = end_data - start_data;
         sub_tile.data_offset = start_data - start_header;
         fseek(dt1, start_header + (n * sub_tile_size), SEEK_SET);
         update_sub_tile_header();
         fseek(dt1, end_data, SEEK_SET);
         // next
         n++;
      }
   }
   // prepare block update
   block.ysize        = -128;
   block.xsize        = 160;
   block.tiles_ptr    = start_header;
   block.tiles_length = end_data - start_header;
   block.tiles_number = n;
}


// ==========================================================================
// write each sub-tile of a floor with walls up
void write_walls_up_loop(BITMAP * src, int x0, int y0)
{
   long x, y, n=0, start_header, start_data, end_data, length;
   int  nx=0, ny=50, present[50][5], nb_pres=0;

   // for each sub-tiles, count the present ones
   for (y=0; y<50; y++)
      for (x=0; x<5; x++)
         present[y][x] = FALSE;
         
   for (y=0; y<src->h; y += 32)
   {
      for (x=128; x>=0; x -= 32)
      {
         if (is_all_empty_wall(src, x0+x, y0+y) == FALSE)
         {
            present[y/32][x/32] = TRUE;
            n++;
         }
      }
   }
   // write sub-tiles headers
   end_data = start_header = ftell(dt1);
   write_fake_sub_tiles_headers(n);

   // write sub-tiles datas
   n = 0;
   for (y=0; y<src->h; y += 32)
   {
      for (x=128; x>=0; x -= 32)
      {
         if (present[y/32][x/32] == TRUE)
         {
            start_data = ftell(dt1);
            write_tile_with_transparency(src, x0+x, y0+y, FALSE);
            sub_tile.format = 0x1001;
            // sub-tile header update
            end_data = ftell(dt1);
            sub_tile.xpos = x;
            sub_tile.ypos = - src->h + y;
            sub_tile.xgrid = 0;
            sub_tile.ygrid = 0;
            sub_tile.data_length = end_data - start_data;
            sub_tile.data_offset = start_data - start_header;
            fseek(dt1, start_header + (n * sub_tile_size), SEEK_SET);
            update_sub_tile_header();
            fseek(dt1, end_data, SEEK_SET);
            // next
            n++;
         }
      }
   }
   // prepare block update
   for (y=0; y<50; y++)
   {
      for (x=0; x<5; x++)
      {
         if (present[y][x] == TRUE)
         {
            if (x>nx)
               nx = x;
            if (y<ny)
               ny = y;
            nb_pres++;
         }
      }
   }
   if (nb_pres)
   {
      nx++;
      ny = (src->h/32) - ny + 1;
      block.ysize = - (ny * 32);
      block.xsize = nx * 32;
   }
   else
   {
      block.ysize = 0;
      block.xsize = 0;
   }
   block.tiles_ptr    = start_header;
   block.tiles_length = end_data - start_header;
   block.tiles_number = n;
}


// ==========================================================================
// write each sub-tile of a floor with walls down
void write_walls_down_loop(BITMAP * src, int x0, int y0)
{
   long x, y, n=0, start_header, start_data, end_data, length;
   int  nx=0, ny=0, present[50][5], nb_pres=0;

   // for each sub-tiles, count the present ones
   for (y=0; y<50; y++)
      for (x=0; x<5; x++)
         present[y][x] = FALSE;
         
   for (y=0; y<src->h; y += 32)
   {
      for (x=128; x>=0; x -= 32)
      {
         if (is_all_empty_wall(src, x0+x, y0+y) == FALSE)
         {
            present[y/32][x/32] = TRUE;
            n++;
         }
      }
   }
   // write sub-tiles headers
   end_data = start_header = ftell(dt1);
   write_fake_sub_tiles_headers(n);

   // write sub-tiles datas
   n = 0;
   for (y=src->h - 32; y>=0; y -= 32)
   {
      for (x=128; x>=0; x -= 32)
      {
         if (present[y/32][x/32] == TRUE)
         {
            start_data = ftell(dt1);
            write_tile_with_transparency(src, x0+x, y0+y, FALSE);
            sub_tile.format = 0x1001;
            // sub-tile header update
            end_data = ftell(dt1);
            sub_tile.xpos = x;
            sub_tile.ypos = y - 96;
            sub_tile.xgrid = 0;
            sub_tile.ygrid = 0;
            sub_tile.data_length = end_data - start_data;
            sub_tile.data_offset = start_data - start_header;
            fseek(dt1, start_header + (n * sub_tile_size), SEEK_SET);
            update_sub_tile_header();
            fseek(dt1, end_data, SEEK_SET);
            // next
            n++;
         }
      }
   }
   // prepare block update
   for (y=0; y<50; y++)
   {
      for (x=0; x<5; x++)
      {
         if (present[y][x] == TRUE)
         {
            if (x>nx)
               nx = x;
            if (y>ny)
               ny = y;
            nb_pres++;
         }
      }
   }
   if (nb_pres)
   {
      nx++;
      ny = ny + 1; // + 2 ?
      block.ysize = - (ny * 32);
      block.xsize = nx * 32;
   }
   else
   {
      block.ysize = 0;
      block.xsize = 0;
   }

   // hack for some few cliff (block 70 in expansion\siege\cliff.dt1)
   nb_pres=0;
   for(y=0; y<4; y++)
      for (x=0; x<5; x++)
         if (present[y][x] == TRUE)
            nb_pres++;
   if (nb_pres == 0)
      block.ysize += (4 * 32);
   
   // continue
   block.tiles_ptr    = start_header;
   block.tiles_length = end_data - start_header;
   block.tiles_number = n;
}


// ==========================================================================
void ini_error(char * str)
{
   printf("error in the .ini file\n");
   printf("%s, at line %li\n", str, ini_line_num);
fflush(stdout);
   exit(1);
}


// ==========================================================================
// read a line from the ini file
// special case : when reading sub-tile flags, ptr must point to 5 bytes
void read_ini_line(LINE_TYPE lt, void * ptr)
{
   char tmpline[80], error[256];
   int  c = 0, n, n2;

   memset(tmpline, 0, sizeof(tmpline));
   memset(error  , 0, sizeof(error  ));
   if (fgets(tmpline, sizeof(tmpline), ini) == NULL)
      ini_error("can't read file");
   if ((lt == LT_ZE) || (ptr == NULL))
   {
      ini_line_num++;
      return ;
   }
   n = strlen(tmpline);
   if (n > 0)
      tmpline[n-1] = 0;
   else
      ini_error("empty line");
   n2 = strlen(line_info[lt].lvalue);
   while (c<n2)
   {
      if (tmpline[c] != line_info[lt].lvalue[c])
      {
         sprintf(error, "unexpected character '%c' instead of '%c'",
            tmpline[c], line_info[lt].lvalue[c]);
         ini_error(error);
      }
      c++;
   }
   // search '='
   while (c<n && tmpline[c] != '=')
      c++;
   if (c >= n)
      ini_error("didn't found the '='");
   if (tmpline[c] != '=')
   {
      sprintf(error, "unexpected character '%c' instead of '='", tmpline[c]);
      ini_error(error);
   }
   c++;
   // while spaces
   while (c<n && tmpline[c] == ' ')
      c++;
   if (c >= n)
      ini_error("no value behind '='");
   // read value
   if ( (lt >= LT_F1) && (lt <= LT_F5) )
   {
      if (sscanf(tmpline + c, "%02X %02X %02X %02X %02X",
         ptr, ptr+1, ptr+2, ptr+3, ptr+4) != 5)
      {
         sprintf(error, "couldn't read the 5 values behind '='");
         ini_error(error);
      }
   }
   else
   {
      if (sscanf(tmpline + c, line_info[lt].format, ptr) != 1)
      {
         sprintf(error, "couldn't read the value behind '='");
         ini_error(error);
      }
   }
   ini_line_num++;
}


// ==========================================================================
void read_ini_all(void)
{
   char error[256], pcx_name[256];
   long b, cur_b, pcx_x, pcx_y, old_pos, new_pos;
   int  pcx_idx;
   char floor_flag[5][5];
   
   read_ini_line(LT_BN, &block_number);
   write_dt1_header();
   write_fake_blocks_headers();
   
   for (b=0; b<block_number; b++)
   {
      memset(&block, 0, sizeof(block));
      read_ini_line(LT_ZE, NULL);
      read_ini_line(LT_BH, &cur_b);
      if (cur_b != b)
      {
         sprintf(error, "block number is not what it ought to be\n(%li instead of %li)", cur_b, b);
         ini_error(error);
      }
      read_ini_line(LT_PF, &pcx_name);
      if (strcmp(pcx_name, "fa") == 0)      pcx_idx = PT_FA;
      else if (strcmp(pcx_name, "fs") == 0) pcx_idx = PT_FS;
      else if (strcmp(pcx_name, "wu") == 0) pcx_idx = PT_WU;
      else if (strcmp(pcx_name, "wd") == 0) pcx_idx = PT_WD;
      else
      {
         sprintf(error, "unknown pcx code (%s)", pcx_name);
         ini_error(error);
      }
      read_ini_line(LT_PX, &pcx_x);
      read_ini_line(LT_PY, &pcx_y);
      read_ini_line(LT_DI, &block.direction);
      read_ini_line(LT_YD, &block.y_delta);
      read_ini_line(LT_TS, &block.tile_sound);
      read_ini_line(LT_AN, &block.animated);
      read_ini_line(LT_OR, &block.orientation);
      read_ini_line(LT_MI, &block.main_index);
      read_ini_line(LT_SI, &block.sub_index);
      read_ini_line(LT_FR, &block.frame);
      read_ini_line(LT_UN, &block.unknown);
      read_ini_line(LT_F1, &floor_flag[0][0]);
      read_ini_line(LT_F2, &floor_flag[1][0]);
      read_ini_line(LT_F3, &floor_flag[2][0]);
      read_ini_line(LT_F4, &floor_flag[3][0]);
      read_ini_line(LT_F5, &floor_flag[4][0]);
      block.sub_tiles_flags[20] = floor_flag[0][0];
      block.sub_tiles_flags[21] = floor_flag[0][1];
      block.sub_tiles_flags[22] = floor_flag[0][2];
      block.sub_tiles_flags[23] = floor_flag[0][3];
      block.sub_tiles_flags[24] = floor_flag[0][4];
      block.sub_tiles_flags[15] = floor_flag[1][0];
      block.sub_tiles_flags[16] = floor_flag[1][1];
      block.sub_tiles_flags[17] = floor_flag[1][2];
      block.sub_tiles_flags[18] = floor_flag[1][3];
      block.sub_tiles_flags[19] = floor_flag[1][4];
      block.sub_tiles_flags[10] = floor_flag[2][0];
      block.sub_tiles_flags[11] = floor_flag[2][1];
      block.sub_tiles_flags[12] = floor_flag[2][2];
      block.sub_tiles_flags[13] = floor_flag[2][3];
      block.sub_tiles_flags[14] = floor_flag[2][4];
      block.sub_tiles_flags[5]  = floor_flag[3][0];
      block.sub_tiles_flags[6]  = floor_flag[3][1];
      block.sub_tiles_flags[7]  = floor_flag[3][2];
      block.sub_tiles_flags[8]  = floor_flag[3][3];
      block.sub_tiles_flags[9]  = floor_flag[3][4];
      block.sub_tiles_flags[0]  = floor_flag[4][0];
      block.sub_tiles_flags[1]  = floor_flag[4][1];
      block.sub_tiles_flags[2]  = floor_flag[4][2];
      block.sub_tiles_flags[3]  = floor_flag[4][3];
      block.sub_tiles_flags[4]  = floor_flag[4][4];

printf("block %li\n", cur_b);
      switch(pcx_idx)
      {
         case PT_FA :
         case PT_FS :
            write_floor_loop(pcx[pcx_idx], pcx_x, pcx_y);
            break;
            
         case PT_WU :
            write_walls_up_loop(pcx[PT_WU], pcx_x, pcx_y);
            break;

         case PT_WD :
            write_walls_down_loop(pcx[PT_WD], pcx_x, pcx_y);
            break;
      }
      write_block_header(b);
   }
}


// ==========================================================================
int main(int argc, char * argv[])
{
   char ini_path[256], ini_name[256], ini_ext[256], tmp_file[256];
   int  i, c, drive_pos = -1, last_slash_pos = -1, period_pos = -1, n;
   
   // header & syntaxe & atexit()
   printf("DT1MAKE v 0.97 beta, by Paul Siramy, Freeware\n"
          "=============================================\n");

   if (argc == 1)
   {
      printf("syntaxe : dt1make <file.ini>\n");
      exit(0);
   }
   ini = fopen(argv[1], "rt");
   if (ini == NULL)
   {
      printf("%s not found\n", argv[1]);
      exit(1);
   }
   
   // install allegro
   allegro_init();
   set_color_depth(8);
   atexit(dt1make_exit);

   // pcx filenames managment
   period_pos = n = strlen(argv[1]);
   for (i=0; i<n; i++)
   {
      if ( argv[1][i] == ':')
         drive_pos = i;
      else if ( (argv[1][i] == '\\') || (argv[1][i] == '/') )
         last_slash_pos = i;
      if ( argv[1][i] == '.')
         period_pos = i;
   }
   memset(ini_path, 0, sizeof (ini_path));
   memset(ini_name, 0, sizeof (ini_name));
   memset(ini_ext,  0, sizeof (ini_ext));
   i = 1 + (last_slash_pos > drive_pos ? last_slash_pos : drive_pos);
   strncpy(ini_path, argv[1], i);
   strncpy(ini_name, argv[1] + i, period_pos - i);
   strncpy(ini_ext,  argv[1] + period_pos, n - period_pos);
   
   // open pcx
   sprintf(tmpfile, "%s%s-fa.pcx", ini_path, ini_name);
   pcx[PT_FA] = load_pcx(tmpfile, &the_pal);
   sprintf(tmpfile, "%s%s-fs.pcx", ini_path, ini_name);
   pcx[PT_FS] = load_pcx(tmpfile, &the_pal);
   sprintf(tmpfile, "%s%s-wu.pcx", ini_path, ini_name);
   pcx[PT_WU] = load_pcx(tmpfile, &the_pal);
   sprintf(tmpfile, "%s%s-wd.pcx", ini_path, ini_name);
   pcx[PT_WD] = load_pcx(tmpfile, &the_pal);

   // dt1
   sprintf(tmpfile, "NEW_%s.dt1", ini_name);
   dt1 = fopen(tmpfile, "wb+");
   
   // read ini
   read_ini_all();
   printf("\n\"%s\" has been saved\n", tmpfile);
   
   // end
   printf("done\n");
   return 0;
}

