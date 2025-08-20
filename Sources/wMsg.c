#include <string.h>
#include "structs.h"
#include "error.h"
#include "wMsg.h"
#include "misc.h"


// ==========================================================================
int wmsg_main(WMSG_S * wmsg)
{
   int  mx, my, mb, done = FALSE;
   int  wx0, wy0, wx1, wy1; // main message window base
   int  i, n, curr_length = 0, max_length = 0;
   int  curr_width, max_width, curr_height;
   int  text_lines = 0, line_height, x, y, x2, y2;
   int  ret = -1, bx0, by0, but_maxwidth = 0, bg, fg, border;
   int  s, k, all_keys;
   char tmp[512];

   // for safety
   if (wmsg == NULL)
      return -1;
   if (wmsg->text == NULL)
      return -1;
   if (wmsg->button == NULL)
      return -1;

   // max width of the text
   n = strlen(wmsg->text) + 1;
   for (i=0; i < n; i++)
   {
      if ( (wmsg->text[i] != 0x0A) &&
           (wmsg->text[i] != 0x0D) &&
           (wmsg->text[i] != 0x00)
         )
         curr_length++;
      else
      {
         if (curr_length > max_length)
            max_length = curr_length;
         curr_length = 0;
         text_lines++;
      }
   }
   max_width    = (max_length + 2) * wmsg->font_width;
   line_height  = wmsg->font_height + 2;
   curr_height  = text_lines * line_height;
   curr_height += line_height;

   // title width and height
   if (wmsg->title != NULL)
   {
      curr_height += line_height * 2;
      curr_width = wmsg->font_width * strlen(wmsg->title);
      if (curr_width > max_width)
         max_width = curr_width;
   }

   // buttons starting positions and width
   but_maxwidth = 0;
   i = 0;
   while (wmsg->button[i].text != NULL)
   {
      n = strlen(wmsg->button[i].text);
      but_maxwidth += (2 + n + wmsg->button[i].right_spaces) *
                      wmsg->font_width;

      // next buttton
      i++;
   }
   but_maxwidth += 2 * wmsg->font_width;
   if (but_maxwidth > max_width)
      max_width = but_maxwidth;
   curr_height += line_height * 5;


   // draw message window, without buttons
   wx0 = (glb_config.screen.width  - max_width)  / 2;
   bx0 = wx0 + wmsg->font_width + ((max_width - but_maxwidth) / 2);
   wy0 = (glb_config.screen.height - curr_height) / 2;
   wx1 = wx0 + max_width;
   wy1 = wy0 + curr_height;
   by0 = wy1 - line_height * 4;
   rectfill(glb_ds1edit.screen_buff, wx0, wy0, wx1, wy1, wmsg->col_win.bg);
   x = wx0 + wmsg->font_width;
   y = wy0 + wmsg->font_height;

   // title
   if (wmsg->title != NULL)
   {
      rectfill(glb_ds1edit.screen_buff, wx0, wy0, wx1, wy0 + line_height*2, wmsg->col_title.bg);
      rect(glb_ds1edit.screen_buff, wx0, wy0, wx1, wy0 + line_height*2, wmsg->col_title.fg);
      hline(glb_ds1edit.screen_buff, wx0, wy0 + line_height*2, wx1, wmsg->col_win.fg);

      text_mode(wmsg->col_title.bg);
      textout(glb_ds1edit.screen_buff, font, wmsg->title, x, y, wmsg->col_title.fg);
      y += line_height * 2;
   }

   // text
   tmp[1] = 0x00;
   n = strlen(wmsg->text) + 1;
   text_mode(wmsg->col_text.bg);
   for (i=0; i < n; i++)
   {
      if ( (wmsg->text[i] != 0x0A) &&
           (wmsg->text[i] != 0x0D) &&
           (wmsg->text[i] != 0x00)
         )
      {
         // draw char
         tmp[0] = wmsg->text[i];
         textout(glb_ds1edit.screen_buff, font, tmp, x, y, wmsg->col_text.fg);
         x += wmsg->font_width;
      }
      else
      {
         x  = wx0 + wmsg->font_width;
         y += line_height;
      }
   }

   // border of main window
   rect(glb_ds1edit.screen_buff, wx0, wy0, wx1, wy1, wmsg->col_win.fg);


   // mouse background
   // show_mouse(NULL);
   mx = mouse_x;
   my = mouse_y;
   mb = mouse_b;

   // main loop
   text_mode(-1);
   while ( ! done)
   {
      // handle keyboard shortcuts
      i = 0;
      while ((wmsg->button[i].text != NULL) && ( ! done) )
      {
         for (s=0; s < MW_SHORTCUT_NUM; s++)
         {
            // is this shortcut is pressed ?
            // check all keys of that combination
            all_keys = TRUE;
            if (wmsg->button[i].shortcut[s].key[0] != 0)
            {
               for (k=0; k < MW_COMBINATION_KEY_NUM; k++)
               {
                  if (wmsg->button[i].shortcut[s].key[k])
                  {
                     if ( ! key[ wmsg->button[i].shortcut[s].key[k] ])
                        all_keys = FALSE;
                  }
               }
            }
            else
               all_keys = FALSE;
            if (all_keys == TRUE)
            {
               done = TRUE;
               ret  = i;

               // wait for all keys of the shortcut to not be pressed
               while (all_keys == TRUE)
               {
                  all_keys = FALSE;
                  for (k=0; k < MW_COMBINATION_KEY_NUM; k++)
                  {
                     if (wmsg->button[i].shortcut[s].key[k])
                     {
                        if (key[ wmsg->button[i].shortcut[s].key[k] ])
                           all_keys = TRUE;
                     }
                  }
               }
            }
         }

         // next button
         i++;
      }

      // draw all buttons
      i  = 0;
      x  = bx0;
      y  = by0;
      y2 = y + (3 * line_height);
      while (wmsg->button[i].text != NULL)
      {
         n  = strlen(wmsg->button[i].text);
         x2 = x + ((n + 2) * wmsg->font_width);

         // mouse over the button ?
         if ((mx >= x) && (mx <= x2) && (my >= y) && (my <= y2))
         {
            // over the button
            bg     = wmsg->button[i].on.bg;
            fg     = wmsg->button[i].on.fg;
            border = wmsg->button[i].on.border;

            // mouse button pressed
            if (mb)
            {
               while (mb)
                  mb = mouse_b;
               done = TRUE;
               ret  = i;
            }
         }
         else
         {
            // not over this button
            bg     = wmsg->button[i].off.bg;
            fg     = wmsg->button[i].off.fg;
            border = wmsg->button[i].off.border;
         }

         // draw current button
         rectfill(glb_ds1edit.screen_buff, x, y, x2, y2, bg);
         textprintf(
            glb_ds1edit.screen_buff,
            font,
            x + wmsg->font_width,
            y + line_height + 2,
            fg,
            "%s", wmsg->button[i].text
         );
         rect(glb_ds1edit.screen_buff, x, y, x2, y2, border);
         
         // next button
         x = x2 + (wmsg->font_width * wmsg->button[i].right_spaces);
         i++;
      }

      misc_draw_screen(mx, my);

      // new mouse state
      mx = mouse_x;
      my = mouse_y;
      mb = mouse_b;
   }

   text_mode(-1);

   return ret;
}
