#ifndef _AREA_BROWSER_H_
#define _AREA_BROWSER_H_

int  area_browser_init          (void);
void area_browser_build         (void);
void area_browser_destroy       (void);
int  area_browser_open_by_name  (const char * area_name);
int  area_browser_open_group    (int group_idx);
int  area_browser_run           (void);
void area_browser_list          (void);
void area_browser_list_ext      (void);

#endif
