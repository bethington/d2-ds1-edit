/* MPQ writer. See mpq_write.h. Every on-disk dword goes through put_le/get_le,
 * and the tables are kept in host order in memory, so a big-endian host writes
 * the same bytes. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "core/mpq_crypt.h"
#include "core/mpq_write.h"

#define HASH_EMPTY    0xFFFFFFFFu
#define HASH_DELETED  0xFFFFFFFEu
#define FLAG_EXISTS   0x80000000u
#define ARCHIVE_MAX   0xFFFFFFFFu

typedef struct BASE_S
{
   unsigned int hash_pos, block_pos, hash_count, block_count;
   unsigned int * hash;    /* decrypted, host order; hash_count * 4 dwords */
   unsigned int * block;   /* decrypted, host order; block_count * 4 dwords */
} BASE_S;

static unsigned int get_le(const unsigned char * p)
{
   return (unsigned int) p[0] | ((unsigned int) p[1] << 8) |
          ((unsigned int) p[2] << 16) | ((unsigned int) p[3] << 24);
}

static void put_le(unsigned char * p, unsigned int v)
{
   p[0] = (unsigned char) (v & 0xFF);
   p[1] = (unsigned char) ((v >> 8) & 0xFF);
   p[2] = (unsigned char) ((v >> 16) & 0xFF);
   p[3] = (unsigned char) ((v >> 24) & 0xFF);
}

static int fail(char * error, int cap, const char * text)
{
   if (error != NULL && cap > 0)
   {
      strncpy(error, text, (size_t) cap - 1);
      error[cap - 1] = '\0';
   }
   return -1;
}

/* Little-endian dwords to f; 1 if all were written. */
static int write_dwords(FILE * f, const unsigned int * v, unsigned int n)
{
   unsigned char buf[4096];

   while (n > 0)
   {
      unsigned int chunk = n > 1024 ? 1024 : n, i;

      for (i = 0; i < chunk; i++)
         put_le(buf + i * 4, v[i]);
      if (fwrite(buf, 4, chunk, f) != chunk)
         return 0;
      v += chunk;
      n -= chunk;
   }
   return 1;
}

/* Little-endian dwords from f, as host values; 1 if all were read. */
static int read_dwords(FILE * f, unsigned int * v, unsigned int n)
{
   unsigned char buf[4096];

   while (n > 0)
   {
      unsigned int chunk = n > 1024 ? 1024 : n, i;

      if (fread(buf, 4, chunk, f) != chunk)
         return 0;
      for (i = 0; i < chunk; i++)
         v[i] = get_le(buf + i * 4);
      v += chunk;
      n -= chunk;
   }
   return 1;
}

/* Copy n bytes of in (from its current position) to out. */
static int copy_bytes(FILE * in, FILE * out, unsigned long n)
{
   unsigned char buf[16384];

   while (n > 0)
   {
      size_t want = n > sizeof(buf) ? sizeof(buf) : (size_t) n;

      if (fread(buf, 1, want, in) != want || fwrite(buf, 1, want, out) != want)
         return 0;
      n -= (unsigned long) want;
   }
   return 1;
}

static int is_pow2(unsigned int n)
{
   return n != 0 && (n & (n - 1)) == 0;
}

static int same_name_ignoring_case(const char * a, const char * b)
{
   for (; *a != '\0' && *b != '\0'; a++, b++)
      if (toupper((unsigned char) *a) != toupper((unsigned char) *b))
         return 0;
   return *a == *b;
}

/* Mark every live slot for this name deleted. The name checks are cleared too, so
   a reader that doesn't look at the block index can't find the name any more. */
static void delete_slot(unsigned int * hash, unsigned int hash_count, const char * name)
{
   unsigned int a = mpq_hash_string(name, MPQ_HASH_A);
   unsigned int b = mpq_hash_string(name, MPQ_HASH_B);
   unsigned int start = mpq_hash_string(name, MPQ_HASH_OFFSET) & (hash_count - 1);
   unsigned int i;

   for (i = 0; i < hash_count; i++)
   {
      unsigned int * e = hash + ((start + i) & (hash_count - 1)) * 4;

      if (e[3] == HASH_EMPTY)
         break;
      if (e[3] != HASH_DELETED && e[0] == a && e[1] == b)
      {
         e[0] = HASH_EMPTY;
         e[1] = HASH_EMPTY;
         e[2] = HASH_EMPTY;
         e[3] = HASH_DELETED;
      }
   }
}

/* Give this name one slot, at locale 0 and platform 0, pointing at this block. Any
   slot the name already has, in whatever locale, is deleted first, so none is left
   pointing at a stale block. The new slot is the first empty or deleted one along
   its probe. */
static int set_slot(unsigned int * hash, unsigned int hash_count, const char * name,
                    unsigned int block)
{
   unsigned int start = mpq_hash_string(name, MPQ_HASH_OFFSET) & (hash_count - 1);
   unsigned int i;

   delete_slot(hash, hash_count, name);
   for (i = 0; i < hash_count; i++)
   {
      unsigned int * e = hash + ((start + i) & (hash_count - 1)) * 4;

      if (e[3] == HASH_EMPTY || e[3] == HASH_DELETED)
      {
         e[0] = mpq_hash_string(name, MPQ_HASH_A);
         e[1] = mpq_hash_string(name, MPQ_HASH_B);
         e[2] = 0;
         e[3] = block;
         return 0;
      }
   }
   return -1;
}

/* Read and check the base. Returns NULL with the open file in *fp, or the
   reason it can't be appended to. */
static const char * load_base(const char * path, FILE ** fp, BASE_S * base)
{
   unsigned char head[32];
   unsigned long long size, hash_bytes, block_bytes;
   long end;
   FILE * f = fopen(path, "rb");

   *fp = NULL;
   memset(base, 0, sizeof(*base));
   if (f == NULL)
      return "The base archive can't be opened.";
   if (fread(head, 1, sizeof(head), f) != sizeof(head) ||
       memcmp(head, "MPQ\x1a", 4) != 0)
   {
      fclose(f);
      return "The base archive isn't an MPQ with its header at the start.";
   }
   if (fseek(f, 0, SEEK_END) != 0 || (end = ftell(f)) < 0)
   {
      fclose(f);
      return "The base archive's size can't be read.";
   }
   size = (unsigned long long) end;
   base->hash_pos = get_le(head + 16);
   base->block_pos = get_le(head + 20);
   base->hash_count = get_le(head + 24);
   base->block_count = get_le(head + 28);
   hash_bytes = (unsigned long long) base->hash_count * 16;
   block_bytes = (unsigned long long) base->block_count * 16;
   if (get_le(head + 4) != 32 || get_le(head + 12) != (3u << 16))
   {
      fclose(f);
      return "The base archive isn't format 0 with 4096-byte sectors.";
   }
   if (!is_pow2(base->hash_count) || base->hash_pos < 32 ||
       base->hash_pos + hash_bytes != base->block_pos ||
       base->block_pos + block_bytes != size || get_le(head + 8) != size)
   {
      fclose(f);
      return "The base archive's tables aren't at its end.";
   }
   base->hash = (unsigned int *) malloc((size_t) hash_bytes);
   base->block = (unsigned int *) malloc((size_t) block_bytes + 16);
   if (base->hash == NULL || base->block == NULL ||
       fseek(f, (long) base->hash_pos, SEEK_SET) != 0 ||
       !read_dwords(f, base->hash, base->hash_count * 4) ||
       fseek(f, (long) base->block_pos, SEEK_SET) != 0 ||
       !read_dwords(f, base->block, base->block_count * 4))
   {
      free(base->hash);
      free(base->block);
      memset(base, 0, sizeof(*base));
      fclose(f);
      return "The base archive's tables can't be read.";
   }
   mpq_decrypt(base->hash, (int) (base->hash_count * 4),
               mpq_hash_string("(hash table)", MPQ_HASH_KEY));
   mpq_decrypt(base->block, (int) (base->block_count * 4),
               mpq_hash_string("(block table)", MPQ_HASH_KEY));
   *fp = f;
   return NULL;
}

/* Put the finished staging file in place of out_path. The old file is kept
   aside until the new one is in, and put back if that fails. */
static int install(const char * staging, const char * out_path)
{
   char * old = (char *) malloc(strlen(out_path) + 5);
   FILE * probe;
   int had, ok;

   if (old == NULL)
      return 0;
   sprintf(old, "%s.old", out_path);
   remove(old);
   probe = fopen(out_path, "rb");
   had = probe != NULL;
   if (probe != NULL)
      fclose(probe);
   if (had && rename(out_path, old) != 0)
   {
      free(old);
      return 0;
   }
   ok = rename(staging, out_path) == 0;
   if (had)
   {
      if (ok)
         remove(old);
      else
         rename(old, out_path);
   }
   free(old);
   return ok;
}

int mpq_write(const char * base_path, const char * out_path,
              const MPQ_WRITE_FILE_S * files, int count, char * error, int cap)
{
   BASE_S base;
   FILE * bf = NULL, * out = NULL;
   unsigned int * hash = NULL, * block = NULL;
   unsigned int hash_count, block_count;
   unsigned long long pos;
   unsigned char head[32];
   char * staging = NULL;
   const char * why;
   int k, closed, result = -1;

   memset(&base, 0, sizeof(base));
   if (out_path == NULL || count < 0 || (count > 0 && files == NULL))
      return fail(error, cap, "Nothing to write.");
   for (k = 0; k < count; k++)
   {
      const char * name = files[k].archive_name;

      if (name == NULL || name[0] == '\0' || files[k].source_path == NULL)
         return fail(error, cap, "A file in the archive has no name.");
      if (strchr(name, '/') != NULL)
         return fail(error, cap, "Archive names use backslashes, not slashes.");
      if (same_name_ignoring_case(name, "(attributes)"))
         return fail(error, cap, "The archive can't hold an (attributes) file.");
   }

   if (base_path != NULL)
   {
      unsigned int * grown;

      why = load_base(base_path, &bf, &base);
      if (why != NULL)
         return fail(error, cap, why);
      hash = base.hash;
      block = base.block;
      hash_count = base.hash_count;
      block_count = base.block_count;
      pos = base.hash_pos;
      grown = (unsigned int *) realloc(block, ((size_t) block_count + (size_t) count + 1) * 16);
      if (grown == NULL)
      {
         result = fail(error, cap, "Out of memory.");
         goto done;
      }
      block = grown;
   }
   else
   {
      hash_count = 16;
      while (hash_count < 2u * ((unsigned int) count + 1))
         hash_count *= 2;
      hash = (unsigned int *) malloc((size_t) hash_count * 16);
      block = (unsigned int *) malloc(((size_t) count + 1) * 16);
      if (hash == NULL || block == NULL)
      {
         result = fail(error, cap, "Out of memory.");
         goto done;
      }
      memset(hash, 0xFF, (size_t) hash_count * 16);
      block_count = 0;
      pos = 32;
   }

   staging = (char *) malloc(strlen(out_path) + 9);
   if (staging == NULL)
   {
      result = fail(error, cap, "Out of memory.");
      goto done;
   }
   sprintf(staging, "%s.staging", out_path);
   out = fopen(staging, "wb");
   if (out == NULL)
   {
      result = fail(error, cap, "The archive can't be created next to its target.");
      goto done;
   }

   if (bf != NULL)
   {
      if (fseek(bf, 0, SEEK_SET) != 0 || !copy_bytes(bf, out, base.hash_pos))
      {
         result = fail(error, cap, "The base archive's files can't be copied.");
         goto done;
      }
      delete_slot(hash, hash_count, "(attributes)");
   }
   else
   {
      memset(head, 0, sizeof(head));
      if (fwrite(head, 1, sizeof(head), out) != sizeof(head))
      {
         result = fail(error, cap, "The archive can't be written.");
         goto done;
      }
   }

   for (k = 0; k < count; k++)
   {
      FILE * in = fopen(files[k].source_path, "rb");
      unsigned char buf[16384];
      unsigned long long size = 0, tables;
      size_t got;

      if (in == NULL)
      {
         char msg[600];

         sprintf(msg, "Can't read %.500s to add it to the archive.", files[k].source_path);
         result = fail(error, cap, msg);
         goto done;
      }
      tables = ((unsigned long long) hash_count + block_count + (unsigned long long) count + 1) * 16;
      while ((got = fread(buf, 1, sizeof(buf), in)) > 0)
      {
         size += got;
         if (pos + size + tables > ARCHIVE_MAX)
         {
            fclose(in);
            result = fail(error, cap, "The archive would be larger than 4 GB.");
            goto done;
         }
         if (fwrite(buf, 1, got, out) != got)
         {
            fclose(in);
            result = fail(error, cap, "The archive can't be written (is the disk full?).");
            goto done;
         }
      }
      closed = ferror(in);
      fclose(in);
      if (closed)
      {
         result = fail(error, cap, "A file can't be read all the way through.");
         goto done;
      }
      block[block_count * 4] = (unsigned int) pos;
      block[block_count * 4 + 1] = (unsigned int) size;
      block[block_count * 4 + 2] = (unsigned int) size;
      block[block_count * 4 + 3] = FLAG_EXISTS;
      pos += size;
      if (set_slot(hash, hash_count, files[k].archive_name, block_count) != 0)
      {
         result = fail(error, cap, "The archive's hash table is full.");
         goto done;
      }
      block_count++;
   }

   {
      unsigned int hash_pos = (unsigned int) pos;
      unsigned int block_pos = hash_pos + hash_count * 16;
      unsigned int total = block_pos + block_count * 16;

      mpq_encrypt(hash, (int) (hash_count * 4), mpq_hash_string("(hash table)", MPQ_HASH_KEY));
      mpq_encrypt(block, (int) (block_count * 4), mpq_hash_string("(block table)", MPQ_HASH_KEY));
      if (!write_dwords(out, hash, hash_count * 4) ||
          !write_dwords(out, block, block_count * 4))
      {
         result = fail(error, cap, "The archive can't be written (is the disk full?).");
         goto done;
      }
      memcpy(head, "MPQ\x1a", 4);
      put_le(head + 4, 32);
      put_le(head + 8, total);
      put_le(head + 12, 3u << 16);   /* format 0, sector shift 3 */
      put_le(head + 16, hash_pos);
      put_le(head + 20, block_pos);
      put_le(head + 24, hash_count);
      put_le(head + 28, block_count);
      if (fseek(out, 0, SEEK_SET) != 0 || fwrite(head, 1, sizeof(head), out) != sizeof(head))
      {
         result = fail(error, cap, "The archive's header can't be written.");
         goto done;
      }
   }
   closed = fclose(out);
   out = NULL;
   if (closed != 0)
   {
      result = fail(error, cap, "The archive can't be written (is the disk full?).");
      goto done;
   }
   if (bf != NULL)
   {
      fclose(bf);
      bf = NULL;
   }
   if (!install(staging, out_path))
   {
      result = fail(error, cap, "The finished archive can't be moved into place.");
      goto done;
   }
   result = 0;

done:
   if (out != NULL)
      fclose(out);
   if (bf != NULL)
      fclose(bf);
   if (result != 0 && staging != NULL)
      remove(staging);
   free(staging);
   free(hash);
   free(block);
   return result;
}
