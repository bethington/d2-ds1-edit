#include "structs.h"
#include "wMsg.h"
#include "msg_quit.h"


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
