/* The editor's commands: one table for menus and hotkeys, so the two can't
 * disagree. Everything here is pure: no display, no input state. ui/menu_bar.c
 * connects it to the keyboard and screen, and ui/interface.c runs the result. */
#ifndef DS1EDIT_COMMANDS_H
#define DS1EDIT_COMMANDS_H

typedef enum
{
   CMD_NONE = 0,
   /* File */
   CMD_NEW_PROJECT, CMD_OPEN_PROJECT, CMD_CLOSE_PROJECT, CMD_SAVE, CMD_SAVE_ALL,
   CMD_OPEN_PRESET, CMD_EXPORT_ASSETS, CMD_PROJECT_SETTINGS, CMD_QUIT,
   /* Edit */
   CMD_UNDO, CMD_CUT, CMD_COPY, CMD_DELETE_SELECTION,
   /* Map */
   CMD_MAP_NEW, CMD_MAP_CLONE, CMD_MAP_REVERT, CMD_MAP_DELETE, CMD_MAP_REMOVE,
   CMD_MAP_RESTORE_SLOT, CMD_MAP_RESTORE_BACKUP, CMD_MAP_PROPERTIES,
   /* View */
   CMD_VIEW_FLOOR1, CMD_VIEW_FLOOR2, CMD_VIEW_ANIMS, CMD_VIEW_OBJECTS,
   CMD_VIEW_WALL1, CMD_VIEW_WALL2, CMD_VIEW_WALL3, CMD_VIEW_WALL4,
   CMD_VIEW_SPECIALS, CMD_VIEW_PATHS, CMD_VIEW_SHADOWS,
   CMD_VIEW_ZOOM_IN, CMD_VIEW_ZOOM_OUT, CMD_VIEW_GRID,
   CMD_VIEW_AREAS, CMD_VIEW_PROPERTIES,
   /* Build */
   CMD_BUILD_MPQ, CMD_BUILD_AND_TEST, CMD_LAUNCH_PROFILE,
   CMD_COUNT
} CMD_ID;

enum { MENU_FILE, MENU_EDIT, MENU_MAP, MENU_VIEW, MENU_BUILD, MENU_COUNT };

#define CMD_MOD_CTRL  1
#define CMD_MOD_SHIFT 2
#define CMD_MOD_ALT   4

#define MENU_ITEM_H   14
#define MENU_DROP_W   248

typedef struct CMD_DEF
{
   CMD_ID      id;
   const char *label;
   int         menu;
   const char *shortcut;   /* shown in the menu; "" for none */
   int         key;        /* ALLEGRO_KEY_* this table dispatches; 0 if none */
   int         mods;       /* exact CMD_MOD_* that must be held with key */
   int         inject_key; /* key the existing polled code answers; 0 if none */
} CMD_DEF;

/* What availability depends on, gathered by the caller. */
typedef struct CMD_CTX
{
   int tile_mode;
   int object_mode;
   int has_map;
   int has_open_entry;   /* the open map has a regular row in the area browser */
   int open_state;       /* MAP_STATE_E of the open map's row */
   int open_is_backup;   /* that row is in a Backup group */
   int any_dirty;
   int paste_active;
   int can_undo_object;
} CMD_CTX;

/* An open menu. menu is -1 when closed. */
typedef struct MENU_STATE
{
   int menu;
   int item;
} MENU_STATE;

/* The table entry the keyboard dispatches for this exact key and modifier set,
 * or CMD_NONE. Keys the existing polled code still owns are never returned. */
CMD_ID commands_find_key(int keycode, int mods);

const CMD_DEF *commands_get(CMD_ID id);
int commands_is_available(CMD_ID id, const CMD_CTX *ctx);

/* Ctrl+Del has one key, and three commands by state. */
CMD_ID commands_ctrl_del_target(const CMD_CTX *ctx);

/* Where the open map's browser row is once row (deleted_group, deleted_entry)
 * has been deleted and the group's entries have shifted up. The map stays open
 * in memory; if its own row went, it no longer has one (-1, -1). */
void commands_loaded_after_delete(int *loaded_group, int *loaded_entry,
                                  int deleted_group, int deleted_entry);

/* Pairs of table entries that dispatch the same key and modifiers. */
int commands_conflict_count(void);

int commands_menu_item_count(int menu);
CMD_ID commands_menu_item(int menu, int index);

void menu_state_open(MENU_STATE *st, int menu);

/* Apply a key to an open menu. Returns the command Enter chose, else CMD_NONE.
 * st->menu becomes -1 once the menu should close. */
CMD_ID menu_state_key(MENU_STATE *st, int keycode, const CMD_CTX *ctx);

/* Geometry, from the 8-pixel built-in font. */
const char *menu_title(int menu);
int menu_title_x(int menu);
int menu_title_at(int mx, int my, int bar_h);
int menu_item_at(int menu, int mx, int my, int bar_h);

#endif
