#ifndef _WINDS1EDIT_STRUCTS_H_

#define _WINDS1EDIT_STRUCTS_H_

// for allegro with MSVC6/7/2008, in console version
#define USE_CONSOLE

#include "types.h"
#include <stdio.h>
#include <string.h>
#include <allegro.h>
#include "mpq\\mpqtypes.h"

#define MAX_MPQ_FILE           4
#define MAX_MOD_DIR            1
#define DT1_IN_DS1_MAX        33
#define DS1_MAX              100
#define DT1_MAX              300
#define ACT_MAX                5
#define FLOOR_MAX_LAYER        2
#define SHADOW_MAX_LAYER       1
#define TAG_MAX_LAYER          1
#define WALL_MAX_LAYER         4
#define OBJ_MAX_START        100
#define OBJ_MAX_GRANULARITY   50
#define WINDS1EDIT_PATH_MAX  100

#define TXT_COL_NAME_LENGTH   30

#define FLG_SELECTED      1
#define FLG_HIDE          2
#define FLG_TMP_SELECT    4
#define FLG_COPY_INFO     8
#define FLG_COPY_STATE   16

// these ones are only for the Edit Objects mode
#define FLG_MOUSE_OVER   32
#define FLG_MOVING       64
#define FLG_ACTIVATED   128

#define IS_SELECTED(x)    ((x) & FLG_SELECTED)
#define IS_HIDE(x)        ((x) & FLG_HIDE)
#define IS_TMP_SELECT(x)  ((x) & FLG_TMP_SELECT)
#define IS_COPY_INFO(x)   ((x) & FLG_COPY_INFO)
#define IS_COPY_STATE(x)  ((x) & FLG_COPY_STATE)
#define IS_MOUSE_OVER(x)  ((x) & FLG_MOUSE_OVER)
#define IS_MOVING(x)      ((x) & FLG_MOVING)
#define IS_ACTIVATED(x)   ((x) & FLG_ACTIVATED)

#define SET_SELECTED(x)   ((x) |= FLG_SELECTED)
#define SET_HIDE(x)       ((x) |= FLG_HIDE)
#define SET_TMP_SELECT(x) ((x) |= FLG_TMP_SELECT)
#define SET_COPY_INFO(x)  ((x) |= FLG_COPY_INFO)
#define SET_COPY_STATE(x) ((x) |= FLG_COPY_STATE)
#define SET_MOUSE_OVER(x) ((x) |= FLG_MOUSE_OVER)
#define SET_MOVING(x)     ((x) |= FLG_MOVING)
#define SET_ACTIVATED(x)  ((x) |= FLG_ACTIVATED)

#define DEL_SELECTED(x)   ((x) &= ~FLG_SELECTED)
#define DEL_HIDE(x)       ((x) &= ~FLG_HIDE)
#define DEL_TMP_SELECT(x) ((x) &= ~FLG_TMP_SELECT)
#define DEL_COPY_INFO(x)  ((x) &= ~FLG_COPY_INFO)
#define DEL_COPY_STATE(x) ((x) &= ~FLG_COPY_STATE)
#define DEL_MOUSE_OVER(x) ((x) &= ~FLG_MOUSE_OVER)
#define DEL_MOVING(x)     ((x) &= ~FLG_MOVING)
#define DEL_ACTIVATED(x)  ((x) &= ~FLG_ACTIVATED)

// Data Flags for undo, tells whiwh data are to be saved (for 1 layer)
#define DF_P1  1
#define DF_P2  2
#define DF_P3  4
#define DF_P4  8
#define DF_OR 16
#define DF_BT 32
#define DF_FL 64

// Message Windows
#define MW_SHORTCUT_NUM        5
#define MW_COMBINATION_KEY_NUM 5

// colormaps index for COF effect
#define COF_25TRANS       53
#define COF_50TRANS      309
#define COF_75TRANS      565
#define COF_ALPHA        821
#define COF_LUMINANCE   1077
#define COF_ALPHABRIGHT 1461

extern char      glb_tiles_path       [30];
extern char      glb_ds1edit_data_dir [80];
extern char      glb_ds1edit_tmp_dir  [80];
extern GLB_MPQ_S glb_mpq_struct       [MAX_MPQ_FILE];

typedef struct SCREEN_S
{
   int width;
   int height;
   int depth;
   int refresh;
} SCREEN_S;

typedef struct XY_S
{
   int x;
   int y;
} XY_S;

typedef struct SCROLL_S
{
   XY_S keyb;
   XY_S mouse;
   XY_S edit;
   int  obj_edit;
} SCROLL_S;

typedef enum GC_ENUM
{
   GC_060, GC_062, GC_064, GC_066, GC_068,
   GC_070, GC_072, GC_074, GC_076, GC_078,
   GC_080, GC_082, GC_084, GC_086, GC_088,
   GC_090, GC_092, GC_094, GC_096, GC_098,
   GC_100,
   GC_110, GC_120, GC_130, GC_140, GC_150,
   GC_160, GC_170, GC_180, GC_190, GC_200,
   GC_210, GC_220, GC_230, GC_240, GC_250,
   GC_260, GC_270, GC_280, GC_290, GC_300,
   GC_MAX
} GC_ENUM;

typedef struct GAMMA_S
{
   char    str[5];
   GC_ENUM val;
} GAMMA_S;

extern GAMMA_S glb_gamma_str[GC_MAX];

typedef struct CONFIG_S
{
   int      fullscreen;
   char     * mpq_file[MAX_MPQ_FILE];
   char     * mod_dir[MAX_MOD_DIR];
   SCREEN_S screen;
   SCROLL_S scroll;
   GC_ENUM  gamma;
   XY_S     mouse_speed;
   int      normal_type2;
   int      always_max_layers;
   int      stretch_sprites;
   int      winobj_scroll_keyb;
   int      winobj_scroll_mouse;
   int      center_zoom;
   int      nb_type1_per_act;
   int      nb_type2_per_act;
   int      minimize_ds1;
   int      lower_speed_zoom_out;
   int      workspace_enable;
} CONFIG_S;

extern CONFIG_S glb_config;

typedef struct WIN_PREVIEW_S
{
   int x0;
   int y0;
   int w;
   int h;
} WIN_PREVIEW_S;

typedef enum BUT_STATE_E
{
   BS_OFF,
   BS_ON
} BUT_STATE_E; // button state

typedef struct BUTTON_S
{
   int         x0, y0;
   int         w, h;
   int         active;
   int         tab_have_tiles;
   BUT_STATE_E state;
   int         mouse_over;  // TRUE / FALSE
   BITMAP      * bmp[2];    // [0]=OFF, [1]=ON
} BUTTON_S;

typedef enum BLK_TYP_E
{
   /* null    */ BT_NULL,
   /* floors  */ BT_STATIC,  BT_ANIMATED,
   /* walls   */ BT_WALL_UP, BT_WALL_DOWN, BT_ROOF, BT_SPECIAL, BT_WALL_ANIMATED,
   /* shadows */ BT_SHADOW,
   BT_MAX
} BLK_TYP_E; // block type

typedef enum BUT_TYP_E
{
   /* null      */ BU_NULL,
   /* floors    */ BU_FLOOR1, BU_FLOOR2,
   /* shadows   */ BU_SHADOW,
   /* walls     */ BU_WALL1,  BU_WALL2,  BU_WALL3,  BU_WALL4,
   /* ok/cancel */ BU_OK,     BU_CANCEL,
   /* random    */ BU_VIEW_RAND,
   BU_MAX
} BUT_TYP_E; // button type

typedef struct WIN_ELEMENT_S
{
   // border (outside)
   int        b_x0, b_y0; // upper-left corner, in the screen
   RLE_SPRITE * border;

   // inside
   int        i_x0, i_y0; // upper-left corner, in the screen
   BITMAP     * inside;
} WIN_ELEMENT_S;


typedef struct SUB_ELM_S
{
   int bt_idx_tab;
   int x1;
   int y1;
   int x2;
   int y2;
   int is_draw;
} SUB_ELM_S;

typedef struct MAIN_LINE_S
{
   SUB_ELM_S * sub_elm;
   int       bt_idx_num;   // # of elements in * bt_idx_tab
   int       max_height;   // max height (in pixels) for this line
} MAIN_LINE_S;

typedef struct WIN_EDIT_S
{
   BUTTON_S      button [BU_MAX];
   BUTTON_S      tab    [BT_MAX];
   WIN_ELEMENT_S w_preview;
   WIN_ELEMENT_S w_tiles;
   MAIN_LINE_S   * main_line_tab[BT_MAX];
   int           main_line_num[BT_MAX];
   BITMAP        * tmp_edit;
} WIN_EDIT_S;

typedef enum ZOOM_E
{
   ZM_11,
   ZM_12,
   ZM_14,
   ZM_18,
   ZM_116,
   ZM_MAX
} ZOOM_E;

// color map type
typedef enum CMAP_E
{
   CM_SELECT,
   CM_TRANS,
   CM_SHADOW,
   CM_MAX
} CMAP_E;

typedef enum COL_E
{
   COL_FLOOR    =  29, // dark  grey,   selected without mouse
   COL_FLOOR_M  =  32, // light grey,   selected with mouse
   COL_WALL     = 151, // dark  blue,   selected without mouse
   COL_WALL_M   = 162, // light blue,   selected with mouse
   COL_MOUSE    = 255, // white,        not selected, with mouse
   COL_TMPSEL   =  92, // orange,       left-click selection, without mouse
   COL_TMPSEL_M = 108, // orange,       left-click selection, with mouse
   COL_PASTE_OK = 132, // green,        no conflicts when pasting
   COL_PASTE_KO =  98, // red,          conflicts when pasting
   COL_SHADOW   = 168  // yellow        shadow easy to view
} COL_E; // tile's color index in color map

typedef enum MODE_E
{
   MOD_T, // Edit Tiles
   MOD_O, // Edit Objects
   MOD_P, // Edit Paths
   MOD_L, // Preview with Lights
   MOD_MAX
} MODE_E;
              
// cof & dcc
#define COMPOSIT_NB 16

typedef struct LAY_INF_S
{
   UBYTE  shad_a;
   UBYTE  shad_b;
   UBYTE  trans_a;
   UBYTE  trans_b;
   char   wclass[4];

   // editor only
   int    bmp_num;
   BITMAP ** bmp;
   int    off_x;
   int    off_y;
   int    last_good_frame;
} LAY_INF_S;

typedef struct COF_S
{
   UBYTE     lay;
   UBYTE     fpd;
   UBYTE     dir;
   long      xoffset;
   long      yoffset;
   LAY_INF_S lay_inf[COMPOSIT_NB];
   UBYTE     * priority;
   long      cur_frame;
   int       cur_dir;
   long      spd_mul;
   long      spd_div;
   long      spd_mod; // = is (mul % div), for extra precision
   long      orderflag; // from data\global\excel\objects.txt, 0 1 or 2
} COF_S;

typedef struct OBJ_DESC_S
{
   int   act;
   int   type;
   int   id;
   char  * desc;
   COF_S * cof;
   int   objects_line;
   int   monstats_line;
   int   usage_count;
} OBJ_DESC_S;

// struct for convenience, for undo
// if bit is set, indicate that this data of this layer is to be save
typedef struct UNDO_DAT_FLG_S
{
   UBYTE f [FLOOR_MAX_LAYER];
   UBYTE s [SHADOW_MAX_LAYER];
   UBYTE w [WALL_MAX_LAYER];
} UNDO_DAT_FLG_S;

typedef struct UNDO_S
{
   FILE           * fptr;
   long           buf_num;
   long           cur_buf_num;
   char           name[150];
   char           tag[4];
   int            in_seg;
   long           seg_offset;
   UBYTE          seg_num;
   long           cell_offset;
   UBYTE          cell_num;
   UBYTE          old_x, old_y;
   UNDO_DAT_FLG_S dat_flg;
} UNDO_S;

typedef enum COL_TYPE_NM
{
   CT_NULL,
   CT_STR,
   CT_NUM
} COL_TYPE_NM;

typedef struct TXT_COL_S
{
   // for loading function
   char        name[TXT_COL_NAME_LENGTH]; // column header

   // fill by loading function
   int         pos;      // column index in the original file
   COL_TYPE_NM type;     // string or num
   int         size;     // strlen()+1 or sizeof(long)
   int         offset;   // offset in a line to get the data
} TXT_COL_S;

typedef struct TXT_S
{
   char      * data;           // buffer having all lines
   int       line_num;         // number of lines
   int       line_size;        // size of 1 line (in bytes)
   int       nb_required_cols; // number of required columns
   int       col_num;          // total number of columns (required + user defined filters)
   TXT_COL_S * col;            // pointer on a table of TXT_COL_S
} TXT_S;

#pragma pack(1)

   typedef struct ANIM_DATA_RECORD_S
   {
      char  cof_name[8];
      long  frames_per_dir;
      long  speed;
      UBYTE flags[144];
   } ANIM_DATA_RECORD_S;

#pragma pack()

typedef struct ANIM_DATA_S
{
   void               * buffer;
   long               buffer_size;
   ANIM_DATA_RECORD_S * block_pointer [256];
   int                nb_records      [256];
} ANIM_DATA_S;

// command-line parameters
typedef struct CMD_LINE_S
{
   // ptr to ds1 filename, NULL if not used
   char * ds1_filename;

   // ptr to ini filename, NULL if not used
   char * ini_filename;

   // lvltype.txt ID, -1 if not used
   int  lvltype_id;

   // lvlprest.txt DEF, -1 if not used
   int  lvlprest_def;

   // -resize : width parameter, >= 1, -1 if not used
   int  resize_width;

   // -resize : height parameter, >= 1, -1 if not used
   int  resize_height;

   // -force_pal : number is act of palette, 0 to 4, -1 if not used
   int  force_pal_num;

   // -no_check_act option (don't compare .ds1 and .txt acts for safety), TRUE / FALSE
   int  no_check_act;

   // -dt1_list : number of file, 1 to DT1_IN_DS1_MAX, -1 if not used
   int  dt1_list_num;

   // -dt1_list : DT1_IN_DS1_MAX ptr to dt1 filenames, unused slots are set to NULL
   char * dt1_list_filename[DT1_IN_DS1_MAX];

   // -debug : activate debug mode
   int  debug_mode;

   // -no_vis_debug : desactivate debug infos on Vis Tiles that have graphics
   int  no_vis_debug;
} CMD_LINE_S;

// Tile Grid states
typedef enum TILEGRID_ENUM
{
   TG_OFF = 0,   // disable
   TG_OVERFLOOR, // enable, over floor but under walls
   TG_OVERWALL,  // enable, over all tiles
   TG_MAX
} TILEGRID_ENUM;

typedef struct GLB_DS1EDIT_S
{
   UBYTE         * d2_pal [ACT_MAX];
   long          pal_size [ACT_MAX];
   PALETTE       vga_pal  [ACT_MAX];
   PALETTE       dummy_pal;
   int           pal_loaded[ACT_MAX];
   BITMAP        * mouse_cursor[MOD_MAX];
   WIN_PREVIEW_S win_preview;
   WIN_EDIT_S    win_edit;
   BITMAP        * big_screen_buff; // for safety about cliping
   BITMAP        * screen_buff;
   BITMAP        * video_page[2];
   int           video_page_num;
   UBYTE         gamma_table[GC_MAX][256];
   GC_ENUM       cur_gamma;
   volatile int  old_fps;
   volatile int  fps;
   volatile int  ticks_elapsed;
   int           screenshot_num;
   COLOR_MAP     cmap[CM_MAX][ACT_MAX];
   OBJ_DESC_S    * obj_desc;
   int           obj_desc_num; // # of objects's description
   MODE_E        mode;
   RLE_SPRITE    * subtile_flag[9][ZM_MAX][25];
   RLE_SPRITE    * subtile_flag_combination[256][ZM_MAX][25];
   RLE_SPRITE    * subtile_nowalk[ZM_MAX][25];
   RLE_SPRITE    * subtile_nojump[ZM_MAX][25];
   BITMAP        * subtile_help;
   int           night_mode;
   TXT_S         * lvltypes_buff;
   TXT_S         * lvlprest_buff;
   TXT_S         * obj_buff;
   TXT_S         * objects_buff;
   int           new_dir1[1],
                 new_dir4[4],
                 new_dir8[8],
                 new_dir16[16],
                 new_dir32[32];
   int           obj_order_ds1_idx;
   int           obj_sub_tile_order[5][5];
   char          version[15];
   int           current_refresh_rate;
   int           col_obj_id;
   int           col_objects_id;
   int           col_frame_delta[8];
   int           col_orderflag[8];
   ANIM_DATA_S   anim_data;
   CMD_LINE_S    cmd_line;
   int           ds1_group_idx;
   int           show_2nd_row;
   char          strtmp[512];
   TILEGRID_ENUM display_tile_grid;
   char          * version_build;
   char          * version_dll;
} GLB_DS1EDIT_S;

extern GLB_DS1EDIT_S glb_ds1edit;

typedef struct BLOCK_TABLE_S
{
   // key
   int       dt1_idx_for_ds1;
   long      main_index;
   long      orientation;
   long      sub_index;

   // datas
   int       dt1_idx;
   long      rarity;
   int       block_idx;
   BLK_TYP_E type;
   int       zero_line;
   int       roof_y;

   // conflicts managment
   char      conflict;
   char      used_by_game;   // True / False
   char      used_by_editor; // True / False

   // animated tile
   long      curr_frame;
   int       updated;
} BLOCK_TABLE_S;

typedef struct PATH_S
{
   long x;
   long y;
   long action;
   int  flags;
} PATH_S;

typedef struct OBJ_LABEL_S
{
   int rx, ry; // upper/left corner of the label, relative to the sub-cell
               // (in pixels, at zoom of 1:1)
   int w, h;   // width & height (pixels)
   int x0, y0; // pixels position on screen
   int flags;
   
   // for moving
   int old_rx;
   int old_ry;
       
} OBJ_LABEL_S;

typedef struct OBJ_S
{
   long        type;
   long        id;
   long        x;     // sub-cell X
   long        y;     // sub-cell Y
   long        ds1_flags;

   PATH_S      path[WINDS1EDIT_PATH_MAX];
   long        path_num;
   int         desc_idx;
   int         flags;
   OBJ_LABEL_S label;

   // for moving
   long         old_x;
   long         old_y;

   // for sorting
   long        tx; // tile X
   long        ty; // tile Y
   long        sx; // sub-tile X
   long        sy; // sub-tile Y

   // random starting animation frame
   UBYTE       frame_delta;
} OBJ_S;

typedef struct BOX_S
{
   int x1, y1, x2, y2;
} BOX_S;

typedef struct EDT_BUT_S
{
   BOX_S box;
   XY_S  txt;
   char  name[10];
   int   flags;
} EDT_BUT_S;

typedef enum EDT_BUT_E
{
   EB_OK, EB_CANCEL, EB_TYPE1, EB_TYPE2,
   EB_ACT1, EB_ACT2, EB_ACT3, EB_ACT4, EB_ACT5,
   EB_MAX
} EDT_BUT_E; // edit obj buttons
              
typedef struct WIN_EDT_OBJ_S
{
   // window itself
   int x0, y0;
   int w, h;
   int obj_idx;
   int cur_type;
   int start_act;
   int cur_act;

   // buttons
   EDT_BUT_S button[EB_MAX]; // OK, Cancel, Type1, Type2, ACT1, 2, 3, 4, 5

   // text
   int num;        // number of row to display
   int start;      // index of the win's 1st row
   int cur;        // current index, pointed by the mouse

   int desc_char;  // max chars of description
   int desc_start; // starting index in glb_ds1edit.obj_desc[]
   int desc_num;   // number of description for this act
   int desc_end;   // ending index
   int desc_cur;   // curent selected index (the object's one before editing)
} WIN_EDT_OBJ_S;

typedef struct CELL_W_S
{
   UBYTE prop1;
   UBYTE prop2;
   UBYTE prop3;
   UBYTE prop4;
   UBYTE orientation;
   int   bt_idx;
   UBYTE flags;
} CELL_W_S;

typedef struct CELL_F_S
{
   UBYTE prop1;
   UBYTE prop2;
   UBYTE prop3;
   UBYTE prop4;
   int   bt_idx;
   UBYTE flags;
} CELL_F_S;

typedef struct CELL_S_S // exactly the same struct for shadow as for the floor
{
   UBYTE prop1;
   UBYTE prop2;
   UBYTE prop3;
   UBYTE prop4;
   int   bt_idx;
   UBYTE flags;
} CELL_S_S;

typedef struct CELL_T_S
{
   // assume the data is 1 dword, and not 4 different bytes
   UDWORD num;
   UBYTE  flags;
} CELL_T_S;

typedef struct GROUP_S
{
   UDWORD tile_x;
   UDWORD tile_y;
   UDWORD width;
   UDWORD height;
   UDWORD unk;
} GROUP_S;

// Object Layer Enum
typedef enum OL_ENUM
{
   OL_NONE,
   OL_TYPEID,
   OL_SPEED,
   OL_DESC,
   OL_MAX
} OL_ENUM;

// color struct for path editing buttons
typedef struct PATH_EDIT_BUT_COL_S
{
   int bg;
   int fg;
   int border;
} PATH_EDIT_BUT_COL_S;

// buttons for path editing window
typedef struct PATH_EDIT_BUT_S
{
   int                 show;
   int                 x0, y0, w, h;
   char                * text;
   int                 text_x0;
   PATH_EDIT_BUT_COL_S col_on;
   PATH_EDIT_BUT_COL_S col_off;
} PATH_EDIT_BUT_S;

// enum of each path editing button
typedef enum PEB_E
{
   PEB_OK,
   PEB_CANCEL,
   PEB_ALLNEW,
   PEB_STOP,
   PEB_SETACTION,
   PEB_MAX
} PEB_E; // Path Editing Button enumeration

// enum of path editing window current mode
typedef enum PEM_E
{
   PEM_NONE,
   PEM_ALLNEW,
   PEM_MAX
} PEM_E; // Path Editing Mode enumeration

// path editing window data
typedef struct PATH_EDIT_WIN_S
{
   int x0, y0;          // screen position
   int w, h;            // dimensions in pixels
   int obj_idx;         // object index that is used, -1 = none
   int obj_dirty;       // if TRUE, need a click on the OK or CANCEL button
                        // before another object can be edited
   int path_start_idx;  // first path index displayed

   // command buttons
   int but_x0, but_y0;  // screen position
   int but_w, but_h;    // dimensions in pixels
   PATH_EDIT_BUT_S button[PEB_MAX]; // buttons

   // colors
   int c_black;
   int c_white;
   int c_green;
   int c_darkgreen;
   int c_yellow;
   int c_gray;
   int c_darkgray;

   // coriginal values of the paths of the object we're working with
   PATH_S path_copy[WINDS1EDIT_PATH_MAX];
   int    path_copy_num;

   // current mode
   PEM_E mode;

   // current Action number
   long curr_action;

} PATH_EDIT_WIN_S;

typedef struct DS1_S
{
   char          dt1_idx[DT1_IN_DS1_MAX];
   int           dt1_mask[DT1_IN_DS1_MAX];
   int           txt_act;
   BLOCK_TABLE_S * block_table;
   int           bt_num;
   UBYTE         wall_layer_mask[WALL_MAX_LAYER];
   UBYTE         floor_layer_mask[FLOOR_MAX_LAYER];
   char          shadow_layer_mask[SHADOW_MAX_LAYER];
   OL_ENUM       objects_layer_mask;
   UBYTE         paths_layer_mask;
   UBYTE         walkable_layer_mask;
   UBYTE         animations_layer_mask;
   UBYTE         special_layer_mask;
   int           subtile_help_display;
   char          name[256];    // long filename with paths
   char          filename[50]; // short filename
   UNDO_S        undo;

   // from file
   long          version;
   long          tag_type;
   long          width;    // from file, +1
   long          height;   // from file, +1
   long          act;      // from file, +1

   // files in the ds1 (not used by the game)
   long          file_num;
   char          * file_buff;
   int           file_len;
   
   // floors
   CELL_F_S      * floor_buff,   // buffer for all floor layers
                 * floor_buff2;  // 2nd buffer, for copy/paste
   int           floor_buff_len; // sizeof the floor buffer (in bytes)
   int           floor_num;      // # of layers in floor buffer
   int           floor_line;     // width * floor_num
   int           floor_len;      // floor_line * height
   
   // shadow
   CELL_S_S      * shadow_buff,   // buffer for all shadow layers
                 * shadow_buff2;  // 2nd buffer, for copy/paste
   int           shadow_buff_len; // sizeof the shadow buffer (in bytes)
   int           shadow_num;      // # of layers in shadow buffer
   int           shadow_line;     // width * shadow_num
   int           shadow_len;      // shadow_line * height
   
   // walls
   CELL_W_S      * wall_buff,    // buffer for all wall layers
                 * wall_buff2;   // 2nd buffer, for copy/paste
   int           wall_buff_len;  // sizeof the wall buffer (in bytes)
   int           wall_num;       // # of layers in wall buffer
   int           wall_line;      // width * wall_num
   int           wall_len;       // wall_line * height

   // tag
   CELL_T_S      * tag_buff,   // buffer for all unk layers
                 * tag_buff2;  // 2nd buffer, for copy/paste
   int           tag_buff_len; // sizeof the unk buffer (in bytes)
   int           tag_num;      // # of layers in unk buffer
   int           tag_line;     // width * unk_num
   int           tag_len;      // unk_line * height

   // groups for tag layer
   long          group_num;
   int           group_size;
   GROUP_S       * group;

   // internal
   ZOOM_E        cur_zoom;
   int           tile_w;
   int           tile_h;
   int           height_mul;
   int           height_div;
   SCROLL_S      cur_scroll;

   // screen position and size for this ds1
   WIN_PREVIEW_S own_wpreview;
   
   // objects and npc paths (paths are in obj struct)
   int           * drawing_order;
   OBJ_S         * obj;
   OBJ_S         * obj_undo;
   long          obj_num;
   long          obj_num_undo;
   int           can_undo_obj;
   int           draw_edit_obj; // edit Type-Id of objects, FALSE / TRUE
   WIN_EDT_OBJ_S win_edt_obj;

   // current animated floor frame
   int           cur_anim_floor_frame;

   // path editing window of this ds1
   PATH_EDIT_WIN_S path_edit_win;

   // save count
   UDWORD save_count;

   // current number of objects
   long current_obj_max;
} DS1_S;

extern DS1_S * glb_ds1;

typedef struct BLOCK_S
{
   long       direction;
   WORD       roof_y;
   UBYTE      sound;
   UBYTE      animated;
   long       size_y;
   long       size_x;
   // long       zeros1;
   long       orientation;
   long       main_index;
   long       sub_index;
   long       rarity;
   /*
   UBYTE      unknown_a;
   UBYTE      unknown_b;
   UBYTE      unknown_c;
   UBYTE      unknown_d;
   */
   UBYTE      sub_tiles_flags[25];
   // int        zeros2[7];
   long       tiles_ptr;
   long       tiles_length;
   long       tiles_number;
   // int        zeros3[12];
} BLOCK_S;

typedef struct DT1_S
{
   int        ds1_usage; // current number of ds1 using this dt1
   char       name[80];
   void       * buffer;
   long       buff_len;
   long       x1; // signature (7)
   long       x2; // signature (6)
   long       block_num;
   long       bh_start;
   
   // block headers : block_num structs of BLOCK_S
   void       * bh_buffer;
   int        bh_buff_len;

   // all blocks in differents zoom format
   BITMAP     ** block_zoom[ZM_MAX]; // ZM_MAX tables of table of pointers
                                     //   to BITMAP
   int        bz_size[ZM_MAX];
} DT1_S;

extern DT1_S * glb_dt1;

typedef struct SUB_TILE_S
{
   WORD  x_pos;
   WORD  y_pos;
   //   WORD  unknown1;
   int   x_grid;
   int   y_grid;
   WORD  format;
   long  length;
   //   WORD  unknown2;
   long  data_offset;
} SUB_TILE_S;

typedef struct ORDER_DATA_S
{
   int idx;
   int height;
} ORDER_DATA_S;

typedef struct TMP_SEL_S
{
   enum type {TMP_NULL, TMP_NEW, TMP_ADD, TMP_DEL, TMP_HIDE} type;
   int start;
   int x1, y1, x2, y2, old_x2, old_y2;
} TMP_SEL_S;

typedef struct PASTE_POS_S
{
   int src_ds1_idx;
   int start;
   int start_x, start_y, old_x, old_y, old_ds1_idx;
   int is_cut;
   int cut_saved;
} PASTE_POS_S;

typedef enum IT_ENUM
{
   IT_NULL,
   IT_NEW,
   IT_ADD,
   IT_DEL
} IT_ENUM; // Identical Type

// txt required cols
typedef enum RQ_ENUM
{
   RQ_LVLTYPE,
   RQ_LVLPREST,
   RQ_OBJ,
   RQ_OBJECTS,
   RQ_MAX
} RQ_ENUM;

extern char ** glb_txt_req_ptr [RQ_MAX];

typedef struct WMSG_FGBG_S
{
   int fg;
   int bg;
} WMSG_FGBG_S;

typedef struct WMSG_BUTCOL_S
{
   int fg;
   int bg;
   int border;
} WMSG_BUTCOL_S;

typedef struct WMSG_BUTKEY_S
{
   int key [MW_COMBINATION_KEY_NUM]; // scancodes of max X keys combination
   int mb; // mouse buttons
} WMSG_BUTKEY_S;

typedef struct WMSG_BUT_S
{
   char          * text;
   int           right_spaces;
   WMSG_BUTCOL_S on;
   WMSG_BUTCOL_S off;
   WMSG_BUTKEY_S shortcut[MW_SHORTCUT_NUM];
} WMSG_BUT_S;

typedef struct WMSG_S
{
   int         font_width;
   int         font_height;
   char        * title;
   char        * text;
   WMSG_FGBG_S col_win;
   WMSG_FGBG_S col_title;
   WMSG_FGBG_S col_text;
   WMSG_BUT_S  * button;
} WMSG_S;

typedef enum DS1ERR_ENUM
{
   DS1ERR_OK        =   0,
   DS1ERR_INICREATE =   1,
   DS1ERR_CMDLINE   =   2,
   DS1ERR_NOHEIGHT  =   3,
   DS1ERR_INITMPQ   =   4,
   DS1ERR_OTHER     = 255
} DS1ERR_ENUM;

typedef enum WBITSTYPE_ENUM
{
   WBITSTYPE_UNKNOWN = 0,
   WBITSTYPE_SAME,
   WBITSTYPE_DIFFERENT
} WBITSTYPE_ENUM;

typedef struct WBITSDATA_S
{
   // mix of bits values for selected tiles
   CELL_F_S mix_f[FLOOR_MAX_LAYER];
   CELL_S_S mix_s[SHADOW_MAX_LAYER];
   CELL_W_S mix_w[WALL_MAX_LAYER];
   CELL_T_S mix_t[TAG_MAX_LAYER];

   // are each bit always the same for the entire selection of tiles ?
   WBITSTYPE_ENUM bit_type_f[FLOOR_MAX_LAYER ][ 4][8];
   WBITSTYPE_ENUM bit_type_s[SHADOW_MAX_LAYER][ 4][8];
   WBITSTYPE_ENUM bit_type_w[WALL_MAX_LAYER  ][ 5][8];
   WBITSTYPE_ENUM bit_type_t[TAG_MAX_LAYER   ][32];
} WBITSDATA_S;

// workspace datas saved in .ds1
typedef enum WRKSPC_ENUM
{
   WRKSPC_TILE_X = 0,   // tile x position in the middle of the screen
   WRKSPC_TILE_Y,       // tile y position in the middle of the screen
   WRKSPC_ZOOM,         // last zoom
   WRKSPC_VERSION,      // version
   WRKSPC_SAVE_COUNT,   // number of times the ds1 has been saved
   WRKSPC_MAX           // number of datas
} WRKSPC_ENUM;

typedef struct WRKSPC_DATAS_S
{
   char * id;
} WRKSPC_DATAS_S;

extern WRKSPC_DATAS_S glb_wrkspc_datas[WRKSPC_MAX];

// debug files
extern char * glb_path_lvltypes_mem;
extern char * glb_path_lvltypes_def;
extern char * glb_path_lvlprest_mem;
extern char * glb_path_lvlprest_def;
extern char * glb_path_obj_mem;
extern char * glb_path_obj_def;
extern char * glb_path_objects_mem;
extern char * glb_path_objects_def;

#endif
