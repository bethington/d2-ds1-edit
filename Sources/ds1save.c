#include "structs.h"
#include "error.h"
#include "ds1save.h"
#include "wPreview.h"


// ==========================================================================
// as expected, save the ds1
//    if is_tmp_file == TRUE, will save a file "*.tmp", else "*-nnn.ds1"
void ds1_save(int ds1_idx, int is_tmp_file)
{
   CELL_W_S * w_ptr;
   CELL_S_S * s_ptr;
   CELL_F_S * f_ptr;
   CELL_T_S * t_ptr;
   int      i=0, x, y, t, cur_o=0, p, used, flen;
   FILE     * out;
   long     n, npc = 0, file_count,
            save_wall_num, save_floor_num;
   char     tmp[512], tmp_name[256], * cptr;
   int      ax, ay, cx, cy, is_data;


   // if no ds1 here, don't process it
   if (strlen(glb_ds1[ds1_idx].name) == 0)
      return;

   // file must have an extension
   strcpy(tmp, glb_ds1[ds1_idx].name);
   if (strlen(tmp) < 4)
   {
      sprintf(tmp, "ds1save(), glb_ds1[%i].ds1_name < 4 chars : \"%s\" (is_tmp_file = %i)",
         ds1_idx, glb_ds1[ds1_idx].name, is_tmp_file);
      ds1edit_error(tmp);
   }

   // set the .TMP or .DS1 extension
   tmp[strlen(tmp)-4] = 0;
   if (is_tmp_file)
   {
      // save a ds1 with the extension .tmp
      sprintf(tmp_name, "%s.tmp", tmp);
      if (file_exists(tmp_name, -1, NULL))
         remove(tmp_name);
   }
   else
   {
      // save a ds1 with the extension .ds1
      // 1st, rename the current .ds1 to an incremental name (for backup)

      // find the 1st free slot
      sprintf(tmp_name, "%s-%03i.ds1", tmp, i);
      while (file_exists(tmp_name, -1, NULL))
      {
         i++;
         sprintf(tmp_name, "%s-%03i.ds1", tmp, i);
      }

      // rename the original ds1 to that incremental name
      if (rename(glb_ds1[ds1_idx].name, tmp_name))
      {
         sprintf(tmp, "ds1save(), couldn't rename %s to %s",
            glb_ds1[ds1_idx].name, tmp_name);
         ds1edit_error(tmp);
      }

      // then, we can save the ds1
      strcpy(tmp_name, glb_ds1[ds1_idx].name);
   }

   // save the ds1, either with a .tmp or a .ds1 extension
   out = fopen(tmp_name, "wb");
   if (out == NULL)
   {
      sprintf(tmp, "ds1save(), can't write %s", tmp_name);
      ds1edit_error(tmp);
   }

   // version
   n = 18;
   fwrite(& n, 4, 1, out);

   // width
   n = glb_ds1[ds1_idx].width - 1;
   fwrite(& n, 4, 1, out);
   
   // height
   n = glb_ds1[ds1_idx].height - 1;
   fwrite(& n, 4, 1, out);
   
   // act
   n = glb_ds1[ds1_idx].act - 1;
   fwrite(& n, 4, 1, out);
   
   // tag type
   fwrite(& glb_ds1[ds1_idx].tag_type, 4, 1, out);

   // filenames

   // count how many filenames (not workspace datas)
   file_count = 0;
   cptr = glb_ds1[ds1_idx].file_buff;
   for (i=0; i < glb_ds1[ds1_idx].file_num; i++)
   {
      flen = strlen(cptr) + 1;
      is_data = FALSE;
      // for all workspace datas
      for(y=0; y < WRKSPC_MAX; y++)
      {
         // check if it's one of our datas

         // for all characters
         x = 0;
         while ( (cptr[x] != 0x00) &&
                 (glb_wrkspc_datas[y].id[x] != 0x00) &&
                 (cptr[x] == glb_wrkspc_datas[y].id[x])
               )
         {
            // continue on next character
            x++;
         }
         if ((glb_wrkspc_datas[y].id[x] == 0x00) && (cptr[x]== '='))
         {
            // this is our data, don't count it
            is_data = TRUE;
            break;
         }
      }
      if (is_data == FALSE)
            file_count++;
      // next filename
      cptr += flen;
   }
   if (glb_config.workspace_enable)
      file_count += WRKSPC_MAX;
   fwrite(& file_count, 4, 1, out);

   // write filenames (not workspace datas)
   cptr = glb_ds1[ds1_idx].file_buff;
   for (i=0; i < glb_ds1[ds1_idx].file_num; i++)
   {
      flen = strlen(cptr) + 1;
      is_data = FALSE;
      // for all workspace datas
      for(y=0; y < WRKSPC_MAX; y++)
      {
         // check if it's one of our datas

         // for all characters
         x = 0;
         while ( (cptr[x] != 0x00) &&
                 (glb_wrkspc_datas[y].id[x] != 0x00) &&
                 (cptr[x] == glb_wrkspc_datas[y].id[x])
               )
         {
            // continue on next character
            x++;
         }
         if ((glb_wrkspc_datas[y].id[x] == 0x00) && (cptr[x]== '='))
         {
            // this is our data, skip it
            is_data = TRUE;
            break;
         }
      }
      if (is_data == FALSE)
         fwrite(cptr, flen, 1, out);

      // next filename
      cptr += flen;
   }

   if (glb_config.workspace_enable)
   {
      // write all workspace datas
      ax = glb_ds1edit.win_preview.x0 + glb_ds1edit.win_preview.w / 2;
      ay = glb_ds1edit.win_preview.y0 + glb_ds1edit.win_preview.h / 2;
      coord_to_tile(ds1_idx, ax, ay, & cx, & cy);
      if ( (glb_ds1edit.mode == MOD_O) ||
           (glb_ds1edit.mode == MOD_P) ||
           (glb_ds1edit.mode == MOD_L) )
      {
         cx /= 5;
         cy /= 5;
      }
      for (i=0; i < WRKSPC_MAX; i++)
      {
         switch (i)
         {
            case WRKSPC_TILE_X :
               sprintf(tmp, "%s=%i", glb_wrkspc_datas[i].id, cx);
               fwrite(tmp, strlen(tmp) + 1, 1, out);
               break;

            case WRKSPC_TILE_Y :
               sprintf(tmp, "%s=%i", glb_wrkspc_datas[i].id, cy);
               fwrite(tmp, strlen(tmp) + 1, 1, out);
               break;

            case WRKSPC_ZOOM :
               sprintf(tmp, "%s=%i", glb_wrkspc_datas[i].id, glb_ds1[ds1_idx].cur_zoom);
               fwrite(tmp, strlen(tmp) + 1, 1, out);
               break;

            case WRKSPC_VERSION :
               sprintf(tmp, "%s=Build YYYY/MM/DD %s, %s",
                  glb_wrkspc_datas[i].id,
                  glb_ds1edit.version_build,
                  glb_ds1edit.version_dll
               );
               fwrite(tmp, strlen(tmp) + 1, 1, out);
               break;

            case WRKSPC_SAVE_COUNT :
               glb_ds1[ds1_idx].save_count++;
               sprintf(tmp, "%s=%lu", glb_wrkspc_datas[i].id, glb_ds1[ds1_idx].save_count);
               fwrite(tmp, strlen(tmp) + 1, 1, out);
               break;
         }
      }
   }

   // minimize ds1 size
   save_wall_num  = glb_ds1[ds1_idx].wall_num;
   save_floor_num = glb_ds1[ds1_idx].floor_num;
   if (glb_config.minimize_ds1 == TRUE)
   {
      // how many wall layers are really used ?
      // (keep a minimum of 1)
      for (i = save_wall_num - 1; i >= 1; i--)
      {
         used = FALSE;

         for (y=0; y < glb_ds1[ds1_idx].height; y++)
         {
            for (x=0; x < glb_ds1[ds1_idx].width; x++)
            {
               t = (y * glb_ds1[ds1_idx].wall_line) + (x * glb_ds1[ds1_idx].wall_num);
               w_ptr = glb_ds1[ds1_idx].wall_buff + t + i;
               if ((w_ptr->prop1 | w_ptr->prop2 | w_ptr->prop3 | w_ptr->prop4 |
                    w_ptr->orientation) != 0)
               {
                  used = TRUE;
                  break;
               }
            }
            if (used)
               break;
         }
         if (used)
            break;
         else
            save_wall_num--;
      }

      // how many floor layers are really used ?
      // (keep a minimum of 1)
      for (i = save_floor_num - 1; i >= 1; i--)
      {
         used = FALSE;

         for (y=0; y < glb_ds1[ds1_idx].height; y++)
         {
            for (x=0; x < glb_ds1[ds1_idx].width; x++)
            {
               t = (y * glb_ds1[ds1_idx].floor_line) + (x * glb_ds1[ds1_idx].floor_num);
               f_ptr = glb_ds1[ds1_idx].floor_buff + t + i;
               if ((f_ptr->prop1 | f_ptr->prop2 | f_ptr->prop3 | f_ptr->prop4) != 0)
               {
                  used = TRUE;
                  break;
               }
            }
            if (used)
               break;
         }
         if (used)
            break;
         else
            save_floor_num--;
      }
   }


   // wall num
   fwrite(& save_wall_num,  4, 1, out);

   // floor num
   fwrite(& save_floor_num, 4, 1, out);

   // walls
   for (i=0; i < save_wall_num; i++)
   {
      // props layer
      for (y=0; y < glb_ds1[ds1_idx].height; y++)
      {
         for (x=0; x < glb_ds1[ds1_idx].width; x++)
         {
            t = (y * glb_ds1[ds1_idx].wall_line) + (x * glb_ds1[ds1_idx].wall_num);
            w_ptr = glb_ds1[ds1_idx].wall_buff + t + i;
            fputc(w_ptr->prop1, out);
            fputc(w_ptr->prop2, out);
            fputc(w_ptr->prop3, out);
            fputc(w_ptr->prop4, out);
         }
      }

      // orientation layer
      for (y=0; y < glb_ds1[ds1_idx].height; y++)
      {
         for (x=0; x < glb_ds1[ds1_idx].width; x++)
         {
            t = (y * glb_ds1[ds1_idx].wall_line) + (x * glb_ds1[ds1_idx].wall_num);
            w_ptr = glb_ds1[ds1_idx].wall_buff + t + i;
            fputc(w_ptr->orientation, out);
            fputc(0, out);
            fputc(0, out);
            fputc(0, out);
         }
      }
   }

   // floors
   for (i=0; i < save_floor_num; i++)
   {
      // props layer
      for (y=0; y < glb_ds1[ds1_idx].height; y++)
      {
         for (x=0; x < glb_ds1[ds1_idx].width; x++)
         {
            t = (y * glb_ds1[ds1_idx].floor_line) + (x * glb_ds1[ds1_idx].floor_num);
            f_ptr = glb_ds1[ds1_idx].floor_buff + t + i;
            fputc(f_ptr->prop1, out);
            fputc(f_ptr->prop2, out);
            fputc(f_ptr->prop3, out);
            fputc(f_ptr->prop4, out);
         }
      }
   }

   // shadows
   for (i=0; i < glb_ds1[ds1_idx].shadow_num; i++)
   {
      // props layer
      for (y=0; y < glb_ds1[ds1_idx].height; y++)
      {
         for (x=0; x < glb_ds1[ds1_idx].width; x++)
         {
            t = (y * glb_ds1[ds1_idx].shadow_line) + (x * glb_ds1[ds1_idx].shadow_num);
            s_ptr = glb_ds1[ds1_idx].shadow_buff + t + i;
            fputc(s_ptr->prop1, out);
            fputc(s_ptr->prop2, out);
            fputc(s_ptr->prop3, out);
            fputc(s_ptr->prop4, out);
         }
      }
   }

   // optional tag layer
   if (glb_ds1[ds1_idx].tag_type)
   {
      for (i=0; i<glb_ds1[ds1_idx].tag_num; i++)
      {
         // props layer
         for (y=0; y < glb_ds1[ds1_idx].height; y++)
         {
            for (x=0; x < glb_ds1[ds1_idx].width; x++)
            {
               t = (y * glb_ds1[ds1_idx].tag_line) + (x * glb_ds1[ds1_idx].tag_num);
               t_ptr = glb_ds1[ds1_idx].tag_buff + t + i;
               fwrite( & t_ptr->num, 4, 1, out);
            }
         }
      }
   }

   // objects
   fwrite(&glb_ds1[ds1_idx].obj_num, 4, 1, out);
   for (i=0; i < glb_ds1[ds1_idx].obj_num; i++)
   {
      fwrite(&glb_ds1[ds1_idx].obj[i].type,      4, 1, out);
      fwrite(&glb_ds1[ds1_idx].obj[i].id,        4, 1, out);
      fwrite(&glb_ds1[ds1_idx].obj[i].x,         4, 1, out);
      fwrite(&glb_ds1[ds1_idx].obj[i].y,         4, 1, out);
      fwrite(&glb_ds1[ds1_idx].obj[i].ds1_flags, 4, 1, out);
      if (glb_ds1[ds1_idx].obj[i].path_num)
         npc++;
   }

   // optional groups
   if (glb_ds1[ds1_idx].tag_type)
   {
      // put a 0 dword
      n = 0;
      fwrite(&n, 4, 1, out);

      // # of groups
      fwrite(&glb_ds1[ds1_idx].group_num, 4, 1, out);

      // groups
      for (i=0; i < glb_ds1[ds1_idx].group_num; i++)
      {
         fwrite(&glb_ds1[ds1_idx].group[i].tile_x, 4, 1, out);
         fwrite(&glb_ds1[ds1_idx].group[i].tile_y, 4, 1, out);
         fwrite(&glb_ds1[ds1_idx].group[i].width,  4, 1, out);
         fwrite(&glb_ds1[ds1_idx].group[i].height, 4, 1, out);
         fwrite(&glb_ds1[ds1_idx].group[i].unk,    4, 1, out);
      }
   }

   // npc paths
   fwrite(&npc, 4, 1, out);
   for (i=0; i < npc; i++)
   {
      while ( ! glb_ds1[ds1_idx].obj[cur_o].path_num)
         cur_o++;
      fwrite(&glb_ds1[ds1_idx].obj[cur_o].path_num, 4, 1, out);
      fwrite(&glb_ds1[ds1_idx].obj[cur_o].x,        4, 1, out);
      fwrite(&glb_ds1[ds1_idx].obj[cur_o].y,        4, 1, out);
      for (p=0; p < glb_ds1[ds1_idx].obj[cur_o].path_num; p++)
      {
         fwrite(&glb_ds1[ds1_idx].obj[cur_o].path[p].x,      4, 1, out);
         fwrite(&glb_ds1[ds1_idx].obj[cur_o].path[p].y,      4, 1, out);
         fwrite(&glb_ds1[ds1_idx].obj[cur_o].path[p].action, 4, 1, out);
      }
      cur_o++;
   }

   // end
   fclose(out);
}
