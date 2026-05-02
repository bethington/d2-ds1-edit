#include <errno.h>
#include <direct.h>
#include <stdlib.h>
#include <string.h>

#include "structs.h"
#include "ui/compat.h"
#include "core/dcc.h"

char glb_tiles_path[30] = "Data\\Global\\Tiles\\";
char glb_ds1edit_data_dir[80] = "";
char glb_ds1edit_tmp_dir[80] = "";
GLB_MPQ_S glb_mpq_struct[MAX_MPQ_FILE];
GLB_MPQ_S *glb_mpq = NULL;

CONFIG_S glb_config;
GLB_DS1EDIT_S glb_ds1edit;
static DS1_S glb_ds1_storage[DS1_MAX];
static DT1_S glb_dt1_storage[DT1_MAX];
DS1_S *glb_ds1 = glb_ds1_storage;
DT1_S *glb_dt1 = glb_dt1_storage;

RGBA_PALETTE *a5_current_palette = NULL;
ALLEGRO_DISPLAY *a5_display = NULL;
ALLEGRO_FONT *a5_font = NULL;
ALLEGRO_EVENT_QUEUE *a5_event_queue = NULL;
ALLEGRO_TIMER *a5_tick_timer = NULL;
ALLEGRO_TIMER *a5_fps_timer = NULL;
ALLEGRO_KEYBOARD_STATE a5_kb_state;
ALLEGRO_MOUSE_STATE a5_ms_state;
ALLEGRO_CONFIG *a5_config = NULL;
float a5_trans_alpha = 0.5f;

static int stub_misc_load_mpq_file_count = 0;
static int stub_dt1_add_count = 0;
static char stub_misc_load_mpq_file_path[MPQTYPES_MAX_PATH];
static const unsigned char *stub_misc_load_mpq_file_data = NULL;
static long stub_misc_load_mpq_file_len = 0;

void test_asset_export_stub_reset(void)
{
    int i;

    stub_misc_load_mpq_file_count = 0;
    stub_dt1_add_count = 0;
    stub_misc_load_mpq_file_path[0] = 0;
    stub_misc_load_mpq_file_data = NULL;
    stub_misc_load_mpq_file_len = 0;

    memset(&glb_config, 0, sizeof(glb_config));
    memset(&glb_ds1edit, 0, sizeof(glb_ds1edit));
    memset(glb_ds1, 0, sizeof(DS1_S) * DS1_MAX);
    memset(glb_dt1, 0, sizeof(DT1_S) * DT1_MAX);
    for (i = 0; i < MAX_MPQ_FILE; i++)
        memset(&glb_mpq_struct[i], 0, sizeof(GLB_MPQ_S));
}

void test_asset_export_stub_set_mpq_file(const char *path,
                                         const unsigned char *data,
                                         long len)
{
    if (path == NULL)
    {
        stub_misc_load_mpq_file_path[0] = 0;
        stub_misc_load_mpq_file_data = NULL;
        stub_misc_load_mpq_file_len = 0;
        return;
    }

    strncpy(stub_misc_load_mpq_file_path, path, sizeof(stub_misc_load_mpq_file_path) - 1);
    stub_misc_load_mpq_file_path[sizeof(stub_misc_load_mpq_file_path) - 1] = 0;
    stub_misc_load_mpq_file_data = data;
    stub_misc_load_mpq_file_len = len;
}

int test_asset_export_stub_get_misc_load_count(void)
{
    return stub_misc_load_mpq_file_count;
}

int test_asset_export_stub_get_dt1_add_count(void)
{
    return stub_dt1_add_count;
}

static int ensure_dir_recursive(const char *path)
{
    char tmp[1024];
    size_t i;

    if (path == NULL || path[0] == 0)
        return 0;

    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = 0;

    for (i = 0; tmp[i] != 0; i++)
    {
        if (tmp[i] == '/' || tmp[i] == '\\')
        {
            char saved = tmp[i];
            if (i > 0 && tmp[i - 1] != ':')
            {
                tmp[i] = 0;
                if (_mkdir(tmp) != 0 && errno != EEXIST)
                    return 0;
            }
            tmp[i] = saved;
        }
    }

    if (_mkdir(tmp) != 0 && errno != EEXIST)
        return 0;
    return 1;
}

int project_ensure_parent_dirs(const char *path)
{
    char parent[1024];
    char *slash;

    if (path == NULL)
        return 0;

    strncpy(parent, path, sizeof(parent) - 1);
    parent[sizeof(parent) - 1] = 0;
    slash = strrchr(parent, '\\');
    if (slash == NULL)
        slash = strrchr(parent, '/');
    if (slash == NULL)
        return 1;

    *slash = 0;
    return ensure_dir_recursive(parent);
}

int misc_load_mpq_file(char *filename, char **buffer, long *buf_len, int output)
{
    stub_misc_load_mpq_file_count++;
    (void)output;

    if (filename != NULL && buffer != NULL && buf_len != NULL &&
        stub_misc_load_mpq_file_data != NULL &&
        stricmp(filename, stub_misc_load_mpq_file_path) == 0)
    {
        *buffer = (char *)malloc((size_t)stub_misc_load_mpq_file_len);
        if (*buffer == NULL)
            return -1;
        memcpy(*buffer, stub_misc_load_mpq_file_data, (size_t)stub_misc_load_mpq_file_len);
        *buf_len = stub_misc_load_mpq_file_len;
        return 0;
    }

    return -1;
}

int mpq_batch_load_in_mem(char *filename, void **buffer, long *buf_len, int output)
{
    (void)filename;
    (void)buffer;
    (void)buf_len;
    (void)output;
    return -1;
}

int misc_get_txt_column_num(RQ_ENUM txt_idx, char *col_name)
{
    (void)txt_idx;
    (void)col_name;
    return -1;
}

void txt_convert_slash(char *txt)
{
    int i;

    if (txt == NULL)
        return;
    for (i = 0; txt[i] != 0; i++)
    {
        if (txt[i] == '/')
            txt[i] = '\\';
    }
}

int area_browser_init(void)
{
    return -1;
}

/* Stubs for the export_progress UI; the asset_export plan emitter calls
 * these but they are dormant in tests because export_task_is_active()
 * always returns 0 here. */
int export_task_is_active(void) { return 0; }
int export_progress_pump(void) { return 0; }
void export_progress_set_current_item(const char *path) { (void) path; }
void export_progress_advance(int delta) { (void) delta; }
void export_progress_force_repaint(void) {}

int dt1_add(char *dt1name)
{
    (void)dt1name;
    stub_dt1_add_count++;
    return -1;
}

int dt1_add_special(char *dt1name)
{
    (void)dt1name;
    return -1;
}

void dt1_del(int dt1_idx)
{
    (void)dt1_idx;
}

void dt1_rebuild_bitmaps_from_cache(RGBA_PALETTE *palette)
{
    (void)palette;
}

DCC_S *dcc_mem_load(void *buff, int buff_len)
{
    (void)buff;
    (void)buff_len;
    return NULL;
}

int dcc_file_header(DCC_S *dcc)
{
    (void)dcc;
    return -1;
}

int dcc_decode(DCC_S *dcc, long directions_bitfield)
{
    (void)dcc;
    (void)directions_bitfield;
    return -1;
}

void dcc_destroy(DCC_S *dcc)
{
    (void)dcc;
}

void dc6_decomp_norm(void *src, ALLEGRO_BITMAP *dst, long size, int x0, int y0)
{
    UBYTE *ptr = (UBYTE *)src;
    long i;
    int x = x0;
    int y = y0;

    for (i = 0; i < size; i++)
    {
        int c = *(ptr++);
        if (c == 0x80)
        {
            x = x0;
            y--;
        }
        else if (c & 0x80)
        {
            x += c & 0x7F;
        }
        else
        {
            int count;
            for (count = 0; count < c; count++)
            {
                int color = *(ptr++);
                i++;
                a5_putpixel(dst, x, y, color);
                x++;
            }
        }
    }
}
