/* txt_read_in_mem() appends a terminator with realloc(). realloc() may move
 * the block, so the function must return the block it got back. This test
 * makes every realloc() move; the old block stays allocated, filled with a
 * marker, so a stale pointer fails an assertion instead of being undefined. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity/unity.h"

static void *s_moved_from;
static void *s_moved_to;

static void *moving_realloc(void *ptr, size_t size)
{
   void *moved = malloc(size);

   if ((moved != NULL) && (ptr != NULL))
      memcpy(moved, ptr, size - 1);
   if (ptr != NULL)
      memset(ptr, 0xDD, size - 1);
   s_moved_from = ptr;
   s_moved_to   = moved;
   return moved;
}

#define realloc moving_realloc
#include "core/txtread.c"
#undef realloc

#include "visual_test_support.h"

static const char s_txt[] = "Name\tId\nact1\t1\n";

/* The rest of txtread.c is not exercised; these only satisfy the linker. */
DS1_S *glb_ds1;
char   glb_ds1edit_data_dir[80];
char   glb_tiles_path[30];
char **glb_txt_req_ptr[RQ_MAX];
char  *glb_path_lvltypes_mem, *glb_path_lvltypes_def;
char  *glb_path_lvlprest_mem, *glb_path_lvlprest_def;
char  *glb_path_obj_mem, *glb_path_obj_def;
char  *glb_path_objects_mem, *glb_path_objects_def;
int misc_get_txt_column_num(RQ_ENUM txt_idx, char *col_name)
{
   (void) txt_idx;
   (void) col_name;
   return 0;
}

int dt1_add(char *dt1name)
{
   (void) dt1name;
   return 0;
}

int dt1_add_special(char *dt1name)
{
   (void) dt1name;
   return 0;
}
void setUp(void) {}
void tearDown(void) {}

void test_returns_the_block_realloc_moved_to(void)
{
   char *text;

   TEST_ASSERT_TRUE(visual_fetch_add("data/global/excel/test.txt", s_txt, strlen(s_txt)));
   text = (char *) txt_read_in_mem("data/global/excel/test.txt");

   TEST_ASSERT_NOT_NULL(s_moved_from);
   TEST_ASSERT_EQUAL_PTR(s_moved_to, text);
   TEST_ASSERT_EQUAL_STRING(s_txt, text);
   free(s_moved_from);
   free(text);
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_returns_the_block_realloc_moved_to);
   return UNITY_END();
}
