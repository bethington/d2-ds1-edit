/* file_copy_checked: map deletion removes the original once this reports
 * success, so a copy that didn't happen must never report it. Failed writes
 * are exercised through a destination that can't be opened; a disk filling up
 * mid-copy isn't reachable from a portable test.
 */

#include <stdio.h>
#include <string.h>
#include "unity/unity.h"

#include "core/file_copy.h"

#define SRC_PATH  "test_file_copy_src.bin"
#define DST_PATH  "test_file_copy_dst.bin"
#define BAD_DST   "test_file_copy_no_such_dir/dst.bin"

static void write_src(const char * data, size_t size)
{
   FILE * f = fopen(SRC_PATH, "wb");
   TEST_ASSERT_NOT_NULL(f);
   TEST_ASSERT_EQUAL_size_t(size, fwrite(data, 1, size, f));
   fclose(f);
}

void setUp(void)
{
   remove(SRC_PATH);
   remove(DST_PATH);
}

void tearDown(void)
{
   remove(SRC_PATH);
   remove(DST_PATH);
}

void test_copy_reproduces_the_source_bytes_across_buffer_boundaries(void)
{
   static char data[20000];
   static char back[sizeof(data) + 1];
   size_t i;
   FILE * f;

   for (i = 0; i < sizeof(data); i++)
      data[i] = (char)(i * 31);
   write_src(data, sizeof(data));

   TEST_ASSERT_EQUAL_INT(FILE_COPY_OK, file_copy_checked(SRC_PATH, DST_PATH));

   f = fopen(DST_PATH, "rb");
   TEST_ASSERT_NOT_NULL(f);
   TEST_ASSERT_EQUAL_size_t(sizeof(data), fread(back, 1, sizeof(back), f));
   fclose(f);
   TEST_ASSERT_EQUAL_MEMORY(data, back, sizeof(data));
}

void test_missing_source_is_reported_apart_from_a_failed_copy(void)
{
   /* A map that lives only in an MPQ has no file to back up, which isn't an
      error; the caller has to be able to tell that from a lost copy. */
   TEST_ASSERT_EQUAL_INT(FILE_COPY_NO_SOURCE,
                         file_copy_checked(SRC_PATH, DST_PATH));
}

void test_unwritable_destination_fails_and_leaves_the_source_alone(void)
{
   FILE * f;

   write_src("map", 3);

   TEST_ASSERT_EQUAL_INT(FILE_COPY_FAILED, file_copy_checked(SRC_PATH, BAD_DST));

   f = fopen(SRC_PATH, "rb");
   TEST_ASSERT_NOT_NULL(f);
   fclose(f);
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_copy_reproduces_the_source_bytes_across_buffer_boundaries);
   RUN_TEST(test_missing_source_is_reported_apart_from_a_failed_copy);
   RUN_TEST(test_unwritable_destination_fails_and_leaves_the_source_alone);
   return UNITY_END();
}
