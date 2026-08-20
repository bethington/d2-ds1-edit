/* Smoke test for the CLI dispatcher + help verb. Shells out to
 * ds1edit.exe and validates exit codes + output for the verbs that
 * don't require an MPQ chain. The export verbs themselves need a real
 * D2 install and are validated manually (see CLI Phase 5a / 5b commit
 * messages for the canonical smoke commands). */

#include <stdio.h>
#include "platform.h"
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#ifndef DS1EDIT_BIN_EXE
#define DS1EDIT_BIN_EXE "bin\\ds1edit.exe"
#endif

void setUp(void) {}
void tearDown(void) {}

/* Run the given command via popen, capture up to cap-1 bytes of stdout
 * + stderr into out_buf, and return the process exit code. */
static int run_capture(const char *cmd, char *out_buf, int cap)
{
   FILE *fp;
   int n = 0;
   int rc;
   if (out_buf == NULL || cap <= 0) return -1;
   out_buf[0] = 0;

   fp = DS1_POPEN(cmd, "r");
   if (fp == NULL) return -1;

   while (n < cap - 1)
   {
      int got = (int) fread(out_buf + n, 1, (size_t) (cap - 1 - n), fp);
      if (got <= 0) break;
      n += got;
   }
   out_buf[n] = 0;

   rc = DS1_PCLOSE(fp);
   return rc;
}

static void test_help_exits_zero_and_lists_verbs(void)
{
   char out[4096];
   int rc = run_capture(DS1EDIT_BIN_EXE " help 2>&1", out, sizeof(out));
   TEST_ASSERT_EQUAL_INT(0, rc);
   /* Help text mentions key verbs. */
   TEST_ASSERT_NOT_NULL(strstr(out, "list-mpqs"));
   TEST_ASSERT_NOT_NULL(strstr(out, "probe"));
   TEST_ASSERT_NOT_NULL(strstr(out, "export-compose"));
   /* And the common-flags block. */
   TEST_ASSERT_NOT_NULL(strstr(out, "--d2-install="));
}

static void test_double_dash_help_alias(void)
{
   char out[4096];
   int rc = run_capture(DS1EDIT_BIN_EXE " --help 2>&1", out, sizeof(out));
   TEST_ASSERT_EQUAL_INT(0, rc);
   TEST_ASSERT_NOT_NULL(strstr(out, "Usage:"));
}

static void test_unknown_verb_fails_with_exit_code_3(void)
{
   char out[4096];
   int rc = run_capture(DS1EDIT_BIN_EXE
      " definitely-not-a-real-verb 2>&1", out, sizeof(out));
   TEST_ASSERT_EQUAL_INT(3, rc);
   TEST_ASSERT_NOT_NULL(strstr(out, "unknown verb"));
}

static void test_probe_without_path_arg_returns_3(void)
{
   char out[4096];
   /* probe needs a positional argument; missing it is a usage error. */
   int rc = run_capture(DS1EDIT_BIN_EXE " probe --no-ini 2>&1",
                        out, sizeof(out));
   TEST_ASSERT_EQUAL_INT(3, rc);
   TEST_ASSERT_NOT_NULL(strstr(out, "missing path"));
}

static void test_export_compose_without_target_or_out_returns_3(void)
{
   char out[4096];
   /* No --target / --token / --out -- usage error. We pass --no-ini
    * so the test doesn't depend on a working MPQ chain. */
   int rc = run_capture(DS1EDIT_BIN_EXE
      " export-compose --no-ini 2>&1", out, sizeof(out));
   /* Either CLI_EXIT_BAD_ARGS (3) for missing args, or NOTHING (2) if
    * the run gets past arg parsing but produces zero output -- both
    * are non-zero "not OK" outcomes which is what we're asserting. */
   {
      char msg[128];
      snprintf(msg, sizeof(msg), "expected exit 2 or 3, got %d", rc);
      TEST_ASSERT_NOT_EQUAL_MESSAGE(0, rc, msg);
      TEST_ASSERT_TRUE_MESSAGE(rc == 2 || rc == 3, msg);
   }
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_help_exits_zero_and_lists_verbs);
   RUN_TEST(test_double_dash_help_alias);
   RUN_TEST(test_unknown_verb_fails_with_exit_code_3);
   RUN_TEST(test_probe_without_path_arg_returns_3);
   RUN_TEST(test_export_compose_without_target_or_out_returns_3);
   return UNITY_END();
}
