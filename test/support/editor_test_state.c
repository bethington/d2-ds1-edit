/* Isolated editor state for map-integrity tests. See editor_test_state.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "structs.h"
#include "error.h"
#include "core/ds1.h"
#include "editor_test_state.h"
#include "test_tmp.h"

/* ---- globals normally defined in main.c ---- */
DS1_S          *glb_ds1 = NULL;
CONFIG_S        glb_config;
GLB_DS1EDIT_S   glb_ds1edit;
char            glb_tiles_path[30]       = "Data\\Global\\Tiles\\";
char            glb_ds1edit_data_dir[80] = "data" DS1_SEP_STR;
char            glb_ds1edit_tmp_dir[80]  = "tmp" DS1_SEP_STR;
WRKSPC_DATAS_S  glb_wrkspc_datas[WRKSPC_MAX] = {
   {("DS1EDIT_WRKSPC_TILE_X")},
   {("DS1EDIT_WRKSPC_TILE_Y")},
   {("DS1EDIT_WRKSPC_ZOOM")},
   {("DS1EDIT_WRKSPC_VERSION")},
   {("DS1EDIT_WRKSPC_SAVE_COUNT")}
};

static char state_root[256];
const char *pkgc_test_filter = NULL;

/* ---- ds1edit_error trap ---- */
jmp_buf *editor_test_error_trap = NULL;
char     editor_test_last_error[1024];
int      editor_test_error_count = 0;

void ds1edit_error(const char *text)
{
   snprintf(editor_test_last_error, sizeof(editor_test_last_error), "%s",
            text ? text : "");
   editor_test_error_count++;
   if (editor_test_error_trap != NULL)
      longjmp(*editor_test_error_trap, 1);
   fprintf(stderr, "ds1edit_error() reached outside a trap: %s\n",
           editor_test_last_error);
   abort();
}

const char *editor_test_state_root(void)
{
   return state_root;
}

void editor_test_state_path(char *out, size_t cap, const char *name)
{
   snprintf(out, cap, "%s%s", state_root, name);
}

static void *xcalloc(size_t n, size_t sz)
{
   void *p = calloc(n ? n : 1, sz ? sz : 1);
   if (p == NULL)
   {
      fprintf(stderr, "editor_test_state: out of memory\n");
      abort();
   }
   return p;
}

static void build_map(int i, int width, int height)
{
   DS1_S *m = &glb_ds1[i];

   memset(m, 0, sizeof(*m));
   snprintf(m->name, sizeof(m->name), "%smap%d.ds1", state_root, i);
   snprintf(m->filename, sizeof(m->filename), "map%d.ds1", i);
   m->version = 18;
   m->width   = width;
   m->height  = height;
   m->act     = 1;

   m->floor_num  = 2;
   m->wall_num   = 2;
   m->shadow_num = 1;
   m->tag_num    = 0;

   m->floor_line = width * m->floor_num;
   m->floor_len  = m->floor_line * height;
   m->floor_buff_len = m->floor_len * (int) sizeof(CELL_F_S);
   m->floor_buff  = (CELL_F_S *) xcalloc((size_t) m->floor_len, sizeof(CELL_F_S));
   m->floor_buff2 = (CELL_F_S *) xcalloc((size_t) m->floor_len, sizeof(CELL_F_S));

   m->shadow_line = width * m->shadow_num;
   m->shadow_len  = m->shadow_line * height;
   m->shadow_buff_len = m->shadow_len * (int) sizeof(CELL_S_S);
   m->shadow_buff  = (CELL_S_S *) xcalloc((size_t) m->shadow_len, sizeof(CELL_S_S));
   m->shadow_buff2 = (CELL_S_S *) xcalloc((size_t) m->shadow_len, sizeof(CELL_S_S));

   m->wall_line = width * m->wall_num;
   m->wall_len  = m->wall_line * height;
   m->wall_buff_len = m->wall_len * (int) sizeof(CELL_W_S);
   m->wall_buff  = (CELL_W_S *) xcalloc((size_t) m->wall_len, sizeof(CELL_W_S));
   m->wall_buff2 = (CELL_W_S *) xcalloc((size_t) m->wall_len, sizeof(CELL_W_S));

   /* ds1_read allocates the (empty) tag buffers even with no tag layer. */
   m->tag_buff  = (CELL_T_S *) xcalloc(1, sizeof(CELL_T_S));
   m->tag_buff2 = (CELL_T_S *) xcalloc(1, sizeof(CELL_T_S));

   m->current_obj_max = OBJ_MAX_START;
   m->obj           = (OBJ_S *) xcalloc(OBJ_MAX_START, sizeof(OBJ_S));
   m->obj_undo      = (OBJ_S *) xcalloc(OBJ_MAX_START, sizeof(OBJ_S));
   m->drawing_order = (int *) xcalloc(OBJ_MAX_START, sizeof(int));

   for (int l = 0; l < WALL_MAX_LAYER; l++)  m->wall_layer_mask[l] = 1;
   for (int l = 0; l < FLOOR_MAX_LAYER; l++) m->floor_layer_mask[l] = 1;
   m->shadow_layer_mask[0] = 3;

   memcpy(m->undo.tag, "UNDO", sizeof(m->undo.tag));
   m->undo.cur_buf_num = -1;
   m->path_edit_win.obj_idx = -1;
}

void editor_test_state_free_slot(int i)
{
   DS1_S *m;

   if (glb_ds1 == NULL)
      return;
   m = &glb_ds1[i];
   if (m->undo.fptr != NULL)
   {
      fclose(m->undo.fptr);
      m->undo.fptr = NULL;
   }
   /* ds1_free() releases the cell, file-list and group buffers but not the
    * object arrays; release those first so the slot leaves nothing behind. */
   free(m->obj);           m->obj = NULL;
   free(m->obj_undo);      m->obj_undo = NULL;
   free(m->drawing_order); m->drawing_order = NULL;
   if (m->name[0] != 0)
      ds1_free(i);
   else
      memset(m, 0, sizeof(*m));
}

int editor_test_state_init(int map_count, int width, int height)
{
   int i;

   if (glb_ds1 == NULL)
      glb_ds1 = (DS1_S *) xcalloc(DS1_MAX, sizeof(DS1_S));
   else
      for (i = 0; i < DS1_MAX; i++)
         editor_test_state_free_slot(i);

   memset(&glb_config, 0, sizeof(glb_config));
   memset(&glb_ds1edit, 0, sizeof(glb_ds1edit));
   glb_config.nb_type1_per_act = 100;
   glb_config.nb_type2_per_act = 150;
   glb_config.default_zoom     = -1;
   glb_config.screen.width     = 800;
   glb_config.screen.height    = 600;
   glb_ds1edit.version_build   = "test";
   glb_ds1edit.version_dll     = "test";
   glb_ds1edit.win_preview.w   = 800;
   glb_ds1edit.win_preview.h   = 600;

   editor_test_error_trap  = NULL;
   editor_test_error_count = 0;
   editor_test_last_error[0] = 0;

   if (!tt_root_create("state", state_root, sizeof(state_root)))
      return 0;
   if (strlen(state_root) >= sizeof(glb_ds1edit_tmp_dir))
      return 0;
   strcpy(glb_ds1edit_tmp_dir, state_root);

   for (i = 0; i < map_count; i++)
      build_map(i, width, height);
   return 1;
}

void editor_test_state_destroy(void)
{
   int i;

   if (glb_ds1 != NULL)
   {
      for (i = 0; i < DS1_MAX; i++)
         editor_test_state_free_slot(i);
   }
   if (state_root[0] != 0)
      tt_remove_tree(state_root);
   state_root[0] = 0;
}
