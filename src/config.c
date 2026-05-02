#include <stdio.h>
#include <string.h>
#include "structs.h"
#include "error.h"
#include "config.h"
#include "core/export_presets.h"

// ==========================================================================
// create a new ds1edit.ini
void ini_create(char *ininame)
{
   FILE *out;
   char tmp[80];

   printf("can't open %s, creating it\n", ininame);
   out = fopen(ininame, "wt");
   if (out == NULL)
   {
      sprintf(tmp, "can't create %s", ininame);
      ds1edit_error(tmp);
   }
   fputs(
       "; D2 install directory. When set, the editor resolves the standard\n"
       "; Blizzard MPQs automatically. Leave blank to auto-detect from the\n"
       "; registry / common install paths.\n"
       "; =================================================================\n"
       "d2_install =\n"
       "\n"
       "; Explicit MPQ paths override d2_install / auto-detection per slot.\n"
       "; File lookup scans mod_dir first, then patch_d2, d2exp, d2data,\n"
       "; d2char (matching D2's own load order).\n"
       "; =================================================================\n"
       "d2char   =\n"
       "d2data   =\n"
       "d2exp    =\n"
       "patch_d2 =\n"
       "mod_dir  =\n"
       "\n"
      "; Optional remote PNG upscaling service. When enabled and a base URL\n"
      "; is configured, export flows can offer remote 2x/4x upscaling.\n"
      "; =================================================================\n"
      "upscale_enabled     = NO\n"
      "upscale_service_url =\n"
      "\n"
      "; PNG export filter. When YES (default), multi-frame DC6 files\n"
      "; (animations, multi-direction sprites) are skipped during export.\n"
      "; Multi-frame export to a dedicated animation format is a planned\n"
      "; future feature; for now PNG output is single-frame only. Flip to\n"
      "; NO to fall back to the prior behavior (export every frame of\n"
      "; multi-frame DC6 alongside single-frame icons).\n"
      "; =================================================================\n"
      "export_dc6_single_frame_only = YES\n"
      "\n"
       "; Example explicit configuration:\n"
       "; d2_install = C:\\Diablo2\n"
       "; mod_dir    = C:\\Diablo2\\mods\\my_mod\n"
      "; upscale_enabled     = YES\n"
      "; upscale_service_url = http://10.0.10.30:8080\n"
       "\n"
       "\n"
       "; data_dir overrides where the editor looks for its runtime data\n"
       "; (palettes, gamma, obj.txt, ds1edit.dt1). Default is Data\\\n"
       "; data_dir =\n"
       "\n"
       "\n"
       "; screen configuration\n"
       "; if full_screen is not set to YES, it'll be a windowed screen\n"
       "; =============================================================\n"
       "full_screen   = YES\n"
       "screen_width  = 800\n"
       "screen_height = 600\n"
       "\n"
       "\n"
       "; Try to use this refresh rate, if possible. Not all drivers are able to\n"
       "; control this at all, and even when they can, not all rates will be\n"
       "; possible on all hardware, so the actual settings may differ from what\n"
       "; you requested. Some non-exhaustive values : 60, 70, 72, 75, 85, 100, 120\n"
       "; ========================================================================\n"
       "refresh_rate = 60\n"
       "\n"
       "\n"
       "; speed of scrolls, in pixels\n"
       "; ===========================\n"
       "keyb_scroll_x  = 40\n"
       "keyb_scroll_y  = 20\n"
       "mouse_scroll_x = 20\n"
       "mouse_scroll_y = 10\n"
       "edit_scroll_x  = 20\n"
       "edit_scroll_y  = 20\n"
       "\n"
       "\n"
       "; speed of scroll in the Object Editing window, in rows\n"
       "; =====================================================\n"
       "obj_edit_scroll = 2\n"
       "\n"
       "\n"
       "\n"
       "\n"
       "\n"
       "; misc options\n"
       "; ============\n"
       "\n"
       "\n"
       "; default gamma correction\n"
       "; valid value : 0.60, 0.62, [...] 0.98, 1.00, 1.10, 1.20, [...], 3.00\n"
       "; -------------------------------------------------------------------\n"
       "gamma_correction = 1.30\n"
       "\n"
       "\n"
       "; does the editor allow you to use Type 2 objects of higher act ?\n"
       "; if this line is NOT set to YES, the editor will allow it\n"
       "; ---------------------------------------------------------------\n"
       "only_normal_type2 = YES\n"
       "\n"
       "\n"
       "; does the editor always modified the ds1 to have 2 floors layers and\n"
       "; 4 walls layers ?\n"
       "; if this line is NOT set to YES, the editor won't make any change\n"
       "; -------------------------------------------------------------------\n"
       "always_max_layers = YES\n"
       "\n"
       "\n"
       "; resize the sprites when zooming\n"
       "; -------------------------------\n"
       "stretch_sprite = YES\n"
       "\n"
       "\n"
       "; when editing an object, posibility to scroll the main tile editing window\n"
       "; with the arrow keys\n"
       "; -------------------------------------------------------------------------\n"
       "winobj_can_scroll_keyb = YES\n"
       "\n"
       "\n"
       "; when editing an object, posibility to scroll the main tile editing window\n"
       "; with the mouse on the border of the screen\n"
       "; -------------------------------------------------------------------------\n"
       "winobj_can_scroll_mouse = NO\n"
       "\n"
       "\n"
       "; after a Centering (with the 'C' key), wich zoom ?\n"
       "; possible values : NO_CHANGE, 1:1, 1:2, 1:4, 1:8, 1:16\n"
       "; -----------------------------------------------------\n"
       "center_zoom = 1:1\n"
       "\n"
       "\n"
       "; default zoom level when opening a DS1 file\n"
       "; possible values : 1:1, 1:2, 1:4, 1:8, 1:16\n"
       "; -----------------------------------------------\n"
       "default_zoom = 1:1\n"
       "\n"
       "\n"
       "; What are the sizes of object tables in DLL for type 1 and 2 ?\n"
       "; normal values are :\n"
       ";    *  60 entries per act for type 1 objects\n"
       ";    * 150 entries per act for type 2 objects\n"
       "; You shouldn't change these values unless you have a modified DLL\n"
       "; ----------------------------------------------------------------\n"
       "nb_type1_per_act = 60\n"
       "nb_type2_per_act = 150\n"
       "\n"
       "\n"
       "; minimize file size of saved ds1 ?\n"
       "; ---------------------------------\n"
       "ds1_saved_minimize = YES\n"
       "\n"
       "\n"
       "; reduce scrolling speed when zooming out ?\n"
       "; -----------------------------------------\n"
       "lower_speed_zoom_out = NO\n"
       "\n"
       "\n"
       "; enable the workspace feature ?\n"
       "; ------------------------------\n"
       "workspace_enable = YES\n"
       "\n"
       "\n"
       "; Export presets shown in the unified Ctrl+Shift+A export action's\n"
       "; scope picker. Each entry is \"name = type | pattern\" where type is\n"
       "; one of all/dt1/dc6/dcc and pattern is a glob matched against\n"
       "; virtual asset paths under the mod overlay. Order in this section\n"
       "; is the order in the picker. Wildcard syntax: * (any chars except\n"
       "; \\), ? (one char except \\), ** (any number of subfolders).\n"
       "; ==================================================================\n"
       "[export_presets]\n"
       "items_all     = dc6 | data\\global\\items\\*.dc6\n"
       "items_inv     = dc6 | data\\global\\items\\inv*.dc6\n"
       "items_potions = dc6 | data\\global\\items\\pot*.dc6\n"
       "tiles_all     = dt1 | data\\global\\tiles\\**\\*.dt1\n"
       "tiles_act1    = dt1 | data\\global\\tiles\\ACT1\\**\\*.dt1\n"
       "tiles_act2    = dt1 | data\\global\\tiles\\ACT2\\**\\*.dt1\n"
       "tiles_act3    = dt1 | data\\global\\tiles\\ACT3\\**\\*.dt1\n"
       "tiles_act4    = dt1 | data\\global\\tiles\\ACT4\\**\\*.dt1\n"
       "tiles_act5    = dt1 | data\\global\\tiles\\ACT5\\**\\*.dt1\n",
       out);

   fclose(out);
   printf("new ds1edit.ini was created\n");
   fprintf(stderr, "new ds1edit.ini was created\n");
}

// ==========================================================================
void ini_read(char *ininame)
{
   typedef enum
   {
      T_NULL,
      T_MPQ,
      T_MOD,
      T_STR,
      T_INT,
      T_GAM,
      T_YES,
      T_ZOOM
   } TYPE_E;
   static struct // 'static' because we need to keep the default string values
   {
      char name[30];
      TYPE_E type;
      void *data_ptr;
      void *def;
   } datas[] =
       {
           {"d2_install", T_MOD, &glb_config.d2_install, ""},
           {"upscale_enabled", T_YES, &glb_config.upscale_enabled, "NO"},
           {"upscale_service_url", T_STR, &glb_config.upscale_service_url, ""},
           {"export_dc6_single_frame_only", T_YES, &glb_config.export_dc6_single_frame_only, "YES"},
           {"d2char", T_MPQ, &glb_config.mpq_file[3], ""},
           {"d2data", T_MPQ, &glb_config.mpq_file[2], ""},
           {"d2exp", T_MPQ, &glb_config.mpq_file[1], ""},
           {"patch_d2", T_MPQ, &glb_config.mpq_file[0], ""},
           {"mod_dir", T_MOD, &glb_config.mod_dir[0], ""},
           {"full_screen", T_YES, &glb_config.fullscreen, "YES"},
           {"screen_width", T_INT, &glb_config.screen.width, (void *)800},
           {"screen_height", T_INT, &glb_config.screen.height, (void *)600},
           {"refresh_rate", T_INT, &glb_config.screen.refresh, (void *)60},
           {"keyb_scroll_x", T_INT, &glb_config.scroll.keyb.x, (void *)40},
           {"keyb_scroll_y", T_INT, &glb_config.scroll.keyb.y, (void *)20},
           {"mouse_scroll_x", T_INT, &glb_config.scroll.mouse.x, (void *)20},
           {"mouse_scroll_y", T_INT, &glb_config.scroll.mouse.y, (void *)10},
           {"edit_scroll_x", T_INT, &glb_config.scroll.edit.x, (void *)20},
           {"edit_scroll_y", T_INT, &glb_config.scroll.edit.y, (void *)20},
           {"obj_edit_scroll", T_INT, &glb_config.scroll.obj_edit, (void *)2},
           {"gamma_correction", T_GAM, &glb_config.gamma, "1.30"},
           {"only_normal_type2", T_YES, &glb_config.normal_type2, "YES"},
           {"always_max_layers", T_YES, &glb_config.always_max_layers, "YES"},
           {"stretch_sprite", T_YES, &glb_config.stretch_sprites, "YES"},
           {"winobj_can_scroll_keyb", T_YES, &glb_config.winobj_scroll_keyb, "YES"},
           {"winobj_can_scroll_mouse", T_YES, &glb_config.winobj_scroll_mouse, "NO"},
           {"center_zoom", T_ZOOM, &glb_config.center_zoom, "1:1"},
           {"default_zoom", T_ZOOM, &glb_config.default_zoom, "1:1"},
           {"nb_type1_per_act", T_INT, &glb_config.nb_type1_per_act, (void *)60},
           {"nb_type2_per_act", T_INT, &glb_config.nb_type2_per_act, (void *)150},
           {"ds1_saved_minimize", T_YES, &glb_config.minimize_ds1, "YES"},
           {"lower_speed_zoom_out", T_YES, &glb_config.lower_speed_zoom_out, "NO"},
           {"workspace_enable", T_YES, &glb_config.workspace_enable, "YES"},
           {"", T_NULL, NULL, NULL} // do not remove
       };
   int i, val, is_ok = TRUE, n, len;
   const char *str;
   char tmp[256], *buf, **tmpptr;

   printf("ini_read()\n");
   fprintf(stderr, "ini_read(), load ds1edit.ini\n");
   fflush(stdout);
   fflush(stderr);

   if (a5_config) al_destroy_config(a5_config);
   a5_config = al_load_config_file(ininame);
   if (a5_config == NULL) a5_config = al_create_config();

   i = 0;
   while (datas[i].type != T_NULL)
   {
      {
         const char *v = al_get_config_value(a5_config, "", datas[i].name);
         str = v ? v : (const char *)datas[i].def;
      }
      if (str == NULL)
      {
         fprintf(
             stderr,
             "   error, line not found : <%s>\n",
             datas[i].name);
         fprintf(
             stdout,
             "   error, line not found : <%s>\n",
             datas[i].name);
         is_ok = FALSE;
      }
      switch (datas[i].type)
      {
      // string
      case T_STR:
         len = strlen(str);
         if (len)
         {
            buf = (char *)malloc(sizeof(char) * (len + 1));
            if (buf == NULL)
               ds1edit_error("read_ini(), malloc() error on string value");
            else
            {
               strcpy(buf, str);
               tmpptr = datas[i].data_ptr;
               *tmpptr = buf;
            }
         }
         break;

      // number
      case T_INT:
         {
            const char *v = al_get_config_value(a5_config, "", datas[i].name);
            val = v ? atoi(v) : (int)datas[i].def;
         }
         *((int *)datas[i].data_ptr) = val;
         break;

      // gamma correction
      case T_GAM:
         n = 0;
         while ((n < GC_MAX) &&
                (strcmp(str, glb_gamma_str[n].str) != 0))
            n++;
         if (n < GC_MAX)
            glb_config.gamma = glb_gamma_str[n].val;
         break;

      // mpq file
      case T_MPQ:
         len = strlen(str);
         if (len)
         {
            buf = (char *)malloc(sizeof(char) * (len + 1));
            if (buf == NULL)
               ds1edit_error("read_ini(), malloc() error on Mpq name");
            else
            {
               strcpy(buf, str);
               tmpptr = datas[i].data_ptr;
               *tmpptr = buf;
            }
         }
         break;

      // mod directory
      case T_MOD:
         len = strlen(str);
         if (len)
         {
            buf = (char *)malloc(sizeof(char) * (len + 1));
            if (buf == NULL)
               ds1edit_error("read_ini(), malloc() error on Mod name");
            else
            {
               strcpy(buf, str);
               if ((buf[strlen(buf) - 1] == '\\') || (buf[strlen(buf) - 1] == '/'))
                  buf[strlen(buf) - 1] = 0;
               tmpptr = datas[i].data_ptr;
               *tmpptr = buf;
            }
         }
         break;

      // read a YES/NO string, but store it as TRUE/FALSE
      case T_YES:
         if (strlen(str))
         {
            if (stricmp(str, "YES") == 0)
               *((int *)datas[i].data_ptr) = TRUE;
            else
               *((int *)datas[i].data_ptr) = FALSE;
         }
         break;

      // zoom string
      case T_ZOOM:
         if (strlen(str))
         {
            // default value
            *((int *)datas[i].data_ptr) = -1;

            // read value
            if (stricmp(str, "2:1") == 0)
               *((int *)datas[i].data_ptr) = ZM_21;
            else if (stricmp(str, "1:1") == 0)
               *((int *)datas[i].data_ptr) = ZM_11;
            else if (stricmp(str, "1:2") == 0)
               *((int *)datas[i].data_ptr) = ZM_12;
            else if (stricmp(str, "1:4") == 0)
               *((int *)datas[i].data_ptr) = ZM_14;
            else if (stricmp(str, "1:8") == 0)
               *((int *)datas[i].data_ptr) = ZM_18;
            else if (stricmp(str, "1:16") == 0)
               *((int *)datas[i].data_ptr) = ZM_116;
         }
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
          ininame);
      ds1edit_error(tmp);
   }

   // [export_presets] section — user-defined wildcard scope presets shown
   // in the unified export action's scope picker.
   export_presets_reset();
   {
      ALLEGRO_CONFIG_ENTRY *ent_iter = NULL;
      const char *key = al_get_first_config_entry(a5_config,
                                                  "export_presets",
                                                  &ent_iter);
      while (key != NULL)
      {
         const char *value = al_get_config_value(a5_config,
                                                 "export_presets", key);
         EXPORT_PRESET_S preset;
         if (value != NULL && export_preset_parse(key, value, &preset))
            export_presets_add(&preset);
         else
            fprintf(stderr,
                    "ini_read(): ignored malformed [export_presets] entry: %s\n",
                    key);
         key = al_get_next_config_entry(&ent_iter);
      }
   }

   // MPQ slot resolution runs later in main() — after preferences are loaded
   // so last_d2_install can supply a fallback before we hit the registry.
}
