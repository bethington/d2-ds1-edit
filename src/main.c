/* Derived from win_ds1edit by Paul Siramy.
 * See NOTICE at the repository root for attribution and license status. */

/*
  - create "Debug" directory
  - added "-debug" in command line : create debug files, like "Debug\D2.lvlprest.headers.txt" or "Debug\Editor.objects.memory.bin"
  - added "-no_vis_debug" in command line : hide Vis debug info for Vis Tiles with graphics

October 30 2011 :
  - unlimited number of objects (dynamic memory allocation)
  - similar objects animates with a random starting frame. fire is more natural, and monsters don't 'dance' anymore.
*/

#define COMPILER_NAME "MSVC"
#define WINDS1EDIT_GUI_LOADER_LINK "https://github.com/bethington/d2-ds1-edit"

#ifndef DS1EDIT_VERSION_STR
#define DS1EDIT_VERSION_STR "dev"
#endif

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
#include "mpq/MpqView.h"
#include "core/dt1.h"
#include "core/ds1.h"
#include "core/cof.h"
#include "ui/edit_window.h"
#include "editor/undo.h"
#include "core/txtread.h"
#include "core/area_browser.h"
#include "ui/props_panel.h"
#include "misc.h"
#include "config.h"
#include "core/animdata.h"
#include "core/asset_export.h"
#include "core/d2install.h"
#include "core/mpq_index.h"
#include "core/preferences.h"
#include "core/project.h"
#include "core/palette.h"
#include "ui/interface.h"
#include "render/preview.h"
#include "cli/cli.h"
#include "platform.h"
#include "ui/ui_assets.h"
#include "ui/input.h"


WRKSPC_DATAS_S glb_wrkspc_datas[WRKSPC_MAX] = // workspace datas saved in .ds1
    {
        {("DS1EDIT_WRKSPC_TILE_X")},
        {("DS1EDIT_WRKSPC_TILE_Y")},
        {("DS1EDIT_WRKSPC_ZOOM")},
        {("DS1EDIT_VERSION")},
        {("DS1EDIT_SAVE_COUNT")}};

GAMMA_S glb_gamma_str[GC_MAX] = // gamma correction string table
    {
        {"0.60", GC_060}, {"0.62", GC_062}, {"0.64", GC_064}, {"0.66", GC_066}, {"0.68", GC_068}, {"0.70", GC_070}, {"0.72", GC_072}, {"0.74", GC_074}, {"0.76", GC_076}, {"0.78", GC_078}, {"0.80", GC_080}, {"0.82", GC_082}, {"0.84", GC_084}, {"0.86", GC_086}, {"0.88", GC_088}, {"0.90", GC_090}, {"0.92", GC_092}, {"0.94", GC_094}, {"0.96", GC_096}, {"0.98", GC_098}, {"1.00", GC_100}, {"1.10", GC_110}, {"1.20", GC_120}, {"1.30", GC_130}, {"1.40", GC_140}, {"1.50", GC_150}, {"1.60", GC_160}, {"1.70", GC_170}, {"1.80", GC_180}, {"1.90", GC_190}, {"2.00", GC_200}, {"2.10", GC_210}, {"2.20", GC_220}, {"2.30", GC_230}, {"2.40", GC_240}, {"2.50", GC_250}, {"2.60", GC_260}, {"2.70", GC_270}, {"2.80", GC_280}, {"2.90", GC_290}, {"3.00", GC_300}};

char *txt_def_lvltype_req[] =
    {
        ("Id"), ("Act"), ("Name"),
        ("File 1"), ("File 2"), ("File 3"), ("File 4"), ("File 5"),
        ("File 6"), ("File 7"), ("File 8"), ("File 9"), ("File 10"),
        ("File 11"), ("File 12"), ("File 13"), ("File 14"), ("File 15"),
        ("File 16"), ("File 17"), ("File 18"), ("File 19"), ("File 20"),
        ("File 21"), ("File 22"), ("File 23"), ("File 24"), ("File 25"),
        ("File 26"), ("File 27"), ("File 28"), ("File 29"), ("File 30"),
        ("File 31"), ("File 32"),
        NULL // DO NOT REMOVE !
};

char *txt_def_lvlprest_req[] =
    {
        ("Def"), ("Dt1Mask"),
        ("File1"), ("File2"), ("File3"), ("File4"), ("File5"), ("File6"),
        ("Name"), ("LevelId"),
        NULL // DO NOT REMOVE !
};

char *txt_def_levels_req[] =
    {
        ("Name"), ("Id"), ("Act"), ("LevelType"),
        ("SizeX"), ("SizeY"), ("OffsetX"), ("OffsetY"),
        ("Waypoint"), ("Quest"),
        ("MonLvl1"), ("MonLvl2"), ("MonLvl3"),
        ("MonDen"), ("MonDen(N)"), ("MonDen(H)"),
        /* Visibility */
        ("Vis0"), ("Vis1"), ("Vis2"), ("Vis3"),
        ("Vis4"), ("Vis5"), ("Vis6"), ("Vis7"),
        ("Warp0"), ("Warp1"), ("Warp2"), ("Warp3"),
        ("Warp4"), ("Warp5"), ("Warp6"), ("Warp7"),
        /* Environment */
        ("Rain"), ("Mud"), ("Intensity"), ("Red"), ("Green"), ("Blue"),
        ("IsInside"), ("Teleport"),
        /* Monster Types */
        ("NumMon"),
        ("mon1"), ("mon2"), ("mon3"), ("mon4"), ("mon5"),
        ("mon6"), ("mon7"), ("mon8"), ("mon9"), ("mon10"),
        ("nmon1"), ("nmon2"), ("nmon3"), ("nmon4"), ("nmon5"),
        ("nmon6"), ("nmon7"), ("nmon8"), ("nmon9"), ("nmon10"),
        ("umon1"), ("umon2"), ("umon3"), ("umon4"), ("umon5"),
        ("umon6"), ("umon7"), ("umon8"), ("umon9"), ("umon10"),
        /* Properties */
        ("DrlgType"), ("SubType"), ("SubTheme"), ("Depend"),
        ("Layer"), ("Pal"), ("LevelName"), ("LevelWarp"),
        ("SoundEnv"), ("Portal"), ("Position"), ("SaveMonsters"),
        NULL // DO NOT REMOVE !
};

char *txt_def_obj_req[] =
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

char *txt_def_objects_req[] =
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

char **glb_txt_req_ptr[RQ_MAX] = {NULL, NULL, NULL, NULL, NULL};

CONFIG_S glb_config;                    // global configuration datas
GLB_DS1EDIT_S glb_ds1edit;              // global datas of the editor
GLB_MPQ_S glb_mpq_struct[MAX_MPQ_FILE]; // global data of 1 mpq
DS1_S *glb_ds1 = NULL;                  // ds1 datas
DT1_S *glb_dt1 = NULL;                  // dt1 datas
/* Sizes must match the extern declarations in structs.h. They were defined
   here as string-literal-sized arrays (19/6/5 bytes) while every other
   translation unit saw [30]/[80]/[80] and wrote accordingly -- which glibc
   FORTIFY catches as a buffer overflow and aborts on. */
char glb_tiles_path[30] = "Data\\Global\\Tiles\\";  /* MPQ virtual path: backslash everywhere */
char glb_ds1edit_data_dir[80] = "assets" DS1_SEP_STR "editor" DS1_SEP_STR;
/* Scratch root for undo buffers. Deliberately not under the asset tree:
   assets/ is read-only content, this is churn. */
char glb_ds1edit_tmp_dir[80] = "tmp" DS1_SEP_STR;

// debug files
char *glb_path_lvltypes_mem = "Debug\\Editor.lvltypes.memory.bin";
char *glb_path_lvltypes_def = "Debug\\D2.lvltypes.headers.txt";
char *glb_path_lvlprest_mem = "Debug\\Editor.lvlprest.memory.bin";
char *glb_path_lvlprest_def = "Debug\\D2.lvlprest.headers.txt";
char *glb_path_obj_mem = "Debug\\Editor.obj.memory.bin";
char *glb_path_obj_def = "Debug\\Editor.obj.headers.txt";
char *glb_path_objects_mem = "Debug\\Editor.objects.memory.bin";
char *glb_path_objects_def = "Debug\\D2.objects.headers.txt";

// ==========================================================================
// near the start of the prog
// ==========================================================================
// Point each RQ_* slot at its column-requirement list.
//
// This lived inside ds1edit_init(), which the CLI skips -- so every CLI verb
// that parsed a table with column requirements hit a NULL glb_txt_req_ptr[]
// entry in txt_load() and segfaulted. Both paths call this now.
void ds1edit_init_txt_requirements(void)
{
   glb_txt_req_ptr[RQ_LVLTYPE]  = txt_def_lvltype_req;
   glb_txt_req_ptr[RQ_LVLPREST] = txt_def_lvlprest_req;
   glb_txt_req_ptr[RQ_OBJ]      = txt_def_obj_req;
   glb_txt_req_ptr[RQ_OBJECTS]  = txt_def_objects_req;
   glb_txt_req_ptr[RQ_LEVELS]   = txt_def_levels_req;
}

void ds1edit_init(void)
{
   FILE *out;
   static struct
   {
      char name[40];
      MODE_E idx;
   } cursor[MOD_MAX] = {
       {UI_DIR "cursor_t.png", MOD_T}, // tiles
       {UI_DIR "cursor_o.png", MOD_O}, // objects
       {UI_DIR "cursor_p.png", MOD_P}, // paths
       {UI_DIR "cursor_l.png", MOD_L}  // lights
   };
   int i, o;
   static int
       dir4[4] = {0, 1, 2, 3},
       dir8[8] = {4, 0, 5, 1, 6, 2, 7, 3},

       dir16[16] = {4, 8, 0, 9, 5, 10, 1, 11,
                    6, 12, 2, 13, 7, 14, 3, 15},

       dir32[32] = {4, 16, 8, 17, 0, 18, 9, 19,
                    5, 20, 10, 21, 1, 22, 11, 23,
                    6, 24, 12, 25, 2, 26, 13, 27,
                    7, 28, 14, 29, 3, 30, 15, 31},

       obj_sub_tile[5][5] = {
           {0, 2, 5, 9, 14},
           {1, 4, 8, 13, 18},
           {3, 7, 12, 17, 21},
           {6, 11, 16, 20, 23},
           {10, 15, 19, 22, 24}};
   char tmp[80];

   // zero mem
   printf("ds1edit_init()\n");
   memset(&glb_config, 0, sizeof(glb_config));
   memset(&glb_ds1edit, 0, sizeof(glb_ds1edit));
   glb_ds1edit.sidebar_width = 250;
   glb_ds1edit.props_panel_width = 360;
   props_panel_init();

   // allocate mem for DT1 & DS1
   i = sizeof(DS1_S) * DS1_MAX;
   printf("\nallocate %i bytes for glb_ds1[%i]\n", i, DS1_MAX);
   glb_ds1 = (DS1_S *)malloc(i);
   if (glb_ds1 == NULL)
   {
      sprintf(
          tmp,
          "ds1edit_init(), error.\n"
          "Can't allocate %i bytes for the glb_ds1[%i] table.",
          i, DS1_MAX);
      ds1edit_error(tmp);
   }
   memset(glb_ds1, 0, i);

   i = sizeof(DT1_S) * DT1_MAX;
   printf("allocate %i bytes for glb_dt1[%i]\n\n", i, DT1_MAX);
   glb_dt1 = (DT1_S *)malloc(i);
   if (glb_dt1 == NULL)
   {
      sprintf(
          tmp,
          "ds1edit_init(), error.\n"
          "Can't allocate %i bytes for the glb_dt1[%i] table.",
          i, DT1_MAX);
      ds1edit_error(tmp);
   }
   memset(glb_dt1, 0, i);

   // set the version
   glb_ds1edit.version_build = DS1EDIT_BUILD;
   glb_ds1edit.version_dll = "Allegro 5";
   strcpy(glb_ds1edit.version, DS1EDIT_BUILD);
   printf(".exe version : %s", COMPILER_NAME);
   printf(
       ", Build Date = %s, Build Mode = %s\n",
       glb_ds1edit.version,
       DS1EDIT_BUILD_MODE);
   // update the version info on disk
   sprintf(tmp, "%sversion", glb_ds1edit_data_dir);
   out = fopen(tmp, "wb");
   if (out != NULL)
   {
      sprintf(tmp, "Build Date = %s", glb_ds1edit.version);
      fwrite(tmp, strlen(tmp), 1, out);
      fclose(out);
   }

   for (i = 0; i < MAX_MPQ_FILE; i++)
   {
      memset(&glb_mpq_struct[i], 0, sizeof(GLB_MPQ_S));
      glb_mpq_struct[i].is_open = FALSE;
   }

   // mouse cursors
   for (i = 0; i < MOD_MAX; i++)
   {
      glb_ds1edit.mouse_cursor[i] =
          al_load_bitmap(cursor[i].name);
      if (glb_ds1edit.mouse_cursor[i] == NULL)
      {
         sprintf(
             tmp,
             "ds1edit_init(), error.\n"
             "Can't open the file \"%s\".",
             cursor[i].name);
         ds1edit_error(tmp);
      }
   }

   // txt
   ds1edit_init_txt_requirements();

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

   for (i = 0; i < 4; i++)
      glb_ds1edit.new_dir4[i] = dir4[i];

   for (i = 0; i < 8; i++)
      glb_ds1edit.new_dir8[i] = dir8[i];

   for (i = 0; i < 16; i++)
      glb_ds1edit.new_dir16[i] = dir16[i];

   for (i = 0; i < 32; i++)
      glb_ds1edit.new_dir32[i] = dir32[i];

   // for re-ordering sub-tile objects, from back to front
   for (i = 0; i < 5; i++)
      for (o = 0; o < 5; o++)
         glb_ds1edit.obj_sub_tile_order[i][o] = obj_sub_tile[i][o];

   // init the default values of the command line
   glb_ds1edit.cmd_line.ds1_filename = NULL;
   glb_ds1edit.cmd_line.ini_filename = NULL;
   glb_ds1edit.cmd_line.lvltype_id = -1;
   glb_ds1edit.cmd_line.lvlprest_def = -1;
   glb_ds1edit.cmd_line.resize_width = -1;
   glb_ds1edit.cmd_line.resize_height = -1;
   glb_ds1edit.cmd_line.force_pal_num = -1;
   glb_ds1edit.cmd_line.no_check_act = FALSE;
   glb_ds1edit.cmd_line.dt1_list_num = -1;
   glb_ds1edit.cmd_line.headless_mode = FALSE;
   glb_ds1edit.cmd_line.selftest_frames = 0;
   glb_ds1edit.cmd_line.selftest_shot = NULL;
   glb_ds1edit.cmd_line.switchbench_group = -1;
   glb_ds1edit.cmd_line.switchbench_count = 0;
   glb_ds1edit.cmd_line.headless_output = NULL;
   glb_ds1edit.cmd_line.area_name = NULL;
   glb_ds1edit.cmd_line.list_areas = FALSE;
   glb_ds1edit.cmd_line.list_areas_ext = FALSE;
   glb_ds1edit.cmd_line.list_files = FALSE;
   glb_ds1edit.cmd_line.audit_lvltypes = FALSE;
   glb_ds1edit.cmd_line.list_files_filter = NULL;
   glb_ds1edit.cmd_line.file_path = NULL;
   glb_ds1edit.cmd_line.export_asset_path = NULL;
   glb_ds1edit.cmd_line.export_asset_output_dir = NULL;
   glb_ds1edit.cmd_line.export_area_name = NULL;
   glb_ds1edit.cmd_line.export_area_output_dir = NULL;
   glb_ds1edit.cmd_line.export_prefix = NULL;
   glb_ds1edit.cmd_line.export_prefix_type = NULL;
   glb_ds1edit.cmd_line.export_prefix_output_dir = NULL;
   for (i = 0; i < DT1_IN_DS1_MAX; i++)
      glb_ds1edit.cmd_line.dt1_list_filename[i] = NULL;

   // 2nd row of infos
   glb_ds1edit.show_2nd_row = FALSE;

   // video pages
   glb_ds1edit.video_page_num = 0;
}

// ==========================================================================
UDWORD ds1edit_get_bitmap_size(ALLEGRO_BITMAP *bmp)
{
   UDWORD size = 0;
   int w, h;

   if (bmp == NULL)
      return 0;

   w = al_get_bitmap_width(bmp);
   h = al_get_bitmap_height(bmp);

   /* Allegro 5 bitmaps are always 32bpp internally */
   size += 4 * (w * h);

   return size;
}

// ==========================================================================
// RLE sprites no longer exist in Allegro 5 -- this is kept as a stub
// that treats the pointer as an ALLEGRO_BITMAP for callers that still
// reference it during the migration period.
UDWORD ds1edit_get_RLE_bitmap_size(ALLEGRO_BITMAP *bmp)
{
   if (bmp == NULL)
      return 0;

   return (UDWORD)(4 * al_get_bitmap_width(bmp) * al_get_bitmap_height(bmp));
}

// ==========================================================================
// Recreate the offscreen render targets after the display exists so Allegro
// can allocate them as display bitmaps instead of memory bitmaps.
void ds1edit_recreate_render_targets(void)
{
   ALLEGRO_BITMAP *new_big_screen_buff;
   ALLEGRO_BITMAP *new_screen_buff;
   int old_width;
   int old_height;
   char tmp[160];

   old_width = glb_config.screen.width;
   old_height = glb_config.screen.height;

   glb_config.screen.width += 600;
   glb_config.screen.height += 600;
   new_big_screen_buff = al_create_bitmap(
       glb_config.screen.width,
       glb_config.screen.height);
   if (new_big_screen_buff == NULL)
   {
      sprintf(tmp, "main(), error.\nCan't recreate big_screen_buff (%i*%i pixels).",
              glb_config.screen.width,
              glb_config.screen.height);
      ds1edit_error(tmp);
   }
   glb_config.screen.width = old_width;
   glb_config.screen.height = old_height;

   new_screen_buff = al_create_sub_bitmap(
       new_big_screen_buff,
       300,
       300,
       glb_config.screen.width,
       glb_config.screen.height);
   if (new_screen_buff == NULL)
   {
      al_destroy_bitmap(new_big_screen_buff);
      sprintf(tmp, "main(), error.\nCan't recreate sub-bitmap screen_buff (%i*%i pixels).",
              glb_config.screen.width,
              glb_config.screen.height);
      ds1edit_error(tmp);
   }

   if (glb_ds1edit.screen_buff != NULL)
      al_destroy_bitmap(glb_ds1edit.screen_buff);
   if (glb_ds1edit.big_screen_buff != NULL)
      al_destroy_bitmap(glb_ds1edit.big_screen_buff);

   glb_ds1edit.big_screen_buff = new_big_screen_buff;
   glb_ds1edit.screen_buff = new_screen_buff;
}

// ==========================================================================
// automatically called at the end, with the help of atexit()
void ds1edit_exit(void)
{
   int i, z, b;
   static int already_called = 0;

   /* Guard against being called twice (atexit + explicit) */
   if (already_called)
      return;
   already_called = 1;

   printf("\nds1edit_exit()\n");

   // Persist preferences (recent projects, last D2 install). Best-effort;
   // failures are logged but not fatal.
   if (prefs_save() == 0)
      fprintf(stderr, "prefs_save: failed\n");

   // Release the preset index (just a flat malloc; tables themselves live
   // in glb_ds1edit.*_buff and are freed elsewhere).
   mpq_index_destroy();

   /* Skip Allegro bitmap cleanup — al_destroy_bitmap is unsafe during shutdown
    * due to heap corruption. The OS reclaims all process memory on exit anyway.
    * We still close file handles and free non-bitmap allocations. */

   // close all mpq
   for (i = 0; i < MAX_MPQ_FILE; i++)
   {
      if (glb_mpq_struct[i].is_open != FALSE)
      {
         fprintf(stderr, "closing %s\n", glb_mpq_struct[i].file_name);
         fflush(stderr);

         glb_mpq = &glb_mpq_struct[i];
         mpq_batch_close();

         memset(&glb_mpq_struct[i], 0, sizeof(GLB_MPQ_S));
         glb_mpq_struct[i].is_open = FALSE;
      }
   }

   // free non-bitmap memory (skip al_destroy_bitmap — causes heap corruption
   // during shutdown; OS reclaims all process memory on exit)
   fprintf(stderr, "exit, memory free :\n");
   fflush(stderr);

   // config, mpq name
   fprintf(stderr, "   * config, mpq names...\n");
   fflush(stderr);
   if (glb_config.d2_install != NULL)
      free(glb_config.d2_install);
   if (glb_config.upscale_service_url != NULL)
      free(glb_config.upscale_service_url);
   for (i = 0; i < MAX_MPQ_FILE; i++)
   {
      if (glb_config.mpq_file[i] != NULL)
         free(glb_config.mpq_file[i]);
   }

   // config, mod directory
   fprintf(stderr, "   * config, mod directory name...\n");
   fflush(stderr);
   for (i = 0; i < MAX_MOD_DIR; i++)
   {
      if (glb_config.mod_dir[i] != NULL)
         free(glb_config.mod_dir[i]);
   }

   // palettes
   fprintf(stderr, "   * palettes...\n");
   fflush(stderr);
   for (i = 0; i < ACT_MAX; i++)
   {
      if (glb_ds1edit.d2_pal[i] != NULL)
      {
         free(glb_ds1edit.d2_pal[i]);
         glb_ds1edit.d2_pal[i] = NULL;
         glb_ds1edit.pal_size[i] = 0;
      }
   }

   // skip DT1/DS1/object/bitmap cleanup (contains al_destroy_bitmap calls)
   // undo buffers — close file handles
   fprintf(stderr, "   * undo buffers...\n");
   fflush(stderr);
   if (glb_ds1 != NULL)
      undo_exit();

   // .txt buffers
   fprintf(stderr, "   * .txt buffers ...\n");
   fflush(stderr);
   if (glb_ds1edit.lvltypes_buff != NULL)
      glb_ds1edit.lvltypes_buff = txt_destroy(glb_ds1edit.lvltypes_buff);
   if (glb_ds1edit.lvlprest_buff != NULL)
      glb_ds1edit.lvlprest_buff = txt_destroy(glb_ds1edit.lvlprest_buff);
   if (glb_ds1edit.obj_buff != NULL)
      glb_ds1edit.obj_buff = txt_destroy(glb_ds1edit.obj_buff);
   if (glb_ds1edit.levels_buff != NULL)
      glb_ds1edit.levels_buff = txt_destroy(glb_ds1edit.levels_buff);
   area_browser_destroy();

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
      for (i = 0; i < DS1_MAX; i++)
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
   printf("upscale_enabled         = %s\n", glb_config.upscale_enabled ? "YES" : "NO");
   printf("upscale_service_url     = %s\n", glb_config.upscale_service_url ? glb_config.upscale_service_url : "");
   printf("fullscreen              = %s\n", glb_config.fullscreen ? "YES" : "NO");
   printf("screen_width            = %i\n", glb_config.screen.width);
   printf("screen_height           = %i\n", glb_config.screen.height);
   printf("refresh_rate            = %i\n", glb_config.screen.refresh);
   printf("keyb_scroll_x           = %i\n", glb_config.scroll.keyb.x);
   printf("keyb_scroll_y           = %i\n", glb_config.scroll.keyb.y);
   printf("mouse_scroll_x          = %i\n", glb_config.scroll.mouse.x);
   printf("mouse_scroll_y          = %i\n", glb_config.scroll.mouse.y);
   printf("edit_scroll_x           = %i\n", glb_config.scroll.edit.x);
   printf("edit_scroll_y           = %i\n", glb_config.scroll.edit.y);
   printf("obj_edit_scroll         = %i\n", glb_config.scroll.obj_edit);
   printf("gamma_correction        = %s\n", glb_gamma_str[glb_config.gamma].str);
   printf("only_normal_type2       = %s\n", glb_config.normal_type2 ? "YES" : "NO");
   printf("always_max_layers       = %s\n", glb_config.always_max_layers ? "YES" : "NO");
   printf("stretch_sprites         = %s\n", glb_config.stretch_sprites ? "YES" : "NO");
   printf("winobj_can_scroll_keyb  = %s\n", glb_config.winobj_scroll_keyb ? "YES" : "NO");
   printf("winobj_can_scroll_mouse = %s\n", glb_config.winobj_scroll_mouse ? "YES" : "NO");

   printf("center_zoom             = ");
   switch (glb_config.center_zoom)
   {
   case -1:
      printf("NO_CHANGE\n");
      break;
   case ZM_21:
      printf("2:1\n");
      break;
   case ZM_11:
      printf("1:1\n");
      break;
   case ZM_12:
      printf("1:2\n");
      break;
   case ZM_14:
      printf("1:4\n");
      break;
   case ZM_18:
      printf("1:8\n");
      break;
   case ZM_116:
      printf("1:16\n");
      break;
   default:
      printf("?\n");
      break;
   }

   printf("default_zoom            = ");
   switch (glb_config.default_zoom)
   {
   case ZM_21:
      printf("2:1\n");
      break;
   case ZM_11:
      printf("1:1\n");
      break;
   case ZM_12:
      printf("1:2\n");
      break;
   case ZM_14:
      printf("1:4\n");
      break;
   case ZM_18:
      printf("1:8\n");
      break;
   case ZM_116:
      printf("1:16\n");
      break;
   default:
      printf("1:1\n");
      break;
   }
   printf("nb_type1_per_act        = %i\n", glb_config.nb_type1_per_act);
   printf("nb_type2_per_act        = %i\n", glb_config.nb_type2_per_act);
   printf("ds1_saved_minimize      = %s\n", glb_config.minimize_ds1 ? "YES" : "NO");
   printf("lower_speed_zoom_out    = %s\n", glb_config.lower_speed_zoom_out ? "YES" : "NO");
   printf("workspace_enable        = %s\n", glb_config.workspace_enable ? "YES" : "NO");
   printf("\n");
}

// Timer callbacks removed -- Allegro 5 uses event-based timers.
// Tick and FPS counting is now handled in the main event loop.

// ==========================================================================
// open all mpq
void ds1edit_open_all_mpq(void)
{
   int i;

   printf("ds1edit_open_all_mpq()\n");
   for (i = 0; i < MAX_MPQ_FILE; i++)
   {
      if (glb_config.mpq_file[i] != NULL)
      {
         fprintf(stdout, "opening mpq %i : %s\n", i, glb_config.mpq_file[i]);
         fprintf(stderr, "opening %s\n", glb_config.mpq_file[i]);
         fflush(stdout);
         fflush(stderr);
         glb_mpq = &glb_mpq_struct[i];
         mpq_batch_open(glb_config.mpq_file[i]);
      }
   }
}

// ==========================================================================
// load palettes of the 5 acts from disk, else from mpq
void ds1edit_load_palettes(void)
{
   int i, entry;
   char palname[80], tmp[150];

   fprintf(stderr, "loading palettes");
   fprintf(stdout, "\nloading palettes\n");
   fflush(stderr);
   fflush(stdout);

   for (i = 0; i < ACT_MAX; i++)
   {
      glb_ds1edit.pal_loaded[i] = TRUE;
      // first checking on disk
      if (misc_load_pal_from_disk(i) == FALSE)
      {
         // not already on disk
         glb_ds1edit.pal_loaded[i] = FALSE;

         // make full path
         sprintf(palname, "Data\\Global\\Palette\\Act%i\\Pal.pl2", i + 1);

         // load the palette
         printf("want to read a palette from mpq : %s\n", palname);
         entry = misc_load_mpq_file(
             palname,
             (char **)&glb_ds1edit.d2_pal[i],
             &glb_ds1edit.pal_size[i],
             TRUE);
         if (entry == -1)
         {
            sprintf(
                tmp,
                "ds1edit_load_palettes() :\n"
                "File \"%s\" not found.",
                palname);
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
// Create the display, settling for less if the preferred mode is refused.
//
// A single attempt was fatal, which is harsh when a host simply declines one
// fullscreen mode or refresh rate -- macOS most of all. Try what the config
// asked for, then a plain resizable window, then a fixed one.
static ALLEGRO_DISPLAY * ds1edit_try_create_display(void)
{
   ALLEGRO_DISPLAY *d = NULL;
   int w = glb_config.screen.width;
   int h = glb_config.screen.height;

   /* The editor redraws unconditionally every iteration. Without vsync that
      is whatever the GPU will give -- 120 fps of identical frames on an idle
      map. SUGGEST rather than REQUIRE: a driver that will not do it should
      still give us a display. */
   al_set_new_display_option(ALLEGRO_VSYNC, 1, ALLEGRO_SUGGEST);

   if (glb_config.fullscreen == TRUE)
   {
      al_set_new_display_flags(ALLEGRO_FULLSCREEN);
      if (glb_config.screen.refresh > 0)
         al_set_new_display_refresh_rate(glb_config.screen.refresh);
      d = al_create_display(w, h);
      if (d != NULL)
         return d;
      fprintf(stderr, "display: fullscreen %ix%i refused, trying a window\n", w, h);
   }

   /* Drop any refresh-rate demand along with fullscreen -- it is the other
      half of a mode the host has already turned down. */
   al_set_new_display_refresh_rate(0);

   al_set_new_display_flags(ALLEGRO_WINDOWED | ALLEGRO_RESIZABLE);
   d = al_create_display(w, h);
   if (d != NULL)
      return d;
   fprintf(stderr, "display: resizable %ix%i refused, trying a fixed window\n", w, h);

   al_set_new_display_flags(ALLEGRO_WINDOWED);
   return al_create_display(w, h);
}


// ==========================================================================
// as expected, the start of the prog
//
// On macOS the real body runs under al_run_main() so that Allegro owns the
// application thread; see the note in docs/MACOS_HANDOFF.md. Everywhere else
// this is an ordinary main().
#ifdef __APPLE__
static int ds1edit_main(int argc, char *argv[]);

int main(int argc, char *argv[])
{
   return al_run_main(argc, argv, ds1edit_main);
}

static int ds1edit_main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
   int i, mpq_num = 0, mod_num = 0, ds1_idx = 0;
   char *ininame = DS1EDIT_INI_NAME;
   static char tmp[512];
   static char tmp2[512];

   // init
#ifdef __APPLE__
   /* Launched from ds1edit.app the CWD is "/", and every resource path below
      is relative. Do this first -- it has to precede the INI read. */
   ds1_mac_fix_working_directory();
#endif
   srand(time(NULL));
   if (!al_init())
      ds1edit_error("main(), error.\nCan't initialize Allegro 5.");

   al_init_image_addon();
   al_init_font_addon();
   al_init_primitives_addon();
   a5_font = al_create_builtin_font();

   if (!al_install_keyboard())
      ds1edit_error("main(), error.\nCan't install the Keyboard Handler.");

   /* CLI mode: if argv[1] is a known verb, run headless and exit before
    * any of the GUI-affecting startup machinery (display creation,
    * cursor loading, screen buffers, etc.). The CLI does its own
    * minimal init; we only pre-initialised Allegro itself + the addons
    * the CLI's MPQ + asset code needs.
    *
    * CRITICAL: force memory bitmaps before the verb runs. Default flags
    * on Windows ask for ALLEGRO_VIDEO_BITMAP, which requires a display.
    * No display + video bitmap = al_create_bitmap returns NULL, which
    * silently fails the DCC decode path. The GUI flow sets this flag
    * a few lines below; we hoist it for CLI mode. */
   if (cli_is_verb(argc, argv))
   {
      int rc;
      al_set_new_bitmap_flags(ALLEGRO_MEMORY_BITMAP);
      rc = cli_run(argc, argv);
      return rc;
   }

   if (atexit(ds1edit_exit) != 0)
      ds1edit_error("main(), error.\nCan't install the 'atexit' Handler.");

   // Use memory bitmaps until display is created
   // TODO: move display creation earlier for GPU-accelerated bitmaps (FPS fix)
   al_set_new_bitmap_flags(ALLEGRO_MEMORY_BITMAP);

   ds1edit_init();

   // Allegro 5 version info
   printf("\n[allegro]\n");
   printf("allegro_version     = %i.%i.%i\n",
          al_get_allegro_version() >> 24,
          (al_get_allegro_version() >> 16) & 0xFF,
          (al_get_allegro_version() >> 8) & 0xFF);
   printf("\n");

   // check data\tmp directory
   sprintf(tmp, "%s" DS1_SEP_STR ".", glb_ds1edit_tmp_dir);
   if (a5_file_exists(tmp) == 0)
   {
      // create tmp directory
      sprintf(tmp, "%s", glb_ds1edit_tmp_dir);
      if (strlen(tmp))
         tmp[strlen(tmp) - 1] = 0;
      if (DS1_MKDIR(tmp) != 0)
      {
         // re-use the tmp var for a different string
         sprintf(
             tmp,
             "main(), error.\n"
             "Can't create directory \"%s\".",
             glb_ds1edit_tmp_dir);
         tmp[strlen(tmp) - 1] = 0;
         ds1edit_error(tmp);
      }
   }

   // check if the config file exists
   sprintf(tmp, DS1EDIT_INI_NAME);
   if (a5_file_exists(tmp) == 0)
   {
      ini_create(tmp);
      printf(
          "main(), error.\n"
          "There was no 'Ds1edit.ini' file in the Ds1edit directory.\n"
          "A new one with default values was created.\n"
          "Edit it to fit your configuration, then restart the program.\n");
      exit(DS1ERR_INICREATE);
   }

   // init (config)
   ini_read(ininame);

   // global preferences (remembered across sessions)
   prefs_load();

   // If the INI didn't specify a D2 install, fall back to the last one the
   // editor used. Registry / common-path detection still runs as a further
   // fallback inside d2install_resolve_mpqs().
   if ((glb_config.d2_install == NULL || glb_config.d2_install[0] == 0) &&
       glb_prefs.last_d2_install[0] != 0)
   {
      size_t len = strlen(glb_prefs.last_d2_install);
      char *buf = (char *)malloc(len + 1);
      if (buf != NULL)
      {
         memcpy(buf, glb_prefs.last_d2_install, len + 1);
         glb_config.d2_install = buf;
         fprintf(stdout,
                 "d2install: seeded from preferences <%s>\n",
                 glb_config.d2_install);
      }
   }

   // Fill empty MPQ slots from the resolved install path (explicit INI
   // entries always win).
   d2install_resolve_mpqs();

   // Record the install we landed on back into preferences for next session.
   if (glb_config.d2_install != NULL && glb_config.d2_install[0] != 0)
   {
      strncpy(glb_prefs.last_d2_install, glb_config.d2_install,
              sizeof(glb_prefs.last_d2_install) - 1);
      glb_prefs.last_d2_install[sizeof(glb_prefs.last_d2_install) - 1] = 0;
   }

   ds1edit_debug();

   // optional data_dir override from ini (default is "Data\")
   {
      const char *data_dir_val = al_get_config_value(a5_config, "", "data_dir");
      if (data_dir_val != NULL && data_dir_val[0] != '\0')
      {
         strncpy(glb_ds1edit_data_dir, data_dir_val, sizeof(glb_ds1edit_data_dir) - 2);
         glb_ds1edit_data_dir[sizeof(glb_ds1edit_data_dir) - 2] = '\0';
         // ensure trailing backslash
         {
            int len = strlen(glb_ds1edit_data_dir);
            if (len > 0 && glb_ds1edit_data_dir[len - 1] != '\\' && glb_ds1edit_data_dir[len - 1] != '/')
            {
               glb_ds1edit_data_dir[len] = DS1_SEP;
               glb_ds1edit_data_dir[len + 1] = '\0';
            }
         }
         printf("data_dir                = %s\n", glb_ds1edit_data_dir);
      }
   }

   // check mod directory
   mod_num = 0;
   if (glb_config.mod_dir[0] != NULL)
   {
      snprintf(tmp, sizeof(tmp), "%s%s.", glb_config.mod_dir[0], DS1_SEP_STR);
      ds1_path_normalize(tmp);
      if (a5_file_exists(tmp) == 0)
      {
         sprintf(
             tmp2,
             "main(), warning.\n"
             "Can't find the Mod Directory defined in 'Ds1edit.ini' :\n"
             "%s",
             tmp);
         printf("%s\n", tmp2);
      }
      else
         mod_num = 1;
   }

   for (i = 0; i < MAX_MPQ_FILE; i++)
   {
      if (glb_config.mpq_file[i] != NULL)
      {
         if (a5_file_exists(glb_config.mpq_file[i]) == 0)
         {
            sprintf(
                tmp2,
                "main(), warning.\n"
                "Can't find this MPQ defined in 'Ds1edit.ini' :\n"
                "%s",
                glb_config.mpq_file[i]);
            printf("%s\n", tmp2);
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
   }
   else if (mpq_num < 4)
      printf("warning : not all the 4 mpq have been found\n");

   // gamma correction
   glb_ds1edit.cur_gamma = glb_config.gamma;
   misc_read_gamma();

   // preview window update
   glb_ds1edit.win_preview.x0 = 0;
   glb_ds1edit.win_preview.y0 = 0;
   glb_ds1edit.win_preview.w = glb_config.screen.width;
   glb_ds1edit.win_preview.h = glb_config.screen.height;

   // edit window
   wedit_load_window_images();
   wedit_make_2nd_buttons();
   misc_walkable_tile_info();

   // screen buffer
   // we're making a big buffer, with 300 pixels on each 4 borders,
   // and then we'll make the true screen buffer be a sub-bitmap of this buffer
   // this is to avoid potential problems with clipings, especially when using
   // the functions from gfx_custom.c

   glb_config.screen.width += 600;
   glb_config.screen.height += 600;
   glb_ds1edit.big_screen_buff = al_create_bitmap(
       glb_config.screen.width,
       glb_config.screen.height);
   if (glb_ds1edit.big_screen_buff == NULL)
   {
      sprintf(tmp, "main(), error.\nCan't create big_screen_buff (%i*%i pixels).",
              glb_config.screen.width,
              glb_config.screen.height);
      ds1edit_error(tmp);
   }
   glb_config.screen.width -= 600;
   glb_config.screen.height -= 600;

   glb_ds1edit.screen_buff = al_create_sub_bitmap(
       glb_ds1edit.big_screen_buff,
       300,
       300,
       glb_config.screen.width,
       glb_config.screen.height);
   if (glb_ds1edit.screen_buff == NULL)
   {
      sprintf(tmp, "main(), error.\nCan't create sub-bitmap screen_buff (%i*%i pixels).",
              glb_config.screen.width,
              glb_config.screen.height);
      ds1edit_error(tmp);
   }

   // open all mpq
   ds1edit_open_all_mpq();

   // load palettes from disk, else from mpq
   ds1edit_load_palettes();

   // set default palette for tile rendering (Act 1)
   // this ensures a5_putpixel/pal_color use correct colors during tile loading
   a5_current_palette = &glb_ds1edit.vga_pal[0];

   // parse the command line
   if (misc_cmd_line_parse(argc, argv))
      ds1edit_error("main(), error.\nProblem in the command line.");

   /* A non-interactive render has to be reproducible.
    *
    * Objects start on a random animation frame -- frame_delta = rand()%256 in
    * ds1.c and objects.c, cur_frame = rand()%fpd in cof.c -- so with the
    * time-based seed above every torch and NPC lands somewhere different on
    * each run, and two renders of the same map differ by around 1% of their
    * pixels. That is enough to drown out a real regression in a golden
    * comparison. Re-seed with a fixed value now that we know nobody is
    * watching the animation play. */
   if ((glb_ds1edit.cmd_line.headless_mode == TRUE) ||
       (glb_ds1edit.cmd_line.selftest_frames > 0))
      srand(DS1EDIT_DETERMINISTIC_SEED);

   // create debug directory if necessary
   if (glb_ds1edit.cmd_line.debug_mode == TRUE)
      DS1_MKDIR("Debug");

   // objects.txt
   fprintf(stderr, "reading objects.txt...");
   read_objects_txt();
   fprintf(stderr, "done\n");

   // obj.txt
   fprintf(stderr, "reading Obj.txt...");
   read_obj_txt();
   fprintf(stderr, "done\n");

   // build the preset index from LvlPrest + Levels + LvlTypes (eager load +
   // pre-joined rows so the preset picker and DS1->preset resolution are
   // both O(1) lookups instead of re-scanning tables per call).
   fprintf(stderr, "building preset index...");
   if (mpq_index_build() < 0)
      fprintf(stderr, "failed\n");
   else
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
             glb_ds1edit.cmd_line.resize_height);
      }
      glb_ds1edit.has_loaded_ds1 = TRUE;
   }
   else if (glb_ds1edit.cmd_line.ini_filename != NULL)
   {
      // .ini

      // 2nd row of infos
      glb_ds1edit.show_2nd_row = TRUE;

      // list of ds1 to open
      misc_open_several_ds1(argv[1]);
      glb_ds1edit.has_loaded_ds1 = TRUE;
   }
   else if (glb_ds1edit.cmd_line.list_areas == TRUE ||
            glb_ds1edit.cmd_line.list_areas_ext == TRUE ||
            glb_ds1edit.cmd_line.list_files == TRUE)
   {
      // --list-areas, --list-areas-ext, or --list-files
      if (area_browser_init() != 0)
         ds1edit_error("main(), error.\nFailed to load Excel data for area browser.");
      if (glb_ds1edit.cmd_line.list_files == TRUE)
         area_browser_list_files(glb_ds1edit.cmd_line.list_files_filter);
      else if (glb_ds1edit.cmd_line.list_areas_ext == TRUE)
         area_browser_list_ext();
      else
         area_browser_list();
      exit(0);
   }
   else if (glb_ds1edit.cmd_line.file_path != NULL)
   {
      // --file "path/to/file.ds1" : load single DS1 from Excel data
      if (area_browser_init() != 0)
         ds1edit_error("main(), error.\nFailed to load Excel data for area browser.");

      if (area_browser_open_by_file(glb_ds1edit.cmd_line.file_path) < 0)
      {
         sprintf(tmp, "main(), error.\nDS1 file '%s' not found in Excel data.",
                 glb_ds1edit.cmd_line.file_path);
         ds1edit_error(tmp);
      }
      glb_ds1edit.has_loaded_ds1 = TRUE;
   }
   else if (glb_ds1edit.cmd_line.export_area_name != NULL)
   {
      int group_idx;
      int pal_idx;
      int exported_count;

      if (area_browser_init() != 0)
      {
         fprintf(stderr, "failed to initialize area browser for export\n");
         return DS1ERR_CMDLINE;
      }

      if (area_browser_open_by_name(glb_ds1edit.cmd_line.export_area_name) < 0)
      {
         fprintf(stderr, "area export failed: area '%s' not found\n",
                 glb_ds1edit.cmd_line.export_area_name);
         return DS1ERR_CMDLINE;
      }

      group_idx = glb_ds1edit.area_browser.selected_group;
      if (group_idx < 0)
      {
         fprintf(stderr, "area export failed: no area selected\n");
         return DS1ERR_CMDLINE;
      }

      pal_idx = palette_resolve_index(glb_ds1edit.area_browser.groups[group_idx].act,
                                      0);
      if (pal_idx < 0 || pal_idx > 4)
      {
         pal_idx = (glb_ds1edit.cmd_line.force_pal_num == -1)
                       ? 0
                       : (glb_ds1edit.cmd_line.force_pal_num - 1);
      }
      a5_current_palette = &glb_ds1edit.vga_pal[pal_idx];

      exported_count = asset_export_area_group_png(
          &glb_ds1edit.area_browser.groups[group_idx],
          glb_ds1edit.cmd_line.export_area_output_dir);
      if (exported_count <= 0)
      {
         fprintf(stderr, "area export failed for %s\n",
                 glb_ds1edit.cmd_line.export_area_name);
         return DS1ERR_CMDLINE;
      }

      printf("exported %i PNG(s) for area %s into %s\n",
             exported_count,
             glb_ds1edit.cmd_line.export_area_name,
             glb_ds1edit.cmd_line.export_area_output_dir);
      return DS1ERR_OK;
   }
   else if (glb_ds1edit.cmd_line.export_prefix != NULL)
   {
      int pal_idx;
      int exported_count;

      pal_idx = (glb_ds1edit.cmd_line.force_pal_num == -1)
                    ? 0
                    : (glb_ds1edit.cmd_line.force_pal_num - 1);
      a5_current_palette = &glb_ds1edit.vga_pal[pal_idx];

      exported_count = asset_export_prefix_png(
          glb_ds1edit.cmd_line.export_prefix,
          glb_ds1edit.cmd_line.export_prefix_type,
          glb_ds1edit.cmd_line.export_prefix_output_dir);
      if (exported_count <= 0)
      {
         fprintf(stderr, "prefix asset export failed for %s (%s)\n",
                 glb_ds1edit.cmd_line.export_prefix,
                 glb_ds1edit.cmd_line.export_prefix_type);
         return DS1ERR_CMDLINE;
      }

      printf("exported %i PNG(s) from prefix %s [%s] into %s\n",
             exported_count,
             glb_ds1edit.cmd_line.export_prefix,
             glb_ds1edit.cmd_line.export_prefix_type,
             glb_ds1edit.cmd_line.export_prefix_output_dir);
      return DS1ERR_OK;
   }
   else if (glb_ds1edit.cmd_line.export_asset_path != NULL)
   {
      int pal_idx;
      int exported_count;

      pal_idx = (glb_ds1edit.cmd_line.force_pal_num == -1)
                    ? 0
                    : (glb_ds1edit.cmd_line.force_pal_num - 1);
      a5_current_palette = &glb_ds1edit.vga_pal[pal_idx];

      exported_count = asset_export_png(
          glb_ds1edit.cmd_line.export_asset_path,
          glb_ds1edit.cmd_line.export_asset_output_dir);
      if (exported_count <= 0)
      {
         fprintf(stderr, "asset export failed for %s\n",
                 glb_ds1edit.cmd_line.export_asset_path);
         return DS1ERR_CMDLINE;
      }

      printf("exported %i PNG(s) from %s into %s\n",
             exported_count,
             glb_ds1edit.cmd_line.export_asset_path,
             glb_ds1edit.cmd_line.export_asset_output_dir);
      return DS1ERR_OK;
   }
   else if (glb_ds1edit.cmd_line.audit_lvltypes == TRUE)
   {
      int mismatch_count;

      mismatch_count = area_browser_audit_lvltypes(stdout);
      if (mismatch_count < 0)
      {
         fprintf(stderr, "LvlTypes audit failed\n");
         return DS1ERR_CMDLINE;
      }

      if (mismatch_count == 0)
         printf("LvlTypes audit: no Name/Act mismatches found\n");
      else
         printf("LvlTypes audit: %d Name/Act mismatches found\n", mismatch_count);

      return DS1ERR_OK;
   }
   else if (glb_ds1edit.cmd_line.area_name != NULL)
   {
      // --area "Act X - Name" : load from Excel data
      if (area_browser_init() != 0)
         ds1edit_error("main(), error.\nFailed to load Excel data for area browser.");

      if (area_browser_open_by_name(glb_ds1edit.cmd_line.area_name) < 0)
      {
         sprintf(tmp, "main(), error.\nArea '%s' not found in Excel data.",
                 glb_ds1edit.cmd_line.area_name);
         ds1edit_error(tmp);
      }
      glb_ds1edit.has_loaded_ds1 = TRUE;
   }
   else
   {
      // no arguments — open editor with sidebar visible, no map loaded
      glb_ds1edit.sidebar_visible = TRUE;
      if (area_browser_init() != 0)
         ds1edit_error("main(), error.\nFailed to load Excel data for area browser.");
   }

   /* (Full-screen browser removed — sidebar is used instead) */

   // syntaxe of the command line
   printf("============================================================\n");
   /* This gate used to test argc rather than what the parse produced, so an
      .ini plus any flag ("ds1edit act5_town.ini --selftest") counted three
      arguments, matched neither the >=4 nor the ==2 case, and was rejected as
      a syntax error. Ask the parser what it found instead. */
   if (glb_ds1edit.cmd_line.ds1_filename != NULL || argc >= 4)
   {
   }
   else if (glb_ds1edit.cmd_line.ini_filename != NULL)
   {
   }
   else if (glb_ds1edit.cmd_line.area_name != NULL ||
            glb_ds1edit.cmd_line.export_area_name != NULL ||
            glb_ds1edit.cmd_line.export_prefix != NULL ||
            glb_ds1edit.cmd_line.list_areas == TRUE ||
            glb_ds1edit.cmd_line.list_areas_ext == TRUE ||
            glb_ds1edit.cmd_line.list_files == TRUE ||
            glb_ds1edit.cmd_line.file_path != NULL ||
            argc == 1) /* no args = GUI browser mode */
   {
   }
   else // syntax error
   {
      printf("syntaxe 1 : ds1edit <file.ds1> <lvlTypes.txt Id> <lvlPrest.txt Def> [options]\n");
      printf("syntaxe 1b : ds1edit --export-asset <file.dt1|file.dcc|file.dc6> <output-dir> [options]\n");
      printf("syntaxe 1c : ds1edit --export-area-assets \"Act X - Name\" <output-dir> [options]\n");
      printf("syntaxe 1d : ds1edit --export-prefix-assets <virtual-prefix> <dt1|dc6|dcc|all> <output-dir> [options]\n");
      printf("syntaxe 2 : ds1edit <file.ini>\n"
             "\n"
             "   file.ini in syntaxe 2 is a text file, each line for 1 ds1 to load,\n"
             "   3 elements : <lvlTypes.txt Id> <lvlPrest.txt Def> <file.ds1>\n");
      printf(
          "main(), error.\n"
          "This program needs to get parameters by the command-line.\n"
          "\n"
          "Check the README.txt to know how to make .bat files,\n"
          "or use a front-end tool like \"ds1edit Loader\" :\n" WINDS1EDIT_GUI_LOADER_LINK "\n");
      exit(DS1ERR_CMDLINE);
   }
   printf("============================================================\n");

   // animdata, animations, and colormaps — only if DS1 files are loaded
   if (glb_ds1edit.has_loaded_ds1)
   {
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
   }

   // headless mode : render one frame and save to file, then exit
   if (glb_ds1edit.cmd_line.headless_mode == TRUE)
   {
      ALLEGRO_BITMAP *old_screen_buff;
      int pal_idx;

      printf("headless mode : rendering to \"%s\"...\n", glb_ds1edit.cmd_line.headless_output);
      fflush(stdout);

      // determine palette
      if (glb_ds1edit.cmd_line.force_pal_num == -1)
         pal_idx = palette_resolve_index(glb_ds1[ds1_idx].txt_act,
                                         glb_ds1[ds1_idx].act);
      else
         pal_idx = glb_ds1edit.cmd_line.force_pal_num - 1;

      a5_current_palette = &glb_ds1edit.vga_pal[pal_idx];
      dt1_rebuild_bitmaps_from_cache(a5_current_palette);

      // render complete map
      old_screen_buff = glb_ds1edit.screen_buff;
      if (wpreview_draw_tiles_big_screenshot(ds1_idx) == 0)
      {
         al_save_bitmap(glb_ds1edit.cmd_line.headless_output,
                        glb_ds1edit.screen_buff);
         printf("headless mode : saved %i x %i screenshot\n",
                al_get_bitmap_width(glb_ds1edit.screen_buff),
                al_get_bitmap_height(glb_ds1edit.screen_buff));
         al_destroy_bitmap(glb_ds1edit.screen_buff);
      }
      else
      {
         printf("headless mode : render failed (empty map?)\n");
      }
      glb_ds1edit.screen_buff = old_screen_buff;

      fflush(stdout);
      fflush(stderr);
      return DS1ERR_OK;
   }

   // start
   fflush(stdout);
   fflush(stderr);

   // display setup (skip if already created by the area browser)
   if (a5_display == NULL)
   {
      a5_display = ds1edit_try_create_display();
      if (a5_display == NULL)
      {
         sprintf(
             tmp,
             "main(), error.\nCan't create display (%i*%i %s).",
             glb_config.screen.width,
             glb_config.screen.height,
             glb_config.fullscreen ? "Fullscreen" : "Windowed");
         ds1edit_error(tmp);
      }
   }

   sprintf(
       tmp,
       "DS1 Editor v%s (%s), Allegro %i.%i.%i",
       DS1EDIT_VERSION_STR,
       DS1EDIT_BUILD_MODE,
       al_get_allegro_version() >> 24,
       (al_get_allegro_version() >> 16) & 0xFF,
       (al_get_allegro_version() >> 8) & 0xFF);
   al_set_window_title(a5_display, tmp);

   // Now that a display exists, promote the hot render surfaces and cached
   // DT1 tiles to display bitmaps so repeated tile blits stay on the GPU.
   al_set_new_bitmap_flags(ALLEGRO_VIDEO_BITMAP);
   ds1edit_recreate_render_targets();
   if (a5_current_palette != NULL)
      dt1_rebuild_bitmaps_from_cache(a5_current_palette);

   // Promote animation bitmaps (DCC/DC6 object sprites) from MEMORY to VIDEO.
   // anim_update_gfx() ran before display existed, so all sprites are memory bitmaps.
   {
      int oi, li, fi;
      int promoted = 0, total = 0;
      for (oi = 0; oi < glb_ds1edit.obj_desc_num; oi++)
      {
         COF_S *cof = glb_ds1edit.obj_desc[oi].cof;
         if (cof == NULL)
            continue;
         for (li = 0; li < COMPOSIT_NB; li++)
         {
            LAY_INF_S *lay = &cof->lay_inf[li];
            if (lay->bmp == NULL)
               continue;
            for (fi = 0; fi < lay->bmp_num; fi++)
            {
               ALLEGRO_BITMAP *old_bmp = lay->bmp[fi];
               if (old_bmp == NULL)
                  continue;
               total++;
               if (al_get_bitmap_flags(old_bmp) & ALLEGRO_MEMORY_BITMAP)
               {
                  ALLEGRO_BITMAP *new_bmp = al_clone_bitmap(old_bmp);
                  if (new_bmp != NULL)
                  {
                     al_destroy_bitmap(old_bmp);
                     lay->bmp[fi] = new_bmp;
                     promoted++;
                  }
               }
            }
         }
      }
   }

   // mouse (skip if already installed by area browser)
   if (!al_is_mouse_installed())
   {
#ifdef __APPLE__
      /* Allegro's macOS mouse driver counts buttons by enumerating HID
         devices, and TCC hides every input device from a process without
         Input Monitoring -- so al_install_mouse() fails on a machine whose
         trackpad plainly works. Ask first, and if the answer is no, say what
         to do about it rather than reporting a driver failure. */
      int mac_hid_ok = ds1_mac_request_input_monitoring();
#endif
      if (!al_install_mouse())
      {
#ifdef __APPLE__
         if (!mac_hid_ok)
         {
            sprintf(
                tmp,
                "main(), error.\n"
                "Can't install the Mouse handler.\n\n"
                "macOS is withholding Input Monitoring from this process, so\n"
                "Allegro cannot see any mouse. Grant it in System Settings >\n"
                "Privacy & Security > Input Monitoring (add ds1edit.app with\n"
                "the + button if it is not listed), then start the editor\n"
                "again -- a new grant does not reach a running process.");
            ds1edit_error(tmp);
         }
#endif
         sprintf(
             tmp,
             "main(), error.\nCan't install the Mouse handler.");
         ds1edit_error(tmp);
      }
   }

   // event queue
   a5_event_queue = al_create_event_queue();
   al_register_event_source(a5_event_queue, al_get_keyboard_event_source());
   al_register_event_source(a5_event_queue, al_get_mouse_event_source());
   al_register_event_source(a5_event_queue, al_get_display_event_source(a5_display));

   /* Edge-detected input. Everything that reads a key or a button goes
      through this; see src/ui/input.h. */
   input_init();

   /* Promote the per-mode cursor artwork to real OS cursors. Needs the
      display, so it cannot happen where the bitmaps are loaded. */
   for (i = 0; i < MOD_MAX; i++)
   {
      glb_ds1edit.hw_cursor[i] = NULL;
      if (glb_ds1edit.mouse_cursor[i] != NULL)
      {
         glb_ds1edit.hw_cursor[i] =
             al_create_mouse_cursor(glb_ds1edit.mouse_cursor[i], 1, 1);
      }
   }

   // timers (Allegro 5 event-based timers replace the old interrupt callbacks)
   a5_tick_timer = al_create_timer(1.0 / 25.0);
   al_register_event_source(a5_event_queue, al_get_timer_event_source(a5_tick_timer));
   al_start_timer(a5_tick_timer);

   a5_fps_timer = al_create_timer(1.0);
   al_register_event_source(a5_event_queue, al_get_timer_event_source(a5_fps_timer));
   al_start_timer(a5_fps_timer);

   if (glb_ds1edit.has_loaded_ds1)
   {
      glb_ds1edit.win_preview.x0 = glb_ds1[ds1_idx].own_wpreview.x0;
      glb_ds1edit.win_preview.y0 = glb_ds1[ds1_idx].own_wpreview.y0;
      glb_ds1edit.win_preview.w = glb_ds1[ds1_idx].own_wpreview.w;
      glb_ds1edit.win_preview.h = glb_ds1[ds1_idx].own_wpreview.h;

      // Initialize palette state so the first frame doesn't trigger a redundant
      // dt1_rebuild_bitmaps_from_cache (already done during init above).
      wpreview_init_palette_state(ds1_idx);
   }
   else
   {
      glb_ds1edit.win_preview.x0 = 0;
      glb_ds1edit.win_preview.y0 = 0;
      glb_ds1edit.win_preview.w = glb_config.screen.width;
      glb_ds1edit.win_preview.h = glb_config.screen.height;
   }

   // Ensure area browser data is available for the sidebar (all launch modes)
   if (glb_ds1edit.area_browser.group_count == 0)
      area_browser_init();

   // main loop
   freopen("stderr.txt", "wt", stderr);
   setvbuf(stderr, NULL, _IONBF, 0); // unbuffered so perf stats survive process kill
   if (glb_ds1edit.area_browser.lvltypes_act_mismatch_count > 0)
   {
      fprintf(stderr,
              "WARNING: LvlTypes.txt contains %d Name/Act mismatches; palette selection uses the Act column as the source of truth. Run --audit-lvltypes for details.\n",
              glb_ds1edit.area_browser.lvltypes_act_mismatch_count);
   }
   interfac_user_handler(ds1_idx);

   // cleanup
   if (a5_tick_timer != NULL)
      al_destroy_timer(a5_tick_timer);
   if (a5_fps_timer != NULL)
      al_destroy_timer(a5_fps_timer);
   if (a5_event_queue != NULL)
      al_destroy_event_queue(a5_event_queue);
   al_destroy_display(a5_display);

   // fclose(stdout);
   fflush(stdout);
   fflush(stderr);
   return DS1ERR_OK;
}
