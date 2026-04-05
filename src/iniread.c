#include "structs.h"
#include "error.h"
#include <string.h>
#include "iniread.h"


// ==========================================================================
void ini_read(char * ininame)
{
   typedef enum {T_NULL, T_MPQ, T_MOD, T_STR, T_INT, T_GAM, T_YES, T_ZOOM} TYPE_E;
   static struct // 'static' because we need to keep the default string values
   {
      char   name[30];
      TYPE_E type;
      void   * data_ptr;
      void   * def;
   } datas[] =
   {
      {"d2char",                  T_MPQ,  & glb_config.mpq_file[3],          ""},
      {"d2data",                  T_MPQ,  & glb_config.mpq_file[2],          ""},
      {"d2exp",                   T_MPQ,  & glb_config.mpq_file[1],          ""},
      {"patch_d2",                T_MPQ,  & glb_config.mpq_file[0],          ""},
      {"mod_dir",                 T_MOD,  & glb_config.mod_dir[0],           ""},
      {"full_screen",             T_YES,  & glb_config.fullscreen,           "YES"},
      {"screen_width",            T_INT,  & glb_config.screen.width,         (void *) 800},
      {"screen_height",           T_INT,  & glb_config.screen.height,        (void *) 600},
      {"screen_depth",            T_INT,  & glb_config.screen.depth,         (void *) 8},
      {"refresh_rate",            T_INT,  & glb_config.screen.refresh,       (void *) 60},
      {"keyb_scroll_x",           T_INT,  & glb_config.scroll.keyb.x,        (void *) 40},
      {"keyb_scroll_y",           T_INT,  & glb_config.scroll.keyb.y,        (void *) 20},
      {"mouse_scroll_x",          T_INT,  & glb_config.scroll.mouse.x,       (void *) 20},
      {"mouse_scroll_y",          T_INT,  & glb_config.scroll.mouse.y,       (void *) 10},
      {"edit_scroll_x",           T_INT,  & glb_config.scroll.edit.x,        (void *) 20},
      {"edit_scroll_y",           T_INT,  & glb_config.scroll.edit.y,        (void *) 20},
      {"obj_edit_scroll",         T_INT,  & glb_config.scroll.obj_edit,      (void *) 2},
      {"mouse_speed_x",           T_INT,  & glb_config.mouse_speed.x,        (void *) 2},
      {"mouse_speed_y",           T_INT,  & glb_config.mouse_speed.y,        (void *) 2},
      {"gamma_correction",        T_GAM,  & glb_config.gamma,                "1.30"},
      {"only_normal_type2",       T_YES,  & glb_config.normal_type2,         "YES"},
      {"always_max_layers",       T_YES,  & glb_config.always_max_layers,    "YES"},
      {"stretch_sprite",          T_YES,  & glb_config.stretch_sprites,      "YES"},
      {"winobj_can_scroll_keyb",  T_YES,  & glb_config.winobj_scroll_keyb,   "YES"},
      {"winobj_can_scroll_mouse", T_YES,  & glb_config.winobj_scroll_mouse,  "NO"},
      {"center_zoom",             T_ZOOM, & glb_config.center_zoom,          "1:1"},
      {"nb_type1_per_act",        T_INT,  & glb_config.nb_type1_per_act,     (void *) 60},
      {"nb_type2_per_act",        T_INT,  & glb_config.nb_type2_per_act,     (void *) 150},
      {"ds1_saved_minimize",      T_YES,  & glb_config.minimize_ds1,         "YES"},
      {"lower_speed_zoom_out",    T_YES,  & glb_config.lower_speed_zoom_out, "NO"},
      {"workspace_enable",        T_YES,  & glb_config.workspace_enable,     "YES"},
      {"", T_NULL, NULL, NULL} // do not remove
   };
   int        i, val, is_ok = TRUE, n, len;
   const char * str;
   char       tmp[256], * buf, ** tmpptr;


   printf("ini_read()\n");
   fprintf(stderr, "ini_read(), load ds1edit.ini\n");
   fflush(stdout);
   fflush(stderr);
   
   set_config_file(ininame);
   i = 0;
   while (datas[i].type != T_NULL)
   {
      str = get_config_string(NULL, datas[i].name, (const char *) datas[i].def);
      if (str == NULL)
      {
         fprintf(
            stderr,
            "   error, line not found : <%s>\n",
            datas[i].name
         );
         fprintf(
            stdout,
            "   error, line not found : <%s>\n",
            datas[i].name
         );
         is_ok = FALSE;
      }
      switch (datas[i].type)
      {
         // string
         case T_STR :
            if (strlen(str))
               datas[i].data_ptr = (char *) str;
            break;

         // number
         case T_INT :
            val = get_config_int(NULL, datas[i].name, (int) datas[i].def);
            * ((int *) datas[i].data_ptr) = val;
            break;

         // gamma correction
         case T_GAM :
            n=0;
            while ( (n < GC_MAX) &&
                    (strcmp(str, glb_gamma_str[n].str) != 0) )
               n++;
            if (n < GC_MAX)
               glb_config.gamma = glb_gamma_str[n].val;
            break;

         // mpq file
         case T_MPQ :
            len = strlen(str);
            if (len)
            {
               buf = (char *) malloc(sizeof(char) * (len+1));
               if (buf == NULL)
                  ds1edit_error("read_ini(), malloc() error on Mpq name");
               else
               {
                  strcpy(buf, str);
                  tmpptr = datas[i].data_ptr;
                  * tmpptr = buf;
               }
            }
            break;

         // mod directory
         case T_MOD :
            len = strlen(str);
            if (len)
            {
               buf = (char *) malloc(sizeof(char) * (len+1));
               if (buf == NULL)
                  ds1edit_error("read_ini(), malloc() error on Mod name");
               else
               {
                  strcpy(buf, str);
                  if ((buf[strlen(buf)-1] == '\\') || (buf[strlen(buf)-1] == '/'))
                     buf[strlen(buf)-1] = 0;
                  tmpptr = datas[i].data_ptr;
                  * tmpptr = buf;
               }
            }
            break;

         // read a YES/NO string, but store it as TRUE/FALSE
         case T_YES :
            if (strlen(str))
            {
               if (stricmp(str, "YES") == 0)
                  * ((int *) datas[i].data_ptr) = TRUE;
               else
                  * ((int *) datas[i].data_ptr) = FALSE;
            }
            break;

         // zoom string
         case T_ZOOM :
            if (strlen(str))
            {
               // default value
               * ((int *) datas[i].data_ptr) = -1;

               // read value
               if (stricmp(str, "1:1") == 0)
                  * ((int *) datas[i].data_ptr) = ZM_11;
               else if (stricmp(str, "1:2") == 0)
                  * ((int *) datas[i].data_ptr) = ZM_12;
               else if (stricmp(str, "1:4") == 0)
                  * ((int *) datas[i].data_ptr) = ZM_14;
               else if (stricmp(str, "1:8") == 0)
                  * ((int *) datas[i].data_ptr) = ZM_18;
               else if (stricmp(str, "1:16") == 0)
                  * ((int *) datas[i].data_ptr) = ZM_116;
            }

               datas[i].data_ptr = (char *) str;
            break;

      }
      i++;
   }
   if (is_ok != TRUE)
   {
      sprintf(
         tmp,
            "%s is not valid.\n"
            "Delete it, and relaunch this prog to create a new good one,\n"
            "then edit it to make changes where necessary, then relaunch this prog",
         ininame
      );
      ds1edit_error(tmp);
   }
}
