/* Allegro's GTK dialogs wrap the editor's X11 window, and segfault inside
 * gdk_x11_window_foreign_new_for_display when GTK picked its Wayland backend.
 * Startup therefore pins GTK to X11 wherever Allegro's dialogs use GTK. */
#include <stdlib.h>
#include <string.h>

#include "unity/unity.h"
#include "platform.h"

void setUp(void) {}
void tearDown(void) {}

#if defined(WIN32) || defined(__APPLE__)

static void test_dialog_setup_leaves_the_environment_alone(void)
{
   const char *before = getenv("GDK_BACKEND");
   ds1_prepare_native_dialogs();
   TEST_ASSERT_EQUAL_STRING(before, getenv("GDK_BACKEND"));
}

#else

static void test_dialog_setup_selects_x11_when_unset(void)
{
   unsetenv("GDK_BACKEND");
   ds1_prepare_native_dialogs();
   TEST_ASSERT_EQUAL_STRING("x11", getenv("GDK_BACKEND"));
}

static void test_dialog_setup_replaces_a_wayland_choice(void)
{
   setenv("GDK_BACKEND", "wayland", 1);
   ds1_prepare_native_dialogs();
   TEST_ASSERT_EQUAL_STRING("x11", getenv("GDK_BACKEND"));
}

#endif

int main(void)
{
   UNITY_BEGIN();
#if defined(WIN32) || defined(__APPLE__)
   RUN_TEST(test_dialog_setup_leaves_the_environment_alone);
#else
   RUN_TEST(test_dialog_setup_selects_x11_when_unset);
   RUN_TEST(test_dialog_setup_replaces_a_wayland_choice);
#endif
   return UNITY_END();
}
