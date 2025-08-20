#include "structs.h"
#include "editpath.h"
#include "misc.h"


// ==============================================================================
int editpath_mouse_in(int ds1_idx, int mx, int my)
{
   PATH_EDIT_WIN_S * pwin = & glb_ds1[ds1_idx].path_edit_win;

   
   if ( (mx >= pwin->x0) && (mx <= pwin->x0 + pwin->w - 1) &&
        (my >= pwin->y0) && (my <= pwin->y0 + pwin->h - 1))
     return TRUE;
  else
     return FALSE;
}


// ==============================================================================
int editpath_mouse_cmdbut_in(int ds1_idx, int mx, int my)
{
   PATH_EDIT_WIN_S * pwin = & glb_ds1[ds1_idx].path_edit_win;

   
   if ( (mx >= pwin->but_x0) && (mx <= pwin->but_x0 + pwin->but_w - 1) &&
        (my >= pwin->but_y0) && (my <= pwin->but_y0 + pwin->but_h - 1))
     return TRUE;
  else
     return FALSE;
}


// ==============================================================================
void editpath_init(int ds1_idx)
{
   PATH_EDIT_WIN_S * pwin = & glb_ds1[ds1_idx].path_edit_win;
   int             i;


   pwin->but_x0 = 10;
   pwin->but_y0 = 2 * 10;
   pwin->but_w  = 49 * 8;
   pwin->but_h  = 80;

   pwin->x0 = 10;
   pwin->y0 = pwin->but_y0 + pwin->but_h + 10;
   pwin->w  = 23 * 8;
   pwin->h  = glb_config.screen.height - pwin->y0 - 20;

   pwin->c_black     = makecol(  0,   0,   0);
   pwin->c_white     = makecol(255, 255, 255);
   pwin->c_green     = makecol(  0, 255,   0);
   pwin->c_darkgreen = makecol(  0, 128,   0);
   pwin->c_yellow    = makecol(255, 255,   0);
   pwin->c_gray      = makecol(128, 128, 128);
   pwin->c_darkgray  = makecol( 64,  64,  64);

   // OK button
   pwin->button[PEB_OK].show           = FALSE;
   pwin->button[PEB_OK].text           = "OK";
   pwin->button[PEB_OK].text_x0        = 3 * 8 + 1;
   pwin->button[PEB_OK].x0             = 10;
   pwin->button[PEB_OK].y0             = 10;
   pwin->button[PEB_OK].w              = 8 * 8;
   pwin->button[PEB_OK].h              = 20;
   pwin->button[PEB_OK].col_on.fg      = pwin->c_black;
   pwin->button[PEB_OK].col_on.bg      = pwin->c_darkgreen;
   pwin->button[PEB_OK].col_on.border  = pwin->c_white;
   pwin->button[PEB_OK].col_off.fg     = pwin->c_green;
   pwin->button[PEB_OK].col_off.bg     = pwin->c_black;
   pwin->button[PEB_OK].col_off.border = pwin->c_green;

   // CANCEL button
   pwin->button[PEB_CANCEL].show           = FALSE;
   pwin->button[PEB_CANCEL].text           = "CANCEL";
   pwin->button[PEB_CANCEL].text_x0        = 1 * 8 + 1;
   pwin->button[PEB_CANCEL].x0             = 80;
   pwin->button[PEB_CANCEL].y0             = 10;
   pwin->button[PEB_CANCEL].w              = 8 * 8;
   pwin->button[PEB_CANCEL].h              = 20;
   pwin->button[PEB_CANCEL].col_on.fg      = pwin->c_black;
   pwin->button[PEB_CANCEL].col_on.bg      = pwin->c_darkgreen;
   pwin->button[PEB_CANCEL].col_on.border  = pwin->c_white;
   pwin->button[PEB_CANCEL].col_off.fg     = pwin->c_green;
   pwin->button[PEB_CANCEL].col_off.bg     = pwin->c_black;
   pwin->button[PEB_CANCEL].col_off.border = pwin->c_green;

   // ALLNEW button
   pwin->button[PEB_ALLNEW].show           = TRUE;
   pwin->button[PEB_ALLNEW].text           = "ALL NEW";
   pwin->button[PEB_ALLNEW].text_x0        = 1 * 8 + 1;
   pwin->button[PEB_ALLNEW].x0             = 10;
   pwin->button[PEB_ALLNEW].y0             = 50;
   pwin->button[PEB_ALLNEW].w              = 9 * 8;
   pwin->button[PEB_ALLNEW].h              = 20;
   pwin->button[PEB_ALLNEW].col_on.fg      = pwin->c_black;
   pwin->button[PEB_ALLNEW].col_on.bg      = pwin->c_darkgreen;
   pwin->button[PEB_ALLNEW].col_on.border  = pwin->c_white;
   pwin->button[PEB_ALLNEW].col_off.fg     = pwin->c_green;
   pwin->button[PEB_ALLNEW].col_off.bg     = pwin->c_black;
   pwin->button[PEB_ALLNEW].col_off.border = pwin->c_green;

   // STOP button
   pwin->button[PEB_STOP].show           = FALSE;
   pwin->button[PEB_STOP].text           = "STOP";
   pwin->button[PEB_STOP].text_x0        = 2 * 8 + 5;
   pwin->button[PEB_STOP].x0             = 10;
   pwin->button[PEB_STOP].y0             = 50;
   pwin->button[PEB_STOP].w              = 9 * 8;
   pwin->button[PEB_STOP].h              = 20;
   pwin->button[PEB_STOP].col_on.fg      = pwin->c_black;
   pwin->button[PEB_STOP].col_on.bg      = pwin->c_darkgreen;
   pwin->button[PEB_STOP].col_on.border  = pwin->c_white;
   pwin->button[PEB_STOP].col_off.fg     = pwin->c_green;
   pwin->button[PEB_STOP].col_off.bg     = pwin->c_black;
   pwin->button[PEB_STOP].col_off.border = pwin->c_green;

   // SETACTION button
   pwin->button[PEB_SETACTION].show           = TRUE;
   pwin->button[PEB_SETACTION].text           = "(Enter) SET ACTION";
   pwin->button[PEB_SETACTION].text_x0        = 1 * 8 + 1;
   pwin->button[PEB_SETACTION].x0             = 160;
   pwin->button[PEB_SETACTION].y0             = 50;
   pwin->button[PEB_SETACTION].w              = 20 * 8 + 1;
   pwin->button[PEB_SETACTION].h              = 20;
   pwin->button[PEB_SETACTION].col_on.fg      = pwin->c_black;
   pwin->button[PEB_SETACTION].col_on.bg      = pwin->c_darkgreen;
   pwin->button[PEB_SETACTION].col_on.border  = pwin->c_white;
   pwin->button[PEB_SETACTION].col_off.fg     = pwin->c_green;
   pwin->button[PEB_SETACTION].col_off.bg     = pwin->c_black;
   pwin->button[PEB_SETACTION].col_off.border = pwin->c_green;

   if (pwin->obj_idx == -1)
   {
      for (i=0; i < PEB_MAX; i++)
      {
         if (i != PEB_SETACTION)
            pwin->button[i].show = FALSE;
      }
   }

   pwin->curr_action = 1;
}


// ==============================================================================
void editpath_search_selected_obj(int ds1_idx)
{
   PATH_EDIT_WIN_S * pwin  = & glb_ds1[ds1_idx].path_edit_win;
   OBJ_S           * pobj  = NULL;
   int             o, nb=0, new_obj_idx=0, i;


   if (glb_ds1[ds1_idx].path_edit_win.obj_dirty != FALSE)
      return;

   for (o=0; o < glb_ds1[ds1_idx].obj_num; o++)
   {
      pobj = & glb_ds1[ds1_idx].obj[o];
      if (IS_SELECTED(pobj->flags) || IS_SELECTED(pobj->label.flags))
      {
         nb++;
         new_obj_idx = o;
      }
   }
   if (nb == 1)
   {
      if (pwin->obj_idx != new_obj_idx)
      {
         pwin->obj_idx = new_obj_idx;
         pobj          = & glb_ds1[ds1_idx].obj[new_obj_idx];
         for (i=0; i < WINDS1EDIT_PATH_MAX; i++)
         {
            pwin->path_copy[i].x      = pobj->path[i].x;
            pwin->path_copy[i].y      = pobj->path[i].y;
            pwin->path_copy[i].action = pobj->path[i].action;
         }
         pwin->path_copy_num = pobj->path_num;
      }

      // activate the ALLNEW button
      pwin->button[PEB_ALLNEW].show = TRUE;
   }
   else
   {
      pwin->obj_idx = -1;
      for (i=0; i < PEB_MAX; i++)
      {
         if (i != PEB_SETACTION)
            pwin->button[i].show = FALSE;
      }
   }
}


// ==============================================================================
void editpath_enter_action(int ds1_idx, long * paction)
{
   PATH_EDIT_WIN_S * pwin  = & glb_ds1[ds1_idx].path_edit_win;
   int             x1, y1, w, h, x2, y2,
                   tx1, ty1, tx2, ty2, tw, th,
                   done, add_x, nb_char;
   BITMAP          * bmp;
   long            lvalue = 0;
   char            svalue[30];


   // show_mouse(NULL);

   w  = 200;
   h  = 80;
   x1 = (glb_config.screen.width  - w) / 2;
   y1 = (glb_config.screen.height - h) / 2;
   x2 = x1 + w - 1;
   y2 = y1 + h - 1;

   tw  = 11 * 8 + 4;
   th  = 16;
   tx1 = x1 + (w  - tw) / 2;
   ty1 = y1 + 40;
   tx2 = tx1 + tw - 1;
   ty2 = ty1 + th - 1;

   nb_char = strlen(ltoa(lvalue, svalue, 10));
   
   // copy background
   bmp = create_bitmap(w, h);
   blit(glb_ds1edit.screen_buff, bmp, x1, y1, 0, 0, w, h);

   // draw window
   rectfill(glb_ds1edit.screen_buff, x1, y1, x2, y2, pwin->c_black);
   rect(glb_ds1edit.screen_buff, x1, y1, x2, y2, pwin->c_white);

   // draw title
   rectfill(glb_ds1edit.screen_buff, x1, y1, x2, y1 + 18, pwin->c_green);
   textout(glb_ds1edit.screen_buff, font, "Type new ACTION value", x1 + 8, y1+6, pwin->c_black);
   rect(glb_ds1edit.screen_buff, x1, y1, x2, y1 + 18, pwin->c_white);

   // main loop
   done = FALSE;
   while ( ! done)
   {
      if (key[KEY_ENTER] || key[KEY_ENTER_PAD])
      {
         while (key[KEY_ENTER] || key[KEY_ENTER_PAD])
         {}
         done      = TRUE;
         * paction = lvalue;
      }
      else if (key[KEY_ESC])
      {
         while (key[KEY_ESC])
         {}
         done = TRUE;
      }
      else if (key[KEY_MINUS] || key[KEY_MINUS_PAD] || key[KEY_EQUALS])
      {
         while (key[KEY_MINUS] || key[KEY_MINUS_PAD] || key[KEY_EQUALS])
         {}
         lvalue = -lvalue;
      }
      else if ((lvalue >= -99999999) && (lvalue <= 99999999))
      {
         if (key[KEY_1] || key[KEY_1_PAD])
         {
            while (key[KEY_1] || key[KEY_1_PAD])
            {}
            lvalue *= 10;
            if (lvalue < 0)
               lvalue -= 1;
            else
               lvalue += 1;
         }
         else if (key[KEY_2] || key[KEY_2_PAD])
         {
            while (key[KEY_2] || key[KEY_2_PAD])
            {}
            lvalue *= 10;
            if (lvalue < 0)
               lvalue -= 2;
            else
               lvalue += 2;
         }
         else if (key[KEY_3] || key[KEY_3_PAD])
         {
            while (key[KEY_3] || key[KEY_3_PAD])
            {}
            lvalue *= 10;
            if (lvalue < 0)
               lvalue -= 3;
            else
               lvalue += 3;
         }
         else if (key[KEY_4] || key[KEY_4_PAD])
         {
            while (key[KEY_4] || key[KEY_4_PAD])
            {}
            lvalue *= 10;
            if (lvalue < 0)
               lvalue -= 4;
            else
               lvalue += 4;
         }
         else if (key[KEY_5] || key[KEY_5_PAD])
         {
            while (key[KEY_5] || key[KEY_5_PAD])
            {}
            lvalue *= 10;
            if (lvalue < 0)
               lvalue -= 5;
            else
               lvalue += 5;
         }
         else if (key[KEY_6] || key[KEY_6_PAD])
         {
            while (key[KEY_6] || key[KEY_6_PAD])
            {}
            lvalue *= 10;
            if (lvalue < 0)
               lvalue -= 6;
            else
               lvalue += 6;
         }
         else if (key[KEY_7] || key[KEY_7_PAD])
         {
            while (key[KEY_7] || key[KEY_7_PAD])
            {}
            lvalue *= 10;
            if (lvalue < 0)
               lvalue -= 7;
            else
               lvalue += 7;
         }
         else if (key[KEY_8] || key[KEY_8_PAD])
         {
            while (key[KEY_8] || key[KEY_8_PAD])
            {}
            lvalue *= 10;
            if (lvalue < 0)
               lvalue -= 8;
            else
               lvalue += 8;
         }
         else if (key[KEY_9] || key[KEY_9_PAD])
         {
            while (key[KEY_9] || key[KEY_9_PAD])
            {}
            lvalue *= 10;
            if (lvalue < 0)
               lvalue -= 9;
            else
               lvalue += 9;
         }
         else if (key[KEY_0] || key[KEY_0_PAD])
         {
            while (key[KEY_0] || key[KEY_0_PAD])
            {}
            lvalue *= 10;
            if (lvalue < 0)
               lvalue -= 0;
            else
               lvalue += 0;
         }
      }

      // draw user input field
      nb_char = strlen(ltoa(lvalue, svalue, 10));
      add_x = 2 + (11 - nb_char) * 8;
      rectfill(glb_ds1edit.screen_buff, tx1, ty1, tx2, ty2, pwin->c_darkgray);
      textprintf(glb_ds1edit.screen_buff, font,
         tx1 + add_x,
         ty1 + 4,
         pwin->c_white,
         "%li",
         lvalue
      );

      // draw screen
      if ( ! done)
      {         
         vsync();
         blit(glb_ds1edit.screen_buff, screen, x1, y1, x1, y1, w, h);
      }
   }

   // restore backgpound
   vsync();
   blit(bmp, screen, 0, 0, x1, y1, w, h);

   // show_mouse(screen);
   destroy_bitmap(bmp);
}



// ==============================================================================
void editpath_draw(int ds1_idx, int mx, int my, int mb, long tx, long ty)
{
   PATH_EDIT_WIN_S * pwin  = & glb_ds1[ds1_idx].path_edit_win;
   PATH_EDIT_BUT_S * pbut  = NULL;
   OBJ_S           * pobj  = NULL;
   PATH_S          * ppath = NULL;
   int             x1, y1, x2, y2, i, done, y, xt;

   
   // first init
   if (pwin->w == 0)
      editpath_init(ds1_idx);

   // selected object
   if (pwin->obj_idx != -1)
      pobj = & glb_ds1[ds1_idx].obj[pwin->obj_idx];

   // process the current mode
   if (pwin->mode)
   {
      switch(pwin->mode)
      {
         case PEM_NONE :
            break;

         case PEM_ALLNEW :
            if (mb & 1)
            {
               if (editpath_mouse_cmdbut_in(ds1_idx, mx, my) == FALSE)
               {
                  if (editpath_mouse_in(ds1_idx, mx, my) == FALSE)
                  {
                     // wait until left mouse button is not pressed anymore
                     while (mouse_b & 1)
                     {}
                     mb = 0;

                     // add this sub-tile to the path list of the object
                     if (pobj->path_num < WINDS1EDIT_PATH_MAX)
                     {
                        i = pobj->path_num;
                        pobj->path[i].x      = tx;
                        pobj->path[i].y      = ty;
                        pobj->path[i].action = pwin->curr_action;
                        pobj->path[i].flags  = 0;
                        pobj->path_num++;
                     }
                  }
               }
            }
            break;
      }
   }

   // ---------------------------------
   // draw paths command buttons window
   // ---------------------------------
   x1 = pwin->but_x0;
   y1 = pwin->but_y0;
   x2 = x1 + pwin->but_w;
   y2 = y1 + pwin->but_h;
   rectfill(glb_ds1edit.screen_buff, x1, y1, x2, y2, pwin->c_black);
   if (editpath_mouse_cmdbut_in(ds1_idx, mx, my))
      rect(glb_ds1edit.screen_buff, x1, y1, x2, y2, pwin->c_yellow);
   else
      rect(glb_ds1edit.screen_buff, x1, y1, x2, y2, pwin->c_white);

   // process the command of the left click mouse
   if (mb & 1)
   {
      // wait until no mouse button is pressed anymore
      while (mouse_b & 1)
      {}
      mb = 0;

      // search which button to update
      done = FALSE;
      for (i=0; (i < PEB_MAX) && ! done; i++)
      {
         pbut = & pwin->button[i];

         if (pbut->show != TRUE)
            continue;

         x1 = pwin->but_x0 + pbut->x0;
         y1 = pwin->but_y0 + pbut->y0;
         x2 = x1 + pbut->w;
         y2 = y1 + pbut->h;

         if ((mx >= x1) && (mx <= x2) && (my >= y1) && (my <= y2))
         {
            switch (i)
            {
               case PEB_OK :
                  // keep all datas
                  // copy them into the backup
                  for (i=0; i < WINDS1EDIT_PATH_MAX; i++)
                  {
                     pwin->path_copy[i].x      = pobj->path[i].x;
                     pwin->path_copy[i].y      = pobj->path[i].y;
                     pwin->path_copy[i].action = pobj->path[i].action;
                  }
                  pwin->path_copy_num = pobj->path_num;

                  // update button activation
                  pwin->button[PEB_OK].show     = FALSE;
                  pwin->button[PEB_CANCEL].show = FALSE;
                  pwin->button[PEB_ALLNEW].show = TRUE;
                  pwin->button[PEB_STOP].show   = FALSE;

                  done = TRUE;
                  pwin->mode = PEM_NONE;
                  pwin->obj_dirty = FALSE;
                  break;

               case PEB_CANCEL :
                  // put back the original datas
                  for (i=0; i < WINDS1EDIT_PATH_MAX; i++)
                  {
                     pobj->path[i].x      = pwin->path_copy[i].x;
                     pobj->path[i].y      = pwin->path_copy[i].y;
                     pobj->path[i].action = pwin->path_copy[i].action;
                     pobj->path[i].flags  = 0;
                  }
                  pobj->path_num = pwin->path_copy_num;

                  // update button activation
                  pwin->button[PEB_OK].show     = FALSE;
                  pwin->button[PEB_CANCEL].show = FALSE;
                  pwin->button[PEB_ALLNEW].show = TRUE;
                  pwin->button[PEB_STOP].show   = FALSE;

                  done = TRUE;
                  pwin->mode = PEM_NONE;
                  pwin->obj_dirty = FALSE;
                  break;

               case PEB_ALLNEW :
                  // update button activation
                  pwin->button[PEB_OK].show     = TRUE;
                  pwin->button[PEB_CANCEL].show = TRUE;
                  pwin->button[PEB_ALLNEW].show = FALSE;
                  pwin->button[PEB_STOP].show   = TRUE;

                  done = TRUE;
                  pwin->mode = PEM_ALLNEW;
                  pwin->obj_dirty = TRUE;
                  pobj->path_num = 0;
                  break;

               case PEB_STOP :
                  // update button activation
                  pwin->button[PEB_OK].show     = TRUE;
                  pwin->button[PEB_CANCEL].show = TRUE;
                  pwin->button[PEB_ALLNEW].show = TRUE;
                  pwin->button[PEB_STOP].show   = FALSE;

                  done = TRUE;
                  pwin->mode = PEM_NONE;
                  break;

               case PEB_SETACTION :
                  editpath_enter_action(ds1_idx, & pwin->curr_action);
                  break;
            }
         }
      }
   }

   if (key[KEY_ENTER] || key[KEY_ENTER_PAD])
   {
      while (key[KEY_ENTER] || key[KEY_ENTER_PAD])
      {}
      editpath_enter_action(ds1_idx, & pwin->curr_action);
   }

   // draw action value
   textprintf(glb_ds1edit.screen_buff, font,
      pwin->x0 + 160,
      pwin->x0 + 27,
      pwin->c_gray,
      "Current Action : %li",
      pwin->curr_action
   );

   // draw buttons
   for (i=0; i < PEB_MAX; i++)
   {
      pbut = & pwin->button[i];

      if (pbut->show != TRUE)
         continue;

      x1 = pwin->but_x0 + pbut->x0;
      xt = x1 + pbut->text_x0;
      y1 = pwin->but_y0 + pbut->y0;
      x2 = x1 + pbut->w;
      y2 = y1 + pbut->h;

      if ((mx >= x1) && (mx <= x2) && (my >= y1) && (my <= y2))
      {
         rectfill(glb_ds1edit.screen_buff, x1, y1, x2, y2, pbut->col_on.bg);
         textout(glb_ds1edit.screen_buff, font, pbut->text, xt, y1+7, pbut->col_on.fg);
         rect(glb_ds1edit.screen_buff, x1, y1, x2, y2, pbut->col_on.border);
      }
      else
      {
         rectfill(glb_ds1edit.screen_buff, x1, y1, x2, y2, pbut->col_off.bg);
         textout(glb_ds1edit.screen_buff, font, pbut->text, xt, y1+7, pbut->col_off.fg);
         rect(glb_ds1edit.screen_buff, x1, y1, x2, y2, pbut->col_off.border);
      }
   }

   // ---------------------------------
   // draw paths window
   // ---------------------------------
   x1 = pwin->x0;
   y1 = pwin->y0;
   x2 = x1 + pwin->w;
   y2 = y1 + pwin->h;
   rectfill(glb_ds1edit.screen_buff, x1, y1, x2, y2, pwin->c_black);
   if (editpath_mouse_in(ds1_idx, mx, my))
      rect(glb_ds1edit.screen_buff, x1, y1, x2, y2, pwin->c_yellow);
   else
      rect(glb_ds1edit.screen_buff, x1, y1, x2, y2, pwin->c_white);

   // draw labels
   pwin->c_green = makecol(0, 255, 0);
   textout(glb_ds1edit.screen_buff, font,
      "Path    X    Y Action",
      x1 + 8, y1 + 10, pwin->c_green
   );
   textout(glb_ds1edit.screen_buff, font,
      "---- ---- ---- ------",
      x1 + 8, y1 + 20, pwin->c_green
   );

/*
textprintf(glb_ds1edit.screen_buff, font,
   pwin->x0 + 8, 400,
   pwin->c_white,
   "obj_idx   = %i",
   pwin->obj_idx
);
textprintf(glb_ds1edit.screen_buff, font,
   pwin->x0 + 8, 415,
   pwin->c_white,
   "obj_dirty = %i",
   pwin->obj_dirty
);
textprintf(glb_ds1edit.screen_buff, font,
   pwin->x0 + 8, 435,
   pwin->c_white,
   "OK.show     = %i",
   pwin->button[PEB_OK].show
);
textprintf(glb_ds1edit.screen_buff, font,
   pwin->x0 + 8, 445,
   pwin->c_white,
   "CANCEL.show = %i",
   pwin->button[PEB_CANCEL].show
);
textprintf(glb_ds1edit.screen_buff, font,
   pwin->x0 + 8, 455,
   pwin->c_white,
   "ALLNEW.show = %i",
   pwin->button[PEB_ALLNEW].show
);
textprintf(glb_ds1edit.screen_buff, font,
   pwin->x0 + 8, 465,
   pwin->c_white,
   "STOP.show   = %i",
   pwin->button[PEB_STOP].show
);
*/

   // if invalid object index, empty window
   if ((pwin->obj_idx < 0) || (pwin->obj_idx >= glb_ds1[ds1_idx].current_obj_max))
      return;

   // draw path datas
   done  = FALSE;
   i     = pwin->path_start_idx;
   y     = y1 + 35;
   ppath = & pobj->path[i];

   if (i >= pobj->path_num)
      done = TRUE;

   while ( ! done)
   {
      if (y + 10 < pwin->y0 + pwin->h)
      {
         // can be draw
         textprintf(glb_ds1edit.screen_buff, font,
            pwin->x0 + 8, y,
            pwin->c_white,
            "%4i %4li %4li %6li",
            i + 1,
            ppath->x,
            ppath->y,
            ppath->action
         );
         i++;
         if (i >= pobj->path_num)
            done = TRUE;
         y += 20;
         ppath++;
      }
      else
         done = TRUE;
   }
}
