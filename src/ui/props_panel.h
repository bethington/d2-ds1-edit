#ifndef _PROPS_PANEL_H_
#define _PROPS_PANEL_H_

void props_panel_init          (void);
void props_panel_draw          (int width, int height);
void props_panel_draw_tab      (int height);
int  props_panel_click         (int mx, int my, int panel_w, int disp_h);
int  props_panel_scroll        (int dz);
void props_panel_handle_keychar(int unichar, int keycode);
void props_panel_calc_shared_counts(void);
void props_panel_apply(void);

#endif
