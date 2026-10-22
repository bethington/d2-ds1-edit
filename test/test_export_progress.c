/* Export progress singleton (src/core/export_progress.c): lifecycle,
 * cancellation, cleanup that lets the next export start clean, and the
 * percent model. The dialog is never drawn here (no font, no screen). */

#include <string.h>

#include "unity/unity.h"
#include "structs.h"
#include "core/export_progress.h"
#include "visual_test_support.h"

static int s_have_mouse;

void setUp(void)
{
   static int once;
   if (!once)
   {
      once = 1;
      if (al_init())
         s_have_mouse = al_install_mouse();
   }
   export_progress_end();
}

void tearDown(void) { export_progress_end(); }

void test_begin_and_end_bracket_one_active_task(void)
{
   const EXPORT_PROGRESS_S *st = export_progress_state();

   TEST_ASSERT_FALSE(export_task_is_active());
   export_progress_begin("Export Assets");
   TEST_ASSERT_TRUE(export_task_is_active());
   TEST_ASSERT_EQUAL_STRING("Export Assets", st->title);
   TEST_ASSERT_EQUAL_INT(EXPORT_STAGE_NONE, st->stage);
   TEST_ASSERT_EQUAL_INT(EXPORT_RESULT_PENDING, st->result);
   export_progress_end();
   TEST_ASSERT_FALSE(export_task_is_active());
}

void test_cancel_between_items_then_cleanup_allows_the_next_export(void)
{
   const EXPORT_PROGRESS_S *st = export_progress_state();

   export_progress_begin("first");
   export_progress_set_stage(EXPORT_STAGE_NATIVE_EXPORT, "Exporting", 3);
   export_progress_set_current_item("Data\\a.dc6");
   export_progress_advance(1);
   export_progress_request_cancel();
   TEST_ASSERT_TRUE(export_progress_cancel_requested());
   TEST_ASSERT_EQUAL_INT(1, st->items_done);
   export_progress_end();

   /* nothing of the canceled task survives */
   TEST_ASSERT_FALSE(export_task_is_active());
   TEST_ASSERT_FALSE(export_progress_cancel_requested());

   export_progress_begin("second");
   TEST_ASSERT_TRUE(export_task_is_active());
   TEST_ASSERT_FALSE(export_progress_cancel_requested());
   TEST_ASSERT_EQUAL_INT(0, st->items_done);
   TEST_ASSERT_EQUAL_INT(0, st->items_total);
   TEST_ASSERT_EQUAL_STRING("", st->current_item);
   TEST_ASSERT_EQUAL_STRING("", st->stage_label);
   TEST_ASSERT_EQUAL_STRING("second", st->title);
}

void test_updates_without_an_active_task_change_nothing(void)
{
   const EXPORT_PROGRESS_S *st = export_progress_state();

   export_progress_set_stage(EXPORT_STAGE_NATIVE_EXPORT, "x", 9);
   export_progress_advance(3);
   export_progress_set_current_item("y");
   export_progress_request_cancel();
   TEST_ASSERT_FALSE(export_task_is_active());
   TEST_ASSERT_FALSE(export_progress_cancel_requested());
   TEST_ASSERT_EQUAL_INT(0, st->items_total);
   TEST_ASSERT_EQUAL_INT(0, st->items_done);
   TEST_ASSERT_EQUAL_INT(0, export_progress_pump());
}

void test_advance_is_monotonic_and_clamped(void)
{
   const EXPORT_PROGRESS_S *st = export_progress_state();

   export_progress_begin("t");
   export_progress_set_stage(EXPORT_STAGE_NATIVE_EXPORT, "n", 4);
   export_progress_advance(0);
   export_progress_advance(-2);
   TEST_ASSERT_EQUAL_INT(0, st->items_done);
   export_progress_advance(3);
   export_progress_advance(3);
   TEST_ASSERT_EQUAL_INT(4, st->items_done);
   /* a new stage restarts its own count */
   export_progress_set_stage(EXPORT_STAGE_LOCAL_UPSCALE, "u", 2);
   TEST_ASSERT_EQUAL_INT(0, st->items_done);
}

void test_percent_model_local_and_remote(void)
{
   export_progress_begin("p");
   TEST_ASSERT_EQUAL_INT(0, export_progress_percent(0));
   export_progress_set_stage(EXPORT_STAGE_PREPARE, "prep", 0);
   TEST_ASSERT_EQUAL_INT(0, export_progress_percent(0));
   export_progress_set_stage(EXPORT_STAGE_NATIVE_EXPORT, "n", 4);
   export_progress_advance(2);
   /* local weights 5 + 35 + 35 = 75: (5 + 17) * 100 / 75 = 29 */
   TEST_ASSERT_EQUAL_INT(29, export_progress_percent(0));
   export_progress_advance(2);
   TEST_ASSERT_EQUAL_INT(53, export_progress_percent(0));
   /* remote weights 5+35+10+25+15+10 = 100: 5 + 35 */
   TEST_ASSERT_EQUAL_INT(40, export_progress_percent(1));
   export_progress_set_stage(EXPORT_STAGE_LOCAL_UPSCALE, "u", 1);
   export_progress_advance(1);
   TEST_ASSERT_EQUAL_INT(100, export_progress_percent(0));
   export_progress_set_stage(EXPORT_STAGE_EXTRACT, "x", 1);
   export_progress_advance(1);
   TEST_ASSERT_EQUAL_INT(100, export_progress_percent(1));
}

void test_long_labels_are_truncated_and_terminated(void)
{
   char long_text[600];
   const EXPORT_PROGRESS_S *st = export_progress_state();

   memset(long_text, 'a', sizeof(long_text) - 1);
   long_text[sizeof(long_text) - 1] = 0;
   export_progress_begin(long_text);
   export_progress_set_stage(EXPORT_STAGE_PREPARE, long_text, 1);
   export_progress_set_current_item(long_text);
   TEST_ASSERT_EQUAL_size_t(EXPORT_PROGRESS_TITLE_MAX - 1, strlen(st->title));
   TEST_ASSERT_EQUAL_size_t(EXPORT_PROGRESS_STAGE_LABEL_MAX - 1, strlen(st->stage_label));
   TEST_ASSERT_EQUAL_size_t(EXPORT_PROGRESS_ITEM_MAX - 1, strlen(st->current_item));
}

void test_pump_reports_cancellation(void)
{
   if (!s_have_mouse)
      TEST_IGNORE_MESSAGE("no mouse driver here; the pump reads mouse state");
   export_progress_begin("pump");
   TEST_ASSERT_EQUAL_INT(0, export_progress_pump());
   export_progress_request_cancel();
   TEST_ASSERT_EQUAL_INT(1, export_progress_pump());
   export_progress_end();
   TEST_ASSERT_EQUAL_INT(0, export_progress_pump());
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_begin_and_end_bracket_one_active_task);
   RUN_TEST(test_cancel_between_items_then_cleanup_allows_the_next_export);
   RUN_TEST(test_updates_without_an_active_task_change_nothing);
   RUN_TEST(test_advance_is_monotonic_and_clamped);
   RUN_TEST(test_percent_model_local_and_remote);
   RUN_TEST(test_long_labels_are_truncated_and_terminated);
   RUN_TEST(test_pump_reports_cancellation);
   return UNITY_END();
}
