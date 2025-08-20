#include "structs.h"
#include "misc.h"
#include "error.h"
#include "anim.h"
#include "editpath.h"
#include "editobj.h"


// ==========================================================================
// helper of the qsort function, for object drawing order
int editobj_qsort_helper_drawing_order(const void * e1, const void * e2)
{
   int   ds1_idx = glb_ds1edit.obj_order_ds1_idx;
   int   i1      = * ((int *) e1),
         i2      = * ((int *) e2);
   OBJ_S * o1, * o2;


   // primary order key  : tile position
   // secondary oder key : modified sub-tile position in that tile
   o1 = & glb_ds1[ds1_idx].obj[i1];
   o2 = & glb_ds1[ds1_idx].obj[i2];
   if (o1->ty == o2->ty)
   {
      if (o1->tx == o2->tx)
      {
         if (glb_ds1edit.obj_sub_tile_order[o1->sx][o1->sy] ==
             glb_ds1edit.obj_sub_tile_order[o2->sx][o2->sy]
         )
            return i1 - i2;
         else
            return glb_ds1edit.obj_sub_tile_order[o1->sy][o1->sx] -
                   glb_ds1edit.obj_sub_tile_order[o2->sy][o2->sx];
      }
      else
         return o1->tx - o2->tx;
   }
   return o1->ty - o2->ty;
}


// ==========================================================================
// sort objects to prepare the drawing function
void editobj_set_drawing_order(int ds1_idx)
{
   OBJ_S * obj;
   int   i;


   // prepare the sort
   for (i=0; i < glb_ds1[ds1_idx].obj_num; i++)
   {
      obj = & glb_ds1[ds1_idx].obj[i];
      obj->tx = obj->x / 5L;
      obj->sx = obj->x % 5L;
      obj->ty = obj->y / 5L;
      obj->sy = obj->y % 5L;
   }

   // force unused objects to be in last position
   for (i = glb_ds1[ds1_idx].obj_num; i < glb_ds1[ds1_idx].current_obj_max; i++)
      glb_ds1[ds1_idx].obj[i].ty = 32000L;

   // re-init the drawing order table to the default
   for (i=0; i < glb_ds1[ds1_idx].current_obj_max; i++)
      glb_ds1[ds1_idx].drawing_order[i] = i;
      
   // sort it
   glb_ds1edit.obj_order_ds1_idx = ds1_idx;
   qsort(
      glb_ds1[ds1_idx].drawing_order,
      glb_ds1[ds1_idx].obj_num,
      sizeof(int),
      editobj_qsort_helper_drawing_order
   );
}


// ==========================================================================
// make the 'label' windows of all objects of 1 ds1
void editobj_make_obj_label(int ds1_idx)
{
   OBJ_LABEL_S * label;
   int         d, o, w, wtmp, h;
   long        type, id;


   for (o=0; o < glb_ds1[ds1_idx].obj_num; o++)
   {
      type  = glb_ds1[ds1_idx].obj[o].type;
      id    = glb_ds1[ds1_idx].obj[o].id;
      d     = glb_ds1[ds1_idx].obj[o].desc_idx;
      label = & glb_ds1[ds1_idx].obj[o].label;

      w = 16 * 8;     // 16 chars
      h =  2 * 8 + 2; // 2 char lines + 2 pixel lines for readibility

      if (d != -1)
      {
         wtmp = 8 * strlen(glb_ds1edit.obj_desc[d].desc);
         if (wtmp > w)
            w = wtmp;
      }
      label->rx    = 0;
      label->ry    = 0;
      label->w     = w;
      label->h     = h;
      label->flags = 0;
   }
}


// ==========================================================================
// draw the objects & the label windows of 1 ds1
//   if is_shadow is TRUE, only draw the ones which are moving (copy, move)
void editobj_draw_obj_lab(int ds1_idx, int is_shadow)
{
   OBJ_LABEL_S * label;
   int         d, dx, dy, o, cx, cy, w, h, p, loop, f1, f2;
   long        type, id, x, y;
   int         ox0, oy0, lx0, ly0;
   int         hm = glb_ds1[ds1_idx].height_mul;
   int         hd = glb_ds1[ds1_idx].height_div;
   int         x1, x2, x3, x4, y1, y2, y3;
   int         color1=0, color2=0, color3=0;
   int         o_idx;


   for (loop=0; loop<3; loop++)
   {
      // loop 0 : all objects (feet)
      // loop 1 : all lines
      // loop 2 : all label windows
      for (o_idx=0; o_idx < glb_ds1[ds1_idx].obj_num; o_idx++)
      {
         o     = glb_ds1[ds1_idx].drawing_order[o_idx];

         type  = glb_ds1[ds1_idx].obj[o].type;
         id    = glb_ds1[ds1_idx].obj[o].id;
         x     = glb_ds1[ds1_idx].obj[o].x;
         y     = glb_ds1[ds1_idx].obj[o].y;

         d     = glb_ds1[ds1_idx].obj[o].desc_idx;
         p     = glb_ds1[ds1_idx].obj[o].path_num;
         f1    = glb_ds1[ds1_idx].obj[o].flags;
         label = & glb_ds1[ds1_idx].obj[o].label;
         w     = label->w;
         h     = label->h;
         f2    = label->flags;

         if (is_shadow)
         {
            if ( ! IS_MOVING(f1 | f2))
               continue;
               
            color1 = 186; // background label
            color2 = 209; // sub-tile & line color
            color3 = 214; // label border color

            x = glb_ds1[ds1_idx].obj[o].old_x;
            y = glb_ds1[ds1_idx].obj[o].old_y;
         }
         else
         {
            x = glb_ds1[ds1_idx].obj[o].x;
            y = glb_ds1[ds1_idx].obj[o].y;
         }
         
         // objects sprite origin, in screen (upper-center corner)
         dx  = ((y-2) * -glb_ds1[ds1_idx].tile_w / 10) + ((x+3) * glb_ds1[ds1_idx].tile_w / 10);
         dy  = ((y-2) *  glb_ds1[ds1_idx].tile_h / 10) + ((x+3) * glb_ds1[ds1_idx].tile_h / 10);
         ox0 = dx - glb_ds1edit.win_preview.x0;
         oy0 = dy - glb_ds1edit.win_preview.y0 - 16;

         // labels origin, in screen (bottom-center border)
         if (is_shadow)
         {
            lx0 = ox0 + (label->old_rx * hm / hd);
            ly0 = oy0 + (label->old_ry * hm / hd);
         }
         else
         {
            lx0 = ox0 + (label->rx * hm / hd);
            ly0 = oy0 + (label->ry * hm / hd);
         }

         // color for NORMAL objects / labels
         if (is_shadow == FALSE)
         {
            color1 = 0;   // background label
            color2 = 255; // sub-tile & line color
            color3 = 255; // label border color
            if (IS_SELECTED(f1))
            {
               color1 = 118;
               color2 = 132;
               color3 = 132;
            }
            else if (IS_MOUSE_OVER(f1))
            {
               color2 = 129;
               if (IS_SELECTED(f2))
               {
                  color1 = 118;
                  color3 = 132;
               }
               else
                  color3 = 129;
            }
            else if (IS_SELECTED(f2))
            {
               color1 = 118;
               color3 = 132;
            }
            else if (IS_MOUSE_OVER(f2))
               color3 = 129;
         }
         
         switch(loop)
         {
            case 0 :
               // draw object sub-tile
               cx = x + 2;
               cy = y - 2;

               dx = (cy * -glb_ds1[ds1_idx].tile_w / 10) + (cx * glb_ds1[ds1_idx].tile_w / 10);
               dy = (cy *  glb_ds1[ds1_idx].tile_h / 10) + (cx * glb_ds1[ds1_idx].tile_h / 10);

               x1 = dx - glb_ds1edit.win_preview.x0;
               x2 = x1 + glb_ds1[ds1_idx].tile_w / 10 - 1;
               x3 = x1 + glb_ds1[ds1_idx].tile_w / 10;
               x4 = x1 + glb_ds1[ds1_idx].tile_w / 5 - 1;

               y1 = dy - glb_ds1edit.win_preview.y0;
               y2 = y1 + glb_ds1[ds1_idx].tile_h / 10 - 1;
               y3 = y1 + glb_ds1[ds1_idx].tile_h / 5 - 2;
   
               line(glb_ds1edit.screen_buff, x1, y2, x2, y1, color2);
               line(glb_ds1edit.screen_buff, x3, y1, x4, y2, color2);
               line(glb_ds1edit.screen_buff, x3, y3, x4, y2, color2);
               line(glb_ds1edit.screen_buff, x1, y2, x2, y3, color2);
               break;


            case 1 :
               // draw shadow lines
               line(glb_ds1edit.screen_buff, ox0+1, oy0+17, ox0+1, oy0+1, 0);
               line(glb_ds1edit.screen_buff, ox0+1, oy0+1,  lx0+1, ly0+1, 0);
               line(glb_ds1edit.screen_buff, lx0+1, ly0+1,  lx0+1, ly0-8, 0);
               
               // draw lines
               line(glb_ds1edit.screen_buff, ox0, oy0+16, ox0, oy0,   color2);
               line(glb_ds1edit.screen_buff, ox0, oy0,    lx0, ly0,   color2);
               line(glb_ds1edit.screen_buff, lx0, ly0,    lx0, ly0-9, color2);

               if (IS_SELECTED(f1))
               {
                  line(glb_ds1edit.screen_buff, ox0-1, oy0+16, ox0-1, oy0,   color2);
                  line(glb_ds1edit.screen_buff, ox0-1, oy0,    lx0-1, ly0,   color2);
                  line(glb_ds1edit.screen_buff, lx0-1, ly0,    lx0-1, ly0-9, color2);
                  rectfill(glb_ds1edit.screen_buff, ox0-2, oy0+14, ox0+1, oy0+16, color2);
               }
               else
                  rectfill(glb_ds1edit.screen_buff, ox0-1, oy0+14, ox0+1, oy0+16, color2);

               break;
            

            case 2 :
               // draw label
               x1 = lx0 - (w/2) - 2;
               x2 = x1 + w + 4;
               y1 = ly0 - 8 - h - 4;
               y2 = y1 + h + 2;

               if (is_shadow == FALSE)
               {
                  label->x0 = x1;
                  label->y0 = y1;
               }
         
               // draw ?
               if ((x1 >= glb_config.screen.width) ||
                   (y1 >= glb_config.screen.height) ||
                   (x2 < 0) || (y2 < 0))
                  break;
               
               rectfill(glb_ds1edit.screen_buff, x1, y1, x2, y2, color1);
               rect    (glb_ds1edit.screen_buff, x1, y1, x2, y2, color3);
      
               // desc
               x2 = x1 + 2 + (w/2);
               y2 = y1 + 2;
               if (d == -1)
                  textprintf(glb_ds1edit.screen_buff, font, x2 - 8, y2, 255, "??");
               else
               {
                  x2 -= (8 * strlen(glb_ds1edit.obj_desc[d].desc) / 2);
                  textprintf(glb_ds1edit.screen_buff, font, x2, y2, 255, "%s",
                     glb_ds1edit.obj_desc[d].desc);
               }
                  
               // object number
               x2 = x1 + 2;
               y2 += 8 + 2;
               textprintf(glb_ds1edit.screen_buff, font, x2, y2, is_shadow ? 255 : 111, "#%i", o);

               // Type
               if (id <= -100)     x2 = lx0 - 3 * 8;
               else if (id <= -10) x2 = lx0 - 2 * 8 - 4;
               else if (id < 0)    x2 = lx0 - 8 - 4;
               else if (id < 10)   x2 = lx0 - 8 - 4;
               else if (id < 100)  x2 = lx0 - 2 * 8;
               else                x2 = lx0 - 2 * 8 - 4;
               textprintf(glb_ds1edit.screen_buff, font, x2, y2, is_shadow ? 255 : 11, "%i", type);


               // Id
               x2 += 1*8;
               textprintf(glb_ds1edit.screen_buff, font, x2, y2, is_shadow ? 255 : 255, " %i", id);

               // paths number
               if (p < 10)
                  x2 = x1 + w + 4 - 2 * 8;
               else
                  x2 = x1 + w + 4 - 3 * 8;
               if (p == 0)
                  textprintf(glb_ds1edit.screen_buff, font, x2, y2, is_shadow ? 255 : 208, "P-");
               else
                  textprintf(glb_ds1edit.screen_buff, font, x2, y2, is_shadow ? 255 : 133, "P%i", p);
               break;
         }
      }
   }
}


// ==========================================================================
// search the description index of 1 object of 1 ds1, given its Type & Id
void editobj_make_obj_desc(int ds1_idx, int obj_idx)
{
   int act, type, id, i, * nb_type_per_act_ptr;


   act  = glb_ds1[ds1_idx].act;
   type = glb_ds1[ds1_idx].obj[obj_idx].type;
   id   = glb_ds1[ds1_idx].obj[obj_idx].id;

   // first try : search the exact act/type/id in data\obj.txt
   for (i=0; i < glb_ds1edit.obj_desc_num; i++)
   {
      if ( (glb_ds1edit.obj_desc[i].act  == act)  &&
           (glb_ds1edit.obj_desc[i].type == type) &&
           (glb_ds1edit.obj_desc[i].id   == id)
      )
      {
         // end
         glb_ds1[ds1_idx].obj[obj_idx].desc_idx = i;
         return;
      }
   }

   // not found in data\obj.txt

   // swap act/id according to object type
   if (type == 1)
      nb_type_per_act_ptr = & glb_config.nb_type1_per_act;
   else if (type == 2)
      nb_type_per_act_ptr = & glb_config.nb_type2_per_act;
   else // shouldn't happen
   {
      glb_ds1[ds1_idx].obj[obj_idx].desc_idx = -1;
      return;
   }

   while (id < 0)
   {
      // object of precedent act
      act--;
      id += (* nb_type_per_act_ptr);
   }
   while (id >= (* nb_type_per_act_ptr))
   {
      // object of next act
      act++;
      id -= (* nb_type_per_act_ptr);
   }

   // search again with these new Act / Id
   for (i=0; i < glb_ds1edit.obj_desc_num; i++)
   {
      if ( (glb_ds1edit.obj_desc[i].act  == act)  &&
           (glb_ds1edit.obj_desc[i].type == type) &&
           (glb_ds1edit.obj_desc[i].id   == id)
      )
      {
         // end
         glb_ds1[ds1_idx].obj[obj_idx].desc_idx = i;
         return;
      }
   }
   
   // not found
   glb_ds1[ds1_idx].obj[obj_idx].desc_idx = -1;
}


// ==========================================================================
// given a description index of 1 object of 1 ds1, compute its right Id
void editobj_make_obj_new_id(int ds1_idx, int obj_idx)
{
   int act, type, id, d, * nb_type_per_act_ptr;


   d    = glb_ds1[ds1_idx].obj[obj_idx].desc_idx;
   act  = glb_ds1edit.obj_desc[d].act;
   type = glb_ds1edit.obj_desc[d].type;
   id   = glb_ds1edit.obj_desc[d].id;

   if (type == 1)
      nb_type_per_act_ptr = & glb_config.nb_type1_per_act;
   else if (type == 2)
      nb_type_per_act_ptr = & glb_config.nb_type2_per_act;
   else // shouldn't happen
   {
      // update the object type / id
      glb_ds1[ds1_idx].obj[obj_idx].type = type;
      glb_ds1[ds1_idx].obj[obj_idx].id   = id;
      return;
   }

   while (act < glb_ds1[ds1_idx].act)
   {
      // object of precedent act
      act++;
      id -= (* nb_type_per_act_ptr);
   }
   while (act > glb_ds1[ds1_idx].act)
   {
      // object of next act
      act--;
      id += (* nb_type_per_act_ptr);
   }

   // update the object type / id
   glb_ds1[ds1_idx].obj[obj_idx].type = type;
   glb_ds1[ds1_idx].obj[obj_idx].id   = id;
}


// ==========================================================================
// clear the 'selected' flag of all the objects & labels of 1 ds1
void editobj_clear_obj_lab_sel(int ds1_idx)
{
   int i;
   

   for (i=0; i < glb_ds1[ds1_idx].obj_num; i++)
   {
      DEL_SELECTED(glb_ds1[ds1_idx].obj[i].label.flags);
      DEL_SELECTED(glb_ds1[ds1_idx].obj[i].flags);
   }
}


// ==========================================================================
// clear the 'mouse_over' flag of all the objects & labels of 1 ds1
void editobj_clear_obj_lab_over(int ds1_idx)
{
   int i;
   

   for (i=0; i < glb_ds1[ds1_idx].obj_num; i++)
   {
      DEL_MOUSE_OVER(glb_ds1[ds1_idx].obj[i].label.flags);
      DEL_MOUSE_OVER(glb_ds1[ds1_idx].obj[i].flags);
   }
}


// ==========================================================================
// clear all the flags of all the objects & labels of 1 ds1
void editobj_clear_obj_lab_flags(int ds1_idx)
{
   int i;
   

   for (i=0; i < glb_ds1[ds1_idx].obj_num; i++)
      glb_ds1[ds1_idx].obj[i].flags = glb_ds1[ds1_idx].obj[i].label.flags = 0;
}


// ==========================================================================
// managment of the mouse focus. only 1 label OR 1 object can have
// the focus at a time ( & label have a higher priority than object feet)
// this function tells if the screen have to be redraw too
// it also handles the mouse_over flag of all objects & labels
// it set pointer datas to the object or label that have the focus
int editobj_over_obj_lab(int ds1_idx, int * ptr_t, int * ptr_o,
                         int cx, int cy, int mx, int my)
{
   OBJ_LABEL_S * label;
   int         loop, o, w, h, x, y, x0, y0, last_t=0, last_o = -1;
   int         redraw = FALSE, o_idx;


   for (loop=0; loop<2; loop++)
   {
      for (o_idx=0; o_idx < glb_ds1[ds1_idx].obj_num; o_idx++)
      {
         o = glb_ds1[ds1_idx].drawing_order[o_idx];
         switch(loop)
         {
            case 0:
               // check sub-tile
               x = glb_ds1[ds1_idx].obj[o].x;
               y = glb_ds1[ds1_idx].obj[o].y;
               if ((cx == x) && (cy == y))
               {
                  // mouse over this object
                  last_t = 2;
                  last_o = o;
               }
               else
               {
                  // mouse NOT over this object
                  if (IS_MOUSE_OVER(glb_ds1[ds1_idx].obj[o].flags))
                  {
                     // mouse WAS over this object, but not anymore
                     redraw = TRUE;
                  }
               }
               break;
               
            case 1:
               // check label
               label = & glb_ds1[ds1_idx].obj[o].label;
               w     = label->w;
               h     = label->h;
               x0    = label->x0;
               y0    = label->y0;;
               if ((mx >= x0) && (my >= y0) && (mx < x0+w+4) &&
                   (my < y0+h+4))
               {
                  // mouse over this label
                  last_t = 1;
                  last_o = o;
               }
               else
               {
                  // mouse NOT over this label
                  if (IS_MOUSE_OVER(glb_ds1[ds1_idx].obj[o].label.flags))
                  {
                     // mouse WAS over this label, but not anymore
                     redraw = TRUE;
                  }
               }
               break;
         }
      }
   }

   if (redraw == TRUE)
   {
      // mouse WAS over an object or label, not anymore
      editobj_clear_obj_lab_over(ds1_idx);
      if (last_o != -1)
      {
         if (last_t == 1) // label
            SET_MOUSE_OVER(glb_ds1[ds1_idx].obj[last_o].label.flags);
         else // object
            SET_MOUSE_OVER(glb_ds1[ds1_idx].obj[last_o].flags);
      }
   }

   if (last_o != -1)
   {
      // mouse is over an object or label
      if (last_t == 1)
      {
         // over a label
         if ( ! IS_MOUSE_OVER(glb_ds1[ds1_idx].obj[last_o].label.flags))
         {
            // new label
            editobj_clear_obj_lab_over(ds1_idx);
            SET_MOUSE_OVER(glb_ds1[ds1_idx].obj[last_o].label.flags);
            redraw = TRUE;
         }
      }
      else
      {
         // over an object
         if ( ! IS_MOUSE_OVER(glb_ds1[ds1_idx].obj[last_o].flags))
         {
            // new object
            editobj_clear_obj_lab_over(ds1_idx);
            SET_MOUSE_OVER(glb_ds1[ds1_idx].obj[last_o].flags);
            redraw = TRUE;
         }
      }
   }
         
   // end
   * ptr_t = last_t;
   * ptr_o = last_o;
   return redraw;
}


// ==========================================================================
// prepare the move of objects & labels
//    moving flags & starting position
void editobj_prepare_moving(int ds1_idx)
{
   OBJ_LABEL_S * label;
   OBJ_S       * ptr_obj;
   int         o;


   for (o=0; o < glb_ds1[ds1_idx].obj_num; o++)
   {
      ptr_obj = & glb_ds1[ds1_idx].obj[o];
      label   = & glb_ds1[ds1_idx].obj[o].label;
      if (IS_SELECTED(ptr_obj->flags | label->flags))
      {
         if (IS_SELECTED(ptr_obj->flags))
            SET_MOVING(ptr_obj->flags);

         if (IS_SELECTED(label->flags))
            SET_MOVING(label->flags);
            
         ptr_obj->old_x = ptr_obj->x;
         ptr_obj->old_y = ptr_obj->y;
         label->old_rx  = label->rx;
         label->old_ry  = label->ry;
      }
   }
}


// ==========================================================================
// handler of moving objects & labels
int editobj_moving_obj_lab(int ds1_idx, int dcx, int dcy, int dmx, int dmy)
{
   OBJ_LABEL_S * label;
   OBJ_S       * ptr_obj;
   static      int old_dcx, old_dcy, old_dmx, old_dmy, old_ds1_idx = -1;
   int         o, redraw = FALSE, save_old = FALSE;


   for (o=0; o < glb_ds1[ds1_idx].obj_num; o++)
   {
      ptr_obj = & glb_ds1[ds1_idx].obj[o];
      if (IS_MOVING(ptr_obj->flags))
      {
         if ((old_dcx != dcx) || (old_dcy != dcy) || (ds1_idx != old_ds1_idx))
         {
            ptr_obj->x = ptr_obj->old_x + dcx;
            ptr_obj->y = ptr_obj->old_y + dcy;

            if (ptr_obj->x < 0)
               ptr_obj->x = 0;
            if (ptr_obj->x >= glb_ds1[ds1_idx].width * 5)
               ptr_obj->x = glb_ds1[ds1_idx].width * 5 - 1;

            if (ptr_obj->y < 0)
               ptr_obj->y = 0;
            if (ptr_obj->y >= glb_ds1[ds1_idx].height * 5)
               ptr_obj->y = glb_ds1[ds1_idx].height * 5 - 1;

            save_old = TRUE;
         }
      }

      label = & glb_ds1[ds1_idx].obj[o].label;
      if (IS_MOVING(label->flags))
      {
         if ((old_dmx != dmx) || (old_dmy != dmy) || (ds1_idx != old_ds1_idx))
         {
            label->rx = label->old_rx + dmx;
            label->ry = label->old_ry + dmy;
            
            save_old = TRUE;
         }
      }
   }
   if ((save_old) || (ds1_idx != old_ds1_idx))
   {
      old_ds1_idx = ds1_idx;
      old_dcx     = dcx;
      old_dcy     = dcy;
      old_dmx     = dmx;
      old_dmy     = dmy;
      redraw      = TRUE;
   }
   return redraw;
}


// ==========================================================================
// end the move of objects & labels
void editobj_end_move_obj_lab(int ds1_idx)
{
   OBJ_LABEL_S * label;
   OBJ_S       * ptr_obj;
   int         o;


   for (o=0; o < glb_ds1[ds1_idx].obj_num; o++)
   {
      ptr_obj = & glb_ds1[ds1_idx].obj[o];
      DEL_MOVING(ptr_obj->flags);

      label = & ptr_obj->label;
      DEL_MOVING(label->flags);
   }
}


// ==========================================================================
// prepare 1 step undo : full backup of objects & labels datas
void editobj_prepare_undo(int ds1_idx)
{
   OBJ_S * ptr_s, * ptr_d;
   int   o;


   for (o=0; o < glb_ds1[ds1_idx].obj_num; o++)
   {
      ptr_s = & glb_ds1[ds1_idx].obj[o];
      ptr_d = & glb_ds1[ds1_idx].obj_undo[o];
      memcpy(ptr_d, ptr_s, sizeof (OBJ_S));
   }
   glb_ds1[ds1_idx].obj_num_undo = glb_ds1[ds1_idx].obj_num;
   glb_ds1[ds1_idx].can_undo_obj = TRUE;
}


// ==========================================================================
// use the backup to restore the previous state of objects & labels
void editobj_undo(int ds1_idx)
{
   OBJ_S * ptr_s, * ptr_d;
   int   o;


   // just in case
   if (glb_ds1[ds1_idx].can_undo_obj == FALSE)
      return;

   for (o=0; o < glb_ds1[ds1_idx].obj_num_undo; o++)
   {
      ptr_s = & glb_ds1[ds1_idx].obj_undo[o];
      ptr_d = & glb_ds1[ds1_idx].obj[o];
      memcpy(ptr_d, ptr_s, sizeof (OBJ_S));
   }
   glb_ds1[ds1_idx].obj_num = glb_ds1[ds1_idx].obj_num_undo;
}


// ==========================================================================
// delete objects (and their labels)
// reorganize the objects table (no hole between them)
void editobj_del_obj(int ds1_idx)
{
   OBJ_S * ptr_s, * ptr_d;
   int   s=0, d=0, n=0, max = glb_ds1[ds1_idx].obj_num, done = FALSE;


   while ( ! done)
   {
      ptr_d = & glb_ds1[ds1_idx].obj[d];
      if (IS_SELECTED(ptr_d->flags))
      {
         // search next non-selected
         while ((s < max) && IS_SELECTED(glb_ds1[ds1_idx].obj[s].flags))
            s++;
         // found one ?
         if (s < max)
         {
            // copy this one
            ptr_s = & glb_ds1[ds1_idx].obj[s];
            memcpy(ptr_d, ptr_s, sizeof (OBJ_S));
            SET_SELECTED(ptr_s->flags);
         }
         else
            done = TRUE;
      }
      if (done == FALSE)
      {
         n++;
         d++;
         s++;
         if (d >= max)
            done = TRUE;
      }
   }
   
   // delete completly all the unused objects datas
   for (s=n; s < max; s++)
      memset( & glb_ds1[ds1_idx].obj[s], 0, sizeof(OBJ_S));
      
   // end
   glb_ds1[ds1_idx].obj_num = n;
}


// ==========================================================================
// copy some objects
// create new objects at the end of the objects table, and flags handle
void editobj_copy_obj(int ds1_idx)
{
   OBJ_S * ptr_s, * ptr_d;
   int   o, d, nb=0, max = glb_ds1[ds1_idx].obj_num;
   long  incr;


   // count selected obj
   for (o=0; o < max; o++)
      if (IS_SELECTED(glb_ds1[ds1_idx].obj[o].flags))
         nb++;

   // just in case
   if (nb <= 0)
      return;
      
   // check if can make new objects
   if ((max + nb) >= glb_ds1[ds1_idx].current_obj_max)
   {
		incr = 1 + (max + nb) - glb_ds1[ds1_idx].current_obj_max;
		if (misc_increase_ds1_objects_max(ds1_idx, incr) != 0)
			return;
   }

   // make new objects, deselect old ones (new ones are already selected)
   d = max;
   for (o=0; o < max; o++)
   {
      if (IS_SELECTED(glb_ds1[ds1_idx].obj[o].flags))
      {
         ptr_s = & glb_ds1[ds1_idx].obj[o];
         ptr_d = & glb_ds1[ds1_idx].obj[d];
         memcpy(ptr_d, ptr_s, sizeof (OBJ_S));
         DEL_SELECTED(ptr_s->flags);
         SET_MOUSE_OVER(ptr_s->flags); // to still see the original objects
		 ptr_d->frame_delta = rand()%256;;
         d++;
      }
   }
   glb_ds1[ds1_idx].obj_num = max + nb;
}


// ==========================================================================
/*
void editobj_search_copy_center(int ds1_idx, int * cx, int * cy)
{
   OBJ_S * ptr_o;
   int   x1=30000, y1=30000, x2 = -30000, y2 = -30000, x, y, o;

   for (o=0; o < glb_ds1[ds1_idx].obj_num; o++)
   {
      ptr_o = & glb_ds1[ds1_idx].obj[o];
      if (IS_SELECTED(ptr_o->flags))
      {
         x = ptr_o->x;
         y = ptr_o->y;
         if (x < x1) x1 = x;
         if (x > x2) x2 = x;
         if (y < y1) y1 = y;
         if (y > y2) y2 = y;
      }
   }
   * cx = x1 + ( (x2 - x1) / 2);
   * cy = y1 + ( (y2 - y1) / 2);
}


// ==========================================================================
int editobj_center_before_copy(int ds1_idx, int * cx0, int * cy0,
                                            int * mx,  int * my)
{
   int dx, dy, cx, cy, x0, y0, o, nb=0;

   // count selected obj
   for (o=0; o < glb_ds1[ds1_idx].obj_num; o++)
      if (IS_SELECTED(glb_ds1[ds1_idx].obj[o].flags))
         nb++;

   // just in case
   if (nb <= 0)
      return 0;
      
   editobj_search_copy_center(ds1_idx, & cx, & cy);

   dx = (cy * -glb_ds1[ds1_idx].tile_w / 10) + (cx * glb_ds1[ds1_idx].tile_w / 10);
   dy = (cy *  glb_ds1[ds1_idx].tile_h / 10) + (cx * glb_ds1[ds1_idx].tile_h / 10);
   x0 = dx - glb_ds1[ds1_idx].own_wpreview.w / 2;
   y0 = dy - glb_ds1[ds1_idx].own_wpreview.h / 2;

   glb_ds1edit.win_preview.x0   = x0;
   glb_ds1edit.win_preview.y0   = y0;
   glb_ds1[ds1_idx].own_wpreview.x0 = x0;
   glb_ds1[ds1_idx].own_wpreview.y0 = y0;

   * cx0 = cx;
   * cy0 = cy;
   * mx = mouse_x;
   * my = mouse_y;

   return nb;
}
*/


// ==========================================================================
// count selected objects
int editobj_count_sel_obj(int ds1_idx)
{
   int o, nb=0;


   for (o=0; o < glb_ds1[ds1_idx].obj_num; o++)
      if (IS_SELECTED(glb_ds1[ds1_idx].obj[o].flags))
         nb++;

   return nb;
}


// ==========================================================================
// add 1 object at the end of object table
//    set it to a default type & id, and prepare its label window
int editobj_insert_obj(int ds1_idx, int cx, int cy)
{
   OBJ_LABEL_S * label;
   OBJ_S       * ptr_o;
   int         d, id, type, obj, w, wtmp, h;
   long        incr;


   obj = glb_ds1[ds1_idx].obj_num;
   if (obj >= glb_ds1[ds1_idx].current_obj_max)
   {
		incr = 1 + obj - glb_ds1[ds1_idx].current_obj_max;
		if (misc_increase_ds1_objects_max(ds1_idx, incr) != 0)
			return 1;
   }

   glb_ds1[ds1_idx].obj_num++;
   ptr_o = & glb_ds1[ds1_idx].obj[obj];

   ptr_o->type     = 1;
   ptr_o->id       = 0;
   ptr_o->x        = cx;
   ptr_o->y        = cy;
   ptr_o->path_num = 0;
   ptr_o->flags    = 0;
   SET_SELECTED(ptr_o->flags);
   ptr_o->frame_delta = rand()%256;
   editobj_make_obj_desc(ds1_idx, obj);

   type  = ptr_o->type;
   id    = ptr_o->id;
   d     = ptr_o->desc_idx;
   label = & ptr_o->label;

   w = 16 * 8;     // 16 chars
   h =  2 * 8 + 2; // 2 char lines + 2 pixel lines for readibility

   if (d != -1)
   {
      wtmp = 8 * strlen(glb_ds1edit.obj_desc[d].desc);
      if (wtmp > w)
         w = wtmp;
   }
   label->rx    = 0;
   label->ry    = 0;
   label->w     = w;
   label->h     = h;
   label->flags = 0;

   return 0;
}


// ==========================================================================
// prepare the objects list window (variable width, buttons, etc)
void editobj_prepare_edit_obj_win(int ds1_idx, int obj_idx)
{
   WIN_EDT_OBJ_S * ptr_w = & glb_ds1[ds1_idx].win_edt_obj;
   OBJ_DESC_S    * ptr_d;
   EDT_BUT_S     * b;
   int           act, w, h, desc_len = 0, len, n, d, i, x0, y0, typ;
   int           myborder = 2;
   static struct{
      int  x1, y1, x2, y2, x, y; // relative to upper / left corner of the
                                 //    window, NOT the screen
      char name[10];
   } but_init[EB_MAX] = {
        {  0,  0,  62, 22,   8,  8, "  OK"},
        { 70,  0, 132, 22,  78,  8, "CANCEL"},
        {210,  0, 272, 22, 218,  8, "TYPE 1"},
        {280,  0, 342, 22, 288,  8, "TYPE 2"},

        {  0, 30,  62, 52,  12, 38, "ACT 1"},
        { 70, 30, 132, 52,  82, 38, "ACT 2"},
        {140, 30, 202, 52, 152, 38, "ACT 3"},
        {210, 30, 272, 52, 222, 38, "ACT 4"},
        {280, 30, 342, 52, 292, 38, "ACT 5"},
     };


   w   = glb_config.screen.width;
   h   = glb_config.screen.height;
   act = ptr_w->cur_act;
   typ = ptr_w->cur_type;

   // max char size of the desc of current act & current type
   ptr_w->desc_end = ptr_w->desc_start = -1;
   for (d=0; d < glb_ds1edit.obj_desc_num; d++)
   {
      ptr_d = & glb_ds1edit.obj_desc[d];
      if ((ptr_d->act == act) && (ptr_d->type == typ))
      {
         len = 1 + strlen(ptr_d->desc);
         if (len > desc_len)
            desc_len = len;
         if (ptr_w->desc_start == -1)
            ptr_w->desc_start = d;
         ptr_w->desc_end = d;
      }
   }
   n = strlen(" Description");
   if (n > desc_len)
      desc_len = n;
   ptr_w->desc_char = desc_len;
   ptr_w->desc_num  = ptr_w->desc_end - ptr_w->desc_start + 1;
   ptr_w->obj_idx   = obj_idx;

   // compute window size & position
   ptr_w->x0  = 5;
   ptr_w->y0  = 15;
   ptr_w->h   = glb_config.screen.height - ptr_w->y0 - 20;
   ptr_w->num = ((ptr_w->h - 69 - (3 * 10)) / 10) - myborder;
   if (ptr_w->num < 1)
      ptr_w->num = 1;
   ptr_w->h   = 69 + 10 * (3 + ptr_w->num + myborder) + 5;
   
   if (glb_ds1edit.obj_desc[ptr_w->desc_cur].act != ptr_w->cur_act)
      ptr_w->start = ptr_w->desc_start;
   else if (glb_ds1[ds1_idx].obj[obj_idx].desc_idx == -1)
   {
      ptr_w->start = ptr_w->desc_start;
   }
   else
   {
      ptr_w->start = ptr_w->desc_cur - (ptr_w->num / 2);
      if (ptr_w->start + ptr_w->num > ptr_w->desc_end)
         ptr_w->start = ptr_w->desc_end - ptr_w->num + 1;
      if (ptr_w->start < ptr_w->desc_start)
         ptr_w->start = ptr_w->desc_start;
   }

   ptr_w->w = 8 * (4 + 3 + desc_len + 5); // type + id + desc + blanks
   if (ptr_w->w < 358)
      ptr_w->w = 358;

   // buttons
   x0 = ptr_w->x0 + 8;
   y0 = ptr_w->y0 + 8;
   for (i=0; i<EB_MAX; i++)
   {
      b = & ptr_w->button[i];
      b->box.x1 = x0 + but_init[i].x1;
      b->box.y1 = y0 + but_init[i].y1;
      b->box.x2 = x0 + but_init[i].x2;
      b->box.y2 = y0 + but_init[i].y2;
      b->txt.x  = x0 + but_init[i].x;
      b->txt.y  = y0 + but_init[i].y;
      strcpy(b->name, but_init[i].name);
      b->flags  = 0;
   }

   // flags of buttons
   SET_ACTIVATED(ptr_w->button[EB_OK].flags);
   SET_ACTIVATED(ptr_w->button[EB_CANCEL].flags);
   SET_ACTIVATED(ptr_w->button[EB_TYPE1].flags);
   SET_ACTIVATED(ptr_w->button[EB_TYPE2].flags);

   if (typ == 1)
   {
      SET_SELECTED(ptr_w->button[EB_TYPE1].flags);

      SET_ACTIVATED(ptr_w->button[EB_ACT1].flags);
      SET_ACTIVATED(ptr_w->button[EB_ACT2].flags);
      SET_ACTIVATED(ptr_w->button[EB_ACT3].flags);
      SET_ACTIVATED(ptr_w->button[EB_ACT4].flags);
      SET_ACTIVATED(ptr_w->button[EB_ACT5].flags);
   }
   else
   {
      SET_SELECTED(ptr_w->button[EB_TYPE2].flags);
      if (glb_config.normal_type2 == TRUE)
      {
         // act 1 always possible
         SET_ACTIVATED(ptr_w->button[EB_ACT1].flags);

         // now, current act & previous ones only, not further
         if (glb_ds1[ds1_idx].act >= 2)
            SET_ACTIVATED(ptr_w->button[EB_ACT2].flags);

         if (glb_ds1[ds1_idx].act >= 3)
            SET_ACTIVATED(ptr_w->button[EB_ACT3].flags);

         if (glb_ds1[ds1_idx].act >= 4)
            SET_ACTIVATED(ptr_w->button[EB_ACT4].flags);

         if (glb_ds1[ds1_idx].act >= 5)
            SET_ACTIVATED(ptr_w->button[EB_ACT5].flags);
      }
      else
      {
         SET_ACTIVATED(ptr_w->button[EB_ACT1].flags);
         SET_ACTIVATED(ptr_w->button[EB_ACT2].flags);
         SET_ACTIVATED(ptr_w->button[EB_ACT3].flags);
         SET_ACTIVATED(ptr_w->button[EB_ACT4].flags);
         SET_ACTIVATED(ptr_w->button[EB_ACT5].flags);
      }
   }

   switch(act)
   {
      case 1 : SET_SELECTED(ptr_w->button[EB_ACT1].flags); break;
      case 2 : SET_SELECTED(ptr_w->button[EB_ACT2].flags); break;
      case 3 : SET_SELECTED(ptr_w->button[EB_ACT3].flags); break;
      case 4 : SET_SELECTED(ptr_w->button[EB_ACT4].flags); break;
      case 5 : SET_SELECTED(ptr_w->button[EB_ACT5].flags); break;
   }

   // end
   glb_ds1[ds1_idx].draw_edit_obj = TRUE;
}


// ==========================================================================
// draw the objects description list window
void editobj_draw_edit_obj(int ds1_idx)
{
   WIN_EDT_OBJ_S * ptr_w = & glb_ds1[ds1_idx].win_edt_obj;
   OBJ_DESC_S    * ptr_d=NULL;
   EDT_BUT_S     * b;
   int           x1, x2, y1, y2, x, y, d, line, bg_color, c, i, size;
   char          * minus_tmp, tmp[150];


   size = glb_ds1edit.obj_buff->col[misc_get_txt_column_num(RQ_OBJ, "Description")].size + 1;
   minus_tmp = (char *) malloc(size);
   if (minus_tmp == NULL)
   {
      sprintf(tmp, "editobj_draw_edit_obj() : can't allocate %i bytes", size);
      ds1edit_error(tmp);
   }

   x1 = ptr_w->x0;
   y1 = ptr_w->y0;
   x2 = x1 + ptr_w->w;
   y2 = y1 + ptr_w->h;
   rectfill(glb_ds1edit.screen_buff, x1, y1, x2, y2, 0);
   rect    (glb_ds1edit.screen_buff, x1, y1, x2, y2, 255);
   
   for (i=0; i < EB_MAX; i++)
   {
      b  = & ptr_w->button[i];
      x1 = b->box.x1;
      y1 = b->box.y1;
      x2 = b->box.x2;
      y2 = b->box.y2;
      x  = b->txt.x;
      y  = b->txt.y;
      if (IS_ACTIVATED(b->flags))
      {
         rect(glb_ds1edit.screen_buff, x1, y1, x2, y2, 129);
         if (IS_SELECTED(b->flags))
         {
            rectfill(glb_ds1edit.screen_buff, x1+2, y1+2, x2-2, y2-2, 116);
            textprintf(glb_ds1edit.screen_buff, font, x+1, y + 1, 0, b->name);
         }
         textprintf(glb_ds1edit.screen_buff, font, x, y, 132, b->name);
      }
      else
      {
         rect(glb_ds1edit.screen_buff, x1, y1, x2, y2, 186);
         if (IS_SELECTED(b->flags))
         {
            rectfill(glb_ds1edit.screen_buff, x1+2, y1+2, x2-2, y2-2, 209);
            textprintf(glb_ds1edit.screen_buff, font, x+1, y + 1, 0, b->name);
         }
         textprintf(glb_ds1edit.screen_buff, font, x, y, 214, b->name);
      }
   }

   x1 = ptr_w->x0 + (8 * 2);
   y1 = ptr_w->y0 + 69;
   textprintf(glb_ds1edit.screen_buff, font, x1, y1, 11, "Type");
   
   x1 += 8 * 5;
   textprintf(glb_ds1edit.screen_buff, font, x1, y1, 255, " Id");
   x1 += 8 * 4;
   textprintf(glb_ds1edit.screen_buff, font, x1, y1, 255, " Description");

   x1 = ptr_w->x0 + (8 * 2);
   y1 += 10;
   textprintf(glb_ds1edit.screen_buff, font, x1, y1, 210, "---- ---  -----------");

   strcpy(minus_tmp, "");
   for (c=0; c < ptr_w->desc_char - 11; c++)
      strcat(minus_tmp, "-");
   x1 += 20 * 8;
   textprintf(glb_ds1edit.screen_buff, font, x1, y1, 210, "%s", minus_tmp);

   // desc themselves
   for (line=0; line < ptr_w->num; line++)
   {
      x1  = ptr_w->x0 + (8 * 2);
      y1 += 10;
      d   = ptr_w->start + line;
      if ( (d >= 0) && (d < glb_ds1edit.obj_desc_num) &&
           (d >= ptr_w->desc_start) && (d <= ptr_w->desc_end)
         )
      {
         if (d == ptr_w->desc_cur)
            bg_color = 121;
         else if (d == ptr_w->cur)
            bg_color = 71;
         else if (line % 2)
            bg_color = 1;
         else
            bg_color = 0;
         if (bg_color)
            rectfill(glb_ds1edit.screen_buff, ptr_w->x0+8, y1-1, ptr_w->x0 + ptr_w->w - 8, y1+8, bg_color);

         ptr_d = & glb_ds1edit.obj_desc[d];

         textprintf(glb_ds1edit.screen_buff, font, x1, y1, 11, "%3i", ptr_d->type);
         x1 += 8 * 5;
         textprintf(glb_ds1edit.screen_buff, font, x1, y1, 255, "%3i", ptr_d->id);
         x1 += 8 * 4;
         textprintf(glb_ds1edit.screen_buff, font, x1, y1, 255, " %s", ptr_d->desc);
      }
   }
   x1 = ptr_w->x0 + (8 * 2);
   y1 += 25;
   textprintf(glb_ds1edit.screen_buff, font, x1, y1, 214, "(scroll down here)");

   free(minus_tmp);
}


// ==========================================================================
// handler of the objects list window
int editobj_edit_obj(int ds1_idx, int * edit_end, int mx, int my, int mb)
{
   WIN_EDT_OBJ_S * ptr_w = & glb_ds1[ds1_idx].win_edt_obj;
   int           redraw = FALSE, new_cur, end, w, wtmp;
   

   * edit_end = FALSE;
   
   // managment
   if ((mx >= ptr_w->button[EB_OK].box.x1) &&
       (mx <= ptr_w->button[EB_OK].box.x2) &&
       (my >= ptr_w->button[EB_OK].box.y1) &&
       (my <= ptr_w->button[EB_OK].box.y2)
      )
   {
      // over OK button
      if (mb & 1)
      {
         glb_ds1[ds1_idx].draw_edit_obj = FALSE;
         redraw = TRUE;
         * edit_end = TRUE;
         glb_ds1[ds1_idx].obj[ptr_w->obj_idx].type     = glb_ds1edit.obj_desc[ptr_w->desc_cur].type;
         glb_ds1[ds1_idx].obj[ptr_w->obj_idx].id       = glb_ds1edit.obj_desc[ptr_w->desc_cur].id;
         glb_ds1[ds1_idx].obj[ptr_w->obj_idx].desc_idx = ptr_w->desc_cur;
         editobj_make_obj_new_id(ds1_idx, ptr_w->obj_idx);
         w = 16 * 8; // 15 chars
         if (ptr_w->desc_cur != -1)
         {
            wtmp = 8 * strlen(glb_ds1edit.obj_desc[ptr_w->desc_cur].desc);
            if (wtmp > w)
               w = wtmp;
         }
         glb_ds1[ds1_idx].obj[ptr_w->obj_idx].label.w = w;
      }
   }
   else if (key[KEY_ENTER] || key[KEY_ENTER_PAD])
   {
      // simulate the click on the OK button
      while (key[KEY_ENTER] || key[KEY_ENTER_PAD])
      {}
      glb_ds1[ds1_idx].draw_edit_obj = FALSE;
      redraw = TRUE;
      * edit_end = TRUE;
      glb_ds1[ds1_idx].obj[ptr_w->obj_idx].type     = glb_ds1edit.obj_desc[ptr_w->desc_cur].type;
      glb_ds1[ds1_idx].obj[ptr_w->obj_idx].id       = glb_ds1edit.obj_desc[ptr_w->desc_cur].id;
      glb_ds1[ds1_idx].obj[ptr_w->obj_idx].desc_idx = ptr_w->desc_cur;
      editobj_make_obj_new_id(ds1_idx, ptr_w->obj_idx);
      w = 16 * 8; // 15 chars
      if (ptr_w->desc_cur != -1)
      {
         wtmp = 8 * strlen(glb_ds1edit.obj_desc[ptr_w->desc_cur].desc);
         if (wtmp > w)
            w = wtmp;
      }
      glb_ds1[ds1_idx].obj[ptr_w->obj_idx].label.w = w;
   }
   else if ((glb_config.winobj_scroll_keyb == FALSE) && key[KEY_UP])
   {
      // scroll the list, up
      ptr_w->start -= glb_config.scroll.obj_edit;
      if (ptr_w->start < ptr_w->desc_start)
         ptr_w->start = ptr_w->desc_start;
      ptr_w->cur = -1;
      redraw = TRUE;
   }
   else if ((glb_config.winobj_scroll_keyb == FALSE) && key[KEY_DOWN])
   {
      // scroll the list, down
      ptr_w->start += glb_config.scroll.obj_edit;
      if ( (ptr_w->start + ptr_w->num - 1) > ptr_w->desc_end)
         ptr_w->start = ptr_w->desc_end - ptr_w->num + 1;
      if (ptr_w->start < ptr_w->desc_start)
         ptr_w->start = ptr_w->desc_start;
      ptr_w->cur = -1;
      redraw = TRUE;
   }
   else if ((mx >= ptr_w->button[EB_CANCEL].box.x1) &&
            (mx <= ptr_w->button[EB_CANCEL].box.x2) &&
            (my >= ptr_w->button[EB_CANCEL].box.y1) &&
            (my <= ptr_w->button[EB_CANCEL].box.y2)
           )
   {
      // over CANCEL button
      if (mb & 1)
      {
         glb_ds1[ds1_idx].draw_edit_obj = FALSE;
         redraw = TRUE;
         * edit_end = TRUE;
      }
   }
   else if ((mx >= ptr_w->button[EB_TYPE1].box.x1) &&
            (mx <= ptr_w->button[EB_TYPE1].box.x2) &&
            (my >= ptr_w->button[EB_TYPE1].box.y1) &&
            (my <= ptr_w->button[EB_TYPE1].box.y2)
           )
   {
      // over TYPE1 button
      if (mb & 1)
      {
         while (mouse_b & 1)
         {
            // wait until the left mouse button is released
         }
         if (ptr_w->cur_type != 1)
            ptr_w->cur_act  = glb_ds1[ds1_idx].act;
         ptr_w->cur_type = 1;
         editobj_prepare_edit_obj_win(ds1_idx, ptr_w->obj_idx);
         redraw = TRUE;
      }
   }
   else if ((mx >= ptr_w->button[EB_TYPE2].box.x1) &&
            (mx <= ptr_w->button[EB_TYPE2].box.x2) &&
            (my >= ptr_w->button[EB_TYPE2].box.y1) &&
            (my <= ptr_w->button[EB_TYPE2].box.y2)
           )
   {
      // over TYPE2 button
      if (mb & 1)
      {
         while (mouse_b & 1)
         {
            // wait until the left mouse button is released
         }
         ptr_w->cur_type = 2;
         ptr_w->cur_act  = glb_ds1[ds1_idx].act;
         editobj_prepare_edit_obj_win(ds1_idx, ptr_w->obj_idx);
         redraw = TRUE;
      }
   }
   else if ((mx >= ptr_w->button[EB_ACT1].box.x1) &&
            (mx <= ptr_w->button[EB_ACT1].box.x2) &&
            (my >= ptr_w->button[EB_ACT1].box.y1) &&
            (my <= ptr_w->button[EB_ACT1].box.y2)
           )
   {
      // over ACT1 button
      if ((mb & 1) && (IS_ACTIVATED(ptr_w->button[EB_ACT1].flags)))
      {
         while (mouse_b & 1)
         {
            // wait until the left mouse button is released
         }
         ptr_w->cur_act = 1;
         editobj_prepare_edit_obj_win(ds1_idx, ptr_w->obj_idx);
         redraw = TRUE;
      }
   }
   else if ((mx >= ptr_w->button[EB_ACT2].box.x1) &&
            (mx <= ptr_w->button[EB_ACT2].box.x2) &&
            (my >= ptr_w->button[EB_ACT2].box.y1) &&
            (my <= ptr_w->button[EB_ACT2].box.y2)
           )
   {
      // over ACT2 button
      if ((mb & 1) && (IS_ACTIVATED(ptr_w->button[EB_ACT2].flags)))
      {
         while (mouse_b & 1)
         {
            // wait until the left mouse button is released
         }
         ptr_w->cur_act = 2;
         editobj_prepare_edit_obj_win(ds1_idx, ptr_w->obj_idx);
         redraw = TRUE;
      }
   }
   else if ((mx >= ptr_w->button[EB_ACT3].box.x1) &&
            (mx <= ptr_w->button[EB_ACT3].box.x2) &&
            (my >= ptr_w->button[EB_ACT3].box.y1) &&
            (my <= ptr_w->button[EB_ACT3].box.y2)
           )
   {
      // over ACT3 button
      if ((mb & 1) && (IS_ACTIVATED(ptr_w->button[EB_ACT3].flags)))
      {
         while (mouse_b & 1)
         {
            // wait until the left mouse button is released
         }
         ptr_w->cur_act = 3;
         editobj_prepare_edit_obj_win(ds1_idx, ptr_w->obj_idx);
         redraw = TRUE;
      }
   }
   else if ((mx >= ptr_w->button[EB_ACT4].box.x1) &&
            (mx <= ptr_w->button[EB_ACT4].box.x2) &&
            (my >= ptr_w->button[EB_ACT4].box.y1) &&
            (my <= ptr_w->button[EB_ACT4].box.y2)
           )
   {
      // over ACT4 button
      if ((mb & 1) && (IS_ACTIVATED(ptr_w->button[EB_ACT4].flags)))
      {
         while (mouse_b & 1)
         {
            // wait until the left mouse button is released
         }
         ptr_w->cur_act = 4;
         editobj_prepare_edit_obj_win(ds1_idx, ptr_w->obj_idx);
         redraw = TRUE;
      }
   }
   else if ((mx >= ptr_w->button[EB_ACT5].box.x1) &&
            (mx <= ptr_w->button[EB_ACT5].box.x2) &&
            (my >= ptr_w->button[EB_ACT5].box.y1) &&
            (my <= ptr_w->button[EB_ACT5].box.y2)
           )
   {
      // over ACT5 button
      if ((mb & 1) && (IS_ACTIVATED(ptr_w->button[EB_ACT5].flags)))
      {
         while (mouse_b & 1)
         {
            // wait until the left mouse button is released
         }
         ptr_w->cur_act = 5;
         editobj_prepare_edit_obj_win(ds1_idx, ptr_w->obj_idx);
         redraw = TRUE;
      }
   }
   else
   {
      new_cur  = (my - ptr_w->y0 - 69 - (2 * 10)) / 10;
      new_cur += ptr_w->start;
      if ( (mx >= ptr_w->x0) && (mx < (ptr_w->x0 + ptr_w->w)) &&
           (my >= ptr_w->x0) && (my < (ptr_w->y0 + ptr_w->h))
         )
      {
         // mouse in the window

         end = ptr_w->start + ptr_w->num - 1;
         if ((new_cur < ptr_w->start) && (ptr_w->start > ptr_w->desc_start))
         {
            // scroll the list, up
            ptr_w->start -= glb_config.scroll.obj_edit;
            if (ptr_w->start < ptr_w->desc_start)
               ptr_w->start = ptr_w->desc_start;
            ptr_w->cur = -1;
            redraw = TRUE;
         }
         else if ((new_cur > end) && (end < ptr_w->desc_end))
         {
            // scroll the list, down
            ptr_w->start += glb_config.scroll.obj_edit;
            if ( (ptr_w->start + ptr_w->num - 1) > ptr_w->desc_end)
               ptr_w->start = ptr_w->desc_end - ptr_w->num + 1;
            if (ptr_w->start < ptr_w->desc_start)
               ptr_w->start = ptr_w->desc_start;
            ptr_w->cur = -1;
            redraw = TRUE;
         }
         else
         {
            // mouse in the list

            if (new_cur != ptr_w->cur)
            {
               // NEW row pointed by mouse
               ptr_w->cur = new_cur;
               redraw = TRUE;
            }

            if (mb & 1)
            {
               if ( (new_cur != ptr_w->desc_cur)   &&
                    (new_cur >= ptr_w->desc_start) &&
                    (new_cur <= ptr_w->desc_end)
                  )
               {
                  // NEW selected row
                  ptr_w->desc_cur = new_cur;
                  redraw = TRUE;
               }
            }
         }
      }
      else
      {
         // mouse out of the window
         if (ptr_w->cur != -1)
         {
            ptr_w->cur = -1;
            redraw = TRUE;
         }
      }
   }

   return redraw;
}


// ==========================================================================
// make a NEW selection of all same object
// return TRUE if at least 1 object has been added
// (it assume not even 1 object is already selected)
void editobj_identical_obj_new(int ds1_idx, int o)
{
   int i;


   // for all objects of that ds1
   for (i=0; i < glb_ds1[ds1_idx].obj_num; i++)
   {
      // same object ?
      if (glb_ds1[ds1_idx].obj[i].desc_idx == glb_ds1[ds1_idx].obj[o].desc_idx)
      {
         // yep, select it
         SET_SELECTED(glb_ds1[ds1_idx].obj[i].flags);
      }
   }
}


// ==========================================================================
// add all same object to the selection
// return TRUE if at least 1 object has been added
int editobj_identical_obj_add(int ds1_idx, int o)
{
   int need_redraw = FALSE, i;
   

   // for all objects of that ds1
   for (i=0; i < glb_ds1[ds1_idx].obj_num; i++)
   {
      // same object ?
      if (glb_ds1[ds1_idx].obj[i].desc_idx == glb_ds1[ds1_idx].obj[o].desc_idx)
      {
         // yep, same object. already selected ?
         if ( ! IS_SELECTED(glb_ds1[ds1_idx].obj[i].flags))
         {
            // no, add it
            SET_SELECTED(glb_ds1[ds1_idx].obj[i].flags);
            need_redraw = TRUE;
         }
      }
   }
   return need_redraw;
}


// ==========================================================================
// delete all same object from the selection
// return TRUE if at least 1 object has been deleted from the selection
int editobj_identical_obj_del(int ds1_idx, int o)
{
   int need_redraw = FALSE, i;


   // for all objects of that ds1
   for (i=0; i < glb_ds1[ds1_idx].obj_num; i++)
   {
      // same object ?
      if (glb_ds1[ds1_idx].obj[i].desc_idx == glb_ds1[ds1_idx].obj[o].desc_idx)
      {
         // yep, same object. already selected ?
         if (IS_SELECTED(glb_ds1[ds1_idx].obj[i].flags))
         {
            // yes, remove it
            DEL_SELECTED(glb_ds1[ds1_idx].obj[i].flags);
            need_redraw = TRUE;
         }
      }
   }
   return need_redraw;
}


// ==========================================================================
// make a NEW selection of all same labels
// return TRUE if at least 1 label has been added
// (it assume not even 1 label is already selected)
void editobj_identical_lab_new(int ds1_idx, int o)
{
   int i;


   // for all objects of that ds1
   for (i=0; i < glb_ds1[ds1_idx].obj_num; i++)
   {
      // same object ?
      if (glb_ds1[ds1_idx].obj[i].desc_idx == glb_ds1[ds1_idx].obj[o].desc_idx)
      {
         // yep, select it
         SET_SELECTED(glb_ds1[ds1_idx].obj[i].label.flags);
      }
   }
}


// ==========================================================================
// add all same labels to the selection
// return TRUE if at least 1 lbel has been added
int editobj_identical_lab_add(int ds1_idx, int o)
{
   int need_redraw = FALSE, i;
   

   // for all objects of that ds1
   for (i=0; i < glb_ds1[ds1_idx].obj_num; i++)
   {
      // same object ?
      if (glb_ds1[ds1_idx].obj[i].desc_idx == glb_ds1[ds1_idx].obj[o].desc_idx)
      {
         // yep, same object. label already selected ?
         if ( ! IS_SELECTED(glb_ds1[ds1_idx].obj[i].label.flags))
         {
            // no, add it
            SET_SELECTED(glb_ds1[ds1_idx].obj[i].label.flags);
            need_redraw = TRUE;
         }
      }
   }
   return need_redraw;
}


// ==========================================================================
// delete all same labels from the selection
// return TRUE if at least 1 label has been deleted from the selection
int editobj_identical_lab_del(int ds1_idx, int o)
{
   int need_redraw = FALSE, i;


   // for all objects of that ds1
   for (i=0; i < glb_ds1[ds1_idx].obj_num; i++)
   {
      // same object ?
      if (glb_ds1[ds1_idx].obj[i].desc_idx == glb_ds1[ds1_idx].obj[o].desc_idx)
      {
         // yep, same object. label already selected ?
         if (IS_SELECTED(glb_ds1[ds1_idx].obj[i].label.flags))
         {
            // yes, remove it
            DEL_SELECTED(glb_ds1[ds1_idx].obj[i].label.flags);
            need_redraw = TRUE;
         }
      }
   }
   return need_redraw;
}


// ==========================================================================
// MAIN objects & labels edition handler
int editobj_handler(int ds1_idx, int cx, int cy, int mx, int my, int mb)
{
   static int  old_ds1_idx = -1, old_t, old_o, cur_type;
   static enum {OM_NONE, OM_NEW, OM_ADD, OM_DEL, OM_MOV, OM_EDT, OM_INS} old_mode;
   int         t=0, o=0, redraw = FALSE;
   static int  start_cx, start_cy, start_mx, start_my;
   int         dif_cx=0, dif_cy=0, dif_mx=0, dif_my=0;
   int         hm = glb_ds1[ds1_idx].height_mul;
   int         hd = glb_ds1[ds1_idx].height_div;
   int         edit_end=0;
   int         i;
   static struct // environment backup : this function is for several ds1
   {
      int old_t, old_o, cur_type;
      int old_mode;
      int t, o;
      int start_cx, start_cy, start_mx, start_my;
      int dif_cx, dif_cy, dif_mx, dif_my;
      int edit_end;
   } backup[DS1_MAX];


   // environement managment
   if (ds1_idx != old_ds1_idx)
   {
      // init the backup datas, the very first time
      if (old_ds1_idx == -1)
      {
         for (i=0; i<DS1_MAX; i++)
         {
            backup[i].old_t    = -1;
            backup[i].old_o    = -1;
            backup[i].cur_type = 0;
            backup[i].old_mode = OM_NONE;
         }
         t = o = dif_cx = dif_cy = dif_mx = dif_my = edit_end = -1;
      }
      
      // backup the current environment for the previous ds1
      if (old_ds1_idx != -1)
      {
         backup[old_ds1_idx].old_t    = old_t;
         backup[old_ds1_idx].old_o    = old_o;
         backup[old_ds1_idx].cur_type = cur_type;
         backup[old_ds1_idx].old_mode = old_mode;
         backup[old_ds1_idx].t        = t;
         backup[old_ds1_idx].o        = o;
         backup[old_ds1_idx].start_cx = start_cx;
         backup[old_ds1_idx].start_cy = start_cy;
         backup[old_ds1_idx].start_mx = start_mx;
         backup[old_ds1_idx].start_my = start_my;
         backup[old_ds1_idx].dif_cx   = dif_cx;
         backup[old_ds1_idx].dif_cy   = dif_cy;
         backup[old_ds1_idx].dif_mx   = dif_mx;
         backup[old_ds1_idx].dif_my   = dif_my;
         backup[old_ds1_idx].edit_end = edit_end;
      }

      // restore the previous environement of the current ds1
      old_t    = backup[ds1_idx].old_t;
      old_o    = backup[ds1_idx].old_o;
      cur_type = backup[ds1_idx].cur_type;
      old_mode = backup[ds1_idx].old_mode;
      t        = backup[ds1_idx].t;
      o        = backup[ds1_idx].o;
      start_cx = backup[ds1_idx].start_cx;
      start_cy = backup[ds1_idx].start_cy;
      start_mx = backup[ds1_idx].start_mx;
      start_my = backup[ds1_idx].start_my;
      dif_cx   = backup[ds1_idx].dif_cx;
      dif_cy   = backup[ds1_idx].dif_cy;
      dif_mx   = backup[ds1_idx].dif_mx;
      dif_my   = backup[ds1_idx].dif_my;
      edit_end = backup[ds1_idx].edit_end;
      
      // done
      old_ds1_idx = ds1_idx;
   }
   
   if (old_mode == OM_EDT)
   {
      // handler of objects Type-Id edition
      if (editobj_edit_obj(ds1_idx, & edit_end, mx, my, mb))
         redraw = TRUE;

      if (edit_end)
      {
         old_mode = OM_NONE;
         while (mouse_b & 1)
         {
            // wait until the left mouse button is released
         }
         anim_update_gfx(FALSE);
      }
   }
   else if (old_mode != OM_MOV)
   {
      // NOT moving obj or label
      
      if (editobj_over_obj_lab(ds1_idx, & t, & o, cx, cy, mx, my))
         redraw = TRUE;

      if (mb & 1)
      {
         // button 1 pressed (& not moving obj or label)

         if (key[KEY_ALT] || key[KEY_ALTGR])
         {
            if (o != -1)
            {
               // moving objects / labels
               start_cx = cx;
               start_cy = cy;
               start_mx = (mx + glb_ds1edit.win_preview.x0) * hd / hm;
               start_my = (my + glb_ds1edit.win_preview.y0) * hd / hm;

               if ((cur_type == 1) && (t == 1))
               {
                  if (IS_SELECTED(glb_ds1[ds1_idx].obj[o].label.flags))
                  {
                     editobj_prepare_undo(ds1_idx);
                     editobj_prepare_moving(ds1_idx);
                     old_mode = OM_MOV;
                     while (mouse_b & 1)
                     {
                        // wait until the left mouse button is released
                     }
                  }
               }
               else if ((cur_type == 2) && (t == 2))
               {
                  if (IS_SELECTED(glb_ds1[ds1_idx].obj[o].flags))
                  {
                     editobj_prepare_undo(ds1_idx);
                     editobj_prepare_moving(ds1_idx);
                     old_mode = OM_MOV;
                     while (mouse_b & 1)
                     {
                        // wait until the left mouse button is released
                     }
                  }
               }
            }
         }
         else if (key[KEY_LSHIFT] || key[KEY_RSHIFT])
         {
            // add to selection
            if ((cur_type == 0) || (t == cur_type))
            {
               cur_type = t;
               if (o != -1)
               {
                  if (t == 1)
                  {
                     // add all identical labels ?
                     if (key[KEY_I])
                     {
                        // add all same labels to the selection
                        if (editobj_identical_lab_add(ds1_idx, o))
                        {
                           // at least 1 label added
                           redraw = TRUE;
                           old_mode = OM_ADD;
                           old_o = o;
                           old_t = t;
                        }
                     }
                     else if ( ! IS_SELECTED(glb_ds1[ds1_idx].obj[o].label.flags))
                     {
                        SET_SELECTED(glb_ds1[ds1_idx].obj[o].label.flags);
                        redraw = TRUE;
                        old_mode = OM_ADD;
                        old_o = o;
                        old_t = t;
                     }
                  }
                  else
                  {
                     // add all identical objects ?
                     if (key[KEY_I])
                     {
                        // add all same object to the selection
                        if (editobj_identical_obj_add(ds1_idx, o))
                        {
                           // at least 1 object added
                           redraw = TRUE;
                           old_mode = OM_ADD;
                           old_o = o;
                           old_t = t;
                        }
                     }
                     else if ( ! IS_SELECTED(glb_ds1[ds1_idx].obj[o].flags))
                     {
                        SET_SELECTED(glb_ds1[ds1_idx].obj[o].flags);
                        redraw = TRUE;
                        old_mode = OM_ADD;
                        old_o = o;
                        old_t = t;
                     }
                  }
               }
            }
         }
         else if (key[KEY_LCONTROL] || key[KEY_RCONTROL])
         {
            // remove from selection
            if (o != -1)
            {
               if ((old_t == 1) && (t == 1))
               {
                  // delete all identical labels from the selection ?
                  if (key[KEY_I])
                  {
                     // delete all same labels from the selection
                     if (editobj_identical_lab_del(ds1_idx, o))
                     {
                        // at least 1 object deleted
                        redraw   = TRUE;
                        old_o    = o;
                        old_mode = OM_DEL;
                     }
                  }
                  else if (IS_SELECTED(glb_ds1[ds1_idx].obj[o].label.flags))
                  {
                     DEL_SELECTED(glb_ds1[ds1_idx].obj[o].label.flags);
                     redraw   = TRUE;
                     old_o    = o;
                     old_mode = OM_DEL;
                  }
               }
               else if ((old_t == 2) && (t == 2))
               {
                  // delete all identical objects from the selection ?
                  if (key[KEY_I])
                  {
                     // delete all same object from the selection
                     if (editobj_identical_obj_del(ds1_idx, o))
                     {
                        // at least 1 object deleted
                        redraw   = TRUE;
                        old_o    = o;
                        old_mode = OM_DEL;
                     }
                  }
                  else if (IS_SELECTED(glb_ds1[ds1_idx].obj[o].flags))
                  {
                     DEL_SELECTED(glb_ds1[ds1_idx].obj[o].flags);
                     redraw   = TRUE;
                     old_o    = o;
                     old_mode = OM_DEL;
                  }
               }
            }
         }
         else
         {
            // new selection

            if ((old_o != o) || (old_t != t) || (old_mode != OM_NEW))
            {
               old_t    = t;
               cur_type = t;
               old_o    = o;
               old_mode = OM_NEW;
               redraw   = TRUE;
               editobj_clear_obj_lab_sel(ds1_idx);
               if (o != -1)
               {
                  if (t == 1)
                  {
                     // select all identical labels to make a new selection ?
                     if (key[KEY_I])
                        editobj_identical_lab_new(ds1_idx, o);
                     else
                     {
                        // select just 1 object
                        SET_SELECTED(glb_ds1[ds1_idx].obj[o].label.flags);
                     }
                  }
                  else
                  {
                     // select all identical objects to make a new selection ?
                     if (key[KEY_I])
                     {
                        // yep
                        editobj_identical_obj_new(ds1_idx, o);
                     }
                     else
                     {
                        // select just 1 object
                        SET_SELECTED(glb_ds1[ds1_idx].obj[o].flags);
                     }
                  }
               }
               else
                  cur_type = 0;
            }
         }

         // update object index for path editing mode
         editpath_search_selected_obj(ds1_idx);
      }
      else if (mb & 2)
      {
         if (o != -1)
         {
            // button 2 pressed (& not moving obj or label)
            // ---> edit 1 object
            editobj_prepare_undo(ds1_idx);

            if (glb_ds1[ds1_idx].obj[o].desc_idx == -1)
            {
               glb_ds1[ds1_idx].win_edt_obj.start_act =
               glb_ds1[ds1_idx].win_edt_obj.cur_act   =
                  glb_ds1[ds1_idx].act;

               glb_ds1[ds1_idx].win_edt_obj.cur_type  = glb_ds1[ds1_idx].obj[o].type;
            }
            else
            {
               glb_ds1[ds1_idx].win_edt_obj.start_act =
               glb_ds1[ds1_idx].win_edt_obj.cur_act   =
                  glb_ds1edit.obj_desc[glb_ds1[ds1_idx].obj[o].desc_idx].act;

               glb_ds1[ds1_idx].win_edt_obj.cur_type =
                  glb_ds1edit.obj_desc[glb_ds1[ds1_idx].obj[o].desc_idx].type;

            }

            glb_ds1[ds1_idx].win_edt_obj.desc_cur = glb_ds1[ds1_idx].obj[o].desc_idx;
            
            editobj_prepare_edit_obj_win(ds1_idx, o);
            old_mode = OM_EDT;

            redraw = TRUE;
         }
      }
      else
      {
         // NO button 1 or 2 pressed (& not moving obj nor label)

         if (key[KEY_DEL] || key[KEY_DEL_PAD])
         {
            // delete objects
            while (key[KEY_DEL] || key[KEY_DEL_PAD])
            {
               // wait until both DEL keys are released
            }
            editobj_prepare_undo(ds1_idx);
            editobj_del_obj(ds1_idx);
            redraw = TRUE;
            anim_update_gfx(FALSE);
         }
         else if (key[KEY_U] && (key[KEY_LCONTROL] || key[KEY_RCONTROL]))
         {
            // undo last modification (movement / copy / delete / edit)
            if (glb_ds1[ds1_idx].can_undo_obj)
            {
               while (key[KEY_U])
               {
                  // wait until the 'U' key is released
               }
               editobj_undo(ds1_idx);
               redraw = TRUE;
            }
            anim_update_gfx(FALSE);
         }
         else if (key[KEY_C] && (key[KEY_LCONTROL] || key[KEY_RCONTROL]))
         {
            // copy objects (only if at least 1 obj (no label) is selected)
// TODO
//           if (editobj_center_before_copy(ds1_idx, & cx, & cy, & mx, & my))
//
            if (editobj_count_sel_obj(ds1_idx))
            {
               while (key[KEY_C])
               {
                  // wait until the 'C' key is released
               }
               editobj_prepare_undo(ds1_idx);
               editobj_copy_obj(ds1_idx);
               old_mode = OM_MOV;
               redraw   = TRUE;
               start_cx = cx;
               start_cy = cy;
               start_mx = (mx + glb_ds1edit.win_preview.x0) * hd / hm;
               start_my = (my + glb_ds1edit.win_preview.y0) * hd / hm;
               editobj_prepare_moving(ds1_idx);
            }
         }
         else if (key[KEY_INSERT])
         {
            // create a new object
            while (key[KEY_INSERT])
            {}
            editobj_prepare_undo(ds1_idx);
            editobj_clear_obj_lab_sel(ds1_idx);
            if (editobj_insert_obj(ds1_idx, cx, cy) == 0)
            {
               old_mode = OM_INS;
               redraw = TRUE;
               anim_update_gfx(FALSE);
            }
         }
      }
   }
   else
   {
      // MOVING, obj or label

      if ( ! mb & 1)
      {
         // still moving it
         dif_cx = cx - start_cx;
         dif_cy = cy - start_cy;

         dif_mx = ((mx + glb_ds1edit.win_preview.x0) * hd / hm) - start_mx;
         dif_my = ((my + glb_ds1edit.win_preview.y0) * hd / hm) - start_my;

         if (editobj_moving_obj_lab(ds1_idx, dif_cx, dif_cy, dif_mx, dif_my))
            redraw = TRUE;
      }
      else
      {
         // end the move
         editobj_end_move_obj_lab(ds1_idx);
         old_mode = OM_NONE;
         redraw   = TRUE;
         while (mouse_b & 1)
         {
            // wait until the left mouse button is released
         }
      }
   }

   if (redraw)
      editobj_set_drawing_order(ds1_idx);

   return redraw;
}
