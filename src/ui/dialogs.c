#include <string.h>
#include "structs.h"
#include "error.h"
#include "dialogs.h"
#include "misc.h"


// ==========================================================================
// wmsg_main - generic message window with buttons
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
   a5_rectfill(glb_ds1edit.screen_buff, wx0, wy0, wx1, wy1, wmsg->col_win.bg);
   x = wx0 + wmsg->font_width;
   y = wy0 + wmsg->font_height;

   // title
   if (wmsg->title != NULL)
   {
      a5_rectfill(glb_ds1edit.screen_buff, wx0, wy0, wx1, wy0 + line_height*2, wmsg->col_title.bg);
      a5_rect(glb_ds1edit.screen_buff, wx0, wy0, wx1, wy0 + line_height*2, wmsg->col_title.fg);
      a5_hline(glb_ds1edit.screen_buff, wx0, wy0 + line_height*2, wx1, wmsg->col_win.fg);

      a5_textout(glb_ds1edit.screen_buff, font, wmsg->title, x, y, wmsg->col_title.fg);
      y += line_height * 2;
   }

   // text
   tmp[1] = 0x00;
   n = strlen(wmsg->text) + 1;
   for (i=0; i < n; i++)
   {
      if ( (wmsg->text[i] != 0x0A) &&
           (wmsg->text[i] != 0x0D) &&
           (wmsg->text[i] != 0x00)
         )
      {
         // draw char
         tmp[0] = wmsg->text[i];
         a5_textout(glb_ds1edit.screen_buff, font, tmp, x, y, wmsg->col_text.fg);
         x += wmsg->font_width;
      }
      else
      {
         x  = wx0 + wmsg->font_width;
         y += line_height;
      }
   }

   // border of main window
   a5_rect(glb_ds1edit.screen_buff, wx0, wy0, wx1, wy1, wmsg->col_win.fg);


   // mouse background
   // show_mouse(NULL);
   mx = a5_mouse_x;
   my = a5_mouse_y;
   mb = a5_mouse_b;

   // main loop
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
                     if ( ! key_pressed( wmsg->button[i].shortcut[s].key[k] ))
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
                  al_rest(0.01);
                  al_get_keyboard_state(&a5_kb_state);
                  all_keys = FALSE;
                  for (k=0; k < MW_COMBINATION_KEY_NUM; k++)
                  {
                     if (wmsg->button[i].shortcut[s].key[k])
                     {
                        if (key_pressed( wmsg->button[i].shortcut[s].key[k] ))
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
               { al_rest(0.01); al_get_mouse_state(&a5_ms_state); mb = a5_mouse_b; }
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
         a5_rectfill(glb_ds1edit.screen_buff, x, y, x2, y2, bg);
         a5_textprintf(
            glb_ds1edit.screen_buff,
            font,
            x + wmsg->font_width,
            y + line_height + 2,
            fg,
            "%s", wmsg->button[i].text
         );
         a5_rect(glb_ds1edit.screen_buff, x, y, x2, y2, border);

         // next button
         x = x2 + (wmsg->font_width * wmsg->button[i].right_spaces);
         i++;
      }

      misc_draw_screen(mx, my);

      // poll new input state
      al_get_keyboard_state(&a5_kb_state);
      al_get_mouse_state(&a5_ms_state);
      mx = a5_mouse_x;
      my = a5_mouse_y;
      mb = a5_mouse_b;
   }


   return ret;
}


// ==========================================================================
// Message Window that appear when user ask to QUIT
// return value :
//    -1 : error
//     0 : save all & quit
//     1 : quit
//     2 : cancel
int msg_quit_main(void)
{
   WMSG_BUT_S buttons[4]; // 4th button is NULL, needed
   WMSG_S     wmsg_quit;
   int        col_black    = makecol(0, 0, 0),
              col_midgreen = makecol(0, 128, 0),
              col_white    = makecol(255, 255, 255),
              col_green    = makecol(0, 255, 0);


   // init buttons
   memset(buttons, 0, sizeof(buttons));

   // button "Save all & Quit"
   buttons[0].text               = "(S) SAVE ALL & QUIT";
   buttons[0].right_spaces       = 3;
   buttons[0].on.fg              = col_black;
   buttons[0].on.bg              = col_midgreen;
   buttons[0].on.border          = col_white;
   buttons[0].off.fg             = col_green;
   buttons[0].off.bg             = col_black;
   buttons[0].off.border         = col_green;
   buttons[0].shortcut[0].key[0] = KEY_S;

   // button "Quit"
   buttons[1].text               = "  (Q) QUIT  ";
   buttons[1].right_spaces       = 3;
   buttons[1].on.fg              = col_black;
   buttons[1].on.bg              = col_midgreen;
   buttons[1].on.border          = col_white;
   buttons[1].off.fg             = col_green;
   buttons[1].off.bg             = col_black;
   buttons[1].off.border         = col_green;
   buttons[1].shortcut[0].key[0] = KEY_A;
   buttons[1].shortcut[1].key[0] = KEY_Q;

   // button "Cancel"
   buttons[2].text               = " (Esc) CANCEL ";
   buttons[2].right_spaces       = 0;
   buttons[2].on.fg              = col_black;
   buttons[2].on.bg              = col_midgreen;
   buttons[2].on.border          = col_white;
   buttons[2].off.fg             = col_green;
   buttons[2].off.bg             = col_black;
   buttons[2].off.border         = col_green;
   buttons[2].shortcut[0].key[0] = KEY_ESC;

   // window setting
   wmsg_quit.title        = "WARNING";
   wmsg_quit.text         = "\nYou are about to quit the DS1 Editor. What do you want to do ?";
   wmsg_quit.font_width   = 8;
   wmsg_quit.font_height  = 8;
   wmsg_quit.col_win.bg   = col_black;
   wmsg_quit.col_win.fg   = col_white;
   wmsg_quit.col_title.bg = col_green;
   wmsg_quit.col_title.fg = col_black;
   wmsg_quit.col_text.bg  = -1;
   wmsg_quit.col_text.fg  = col_green;
   wmsg_quit.button       = & buttons[0];

   return wmsg_main( & wmsg_quit);
}


// ==========================================================================
// Message Window that appear after user asked to SAVE a ds1
// (this is only an informative window)
// return value :
//    -1 : error
//     0 : ok
int msg_save_main(void)
{
   WMSG_BUT_S buttons[2]; // 2nd button is NULL, needed
   WMSG_S     wmsg_save;
   int        col_black    = makecol(0, 0, 0),
              col_midgreen = makecol(0, 128, 0),
              col_white    = makecol(255, 255, 255),
              col_green    = makecol(0, 255, 0);


   // init buttons
   memset(buttons, 0, sizeof(buttons));

   // button "OK"
   buttons[0].text               = "  OK  ";
   buttons[0].right_spaces       = 0;
   buttons[0].on.fg              = col_black;
   buttons[0].on.bg              = col_midgreen;
   buttons[0].on.border          = col_white;
   buttons[0].off.fg             = col_green;
   buttons[0].off.bg             = col_black;
   buttons[0].off.border         = col_green;
   buttons[0].shortcut[0].key[0] = KEY_ENTER;
   buttons[0].shortcut[1].key[0] = KEY_ENTER_PAD;
   buttons[0].shortcut[2].key[0] = KEY_ESC;

   // window setting
   wmsg_save.title        = "Information";
   wmsg_save.text         = "\nYour DS1 has been saved";
   wmsg_save.font_width   = 8;
   wmsg_save.font_height  = 8;
   wmsg_save.col_win.bg   = col_black;
   wmsg_save.col_win.fg   = col_white;
   wmsg_save.col_title.bg = col_green;
   wmsg_save.col_title.fg = col_black;
   wmsg_save.col_text.bg  = -1;
   wmsg_save.col_text.fg  = col_green;
   wmsg_save.button       = & buttons[0];

   return wmsg_main( & wmsg_save);
}
