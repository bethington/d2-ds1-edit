#ifndef _AREA_BROWSER_H_
#define _AREA_BROWSER_H_

#include <stdio.h>

int  area_browser_init          (void);
void area_browser_build         (void);
void area_browser_destroy       (void);
int  area_browser_open_by_name  (const char * area_name);
int  area_browser_open_group    (int group_idx);
int  area_browser_run           (void);
void area_group_add_entry       (AREA_GROUP_S * grp, int lvltype_id,
                                  int def, const char * ds1_path);
void area_browser_scan_backups  (void);
void area_browser_list          (void);
void area_browser_list_ext      (void);
void area_browser_draw_sidebar  (int width, int height);
void area_browser_draw_sidebar_tab(int height);
int  area_browser_sidebar_click (int mx, int my, int sidebar_w, int disp_h);
int  area_browser_sidebar_scroll(int dz);
int  area_browser_switch_area   (int group_idx);

/* DS1 keyboard navigation — returns new ds1_idx or -1 if no change */
int  area_browser_nav_up        (void);
int  area_browser_nav_down      (void);
int  area_browser_nav_left      (void);
int  area_browser_nav_right     (void);
int  area_browser_nav_pgup      (void);
int  area_browser_nav_pgdn      (void);
int  area_browser_nav_home      (void);
int  area_browser_nav_end       (void);
int  area_browser_switch_single (int group_idx, int entry_idx);
void area_browser_list_files    (const char * filter);
int  area_browser_open_by_file  (const char * ds1_path);
int  area_browser_audit_lvltypes(FILE * out);

#endif
