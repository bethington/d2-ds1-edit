/* The --selftest pass/fail rule for frame timing.
 *
 * The rule exists to back up the MEMORY-bitmap draw guard (that regression
 * shipped twice, both times at under 1 FPS). It compared the mean wall time
 * of a frame with 250 ms, and on a host busy with parallel test lanes frames
 * waited seconds for a core: about one healthy windowed run in 150 exited 1.
 * The cases below are the measured shapes from the repro loop.
 */

#include <string.h>
#include "unity/unity.h"
#include "ui/selftest_verdict.h"

void setUp(void) {}
void tearDown(void) {}

/* act1_town 1:8 on a loaded host: wall-time mean 400+ ms (median 336 ms,
   stalls to 2 s), but only ~50 ms of the editor's own CPU per frame. */
static void test_loaded_host_passes(void)
{
   TEST_ASSERT_NULL(selftest_render_verdict(60, 60 * 50.0, 0));
}

/* The editor itself spending over 250 ms of CPU per frame is a collapse. */
static void test_cpu_bound_collapse_fails(void)
{
   const char *why = selftest_render_verdict(60, 60 * 400.0, 0);
   TEST_ASSERT_NOT_NULL(why);
   TEST_ASSERT_NOT_NULL(strstr(why, "MEMORY bitmap"));
}

static void test_limit_is_strict(void)
{
   TEST_ASSERT_NULL(selftest_render_verdict(4, 4 * SELFTEST_RENDER_LIMIT_MS, 0));
   TEST_ASSERT_NOT_NULL(selftest_render_verdict(4, 4 * SELFTEST_RENDER_LIMIT_MS + 1.0, 0));
}

static void test_guard_hit_fails_whatever_the_timing(void)
{
   const char *why = selftest_render_verdict(60, 60 * 1.0, 1);
   TEST_ASSERT_NOT_NULL(why);
   TEST_ASSERT_NOT_NULL(strstr(why, "MEMORY bitmap was drawn"));
}

static void test_no_frames_is_not_a_timing_failure(void)
{
   TEST_ASSERT_NULL(selftest_render_verdict(0, 0.0, 0));
}

/* The clock the rule reads must count this thread's work. */
static void test_thread_cpu_clock_advances(void)
{
   volatile double x = 0.0;
   double t0 = selftest_thread_cpu_ms(), t1;
   long i;

   for (i = 0; i < 200000000L && (selftest_thread_cpu_ms() - t0) < 50.0; i++)
      x += (double)i * 0.5;
   t1 = selftest_thread_cpu_ms();
   TEST_ASSERT_TRUE(t1 - t0 >= 30.0);
   TEST_ASSERT_TRUE(t1 - t0 < 10000.0);
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_loaded_host_passes);
   RUN_TEST(test_cpu_bound_collapse_fails);
   RUN_TEST(test_limit_is_strict);
   RUN_TEST(test_guard_hit_fails_whatever_the_timing);
   RUN_TEST(test_no_frames_is_not_a_timing_failure);
   RUN_TEST(test_thread_cpu_clock_advances);
   return UNITY_END();
}
