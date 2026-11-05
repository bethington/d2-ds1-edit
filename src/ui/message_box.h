#ifndef DS1EDIT_UI_MESSAGE_BOX_H
#define DS1EDIT_UI_MESSAGE_BOX_H

#include <allegro5/allegro.h>
#include <allegro5/allegro_native_dialog.h>

/* Every native message box goes through here instead of
 * al_show_native_message_box().
 *
 * The editor puts the command in the title ("Build MPQ"), what happened in the
 * heading ("The MPQ was built.") and the detail in the text. Allegro 5.2's macOS
 * alert shows only the title, in bold, and the text: the heading is dropped, so
 * a box said "Build & Test" and the detail but never "The game wasn't started.".
 * An NSAlert has no title bar to show the title in anyway, so on macOS the
 * heading takes its place. Elsewhere the box gets all three as before. */
static inline int ds1_message_box(ALLEGRO_DISPLAY *display, const char *title,
                                  const char *heading, const char *text,
                                  const char *buttons, int flags)
{
#ifdef __APPLE__
   if (heading != NULL && heading[0] != '\0')
   {
      title = heading;
      heading = "";
   }
#endif
   return al_show_native_message_box(display, title, heading, text, buttons, flags);
}

#endif
