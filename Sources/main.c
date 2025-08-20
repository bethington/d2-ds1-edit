/*
  - create "Debug" directory
  - added "-debug" in command line : create debug files, like "Debug\D2.lvlprest.headers.txt" or "Debug\Editor.objects.memory.bin"
  - added "-no_vis_debug" in command line : hide Vis debug info for Vis Tiles with graphics

October 30 2011 :
  - unlimited number of objects (dynamic memory allocation)
  - similar objects animates with a random starting frame. fire is more natural, and monsters don't 'dance' anymore.
*/

#define COMPILER_NAME              "Visual C++ 2010 Express Edition"
#define WINDS1EDIT_GUI_LOADER_LINK "http://d2mods.com/forum/viewtopic.php?f=81&t=21281"
#define DS1EDIT_GOOD_DLL           "Allegro 4.4.2, MSVC"
#define DS1EDIT_DLL_LINK           "http://paul.siramy.free.fr/_divers/ds1/allegro-4.4.2-monolith-md.zip"

#ifdef WIN32
   #define DS1EDIT_BUILD __DATE__
#else
   #define DS1EDIT_BUILD "?"
#endif

#ifdef _DEBUG
   #define DS1EDIT_BUILD_MODE "Debug"
#elif NDEBUG
   #define DS1EDIT_BUILD_MODE "Release"
#else
   #define DS1EDIT_BUILD_MODE "unknown"
#endif

#include "structs.h"
#include "error.h"
#include "mpq/mpqview.h"
#include "dt1misc.h"
#include "ds1misc.h"
#include "anim.h"
#include "wEdit.h"
#include "undo.h"
#include "txtread.h"
#include "misc.h"
#include "inicreat.h"
#include "iniread.h"
#include "animdata.h"
#include "interfac.h"


WRKSPC_DATAS_S glb_wrkspc_datas[WRKSPC_MAX] = // workspace datas saved in .ds1
{
   {("DS1EDIT_WRKSPC_TILE_X")},
   {("DS1EDIT_WRKSPC_TILE_Y")},
   {("DS1EDIT_WRKSPC_ZOOM")},
   {("DS1EDIT_VERSION")},
   {("DS1EDIT_SAVE_COUNT")}
};

GAMMA_S glb_gamma_str[GC_MAX] = // gamma correction string table
{
   {"0.60", GC_060}, {"0.62", GC_062}, {"0.64", GC_064},
   {"0.66", GC_066}, {"0.68", GC_068}, {"0.70", GC_070},
   {"0.72", GC_072}, {"0.74", GC_074}, {"0.76", GC_076},
   {"0.78", GC_078}, {"0.80", GC_080}, {"0.82", GC_082},
   {"0.84", GC_084}, {"0.86", GC_086}, {"0.88", GC_088},
   {"0.90", GC_090}, {"0.92", GC_092}, {"0.94", GC_094},
   {"0.96", GC_096}, {"0.98", GC_098}, {"1.00", GC_100},
   {"1.10", GC_110}, {"1.20", GC_120}, {"1.30", GC_130},
   {"1.40", GC_140}, {"1.50", GC_150}, {"1.60", GC_160},
   {"1.70", GC_170}, {"1.80", GC_180}, {"1.90", GC_190},
   {"2.00", GC_200}, {"2.10", GC_210}, {"2.20", GC_220},
   {"2.30", GC_230}, {"2.40", GC_240}, {"2.50", GC_250},
   {"2.60", GC_260}, {"2.70", GC_270}, {"2.80", GC_280},
   {"2.90", GC_290}, {"3.00", GC_300}
};

char * txt_def_lvltype_req[] =
{
   ("Id"),      ("Act"),
   ("File 1"),  ("File 2"),  ("File 3"),  ("File 4"),  ("File 5"),
   ("File 6"),  ("File 7"),  ("File 8"),  ("File 9"),  ("File 10"),
   ("File 11"), ("File 12"), ("File 13"), ("File 14"), ("File 15"),
   ("File 16"), ("File 17"), ("File 18"), ("File 19"), ("File 20"),
   ("File 21"), ("File 22"), ("File 23"), ("File 24"), ("File 25"),
   ("File 26"), ("File 27"), ("File 28"), ("File 29"), ("File 30"),
   ("File 31"), ("File 32"),
   NULL // DO NOT REMOVE !
};

char * txt_def_lvlprest_req[] =
{
   ("Def"),   ("Dt1Mask"),
   ("File1"), ("File2"), ("File3"), ("File4"), ("File5"), ("File6"),
   NULL // DO NOT REMOVE !
};

char * txt_def_obj_req[] =
{
   // number
   ("Act"),
   ("Type"),
   ("Id"),
   ("Direction"),
   ("Index"),
   ("Objects.txt_ID"),
   ("Monstats.txt_ID"),

   // text
   ("Base"),
   ("Token"),
   ("Mode"),
   ("Class"),
   ("HD"), ("TR"), ("LG"), ("RA"), ("LA"), ("RH"), ("LH"), ("SH"),
   ("S1"), ("S2"), ("S3"), ("S4"), ("S5"), ("S6"), ("S7"), ("S8"),
   ("Colormap"),
   ("Description"),

   NULL // DO NOT REMOVE !
};

char * txt_def_objects_req[] =
{
   // number
   ("Id"),
   ("SizeX"),
   ("SizeY"),
   ("FrameCnt0"),
   ("FrameCnt1"),
   ("FrameCnt2"),
   ("FrameCnt3"),
   ("FrameCnt4"),
   ("FrameCnt5"),
   ("FrameCnt6"),
   ("FrameCnt7"),
   ("FrameDelta0"),
   ("FrameDelta1"),
   ("FrameDelta2"),
   ("FrameDelta3"),
   ("FrameDelta4"),
   ("FrameDelta5"),
   ("FrameDelta6"),
   ("FrameDelta7"),
   ("CycleAnim0"),
   ("CycleAnim1"),
   ("CycleAnim2"),
   ("CycleAnim3"),
   ("CycleAnim4"),
   ("CycleAnim5"),
   ("CycleAnim6"),
   ("CycleAnim7"),
   ("Lit0"),
   ("Lit1"),
   ("Lit2"),
   ("Lit3"),
   ("Lit4"),
   ("Lit5"),
   ("Lit6"),
   ("Lit7"),
   ("BlocksLight0"),
   ("BlocksLight1"),
   ("BlocksLight2"),
   ("BlocksLight3"),
   ("BlocksLight4"),
   ("BlocksLight5"),
   ("BlocksLight6"),
   ("BlocksLight7"),
   ("Start0"),
   ("Start1"),
   ("Start2"),
   ("Start3"),
   ("Start4"),
   ("Start5"),
   ("Start6"),
   ("Start7"),
   ("BlocksVis"),
   ("Trans"),
   ("OrderFlag0"),
   ("OrderFlag1"),
   ("OrderFlag2"),
   ("OrderFlag3"),
   ("OrderFlag4"),
   ("OrderFlag5"),
   ("OrderFlag6"),
   ("OrderFlag7"),
   ("Mode0"),
   ("Mode1"),
   ("Mode2"),
   ("Mode3"),
   ("Mode4"),
   ("Mode5"),
   ("Mode6"),
   ("Mode7"),
   ("Yoffset"),
   ("Xoffset"),
   ("Draw"),
   ("Red"),
   ("Green"),
   ("Blue"),
   ("TotalPieces"),
   ("SubClass"),
   ("Xspace"),
   ("YSpace"),
   ("OperateRange"),
   ("Act"),
   ("Sync"),
   ("Flicker"),
   ("Overlay"),
   ("CollisionSubst"),
   ("Left"),
   ("Top"),
   ("Width"),
   ("Height"),
   ("BlockMissile"),
   ("DrawUnder"),
   ("HD"), ("TR"), ("LG"), ("RA"), ("LA"), ("RH"), ("LH"), ("SH"),
   ("S1"), ("S2"), ("S3"), ("S4"), ("S5"), ("S6"), ("S7"), ("S8"),

   // text
   ("Token"),

   NULL // DO NOT REMOVE !
};

char ** glb_txt_req_ptr[RQ_MAX] = {NULL, NULL, NULL, NULL};
       
CONFIG_S      glb_config;                     // global configuration datas
GLB_DS1EDIT_S glb_ds1edit;                    // global datas of the editor
GLB_MPQ_S     glb_mpq_struct [MAX_MPQ_FILE];  // global data of 1 mpq
DS1_S         * glb_ds1                = NULL; // ds1 datas
DT1_S         * glb_dt1                = NULL; // dt1 datas
char          glb_tiles_path        [] = "Data\\Global\\Tiles\\";
char          glb_ds1edit_data_dir  [] = "Data\\";
char          glb_ds1edit_tmp_dir   [] = "Tmp\\";

// debug files
char          * glb_path_lvltypes_mem = "Debug\\Editor.lvltypes.memory.bin";
char          * glb_path_lvltypes_def = "Debug\\D2.lvltypes.headers.txt";
char          * glb_path_lvlprest_mem = "Debug\\Editor.lvlprest.memory.bin";
char          * glb_path_lvlprest_def = "Debug\\D2.lvlprest.headers.txt";
char          * glb_path_obj_mem      = "Debug\\Editor.obj.memory.bin";
char          * glb_path_obj_def      = "Debug\\Editor.obj.headers.txt";
char          * glb_path_objects_mem  = "Debug\\Editor.objects.memory.bin";
char          * glb_path_objects_def  = "Debug\\D2.objects.headers.txt";


// ==========================================================================
// near the start of the prog
void ds1edit_init(void)
{
   FILE * out;
   static struct
   {
      char   name[40];
      MODE_E idx;
   } cursor[MOD_MAX] = {
        {"pcx\\cursor_t.pcx", MOD_T}, // tiles
        {"pcx\\cursor_o.pcx", MOD_O}, // objects
        {"pcx\\cursor_p.pcx", MOD_P}, // paths
        {"pcx\\cursor_l.pcx", MOD_L}  // lights
     };
   int  i, o;
   static int
        dir4[4]   = { 0,  1,  2,  3},
        dir8[8]   = { 4,  0,  5,  1,  6,  2,  7,  3},

        dir16[16] = { 4,  8,  0,  9,  5, 10,  1, 11,
                      6, 12,  2, 13,  7, 14,  3, 15},

        dir32[32] = { 4, 16,  8, 17,  0, 18,  9, 19,
                      5, 20, 10, 21,  1, 22, 11, 23,
                      6, 24, 12, 25,  2, 26, 13, 27,
                      7, 28, 14, 29,  3, 30, 15, 31},

        obj_sub_tile[5][5] = {
           { 0,  2,  5,  9, 14},
           { 1,  4,  8, 13, 18},
           { 3,  7, 12, 17, 21},
           { 6, 11, 16, 20, 23},
           {10, 15, 19, 22, 24}
        };
   char tmp[80];


   // zero mem
   printf("ds1edit_init()\n");
   memset( & glb_config,  0, sizeof(glb_config));
   memset( & glb_ds1edit, 0, sizeof(glb_ds1edit));

   // allocate mem for DT1 & DS1
   i = sizeof(DS1_S) * DS1_MAX;
   printf("\nallocate %i bytes for glb_ds1[%i]\n", i, DS1_MAX);
   glb_ds1 = (DS1_S *) malloc(i);
   if (glb_ds1 == NULL)
   {
      sprintf(
         tmp,
         "ds1edit_init(), error.\n"
         "Can't allocate %i bytes for the glb_ds1[%i] table.", i, DS1_MAX
      );
      ds1edit_error(tmp);
   }
   memset(glb_ds1, 0, i);

   i = sizeof(DT1_S) * DT1_MAX;
   printf("allocate %i bytes for glb_dt1[%i]\n\n", i, DT1_MAX);
   glb_dt1 = (DT1_S *) malloc(i);
   if (glb_dt1 == NULL)
   {
      sprintf(
         tmp,
         "ds1edit_init(), error.\n"
         "Can't allocate %i bytes for the glb_dt1[%i] table.", i, DT1_MAX
      );
      ds1edit_error(tmp);
   }
   memset(glb_dt1, 0, i);

   // set the version
   glb_ds1edit.version_build = DS1EDIT_BUILD;
   glb_ds1edit.version_dll   = DS1EDIT_GOOD_DLL;
   strcpy(glb_ds1edit.version, DS1EDIT_BUILD);
   printf(".exe version : %s", COMPILER_NAME);
   printf(
      ", Build Date = %s, Build Mode = %s\n",
      glb_ds1edit.version,
      DS1EDIT_BUILD_MODE
   );

   // update the version info on disk
   sprintf(tmp, "%sversion", glb_ds1edit_data_dir);
   out = fopen(tmp, "wb");
   if (out != NULL)
   {
      sprintf(tmp, "Build Date = %s", glb_ds1edit.version);
      fwrite(tmp, strlen(tmp), 1, out);
      fclose(out);
   }

   for (i=0; i<MAX_MPQ_FILE; i++)
   {
      memset( & glb_mpq_struct[i], 0, sizeof(GLB_MPQ_S));
      glb_mpq_struct[i].is_open = FALSE;
   }

   // mouse cursors
   for (i=0; i<MOD_MAX; i++)
   {
      glb_ds1edit.mouse_cursor[i] =
         load_pcx(cursor[i].name, glb_ds1edit.dummy_pal);
      if (glb_ds1edit.mouse_cursor[i] == NULL)
      {
         sprintf(
            tmp,
            "ds1edit_init(), error.\n"
            "Can't open the file \"%s\".", cursor[i].name
         );
         ds1edit_error(tmp);
      }
   }

   // txt
   glb_txt_req_ptr[RQ_LVLTYPE]  = txt_def_lvltype_req;
   glb_txt_req_ptr[RQ_LVLPREST] = txt_def_lvlprest_req;
   glb_txt_req_ptr[RQ_OBJ]      = txt_def_obj_req;
   glb_txt_req_ptr[RQ_OBJECTS]  = txt_def_objects_req;

   // debug files
   remove(glb_path_lvltypes_mem);
   remove(glb_path_lvltypes_def);
   remove(glb_path_lvlprest_mem);
   remove(glb_path_lvlprest_def);
   remove(glb_path_obj_mem);
   remove(glb_path_obj_def);
   remove(glb_path_objects_mem);
   remove(glb_path_objects_def);

   // tables
   glb_ds1edit.new_dir1[0] = 0;

   for (i=0; i < 4; i++)
      glb_ds1edit.new_dir4[i] = dir4[i];

   for (i=0; i < 8; i++)
      glb_ds1edit.new_dir8[i] = dir8[i];
   
   for (i=0; i < 16; i++)
      glb_ds1edit.new_dir16[i] = dir16[i];

   for (i=0; i < 32; i++)
      glb_ds1edit.new_dir32[i] = dir32[i];

   // for re-ordering sub-tile objects, from back to front
   for (i=0; i < 5; i++)
      for (o=0; o < 5; o++)
         glb_ds1edit.obj_sub_tile_order[i][o] = obj_sub_tile[i][o];

   // init the default values of the command line
   glb_ds1edit.cmd_line.ds1_filename  = NULL;
   glb_ds1edit.cmd_line.ini_filename  = NULL;
   glb_ds1edit.cmd_line.lvltype_id    = -1;
   glb_ds1edit.cmd_line.lvlprest_def  = -1;
   glb_ds1edit.cmd_line.resize_width  = -1;
   glb_ds1edit.cmd_line.resize_height = -1;
   glb_ds1edit.cmd_line.force_pal_num = -1;
   glb_ds1edit.cmd_line.no_check_act  = FALSE;
   glb_ds1edit.cmd_line.dt1_list_num  = -1;
   for (i=0; i < DT1_IN_DS1_MAX; i++)
      glb_ds1edit.cmd_line.dt1_list_filename[i] = NULL;

   // 2nd row of infos
   glb_ds1edit.show_2nd_row = FALSE;

   // video pages
   glb_ds1edit.video_page_num = 0;
}


// ==========================================================================
UDWORD ds1edit_get_bitmap_size(BITMAP * bmp)
{
   UDWORD size = 0;
   
   if (bmp == NULL)
      return 0;
   
   size += sizeof(BITMAP);
   size += (sizeof(UBYTE *) * bmp->h);

   switch (bitmap_color_depth(bmp))
   {
      case  8 : size += bmp->w * bmp->h; break;
      case 15 :
      case 16 : size += 2 * (bmp->w * bmp->h); break;
      case 24 : size += 3 * (bmp->w * bmp->h); break;
      case 32 : size += 4 * (bmp->w * bmp->h); break;
      default :
         break;
   }

   return size;
}



// ==========================================================================
UDWORD ds1edit_get_RLE_bitmap_size(RLE_SPRITE * rle)
{
   UDWORD size = 0;
   
   if (rle == NULL)
      return 0;
   
   size += sizeof(RLE_SPRITE);
   size += rle->size;

   return size;
}


// ==========================================================================
// automatically called at the end, with the help of atexit()
void ds1edit_exit(void)
{
   int i, z, b;
   

   printf("\nds1edit_exit()\n");

   // close all mpq
   for (i=0; i<MAX_MPQ_FILE; i++)
   {
      if (glb_mpq_struct[i].is_open != FALSE)
      {
         fprintf(stderr, "closing %s\n", glb_mpq_struct[i].file_name);
         fflush(stderr);

         glb_mpq = & glb_mpq_struct[i];
         mpq_batch_close();
         
         memset( & glb_mpq_struct[i], 0, sizeof(GLB_MPQ_S));
         glb_mpq_struct[i].is_open = FALSE;
      }
   }
   
   // free all mem
   fprintf(stderr, "exit, memory free :\n");
   fflush(stderr);

   // mouse cursor
   fprintf(stderr, "   * mouse cursor...\n");
   fflush(stderr);
   show_mouse(NULL);
   for (i=0; i<MOD_MAX; i++)
   {
      if (glb_ds1edit.mouse_cursor[i] != NULL)
      {
         destroy_bitmap(glb_ds1edit.mouse_cursor[i]);
         glb_ds1edit.mouse_cursor[i] = NULL;
      }
   }
      
   // screen buffer
   fprintf(stderr, "   * screen buffer...\n");
   fflush(stderr);
   if (glb_ds1edit.screen_buff != NULL)
   {
      destroy_bitmap(glb_ds1edit.screen_buff);
      glb_ds1edit.screen_buff = NULL;
   }

   if (glb_ds1edit.big_screen_buff != NULL)
   {
      destroy_bitmap(glb_ds1edit.big_screen_buff);
      glb_ds1edit.big_screen_buff = NULL;
   }
   
   // config, mpq name
   fprintf(stderr, "   * config, mpq names...\n");
   fflush(stderr);
   for (i=0; i<MAX_MPQ_FILE; i++)
   {
      if(glb_config.mpq_file[i] != NULL)
         free(glb_config.mpq_file[i]);
   }

   // config, mod directory
   fprintf(stderr, "   * config, mod directory name...\n");
   fflush(stderr);
   for (i=0; i<MAX_MOD_DIR; i++)
   {
      if(glb_config.mod_dir[i] != NULL)
         free(glb_config.mod_dir[i]);
   }

   // palettes
   fprintf(stderr, "   * palettes...\n");
   fflush(stderr);
   for (i=0; i<ACT_MAX; i++)
   {
      if(glb_ds1edit.d2_pal[i] != NULL)
      {
         free(glb_ds1edit.d2_pal[i]);
         glb_ds1edit.d2_pal[i] = NULL;
         glb_ds1edit.pal_size[i] = 0;
      }
   }

   // dt1
   fprintf(stderr, "   * DT1 files...\n");
   fflush(stderr);
   if (glb_dt1 != NULL)
   {
      for (i=0; i<DT1_MAX; i++)
         dt1_free(i);
   }

   // ds1
   fprintf(stderr, "   * DS1 files...\n");
   fflush(stderr);
   if (glb_ds1 != NULL)
   {
      for (i=0; i<DS1_MAX; i++)
         ds1_free(i);
   }

   // objects descriptions
   fprintf(stderr, "   * objects descriptions...\n");
   fflush(stderr);
   if (glb_ds1edit.obj_desc != NULL)
   {
      anim_exit();
      free(glb_ds1edit.obj_desc);
      glb_ds1edit.obj_desc = NULL;
      glb_ds1edit.obj_desc_num = 0;
   }

   // buttons & tab
   fprintf(stderr, "   * buttons & tab...\n");
   fflush(stderr);
   wedit_free();

   // undo buffers
   fprintf(stderr, "   * undo buffers...\n");
   fflush(stderr);
   if (glb_ds1 != NULL)
      undo_exit();

   // walkable infos tiles
   fprintf(stderr, "   * walkable info tiles...\n");
   fflush(stderr);
   for (z=0; z<ZM_MAX; z++)
   {
      for (i=0; i<25; i++)
      {
         if (glb_ds1edit.subtile_nowalk[z][i] != NULL)
         {
            destroy_rle_sprite(glb_ds1edit.subtile_nowalk[z][i]);
            glb_ds1edit.subtile_nowalk[z][i] = NULL;
         }

         if (glb_ds1edit.subtile_nojump[z][i] != NULL)
         {
            destroy_rle_sprite(glb_ds1edit.subtile_nojump[z][i]);
            glb_ds1edit.subtile_nojump[z][i] = NULL;
         }
      }
   }

   if (glb_ds1edit.subtile_help != NULL)
   {
      destroy_bitmap(glb_ds1edit.subtile_help);
      glb_ds1edit.subtile_help = NULL;
   }


   // walkable infos tiles, combinations
   fprintf(stderr, "   * walkable info tiles combinations...\n");
   fflush(stderr);
   for (b=0; b<256; b++)
   {
      for (z=0; z<ZM_MAX; z++)
      {
         for (i=0; i<25; i++)
         {
            if (glb_ds1edit.subtile_flag[b][z][i] != NULL)
            {
               destroy_rle_sprite(glb_ds1edit.subtile_flag[b][z][i]);
               glb_ds1edit.subtile_flag[b][z][i] = NULL;
            }
         }
      }
   }

   // .txt buffers
   fprintf(stderr, "   * .txt buffers ...\n");
   fflush(stderr);
   if (glb_ds1edit.lvltypes_buff != NULL)
      glb_ds1edit.lvltypes_buff = txt_destroy(glb_ds1edit.lvltypes_buff);
   if (glb_ds1edit.lvlprest_buff != NULL)
      glb_ds1edit.lvlprest_buff = txt_destroy(glb_ds1edit.lvlprest_buff);
   if (glb_ds1edit.obj_buff != NULL)
      glb_ds1edit.obj_buff = txt_destroy(glb_ds1edit.obj_buff);

   // animdata.d2
   fprintf(stderr, "   * animdata.d2 buffer ...\n");
   fflush(stderr);
   if (glb_ds1edit.anim_data.buffer)
      free(glb_ds1edit.anim_data.buffer);

   // obj in ds1
   fprintf(stderr, "   * glb_ds1[] & glb_dt1[] & glb_ds1[].obj & glb_ds1[].obj_undo ...\n");
   fflush(stderr);

   // ds1 & dt1
   if (glb_ds1 != NULL)
   {
      for (i=0; i < DS1_MAX; i++)
      {
         if (glb_ds1[i].obj != NULL)
            free(glb_ds1[i].obj);
         if (glb_ds1[i].obj_undo != NULL)
            free(glb_ds1[i].obj_undo);
      }
      free(glb_ds1);
   }
   if (glb_dt1 != NULL)
      free(glb_dt1);
   
   fflush(stderr);
}


// ==========================================================================
// just for debug purpose
void ds1edit_debug(void)
{
   printf("\n");
   printf("d2char                  = %s\n", glb_config.mpq_file[3]);
   printf("d2data                  = %s\n", glb_config.mpq_file[2]);
   printf("d2exp                   = %s\n", glb_config.mpq_file[1]);
   printf("patch_d2                = %s\n", glb_config.mpq_file[0]);
   printf("mod_dir                 = %s\n", glb_config.mod_dir[0]);
   printf("fullscreen              = %s\n", glb_config.fullscreen ? "YES" : "NO");
   printf("screen_width            = %i\n", glb_config.screen.width);
   printf("screen_height           = %i\n", glb_config.screen.height);
   printf("screen_depth            = %i\n", glb_config.screen.depth);
   printf("refresh_rate            = %i\n", glb_config.screen.refresh);
   printf("keyb_scroll_x           = %i\n", glb_config.scroll.keyb.x);
   printf("keyb_scroll_y           = %i\n", glb_config.scroll.keyb.y);
   printf("mouse_scroll_x          = %i\n", glb_config.scroll.mouse.x);
   printf("mouse_scroll_y          = %i\n", glb_config.scroll.mouse.y);
   printf("edit_scroll_x           = %i\n", glb_config.scroll.edit.x);
   printf("edit_scroll_y           = %i\n", glb_config.scroll.edit.y);   
   printf("obj_edit_scroll         = %i\n", glb_config.scroll.obj_edit);
   printf("mouse_speed_x           = %i\n", glb_config.mouse_speed.x);
   printf("mouse_speed_y           = %i\n", glb_config.mouse_speed.y);
   printf("gamma_correction        = %s\n", glb_gamma_str[glb_config.gamma].str);
   printf("only_normal_type2       = %s\n", glb_config.normal_type2        ? "YES" : "NO");
   printf("always_max_layers       = %s\n", glb_config.always_max_layers   ? "YES" : "NO");
   printf("stretch_sprites         = %s\n", glb_config.stretch_sprites     ? "YES" : "NO");
   printf("winobj_can_scroll_keyb  = %s\n", glb_config.winobj_scroll_keyb  ? "YES" : "NO");
   printf("winobj_can_scroll_mouse = %s\n", glb_config.winobj_scroll_mouse ? "YES" : "NO");

   printf("center_zoom             = ");
   switch(glb_config.center_zoom)
   {
      case -1    : printf("NO_CHANGE\n"); break;
      case ZM_11 : printf("1:1\n"); break;
      case ZM_12 : printf("1:2\n"); break;
      case ZM_14 : printf("1:4\n"); break;
      case ZM_18 : printf("1:8\n"); break;
      case ZM_116: printf("1:16\n"); break;
      default : printf("?\n"); break;
   }

   printf("nb_type1_per_act        = %i\n", glb_config.nb_type1_per_act);
   printf("nb_type2_per_act        = %i\n", glb_config.nb_type2_per_act);
   printf("ds1_saved_minimize      = %s\n", glb_config.minimize_ds1         ? "YES" : "NO");
   printf("lower_speed_zoom_out    = %s\n", glb_config.lower_speed_zoom_out ? "YES" : "NO");
   printf("workspace_enable        = %s\n", glb_config.workspace_enable     ? "YES" : "NO");
   printf("\n");
}


// ==========================================================================
// 1 tick each 1/25 of a second
void ds1edit_counter_tick(void)
{
   glb_ds1edit.ticks_elapsed++;
}
END_OF_FUNCTION(ds1edit_counter_tick);


// ==========================================================================
// 1 tick each second
void ds1edit_counter_fps(void)
{
   glb_ds1edit.old_fps = glb_ds1edit.fps;
   glb_ds1edit.fps = 0;
}
END_OF_FUNCTION(ds1edit_counter_fps);


// ==========================================================================
// open all mpq
void ds1edit_open_all_mpq(void)
{
   int  i;

   printf("ds1edit_open_all_mpq()\n");
   for (i=0; i<MAX_MPQ_FILE; i++)
   {
      if (glb_config.mpq_file[i] != NULL)
      {
         fprintf(stdout, "opening mpq %i : %s\n", i, glb_config.mpq_file[i]);
         fprintf(stderr, "opening %s\n", glb_config.mpq_file[i]);
         fflush(stdout);
         fflush(stderr);
         glb_mpq = & glb_mpq_struct[i];
         mpq_batch_open(glb_config.mpq_file[i]);
      }
   }
}


// ==========================================================================
// load palettes of the 5 acts from disk, else from mpq
void ds1edit_load_palettes(void)
{
   int  i, entry;
   char palname[80], tmp[150];
   
   fprintf(stderr, "loading palettes");
   fprintf(stdout, "\nloading palettes\n");
   fflush(stderr);
   fflush(stdout);
   
   for (i=0; i<ACT_MAX; i++)
   {
      glb_ds1edit.pal_loaded[i] = TRUE;
      // first checking on disk
      if (misc_load_pal_from_disk(i) == FALSE)
      {
         // not already on disk
         glb_ds1edit.pal_loaded[i] = FALSE;
         
         // make full path
         sprintf(palname, "Data\\Global\\Palette\\Act%i\\Pal.pl2", i+1);
         
         // load the palette
         printf("want to read a palette from mpq : %s\n", palname);
         entry = misc_load_mpq_file(
                    palname,
                    (char **) & glb_ds1edit.d2_pal[i],
                    & glb_ds1edit.pal_size[i],
                    TRUE
                 );
         if (entry == -1)
         {
            sprintf(
               tmp,
               "ds1edit_load_palettes() :\n"
               "File \"%s\" not found.",
               palname
            );
            if (i < 4)
               ds1edit_error(tmp);
            else
               printf("warning :\n%s\n", tmp);
         }

         // save it for the next time
         misc_save_pal_on_disk(i, glb_ds1edit.d2_pal[i]);
      }

      // palette loaded, either from disk of from mpq, reorder it
      misc_pl2_correct(i);
      misc_pal_d2_2_vga(i);
      fprintf(stderr, ".");
      fflush(stderr);
   }
   fprintf(stderr, "\n");
   fflush(stderr);
   printf("\n");
}


// ==========================================================================
// as expected, the start of the prog
int main(int argc, char * argv[])
{
   int         i, mpq_num=0, mod_num=0, ds1_idx=0;
   char        * ininame = "ds1edit.ini";
   int         res_w, res_h;
   static char tmp  [512];
   static char tmp2 [512];

   // init
   srand(time(NULL));
   allegro_init();

   set_gfx_mode(GFX_TEXT, 80, 25, 0, 0);
   
   if (install_keyboard() != 0)
      ds1edit_error("main(), error.\nCan't install the Keyboard Handler.");
   if (install_timer() != 0)
      ds1edit_error("main(), error.\nCan't install the Timer Handler.");

   set_color_depth(8);
   if (atexit(ds1edit_exit) != 0)
      ds1edit_error("main(), error.\nCan't install the 'atexit' Handler.");

   ds1edit_init();

   // some allegro infos
   printf("\n[allegro var]\n");
   printf("allegro_id          = \"%s\"\n", allegro_id);
   printf("os_type             = ");
   switch(os_type)
   {
       case OSTYPE_UNKNOWN  : printf("unknown, or regular MSDOS\n"); break;
       case OSTYPE_WIN3     : printf("Windows 3.1 or earlier\n");    break;
       case OSTYPE_WIN95    : printf("Windows 95\n");                break;
       case OSTYPE_WIN98    : printf("Windows 98\n");                break;
       case OSTYPE_WINME    : printf("Windows ME\n");                break;
       case OSTYPE_WINNT    : printf("Windows NT\n");                break;
       case OSTYPE_WIN2000  : printf("Windows 2000\n");              break;
       case OSTYPE_WINXP    : printf("Windows XP\n");                break;
       case OSTYPE_WIN2003  : printf("Windows 2003\n");              break;
       case OSTYPE_WINVISTA : printf("Windows Vista\n");             break;
       case OSTYPE_WIN7     : printf("Windows Seven\n");             break;
       case OSTYPE_OS2      : printf("OS/2\n");                      break;
       case OSTYPE_WARP     : printf("OS/2 Warp 3\n");               break;
       case OSTYPE_DOSEMU   : printf("Linux DOSEMU\n");              break;
       case OSTYPE_OPENDOS  : printf("Caldera OpenDOS\n");           break;
       case OSTYPE_LINUX    : printf("Linux\n");                     break;
       case OSTYPE_SUNOS    : printf("SunOS/Solaris\n");             break;
       case OSTYPE_FREEBSD  : printf("FreeBSD\n");                   break;
       case OSTYPE_NETBSD   : printf("NetBSD\n");                    break;
       case OSTYPE_IRIX     : printf("IRIX\n");                      break;
       case OSTYPE_DARWIN   : printf("Darwin\n");                    break;
       case OSTYPE_QNX      : printf("QNX\n");                       break;
       case OSTYPE_UNIX     : printf("Unknown Unix variant\n");      break;
       case OSTYPE_BEOS     : printf("BeOS\n");                      break;
       case OSTYPE_HAIKU    : printf("Haiku\n");                     break;
       case OSTYPE_MACOS    : printf("MacOS\n");                     break;
       case OSTYPE_MACOSX   : printf("MacOS X\n");                   break;
       case OSTYPE_PSP      : printf("Psp\n");                       break;
       default : printf("?\n"); break;
   }
   printf("os_version          = %i\n", os_version);
   printf("os_revision         = %i\n", os_revision);
   printf("os_multitasking     = %s\n", os_multitasking ? "TRUE" : "FALSE");
   printf("cpu_vendor          = %s\n", cpu_vendor);
   printf("cpu_family          = ");
   switch(cpu_family)
   {
       case CPU_FAMILY_UNKNOWN  : printf("unknown\n");                             break;
       case CPU_FAMILY_I386     : printf("Intel-compatible 386\n");                break;
       case CPU_FAMILY_I486     : printf("Intel-compatible 486\n");                break;
       case CPU_FAMILY_I586     : printf("Pentium or equivalent\n");               break;
       case CPU_FAMILY_I686     : printf("Pentium Pro, II, III or equivalent\n");  break;
       case CPU_FAMILY_ITANIUM  : printf("Itanium processor\n");                   break;
       case CPU_FAMILY_POWERPC  : printf("PowerPC processor\n");                   break;
       case CPU_FAMILY_EXTENDED : printf("needs to be read from the cpu_model\n"); break;
       default : printf("?\n"); break;
   }
   printf("cpu_model           = ");
   if (cpu_family == CPU_FAMILY_I586)
   {
      switch(cpu_model)
      {
         case CPU_MODEL_PENTIUM : printf("CPU_MODEL_PENTIUM\n"); break;
         case CPU_MODEL_K5      : printf("CPU_MODEL_K5\n");      break;
         case CPU_MODEL_K6      : printf("CPU_MODEL_K6\n");      break;
         default : printf("?\n"); break;
      }
   }
   else if (cpu_family == CPU_FAMILY_I686)
   {
      switch(cpu_model)
      {
         case CPU_MODEL_PENTIUMPRO           : printf("CPU_MODEL_PENTIUMPRO\n");           break;
         case CPU_MODEL_PENTIUMII            : printf("CPU_MODEL_PENTIUMII\n");            break;
         case CPU_MODEL_PENTIUMIIIKATMAI     : printf("CPU_MODEL_PENTIUMIIIKATMAI\n");     break;
         case CPU_MODEL_PENTIUMIIICOPPERMINE : printf("CPU_MODEL_PENTIUMIIICOPPERMINE\n"); break;
         case CPU_MODEL_ATHLON               : printf("CPU_MODEL_ATHLON\n");               break;
         case CPU_MODEL_DURON                : printf("CPU_MODEL_DURON\n");                break;
         default : printf("?\n"); break;
      }
   }
   else if (cpu_family == CPU_FAMILY_EXTENDED)
   {
      switch(cpu_model)
      {
         case CPU_MODEL_PENTIUMIV : printf("CPU_MODEL_PENTIUMIV\n"); break;
         case CPU_MODEL_XEON      : printf("CPU_MODEL_XEON\n");      break;
         case CPU_MODEL_ATHLON64  : printf("CPU_MODEL_ATHLON64\n");  break;
         case CPU_MODEL_OPTERON   : printf("CPU_MODEL_OPTERON\n");   break;
         default : printf("?\n"); break;
      }
   }
   else if (cpu_family == CPU_FAMILY_POWERPC)
   {
      switch(cpu_model)
      {
         case CPU_MODEL_POWERPC_601  : printf("CPU_MODEL_POWERPC_601\n");  break;
         case CPU_MODEL_POWERPC_602  : printf("CPU_MODEL_POWERPC_602\n");  break;
         case CPU_MODEL_POWERPC_603  : printf("CPU_MODEL_POWERPC_603\n");  break;
         case CPU_MODEL_POWERPC_604  : printf("CPU_MODEL_POWERPC_604\n");  break;
         case CPU_MODEL_POWERPC_620  : printf("CPU_MODEL_POWERPC_620\n");  break;
         case CPU_MODEL_POWERPC_750  : printf("CPU_MODEL_POWERPC_750\n");  break;
         case CPU_MODEL_POWERPC_7400 : printf("CPU_MODEL_POWERPC_7400\n"); break;
         case CPU_MODEL_POWERPC_7450 : printf("CPU_MODEL_POWERPC_7450\n"); break;
         default : printf("?\n"); break;
      }
   }
   else
      printf("-\n");
   printf("cpu_capabilities    = 0x%08X\n", cpu_capabilities);
   printf("desktop_color_depth = %ibpp\n", desktop_color_depth());
   get_desktop_resolution( & res_w, & res_h);
   printf("desktop_resolution  = %i * %i\n", res_w, res_h);
   printf("\n");

   sprintf(
      tmp,
      "DS1 Editor, %s Build %s, %s",
      DS1EDIT_BUILD_MODE,
      glb_ds1edit.version,
      allegro_id
   );
   set_window_title(tmp);

   // allegro dll safety check
   if (strcmp(allegro_id, DS1EDIT_GOOD_DLL) != 0)
   {
      sprintf(tmp,
         "main(), error.\n"
         "\n"
         "The current Allegro DLL that you're using is :\n"
         "   * %s\n"
         "\n"
         "It should be :\n"
         "   * %s\n"
         "\n"
         "Download the good Allegro DLL from :\n"
         "\n"
         "   %s\n"
         "\n"
         "Then extract the DLL into the Ds1edit directory and restart the program.",
         allegro_id,
         DS1EDIT_GOOD_DLL,
         DS1EDIT_DLL_LINK
      );
      ds1edit_error(tmp);
   }

   // check data\tmp directory
   sprintf(tmp, "%s%s\\.", glb_ds1edit_data_dir, glb_ds1edit_tmp_dir);
   if (file_exists(tmp, -1, NULL) == 0)
   {
      // create tmp directory
      sprintf(tmp, "%s%s", glb_ds1edit_data_dir, glb_ds1edit_tmp_dir);
      if (strlen(tmp))
         tmp[strlen(tmp) - 1] = 0;
      if (mkdir(tmp) != 0)
      {
         // re-use the tmp var for a different string
         sprintf(
            tmp,
            "main(), error.\n"
            "Can't create directory \"%s%s\".",
            glb_ds1edit_data_dir, glb_ds1edit_tmp_dir);
         tmp[strlen(tmp) - 1] = 0;
         ds1edit_error(tmp);
      }
   }
   
   // check if ds1edit.ini exists
   sprintf(tmp, "ds1edit.ini");
   if (file_exists(tmp, -1, NULL) == 0)
   {
      ini_create(tmp);
      allegro_message(
         "main(), error.\n"
         "There was no 'Ds1edit.ini' file in the Ds1edit directory.\n"
         "A new one with default values was created.\n"
         "Edit it to fit your configuration, then restart the program."
      );
      exit(DS1ERR_INICREATE);
   }

   // init (config)
   ini_read(ininame);
   ds1edit_debug();

   // check mod directory
   mod_num = 0;
   if (glb_config.mod_dir[0] != NULL)
   {
      sprintf(tmp, "%s\\.", glb_config.mod_dir[0]);
      if (file_exists(tmp, -1, NULL) == 0)
      {
         sprintf(
            tmp2,
            "main(), warning.\n"
            "Can't find the Mod Directory defined in 'Ds1edit.ini' :\n"
            "%s",
            tmp
         );
         printf("%s\n", tmp2);
         allegro_message(tmp2);
      }
      else
         mod_num = 1;
   }

   for (i=0; i<MAX_MPQ_FILE; i++)
   {
      if (glb_config.mpq_file[i] != NULL)
      {
         if (file_exists(glb_config.mpq_file[i], -1, NULL) == 0)
         {
            sprintf(
               tmp2,
               "main(), warning.\n"
               "Can't find this MPQ defined in 'Ds1edit.ini' :\n"
               "%s",
               glb_config.mpq_file[i]
            );
            printf("%s\n", tmp2);
            allegro_message(tmp2);
         }
         else
            mpq_num++;
      }
   }
   if ((mod_num == 0) && (mpq_num == 0))
   {
      sprintf(
         tmp,
         "main(), error.\n"
         "No Mod Directory and no MPQ files available : it can't work.");
      ds1edit_error(tmp);
      allegro_message(tmp);
   }
   else if (mpq_num < 4)
      printf("warning : not all the 4 mpq have been found\n");

   // gamma correction
   glb_ds1edit.cur_gamma = glb_config.gamma;
   misc_read_gamma();

   // preview window update
   glb_ds1edit.win_preview.x0 = 0;
   glb_ds1edit.win_preview.y0 = 0;
   glb_ds1edit.win_preview.w  = glb_config.screen.width;
   glb_ds1edit.win_preview.h  = glb_config.screen.height;

   // edit window
   wedit_read_pcx();
   wedit_make_2nd_buttons();
   misc_walkable_tile_info_pcx();

   // screen buffer (we're still in 8bpp color depth !)

   // we're making a big buffer, with 300 pixels on each 4 borders,
   // and then we'll make the true screen buffer be a sub-bitmap of this buffer
   // this is to avoid potential problems with clipings, especially when using
   // the functions from gfx_custom.c
   // yes, I'm hidding the problem under the carpet

   glb_config.screen.width  += 600;
   glb_config.screen.height += 600;
   glb_ds1edit.big_screen_buff = create_bitmap(
      glb_config.screen.width,
      glb_config.screen.height
   );
   if (glb_ds1edit.big_screen_buff == NULL)
   {
      sprintf(tmp, "main(), error.\nCan't create big_screen_buff (%i*%i pixels).",
         glb_config.screen.width,
         glb_config.screen.height
      );
      ds1edit_error(tmp);
   }
   glb_config.screen.width  -= 600;
   glb_config.screen.height -= 600;

   glb_ds1edit.screen_buff = create_sub_bitmap(
      glb_ds1edit.big_screen_buff,
      300,
      300,
      glb_config.screen.width,
      glb_config.screen.height
   );
   if (glb_ds1edit.screen_buff == NULL)
   {
      sprintf(tmp, "main(), error.\nCan't create sub-bitmap screen_buff (%i*%i pixels).",
         glb_config.screen.width,
         glb_config.screen.height
      );
      ds1edit_error(tmp);
   }

   // open all mpq
   ds1edit_open_all_mpq();
   
   // load palettes from disk, else from mpq
   ds1edit_load_palettes();
   
   // parse the command line
   if (misc_cmd_line_parse (argc, argv))
      ds1edit_error("main(), error.\nProblem in the command line.");

   // create debug directory if necessary
   if (glb_ds1edit.cmd_line.debug_mode == TRUE)
      mkdir("Debug");

   // objects.txt
   fprintf(stderr, "reading objects.txt...");
   read_objects_txt();
   fprintf(stderr, "done\n");

   // obj.txt
   fprintf(stderr, "reading Obj.txt...");
   read_obj_txt();
   fprintf(stderr, "done\n");

   if (glb_ds1edit.cmd_line.ds1_filename != NULL)
   {
      // .ds1

      // read the ds1
      if (glb_ds1edit.cmd_line.dt1_list_num != -1)
      {
         // force dt1
         misc_open_1_ds1_force_dt1(ds1_idx);
      }
      else
      {
         // find dt1 list from .txt
         misc_open_1_ds1(
            ds1_idx,
            glb_ds1edit.cmd_line.ds1_filename,
            glb_ds1edit.cmd_line.lvltype_id,
            glb_ds1edit.cmd_line.lvlprest_def,
            glb_ds1edit.cmd_line.resize_width,
            glb_ds1edit.cmd_line.resize_height
         );
      }
   }
   else if (glb_ds1edit.cmd_line.ini_filename != NULL)
   {
      // .ini

      // 2nd row of infos
      glb_ds1edit.show_2nd_row = TRUE;

      // list of ds1 to open
      misc_open_several_ds1(argv[1]);
   }
   else
   {
      // bug
      ds1edit_error("main(), error.\nBug : neither .DS1 nor a .INI in the command line.");
   }

   // syntaxe of the command line
   printf("============================================================\n");
   if (argc >= 4) // at least 3 arguments (ds1 name + ID + DEF + options)
   {
   }
   else if (argc == 2) // 1 argument (assume it's a .ini file)
   {
   }
   else // syntax error
   {
      printf("syntaxe 1 : ds1edit <file.ds1> <lvlTypes.txt Id> <lvlPrest.txt Def> [options]\n");
      printf("syntaxe 2 : ds1edit <file.ini>\n"
             "\n"
             "   file.ini in syntaxe 2 is a text file, each line for 1 ds1 to load,\n"
             "   3 elements : <lvlTypes.txt Id> <lvlPrest.txt Def> <file.ds1>\n");
      allegro_message(
         "main(), error.\n"
         "This program needs to get parameters by the command-line.\n"
         "\n"
         "Check the README.txt to know how to make .bat files,\n"
         "or use a front-end tool like \"ds1edit Loader\" :\n"         
         WINDS1EDIT_GUI_LOADER_LINK
      );
      exit(DS1ERR_CMDLINE);
   }
   printf("============================================================\n");

   // animdata.d2
   printf("\nanimdata_load()\n");
   fflush(stdout);
   fflush(stderr);
   animdata_load();

   // load necessary objects animation
   printf("loading ds1 objects animations :\n");
   fprintf(stderr, "loading ds1 objects animations : ");
   fflush(stdout);
   fflush(stderr);

   anim_update_gfx(TRUE); // TRUE is for "show dot progression"

   printf("\n");
   fprintf(stderr, "\n");
   fflush(stdout);
   fflush(stderr);
   
   // colormaps
   printf("\ncolor maps...");
   fprintf(stderr, "color maps");
   misc_make_cmaps();
   printf("done\n");
   fprintf(stderr, "done\n");

   // start
   fflush(stdout);
   fflush(stderr);
   if ( (glb_config.screen.depth !=  8) &&
        (glb_config.screen.depth != 15) &&
        (glb_config.screen.depth != 16) &&
        (glb_config.screen.depth != 24) &&
        (glb_config.screen.depth != 32)
      )
   {
      sprintf(
         tmp,
         "main(), error.\nInvalid color depth (%i).\nValid values are : 8, 15, 16, 24 and 32.\nCheck the 'Ds1edit.ini' file, at the 'screen_depth' line",
         glb_config.screen.depth
      );
      ds1edit_error(tmp);
   }

   // set color depth for the screen
   set_color_depth(glb_config.screen.depth);

   request_refresh_rate(glb_config.screen.refresh);
   if (glb_config.fullscreen == TRUE)
   {
      if (set_gfx_mode(GFX_AUTODETECT_FULLSCREEN, glb_config.screen.width,
             glb_config.screen.height, 0, 0) != 0)
      {
         sprintf(
            tmp,
            "main(), error.\nCan't initialize this graphical mode : "
            "%i*%i %ibpp %iHz %s.\nAllegro_error = \"%s\".",
            glb_config.screen.width,
            glb_config.screen.height,
            glb_config.screen.depth,
            glb_config.screen.refresh,
            "Fullscreen",
            allegro_error
         );
         ds1edit_error(tmp);
      }
   }
   else
   {
      if (set_gfx_mode(GFX_AUTODETECT_WINDOWED, glb_config.screen.width,
             glb_config.screen.height, 0, 0) != 0)
      {
         sprintf(
            tmp,
            "main(), error.\nCan't initialize this graphical mode : "
            "%i*%i %ibpp %iHz %s.\nAllegro_error = \"%s\".",
            glb_config.screen.width,
            glb_config.screen.height,
            glb_config.screen.depth,
            glb_config.screen.refresh,
            "Windowed",
            allegro_error
         );
         ds1edit_error(tmp);
      }
   }
   glb_ds1edit.current_refresh_rate = get_refresh_rate();

   // video pages
   glb_ds1edit.video_page[0] = create_video_bitmap(
      glb_config.screen.width,
      glb_config.screen.height
   );
   if (glb_ds1edit.video_page[0] == NULL)
   {
      sprintf(tmp, "main(), error.\nCan't create video page 1.",
         glb_config.screen.width,
         glb_config.screen.height
      );
      ds1edit_error(tmp);
   }

   glb_ds1edit.video_page[1] = create_video_bitmap(
      glb_config.screen.width,
      glb_config.screen.height
   );
   if (glb_ds1edit.video_page[1] == NULL)
   {
      sprintf(tmp, "main(), error.\nCan't create video page 2.",
         glb_config.screen.width,
         glb_config.screen.height
      );
      ds1edit_error(tmp);
   }

   // when the editor is minimized, stop it until the user returns
   set_display_switch_mode(SWITCH_PAUSE);

   // get back to a 8bpp color depth, for all the next BITMAP creation
   set_color_depth(8);

   text_mode(-1); // draw text as sprite, no background color

   LOCK_VARIABLE(glb_ds1edit.old_fps);
   LOCK_VARIABLE(glb_ds1edit.fps);
   LOCK_VARIABLE(glb_ds1edit.ticks_elapsed);
   LOCK_FUNCTION(ds1edit_counter_tick);
   LOCK_FUNCTION(ds1edit_counter_fps);

   if (install_int(ds1edit_counter_tick, 1000 / 25) != 0)
   {
      sprintf(
         tmp,
         "main(), error.\nCan't install the 'counter_tick' timer handler at 25Hz."
      );
      ds1edit_error(tmp);
   }
   if (install_int(ds1edit_counter_fps, 1000) != 0)
   {
      sprintf(
         tmp,
         "main(), error.\nCan't install the 'counter_fps' timer handler at 1Hz."
      );
      ds1edit_error(tmp);
   }

   if (install_mouse() == -1)
   {
      sprintf(
         tmp,
         "main(), error.\nCan't install the Mouse handler."
      );
      ds1edit_error(tmp);
   }
   set_mouse_speed(glb_config.mouse_speed.x, glb_config.mouse_speed.y);

   // show_mouse(screen);

   glb_ds1edit.win_preview.x0 = glb_ds1[ds1_idx].own_wpreview.x0;
   glb_ds1edit.win_preview.y0 = glb_ds1[ds1_idx].own_wpreview.y0;
   glb_ds1edit.win_preview.w  = glb_ds1[ds1_idx].own_wpreview.w ;
   glb_ds1edit.win_preview.h  = glb_ds1[ds1_idx].own_wpreview.h ;


   // main loop
   freopen("stderr.txt", "wt", stderr);
   interfac_user_handler(ds1_idx);

   if (glb_ds1edit.video_page[0] != NULL)
      destroy_bitmap(glb_ds1edit.video_page[0]);

   if (glb_ds1edit.video_page[1] != NULL)
      destroy_bitmap(glb_ds1edit.video_page[1]);

   // end
   set_gfx_mode(GFX_TEXT, 80, 25, 0, 0);
   // fclose(stdout);
   fflush(stdout);
   fflush(stderr);
   return DS1ERR_OK;
}
END_OF_MAIN();
