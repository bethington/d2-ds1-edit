/* mpq_write: the archives it writes must load through the editor's own reader,
 * carry only what the game accepts, and leave the target alone on failure.
 * Nothing here touches a game install. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity/unity.h"
#include "structs.h"
#include "error.h"
#include "mpq/MpqView.h"
#include "mpq/mpqtypes.h"
#include "core/mpq_crypt.h"
#include "core/mpq_write.h"
#include "support/test_tmp.h"

CONFIG_S glb_config;

static GLB_MPQ_S mpq_struct;
static char dir[512];
static char err[512];

void ds1edit_error(const char * text)
{
   TEST_FAIL_MESSAGE(text);
}

void setUp(void)
{
   size_t n;

   TEST_ASSERT_TRUE(tt_root_create("mpq_write", dir, sizeof(dir)));
   n = strlen(dir);
   if (n > 0 && (dir[n - 1] == '/' || dir[n - 1] == '\\'))
      dir[n - 1] = 0;
   memset(&glb_config, 0, sizeof(glb_config));
   err[0] = 0;
}

void tearDown(void)
{
   tt_remove_tree(dir);
}

static void path_of(char * out, const char * rel)
{
   snprintf(out, 768, "%s/%s", dir, rel);
}

/* A source file of n bytes, each a function of its seed and position. */
static void make_source(const char * rel, int n, int seed)
{
   char path[768];
   unsigned char * b = (unsigned char *) malloc((size_t) n + 1);
   int i;

   TEST_ASSERT_NOT_NULL(b);
   for (i = 0; i < n; i++)
      b[i] = (unsigned char) (i * 7 + seed);
   path_of(path, rel);
   TEST_ASSERT_TRUE(tt_write_file(path, b, (size_t) n));
   free(b);
}

/* Look name up in the archive through the editor's reader. Returns its entry
   number, or -1 when absent; *buf is malloc'd when found. */
static int read_back(const char * rel, const char * name, unsigned char ** buf, long * len)
{
   char path[768];
   int entry;

   path_of(path, rel);
   memset(&mpq_struct, 0, sizeof(mpq_struct));
   glb_mpq = &mpq_struct;
   mpq_batch_open(path);
   *buf = NULL;
   *len = 0;
   entry = mpq_batch_load_in_mem((char *) name, (void **) buf, len, 0);
   mpq_batch_close();
   return entry;
}

static void expect_file(const char * rel, const char * name, const char * source_rel)
{
   char src[768];
   unsigned char * want, * got;
   size_t want_len;
   long got_len;

   path_of(src, source_rel);
   want = tt_read_file(src, &want_len);
   TEST_ASSERT_NOT_NULL(want);
   TEST_ASSERT_TRUE(read_back(rel, name, &got, &got_len) != -1);
   TEST_ASSERT_EQUAL_INT((int) want_len, (int) got_len);
   TEST_ASSERT_EQUAL_MEMORY(want, got, want_len);
   free(want);
   free(got);
}

static void expect_absent(const char * rel, const char * name)
{
   unsigned char * got;
   long got_len;

   TEST_ASSERT_EQUAL_INT(-1, read_back(rel, name, &got, &got_len));
}

static unsigned int le(const unsigned char * p)
{
   return (unsigned int) p[0] | ((unsigned int) p[1] << 8) |
          ((unsigned int) p[2] << 16) | ((unsigned int) p[3] << 24);
}

static void put_le(unsigned char * p, unsigned int v)
{
   p[0] = (unsigned char) v;
   p[1] = (unsigned char) (v >> 8);
   p[2] = (unsigned char) (v >> 16);
   p[3] = (unsigned char) (v >> 24);
}

static int write_one(const char * base_rel, const char * out_rel, const char * name,
                     const char * source_rel)
{
   char base[768], out[768], src[768];
   MPQ_WRITE_FILE_S f;

   path_of(out, out_rel);
   path_of(src, source_rel);
   if (base_rel != NULL)
      path_of(base, base_rel);
   f.archive_name = name;
   f.source_path = src;
   return mpq_write(base_rel != NULL ? base : NULL, out, &f, 1, err, sizeof(err));
}

/* A base archive written by hand: one stored byte per name, one hash slot per
   name by linear probing, and a hash table of exactly hash_count slots. */
static void make_base(const char * rel, int hash_count, const char ** names, int n)
{
   char path[768];
   unsigned char * img;
   unsigned int * hash, * block;
   size_t hash_pos, block_pos, total;
   int i;

   hash = (unsigned int *) malloc((size_t) hash_count * 16);
   block = (unsigned int *) malloc((size_t) n * 16);
   TEST_ASSERT_TRUE(hash != NULL && block != NULL);
   memset(hash, 0xFF, (size_t) hash_count * 16);
   hash_pos = 32 + (size_t) n;
   block_pos = hash_pos + (size_t) hash_count * 16;
   total = block_pos + (size_t) n * 16;
   img = (unsigned char *) calloc(total, 1);
   TEST_ASSERT_NOT_NULL(img);
   for (i = 0; i < n; i++)
   {
      unsigned int s = mpq_hash_string(names[i], MPQ_HASH_OFFSET) & (unsigned) (hash_count - 1);

      while (hash[s * 4 + 3] != 0xFFFFFFFFu)
         s = (s + 1) & (unsigned) (hash_count - 1);
      hash[s * 4] = mpq_hash_string(names[i], MPQ_HASH_A);
      hash[s * 4 + 1] = mpq_hash_string(names[i], MPQ_HASH_B);
      hash[s * 4 + 2] = (unsigned int) i;   /* a different locale for each */
      hash[s * 4 + 3] = (unsigned int) i;
      img[32 + i] = (unsigned char) ('0' + i);
      block[i * 4] = (unsigned int) (32 + i);
      block[i * 4 + 1] = 1;
      block[i * 4 + 2] = 1;
      block[i * 4 + 3] = 0x80000000u;
   }
   mpq_encrypt(hash, hash_count * 4, mpq_hash_string("(hash table)", MPQ_HASH_KEY));
   mpq_encrypt(block, n * 4, mpq_hash_string("(block table)", MPQ_HASH_KEY));
   for (i = 0; i < hash_count * 4; i++)
      put_le(img + hash_pos + (size_t) i * 4, hash[i]);
   for (i = 0; i < n * 4; i++)
      put_le(img + block_pos + (size_t) i * 4, block[i]);
   put_le(img, 0x1A51504Du);
   put_le(img + 4, 32);
   put_le(img + 8, (unsigned int) total);
   put_le(img + 12, 0x00030000u);
   put_le(img + 16, (unsigned int) hash_pos);
   put_le(img + 20, (unsigned int) block_pos);
   put_le(img + 24, (unsigned int) hash_count);
   put_le(img + 28, (unsigned int) n);
   path_of(path, rel);
   TEST_ASSERT_TRUE(tt_write_file(path, img, total));
   free(img);
   free(hash);
   free(block);
}

void test_known_keys(void)
{
   TEST_ASSERT_EQUAL_HEX32(0xC3AF3770u, mpq_hash_string("(hash table)", MPQ_HASH_KEY));
   TEST_ASSERT_EQUAL_HEX32(0xEC83B3A3u, mpq_hash_string("(block table)", MPQ_HASH_KEY));
}

void test_encrypt_round_trips(void)
{
   unsigned int orig[8] = { 1, 2, 3, 0xFFFFFFFFu, 0x12345678u, 0, 77, 0x80000000u };
   unsigned int d[8];

   memcpy(d, orig, sizeof(d));
   mpq_encrypt(d, 8, 0xC3AF3770u);
   TEST_ASSERT_TRUE(memcmp(d, orig, sizeof(d)) != 0);
   mpq_decrypt(d, 8, 0xC3AF3770u);
   TEST_ASSERT_EQUAL_MEMORY(orig, d, sizeof(d));
}

void test_new_archive_reads_back(void)
{
   char out[768], a[768], b[768];
   MPQ_WRITE_FILE_S f[2];

   make_source("a.bin", 100, 1);
   make_source("b.bin", 10000, 2);
   path_of(out, "new.mpq");
   path_of(a, "a.bin");
   path_of(b, "b.bin");
   f[0].archive_name = "data\\global\\a.bin";
   f[0].source_path = a;
   f[1].archive_name = "data\\global\\tiles\\b.bin";
   f[1].source_path = b;
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, mpq_write(NULL, out, f, 2, err, sizeof(err)), err);
   expect_file("new.mpq", "data\\global\\a.bin", "a.bin");
   expect_file("new.mpq", "data\\global\\tiles\\b.bin", "b.bin");
   expect_absent("new.mpq", "data\\global\\c.bin");
}

void test_name_case_and_slash(void)
{
   char out[768];

   make_source("a.bin", 50, 3);
   TEST_ASSERT_EQUAL_INT(-1, write_one(NULL, "slash.mpq", "data/global/a.bin", "a.bin"));
   TEST_ASSERT_TRUE(strlen(err) > 0);
   TEST_ASSERT_EQUAL_INT(-1, write_one(NULL, "empty.mpq", "", "a.bin"));
   TEST_ASSERT_EQUAL_INT(-1, write_one(NULL, "attr.mpq", "(Attributes)", "a.bin"));
   path_of(out, "slash.mpq");
   TEST_ASSERT_FALSE(tt_file_exists(out));
   path_of(out, "empty.mpq");
   TEST_ASSERT_FALSE(tt_file_exists(out));
   path_of(out, "attr.mpq");
   TEST_ASSERT_FALSE(tt_file_exists(out));

   TEST_ASSERT_EQUAL_INT_MESSAGE(0, write_one(NULL, "case.mpq", "Data\\Global\\A.BIN", "a.bin"), err);
   expect_file("case.mpq", "data\\global\\a.bin", "a.bin");
}

void test_flags_and_header(void)
{
   char out[768];
   unsigned char * img;
   unsigned int * blocks;
   size_t len, hash_pos, block_pos, hash_count, block_count, i;

   make_source("a.bin", 100, 1);
   make_source("b.bin", 5000, 2);
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, write_one(NULL, "new.mpq", "x\\a.bin", "a.bin"), err);
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, write_one("new.mpq", "two.mpq", "x\\b.bin", "b.bin"), err);
   path_of(out, "two.mpq");
   img = tt_read_file(out, &len);
   TEST_ASSERT_NOT_NULL(img);
   TEST_ASSERT_TRUE(len > 32);
   TEST_ASSERT_EQUAL_HEX8('M', img[0]);
   TEST_ASSERT_EQUAL_HEX8('P', img[1]);
   TEST_ASSERT_EQUAL_HEX8('Q', img[2]);
   TEST_ASSERT_EQUAL_HEX8(0x1A, img[3]);
   TEST_ASSERT_EQUAL_UINT32(32, le(img + 4));
   TEST_ASSERT_EQUAL_UINT32((unsigned int) len, le(img + 8));
   TEST_ASSERT_EQUAL_HEX32(0x00030000u, le(img + 12));   /* format 0, shift 3 */
   hash_pos = le(img + 16);
   block_pos = le(img + 20);
   hash_count = le(img + 24);
   block_count = le(img + 28);
   TEST_ASSERT_TRUE(hash_count >= 16 && (hash_count & (hash_count - 1)) == 0);
   TEST_ASSERT_EQUAL_UINT32(2, (unsigned int) block_count);
   TEST_ASSERT_EQUAL_UINT32((unsigned int) (hash_pos + hash_count * 16), (unsigned int) block_pos);
   TEST_ASSERT_EQUAL_UINT32((unsigned int) len, (unsigned int) (block_pos + block_count * 16));

   blocks = (unsigned int *) malloc(block_count * 16);
   TEST_ASSERT_NOT_NULL(blocks);
   for (i = 0; i < block_count * 4; i++)
      blocks[i] = le(img + block_pos + i * 4);
   mpq_decrypt(blocks, (int) (block_count * 4), mpq_hash_string("(block table)", MPQ_HASH_KEY));
   for (i = 0; i < block_count; i++)
   {
      TEST_ASSERT_EQUAL_HEX32(0x80000000u, blocks[i * 4 + 3]);
      TEST_ASSERT_EQUAL_UINT32(blocks[i * 4 + 1], blocks[i * 4 + 2]);
   }
   free(blocks);
   free(img);
}

void test_append_keeps_base(void)
{
   char base[768], out[768], a[768], b[768], c[768], b2[768];
   MPQ_WRITE_FILE_S f[2];
   unsigned char * before, * after;
   size_t bl, al;

   make_source("a.bin", 300, 1);
   make_source("b.bin", 4100, 2);
   make_source("c.bin", 77, 3);
   make_source("b2.bin", 6000, 4);
   path_of(base, "base.mpq");
   path_of(out, "out.mpq");
   path_of(a, "a.bin");
   path_of(b, "b.bin");
   path_of(c, "c.bin");
   path_of(b2, "b2.bin");
   f[0].archive_name = "n\\a";
   f[0].source_path = a;
   f[1].archive_name = "n\\b";
   f[1].source_path = b;
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, mpq_write(NULL, base, f, 2, err, sizeof(err)), err);

   f[0].archive_name = "n\\c";
   f[0].source_path = c;
   f[1].archive_name = "N\\B";
   f[1].source_path = b2;
   before = tt_read_file(base, &bl);
   TEST_ASSERT_NOT_NULL(before);
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, mpq_write(base, out, f, 2, err, sizeof(err)), err);
   after = tt_read_file(base, &al);
   TEST_ASSERT_NOT_NULL(after);
   TEST_ASSERT_EQUAL_INT((int) bl, (int) al);
   TEST_ASSERT_EQUAL_MEMORY(before, after, bl);
   free(before);
   free(after);

   expect_file("out.mpq", "n\\a", "a.bin");
   expect_file("out.mpq", "n\\c", "c.bin");
   expect_file("out.mpq", "n\\b", "b2.bin");
}

void test_replacing_a_name_leaves_one_slot_at_locale_zero(void)
{
   const char * names[3] = { "n\\a", "n\\a", "n\\b" };   /* "n\\a" in locales 0 and 1 */
   char path[768];
   unsigned char * img, * got;
   unsigned int * hash;
   unsigned int a, b, hash_pos, hash_count, i, live = 0, block = 0, locale = 99;
   size_t len;
   long glen;

   make_base("base.mpq", 16, names, 3);
   make_source("c.bin", 20, 5);
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, write_one("base.mpq", "out.mpq", "n\\a", "c.bin"), err);

   path_of(path, "out.mpq");
   img = tt_read_file(path, &len);
   TEST_ASSERT_NOT_NULL(img);
   hash_pos = le(img + 16);
   hash_count = le(img + 24);
   hash = (unsigned int *) malloc((size_t) hash_count * 16);
   TEST_ASSERT_NOT_NULL(hash);
   for (i = 0; i < hash_count * 4; i++)
      hash[i] = le(img + hash_pos + (size_t) i * 4);
   mpq_decrypt(hash, (int) (hash_count * 4), mpq_hash_string("(hash table)", MPQ_HASH_KEY));
   a = mpq_hash_string("n\\a", MPQ_HASH_A);
   b = mpq_hash_string("n\\a", MPQ_HASH_B);
   for (i = 0; i < hash_count; i++)
      if (hash[i * 4] == a && hash[i * 4 + 1] == b && hash[i * 4 + 3] < 0xFFFFFFFEu)
      {
         live++;
         locale = hash[i * 4 + 2];
         block = hash[i * 4 + 3];
      }
   TEST_ASSERT_EQUAL_UINT32(1, live);
   TEST_ASSERT_EQUAL_UINT32(0, locale);
   TEST_ASSERT_EQUAL_UINT32(3, block);   /* the base had blocks 0 to 2 */
   free(hash);
   free(img);

   TEST_ASSERT_TRUE(read_back("out.mpq", "n\\a", &got, &glen) != -1);
   TEST_ASSERT_EQUAL_INT(20, (int) glen);
   free(got);
   TEST_ASSERT_TRUE(read_back("out.mpq", "n\\b", &got, &glen) != -1);
   TEST_ASSERT_EQUAL_HEX8('2', got[0]);
   free(got);
}

void test_append_drops_attributes(void)
{
   const char * names[3] = { "n\\a", "(attributes)", "n\\b" };
   unsigned char * got;
   long len;

   make_base("base.mpq", 16, names, 3);
   make_source("c.bin", 20, 5);
   /* the base does have one, as far as the reader can tell */
   TEST_ASSERT_TRUE(read_back("base.mpq", "(attributes)", &got, &len) != -1);
   free(got);

   TEST_ASSERT_EQUAL_INT_MESSAGE(0, write_one("base.mpq", "out.mpq", "n\\c", "c.bin"), err);
   expect_absent("out.mpq", "(attributes)");
   TEST_ASSERT_TRUE(read_back("out.mpq", "n\\a", &got, &len) != -1);
   TEST_ASSERT_EQUAL_INT(1, (int) len);
   TEST_ASSERT_EQUAL_HEX8('0', got[0]);
   free(got);
   TEST_ASSERT_TRUE(read_back("out.mpq", "n\\b", &got, &len) != -1);
   TEST_ASSERT_EQUAL_HEX8('2', got[0]);
   free(got);
   TEST_ASSERT_TRUE(read_back("out.mpq", "n\\c", &got, &len) != -1);
   TEST_ASSERT_EQUAL_INT(20, (int) len);
   free(got);
}

static void patch_file(const char * rel, size_t at, unsigned char value)
{
   char path[768];
   unsigned char * img;
   size_t len;

   path_of(path, rel);
   img = tt_read_file(path, &len);
   TEST_ASSERT_NOT_NULL(img);
   img[at] = value;
   TEST_ASSERT_TRUE(tt_write_file(path, img, len));
   free(img);
}

static void expect_refused(const char * base_rel)
{
   char out[768];

   path_of(out, "out.mpq");
   remove(out);
   err[0] = 0;
   TEST_ASSERT_EQUAL_INT(-1, write_one(base_rel, "out.mpq", "n\\c", "c.bin"));
   TEST_ASSERT_TRUE(strlen(err) > 0);
   TEST_ASSERT_FALSE(tt_file_exists(out));
}

void test_append_refuses_unusual_base(void)
{
   const char * names[2] = { "n\\a", "n\\b" };
   char path[768];
   unsigned char * img, * longer;
   size_t len;

   make_source("c.bin", 20, 5);

   make_base("shift.mpq", 16, names, 2);
   patch_file("shift.mpq", 14, 4);
   expect_refused("shift.mpq");

   make_base("magic.mpq", 16, names, 2);
   patch_file("magic.mpq", 0, 'X');
   expect_refused("magic.mpq");

   make_base("format.mpq", 16, names, 2);
   patch_file("format.mpq", 12, 1);
   expect_refused("format.mpq");

   make_base("hsize.mpq", 16, names, 2);
   patch_file("hsize.mpq", 4, 44);
   expect_refused("hsize.mpq");

   make_base("pow2.mpq", 16, names, 2);
   patch_file("pow2.mpq", 24, 12);
   expect_refused("pow2.mpq");

   /* Bytes after the block table: the tables are not at the end. */
   make_base("tail.mpq", 16, names, 2);
   path_of(path, "tail.mpq");
   img = tt_read_file(path, &len);
   TEST_ASSERT_NOT_NULL(img);
   longer = (unsigned char *) calloc(len + 64, 1);
   TEST_ASSERT_NOT_NULL(longer);
   memcpy(longer, img, len);
   TEST_ASSERT_TRUE(tt_write_file(path, longer, len + 64));
   free(img);
   free(longer);
   expect_refused("tail.mpq");

   expect_refused("no_such_base.mpq");
}

void test_full_table_refused(void)
{
   const char * names[3] = { "n\\a", "n\\b", "n\\c" };
   char base[768], out[768], st[768], d[768], e[768], o[768];
   MPQ_WRITE_FILE_S f[2];
   unsigned char * before, * after;
   size_t bl, al;

   make_base("base.mpq", 4, names, 3);
   make_source("d.bin", 10, 1);
   make_source("e.bin", 10, 2);
   make_source("old.bin", 33, 9);
   path_of(base, "base.mpq");
   path_of(out, "out.mpq");
   path_of(d, "d.bin");
   path_of(e, "e.bin");
   path_of(o, "old.bin");
   snprintf(st, sizeof(st), "%s.staging", out);

   /* an existing output to protect */
   f[0].archive_name = "z\\old";
   f[0].source_path = o;
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, mpq_write(NULL, out, f, 1, err, sizeof(err)), err);
   before = tt_read_file(out, &bl);
   TEST_ASSERT_NOT_NULL(before);

   f[0].archive_name = "n\\d";
   f[0].source_path = d;
   f[1].archive_name = "n\\e";
   f[1].source_path = e;
   TEST_ASSERT_EQUAL_INT(-1, mpq_write(base, out, f, 2, err, sizeof(err)));
   TEST_ASSERT_TRUE(strstr(err, "full") != NULL);
   after = tt_read_file(out, &al);
   TEST_ASSERT_NOT_NULL(after);
   TEST_ASSERT_EQUAL_INT((int) bl, (int) al);
   TEST_ASSERT_EQUAL_MEMORY(before, after, bl);
   TEST_ASSERT_FALSE(tt_file_exists(st));
   free(before);
   free(after);
}

void test_failure_keeps_old_output(void)
{
   char out[768], st[768], a[768], missing[768];
   MPQ_WRITE_FILE_S f[2];
   unsigned char * before, * after;
   size_t bl, al;

   make_source("a.bin", 40, 1);
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, write_one(NULL, "out.mpq", "n\\a", "a.bin"), err);
   path_of(out, "out.mpq");
   snprintf(st, sizeof(st), "%s.staging", out);
   before = tt_read_file(out, &bl);
   TEST_ASSERT_NOT_NULL(before);

   path_of(a, "a.bin");
   path_of(missing, "missing.bin");
   f[0].archive_name = "n\\a";
   f[0].source_path = a;
   f[1].archive_name = "n\\m";
   f[1].source_path = missing;
   TEST_ASSERT_EQUAL_INT(-1, mpq_write(NULL, out, f, 2, err, sizeof(err)));
   TEST_ASSERT_TRUE(strlen(err) > 0);
   after = tt_read_file(out, &al);
   TEST_ASSERT_NOT_NULL(after);
   TEST_ASSERT_EQUAL_INT((int) bl, (int) al);
   TEST_ASSERT_EQUAL_MEMORY(before, after, bl);
   TEST_ASSERT_FALSE(tt_file_exists(st));
   free(before);
   free(after);
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_known_keys);
   RUN_TEST(test_encrypt_round_trips);
   RUN_TEST(test_new_archive_reads_back);
   RUN_TEST(test_name_case_and_slash);
   RUN_TEST(test_flags_and_header);
   RUN_TEST(test_append_keeps_base);
   RUN_TEST(test_replacing_a_name_leaves_one_slot_at_locale_zero);
   RUN_TEST(test_append_drops_attributes);
   RUN_TEST(test_append_refuses_unusual_base);
   RUN_TEST(test_full_table_refused);
   RUN_TEST(test_failure_keeps_old_output);
   return UNITY_END();
}
