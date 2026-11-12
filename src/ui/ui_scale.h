/* ui_scale.h -- one scale for the bars and side panels, and the font for them.
 *
 * Migrated UI draws in logical pixels between ui_begin() and ui_end(), so every
 * rectangle, line and hit test scales together and 1 logical pixel is what it
 * was at 100%. Text is the exception: the font is loaded at its physical size
 * and drawn untransformed, so it stays sharp at any scale. See ui_text() in
 * compat.h. Screens not yet migrated keep drawing a5_font at 1:1.
 */

#ifndef UI_SCALE_H
#define UI_SCALE_H

#define UI_SCALE_MIN      1.0f
#define UI_SCALE_MAX      3.0f
#define UI_SCALE_BASE_H   1080   /* the screen height that reads as 100% */
#define UI_FONT_EM        11     /* logical size of the font, in pixels */

/* The scale for a screen: the ui_scale setting as a percentage when it is set,
   else the screen height against UI_SCALE_BASE_H, to the nearest quarter. Pixels
   rather than physical size, which a host cannot reliably know (a Mac driven
   remotely reports whatever its virtual display claims). The screen rather than
   the window, so resizing the window does not resize the text. */
static inline float ui_scale_choose(int percent, int screen_h)
{
   float s = percent > 0 ? (float)percent / 100.0f
                         : (float)(int)((float)screen_h * 4.0f / UI_SCALE_BASE_H + 0.5f) / 4.0f;

   return s < UI_SCALE_MIN ? UI_SCALE_MIN : (s > UI_SCALE_MAX ? UI_SCALE_MAX : s);
}

/* Set the scale for the current display and load the font at that size. Cheap
   when nothing changed, so it can run on every resize. */
void ui_scale_refresh(void);

/* Grow a window size from the settings by the scale, within the primary screen,
   so a window keeps its layout on a denser screen instead of filling up with
   panels. Call before the display exists. */
void ui_scale_window_size(int *width, int *height);
void ui_scale_release(void);

#endif
