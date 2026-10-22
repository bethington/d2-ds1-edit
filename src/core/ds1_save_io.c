/* Checked, recoverable DS1 saving. See ds1_save_io.h.
 *
 * The serialization is the editor's version-18 writer (derived from
 * win_ds1edit by Paul Siramy, originally ds1save.c; see NOTICE at the
 * repository root). What changed is how it reaches the disk: the old writer
 * renamed the existing map into a backup *before* writing, ignored every
 * fwrite/fputc result and the fclose result, and changed the map's name for
 * copy-on-save before knowing the save would succeed. A full disk or a failed
 * close therefore left a truncated map under the real name and a "saved"
 * message on screen.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>

#include "structs.h"
#include "core/project.h"
#include "core/ds1.h"
#include "core/ds1_save_io.h"
#include "render/preview.h"

// ==========================================================================
// default (stdio) operations

static FILE * io_open(const char * path, const char * mode) { return fopen(path, mode); }
static size_t io_write(const void * p, size_t s, size_t n, FILE * f) { return fwrite(p, s, n, f); }
static int    io_write_byte(int c, FILE * f) { return fputc(c, f); }
static int    io_close(FILE * f) { return fclose(f); }
static int    io_rename(const char * a, const char * b) { return rename(a, b); }
static int    io_remove(const char * p) { return remove(p); }

static const DS1_SAVE_IO default_io = {
   io_open, io_write, io_write_byte, io_close, io_rename, io_remove
};

const DS1_SAVE_IO * ds1_save_default_io(void)
{
   return & default_io;
}

// ==========================================================================
// checked serialization

typedef struct DS1_WRITER
{
   const DS1_SAVE_IO * io;
   FILE              * f;
   int                 ok;
} DS1_WRITER;

static void w_bytes(DS1_WRITER * w, const void * data, size_t n)
{
   if (! w->ok || n == 0)
      return;
   if (w->io->write(data, 1, n, w->f) != n)
      w->ok = 0;
}

/* on-disk dwords are 32-bit little-endian whatever the host's `long` is */
static void w_i32(DS1_WRITER * w, long v)
{
   uint32_t      u = (uint32_t) v;
   unsigned char b[4];

   b[0] = (unsigned char) (u & 0xFF);
   b[1] = (unsigned char) ((u >> 8) & 0xFF);
   b[2] = (unsigned char) ((u >> 16) & 0xFF);
   b[3] = (unsigned char) ((u >> 24) & 0xFF);
   w_bytes(w, b, 4);
}

static void w_byte(DS1_WRITER * w, int c)
{
   if (! w->ok)
      return;
   if (w->io->write_byte(c, w->f) == EOF)
      w->ok = 0;
}

static int is_workspace_entry(const char * cptr)
{
   int x, y;

   // for all workspace datas
   for (y = 0; y < WRKSPC_MAX; y++)
   {
      // for all characters
      x = 0;
      while ( (cptr[x] != 0x00) &&
              (glb_wrkspc_datas[y].id[x] != 0x00) &&
              (cptr[x] == glb_wrkspc_datas[y].id[x])
            )
      {
         x++;
      }
      if ((glb_wrkspc_datas[y].id[x] == 0x00) && (cptr[x] == '='))
         return TRUE;
   }
   return FALSE;
}

/* Write map `ds1_idx` as a version-18 DS1. `save_count` is the value to
 * record in the workspace entries (the caller commits it on success). */
static void ds1_serialize(DS1_WRITER * w, int ds1_idx, UDWORD save_count)
{
   DS1_S    * d = & glb_ds1[ds1_idx];
   CELL_W_S * w_ptr;
   CELL_S_S * s_ptr;
   CELL_F_S * f_ptr;
   CELL_T_S * t_ptr;
   int      i, x, y, t, cur_o = 0, p, used, flen;
   long     npc = 0, file_count, save_wall_num, save_floor_num;
   char     tmp[512], * cptr;
   int      ax, ay, cx, cy;

   w_i32(w, 18);              // version
   w_i32(w, d->width - 1);    // width
   w_i32(w, d->height - 1);   // height
   w_i32(w, d->act - 1);      // act
   w_i32(w, d->tag_type);     // tag type

   // filenames: count the ones that are not workspace datas
   file_count = 0;
   cptr = d->file_buff;
   for (i = 0; cptr != NULL && i < d->file_num; i++)
   {
      flen = strlen(cptr) + 1;
      if (! is_workspace_entry(cptr))
         file_count++;
      cptr += flen;
   }
   if (glb_config.workspace_enable)
      file_count += WRKSPC_MAX;
   w_i32(w, file_count);

   // write filenames (not workspace datas)
   cptr = d->file_buff;
   for (i = 0; cptr != NULL && i < d->file_num; i++)
   {
      flen = strlen(cptr) + 1;
      if (! is_workspace_entry(cptr))
         w_bytes(w, cptr, flen);
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
      for (i = 0; i < WRKSPC_MAX; i++)
      {
         switch (i)
         {
            case WRKSPC_TILE_X :
               snprintf(tmp, sizeof(tmp), "%s=%i", glb_wrkspc_datas[i].id, cx);
               break;

            case WRKSPC_TILE_Y :
               snprintf(tmp, sizeof(tmp), "%s=%i", glb_wrkspc_datas[i].id, cy);
               break;

            case WRKSPC_ZOOM :
               snprintf(tmp, sizeof(tmp), "%s=%i", glb_wrkspc_datas[i].id, d->cur_zoom);
               break;

            case WRKSPC_VERSION :
               snprintf(tmp, sizeof(tmp), "%s=Build YYYY/MM/DD %s, %s",
                  glb_wrkspc_datas[i].id,
                  glb_ds1edit.version_build ? glb_ds1edit.version_build : "",
                  glb_ds1edit.version_dll ? glb_ds1edit.version_dll : "");
               break;

            case WRKSPC_SAVE_COUNT :
            default :
               /* save_count is UDWORD; promote deliberately for "%lu". */
               snprintf(tmp, sizeof(tmp), "%s=%lu", glb_wrkspc_datas[i].id,
                        (unsigned long) save_count);
               break;
         }
         w_bytes(w, tmp, strlen(tmp) + 1);
      }
   }

   // minimize ds1 size
   save_wall_num  = d->wall_num;
   save_floor_num = d->floor_num;
   if (glb_config.minimize_ds1 == TRUE)
   {
      // how many wall layers are really used ? (keep a minimum of 1)
      for (i = save_wall_num - 1; i >= 1; i--)
      {
         used = FALSE;
         for (y = 0; y < d->height && ! used; y++)
         {
            for (x = 0; x < d->width; x++)
            {
               t = (y * d->wall_line) + (x * d->wall_num);
               w_ptr = d->wall_buff + t + i;
               if ((w_ptr->prop1 | w_ptr->prop2 | w_ptr->prop3 | w_ptr->prop4 |
                    w_ptr->orientation) != 0)
               {
                  used = TRUE;
                  break;
               }
            }
         }
         if (used)
            break;
         save_wall_num--;
      }

      // how many floor layers are really used ? (keep a minimum of 1)
      for (i = save_floor_num - 1; i >= 1; i--)
      {
         used = FALSE;
         for (y = 0; y < d->height && ! used; y++)
         {
            for (x = 0; x < d->width; x++)
            {
               t = (y * d->floor_line) + (x * d->floor_num);
               f_ptr = d->floor_buff + t + i;
               if ((f_ptr->prop1 | f_ptr->prop2 | f_ptr->prop3 | f_ptr->prop4) != 0)
               {
                  used = TRUE;
                  break;
               }
            }
         }
         if (used)
            break;
         save_floor_num--;
      }
   }

   w_i32(w, save_wall_num);   // wall num
   w_i32(w, save_floor_num);  // floor num

   // walls
   for (i = 0; i < save_wall_num; i++)
   {
      // props layer
      for (y = 0; y < d->height; y++)
      {
         for (x = 0; x < d->width; x++)
         {
            t = (y * d->wall_line) + (x * d->wall_num);
            w_ptr = d->wall_buff + t + i;
            w_byte(w, w_ptr->prop1);
            w_byte(w, w_ptr->prop2);
            w_byte(w, w_ptr->prop3);
            w_byte(w, w_ptr->prop4);
         }
      }

      // orientation layer
      for (y = 0; y < d->height; y++)
      {
         for (x = 0; x < d->width; x++)
         {
            t = (y * d->wall_line) + (x * d->wall_num);
            w_ptr = d->wall_buff + t + i;
            w_byte(w, w_ptr->orientation);
            w_byte(w, 0);
            w_byte(w, 0);
            w_byte(w, 0);
         }
      }
   }

   // floors
   for (i = 0; i < save_floor_num; i++)
   {
      for (y = 0; y < d->height; y++)
      {
         for (x = 0; x < d->width; x++)
         {
            t = (y * d->floor_line) + (x * d->floor_num);
            f_ptr = d->floor_buff + t + i;
            w_byte(w, f_ptr->prop1);
            w_byte(w, f_ptr->prop2);
            w_byte(w, f_ptr->prop3);
            w_byte(w, f_ptr->prop4);
         }
      }
   }

   // shadows
   for (i = 0; i < d->shadow_num; i++)
   {
      for (y = 0; y < d->height; y++)
      {
         for (x = 0; x < d->width; x++)
         {
            t = (y * d->shadow_line) + (x * d->shadow_num);
            s_ptr = d->shadow_buff + t + i;
            w_byte(w, s_ptr->prop1);
            w_byte(w, s_ptr->prop2);
            w_byte(w, s_ptr->prop3);
            w_byte(w, s_ptr->prop4);
         }
      }
   }

   // optional tag layer
   if (d->tag_type)
   {
      for (i = 0; i < d->tag_num; i++)
      {
         for (y = 0; y < d->height; y++)
         {
            for (x = 0; x < d->width; x++)
            {
               t = (y * d->tag_line) + (x * d->tag_num);
               t_ptr = d->tag_buff + t + i;
               w_i32(w, (long) t_ptr->num);
            }
         }
      }
   }

   // objects
   w_i32(w, d->obj_num);
   for (i = 0; i < d->obj_num; i++)
   {
      w_i32(w, d->obj[i].type);
      w_i32(w, d->obj[i].id);
      w_i32(w, d->obj[i].x);
      w_i32(w, d->obj[i].y);
      w_i32(w, d->obj[i].ds1_flags);
      if (d->obj[i].path_num)
         npc++;
   }

   // optional groups
   if (d->tag_type)
   {
      w_i32(w, 0);              // the dword before the group count
      w_i32(w, d->group_num);   // # of groups
      for (i = 0; i < d->group_num; i++)
      {
         w_i32(w, (long) d->group[i].tile_x);
         w_i32(w, (long) d->group[i].tile_y);
         w_i32(w, (long) d->group[i].width);
         w_i32(w, (long) d->group[i].height);
         w_i32(w, (long) d->group[i].unk);
      }
   }

   // npc paths
   w_i32(w, npc);
   for (i = 0; i < npc; i++)
   {
      while (! d->obj[cur_o].path_num)
         cur_o++;
      w_i32(w, d->obj[cur_o].path_num);
      w_i32(w, d->obj[cur_o].x);
      w_i32(w, d->obj[cur_o].y);
      for (p = 0; p < d->obj[cur_o].path_num; p++)
      {
         w_i32(w, d->obj[cur_o].path[p].x);
         w_i32(w, d->obj[cur_o].path[p].y);
         w_i32(w, d->obj[cur_o].path[p].action);
      }
      cur_o++;
   }
}

// ==========================================================================
// publish

static int path_exists(const char * path)
{
   struct stat st;
   return stat(path, & st) == 0;
}

static int fail(char * error, size_t cap, const char * fmt, const char * a, const char * b)
{
   if (error != NULL && cap > 0)
      snprintf(error, cap, fmt, a, b ? b : "");
   fprintf(stderr, "ds1_save: ");
   fprintf(stderr, fmt, a, b ? b : "");
   fprintf(stderr, "\n");
   return -1;
}

int ds1_save_with_io(int ds1_idx, int is_tmp_file, const DS1_SAVE_IO * io,
                     char * error, size_t error_cap)
{
   DS1_S      * d;
   DS1_WRITER w;
   char       target[sizeof(glb_ds1[0].name)];
   char       base[sizeof(glb_ds1[0].name)];
   char       dest[sizeof(glb_ds1[0].name) + 16];
   char       staging[sizeof(dest) + sizeof(DS1_SAVE_STAGING_SUFFIX)];
   char       backup[sizeof(glb_ds1[0].name) + 16];
   int        redirected = FALSE, close_rc, i;
   size_t     len;
   UDWORD     new_save_count;

   if (error != NULL && error_cap > 0)
      error[0] = 0;
   if (io == NULL)
      io = & default_io;
   if ((ds1_idx < 0) || (ds1_idx >= DS1_MAX))
      return fail(error, error_cap, "no map slot %s%s", "", NULL);

   d = & glb_ds1[ds1_idx];

   // if no ds1 here, there is nothing to save
   if (strlen(d->name) == 0)
      return 0;

   snprintf(target, sizeof(target), "%s", d->name);

   // Phase 6 copy-on-save: on a real save (not the rolling .TMP autosave),
   // if a project is open and the DS1 was loaded from outside it, redirect
   // the write into the project's Global\Tiles\<suffix> tree. After the
   // first redirected save, the map's name lives inside the project, so
   // subsequent saves are in-place and the backup-rename path works.
   // TMP saves stay next to the origin so auto-saves don't silently move a
   // file into the project before the user has explicitly committed to it.
   // The new name is committed only once the file is actually there.
   if (! is_tmp_file)
   {
      char redir[sizeof(glb_ds1[0].name)];
      int  r = project_redirect_ds1_save_path(d->name, redir, (int) sizeof(redir));

      if (r < 0)
         return fail(error, error_cap,
                     "the project copy of %s would need a path longer than the "
                     "editor supports; nothing was saved%s", d->name, NULL);
      if (r > 0)
      {
         fprintf(stdout, "copy-on-save: %s -> %s\n", d->name, redir);
         fprintf(stderr, "copy-on-save: %s -> %s\n", d->name, redir);
         if (! project_ensure_parent_dirs(redir))
            return fail(error, error_cap,
                        "can't create the project folder for %s; nothing was "
                        "saved and %s is unchanged", redir, d->name);
         snprintf(target, sizeof(target), "%s", redir);
         redirected = TRUE;
      }
   }

   // file must have an extension
   len = strlen(target);
   if (len < 4)
      return fail(error, error_cap, "map name \"%s\" is too short to carry an "
                  "extension%s", target, NULL);
   memcpy(base, target, len - 4);
   base[len - 4] = 0;

   if (is_tmp_file)
      snprintf(dest, sizeof(dest), "%s.tmp", base);
   else
      snprintf(dest, sizeof(dest), "%s", target);
   snprintf(staging, sizeof(staging), "%s%s", dest, DS1_SAVE_STAGING_SUFFIX);

   // 1. serialize into the staging file, checking every write and the close
   new_save_count = d->save_count + (glb_config.workspace_enable ? 1 : 0);
   w.io = io;
   w.ok = 1;
   w.f  = io->open(staging, "wb");
   if (w.f == NULL)
      return fail(error, error_cap, "can't write %s; %s is unchanged", staging, dest);
   ds1_serialize(& w, ds1_idx, new_save_count);
   close_rc = io->close(w.f);
   if (! w.ok || close_rc != 0)
   {
      io->remove(staging);
      return fail(error, error_cap, "writing %s failed (disk full or I/O error); "
                  "%s is unchanged", staging, dest);
   }

   // 2. publish
   if (is_tmp_file)
   {
      // the rolling autosave: replace the previous .tmp
      if (path_exists(dest) && io->remove(dest) != 0)
      {
         io->remove(staging);
         return fail(error, error_cap, "can't replace the autosave %s%s", dest, NULL);
      }
      if (io->rename(staging, dest) != 0)
      {
         io->remove(staging);
         return fail(error, error_cap, "can't publish the autosave %s%s", dest, NULL);
      }
   }
   else if (path_exists(dest))
   {
      // keep the current file as the first free <name>-NNN.ds1
      i = 0;
      do
      {
         snprintf(backup, sizeof(backup), "%s-%03i.ds1", base, i);
         i++;
      } while (path_exists(backup));

      if (io->rename(dest, backup) != 0)
      {
         io->remove(staging);
         return fail(error, error_cap, "couldn't back up %s to %s; nothing was "
                     "changed", dest, backup);
      }
      if (io->rename(staging, dest) != 0)
      {
         if (io->rename(backup, dest) == 0)
         {
            io->remove(staging);
            return fail(error, error_cap, "couldn't replace %s; it is unchanged%s",
                        dest, NULL);
         }
         return fail(error, error_cap, "couldn't replace %s; the previous version "
                     "is preserved as %s", dest, backup);
      }
   }
   else
   {
      // fresh path (e.g. the first copy-on-save into the project)
      if (io->rename(staging, dest) != 0)
      {
         io->remove(staging);
         return fail(error, error_cap, "couldn't create %s%s", dest, NULL);
      }
   }

   // 3. the file is in place: commit the state changes
   if (redirected)
      snprintf(d->name, sizeof(d->name), "%s", target);
   d->save_count = new_save_count;
   return 0;
}

int ds1_save_checked(int ds1_idx, int is_tmp_file, char * error, size_t error_cap)
{
   return ds1_save_with_io(ds1_idx, is_tmp_file, & default_io, error, error_cap);
}
