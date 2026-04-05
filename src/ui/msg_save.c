#include "structs.h"
#include "wMsg.h"
#include "msg_save.h"


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
