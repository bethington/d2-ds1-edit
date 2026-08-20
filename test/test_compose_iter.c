#include <string.h>

#include "unity/unity.h"

#include "core/compose_iter.h"
#include "core/compose_palette.h"

/* Production code reads glb_config.compose_use_full_folder_names via
 * compose_iter_use_full_folder_names. We stub that here so the test
 * binary doesn't need to drag in structs.h + Allegro. */
static int g_use_full_folder_names = 0;
int compose_iter_use_full_folder_names(void)
{
   return g_use_full_folder_names;
}

void setUp(void)
{
   g_use_full_folder_names = 0;
}
void tearDown(void) {}

/* ---- Default list shape ------------------------------------------ */

static void test_default_lists_are_nonempty_and_terminated(void)
{
   int n_modes   = compose_iter_default_mode_count();
   int n_weapons = compose_iter_default_weapon_count();
   int n_classes = compose_iter_player_class_count();
   TEST_ASSERT_GREATER_THAN_INT(0, n_modes);
   TEST_ASSERT_GREATER_THAN_INT(0, n_weapons);
   TEST_ASSERT_EQUAL_INT(7, n_classes);

   /* Out-of-range returns NULL. */
   TEST_ASSERT_NULL(compose_iter_default_mode_at(-1));
   TEST_ASSERT_NULL(compose_iter_default_mode_at(n_modes));
   TEST_ASSERT_NULL(compose_iter_default_weapon_at(-1));
   TEST_ASSERT_NULL(compose_iter_default_weapon_at(n_weapons));
   TEST_ASSERT_NULL(compose_iter_player_class_at(-1));
   TEST_ASSERT_NULL(compose_iter_player_class_at(n_classes));
}

static void test_default_modes_include_idle_and_walk(void)
{
   int found_nu = 0, found_wl = 0;
   int n = compose_iter_default_mode_count();
   int i;
   for (i = 0; i < n; i++)
   {
      const char *m = compose_iter_default_mode_at(i);
      TEST_ASSERT_NOT_NULL(m);
      if (strcmp(m, "NU") == 0) found_nu = 1;
      if (strcmp(m, "WL") == 0) found_wl = 1;
   }
   TEST_ASSERT_TRUE(found_nu);
   TEST_ASSERT_TRUE(found_wl);
}

static void test_player_classes_include_necromancer(void)
{
   int found_ne = 0;
   int i;
   for (i = 0; i < compose_iter_player_class_count(); i++)
   {
      const char *c = compose_iter_player_class_at(i);
      TEST_ASSERT_NOT_NULL(c);
      if (strcmp(c, "NE") == 0) found_ne = 1;
   }
   TEST_ASSERT_TRUE(found_ne);
}

/* ---- Per-category metadata --------------------------------------- */

static void test_category_base_paths(void)
{
   TEST_ASSERT_EQUAL_STRING("data\\global\\chars",
      compose_iter_category_base(COMPOSE_CATEGORY_PLAYER_CHAR));
   TEST_ASSERT_EQUAL_STRING("data\\global\\monsters",
      compose_iter_category_base(COMPOSE_CATEGORY_MONSTER));
   /* NPCs share the monsters\ subtree in real D2. */
   TEST_ASSERT_EQUAL_STRING("data\\global\\monsters",
      compose_iter_category_base(COMPOSE_CATEGORY_NPC));
   TEST_ASSERT_EQUAL_STRING("data\\global\\objects",
      compose_iter_category_base(COMPOSE_CATEGORY_OBJECT));
   TEST_ASSERT_NULL(
      compose_iter_category_base(COMPOSE_CATEGORY_NONE));
}

static void test_category_skin(void)
{
   TEST_ASSERT_EQUAL_STRING("LIT",
      compose_iter_category_skin(COMPOSE_CATEGORY_PLAYER_CHAR));
   /* Monsters get per-layer skin from MonStats2, default empty. */
   TEST_ASSERT_EQUAL_STRING("",
      compose_iter_category_skin(COMPOSE_CATEGORY_MONSTER));
   /* Objects use a uniform "lit" everywhere. */
   TEST_ASSERT_EQUAL_STRING("lit",
      compose_iter_category_skin(COMPOSE_CATEGORY_OBJECT));
}

static void test_category_folder(void)
{
   TEST_ASSERT_EQUAL_STRING("Player_Characters",
      compose_iter_category_folder(COMPOSE_CATEGORY_PLAYER_CHAR));
   TEST_ASSERT_EQUAL_STRING("Monsters",
      compose_iter_category_folder(COMPOSE_CATEGORY_MONSTER));
   TEST_ASSERT_EQUAL_STRING("NPCs",
      compose_iter_category_folder(COMPOSE_CATEGORY_NPC));
   TEST_ASSERT_EQUAL_STRING("Objects",
      compose_iter_category_folder(COMPOSE_CATEGORY_OBJECT));
}

/* ---- Output path builder ----------------------------------------- */

static void test_output_path_codes_only(void)
{
   char buf[512];
   /* compose_use_full_folder_names = 0 (default in setUp). */
   int ok = compose_iter_build_output_path(
      buf, (int) sizeof(buf),
      "C:\\out", COMPOSE_CATEGORY_PLAYER_CHAR,
      "NE", "Necromancer",
      "WL", "HTH", 3);
   TEST_ASSERT_EQUAL_INT(1, ok);
   TEST_ASSERT_EQUAL_STRING(
      "C:\\out\\Player_Characters\\NE\\NEWLHTH_dir3.png", buf);
}

static void test_output_path_full_folder_names(void)
{
   char buf[512];
   g_use_full_folder_names = 1;
   int ok = compose_iter_build_output_path(
      buf, (int) sizeof(buf),
      "C:\\out", COMPOSE_CATEGORY_PLAYER_CHAR,
      "NE", "Necromancer",
      "WL", "HTH", 3);
   TEST_ASSERT_EQUAL_INT(1, ok);
   TEST_ASSERT_EQUAL_STRING(
      "C:\\out\\Player_Characters\\Necromancer\\NEWLHTH_dir3.png", buf);
}

static void test_output_path_no_wclass(void)
{
   char buf[512];
   /* Monsters typically have empty weapon class. */
   int ok = compose_iter_build_output_path(
      buf, (int) sizeof(buf),
      "C:\\out", COMPOSE_CATEGORY_MONSTER,
      "AN", "Andariel",
      "NU", "", 0);
   TEST_ASSERT_EQUAL_INT(1, ok);
   TEST_ASSERT_EQUAL_STRING(
      "C:\\out\\Monsters\\AN\\ANNU_dir0.png", buf);
}

static void test_output_path_full_name_with_unsafe_chars(void)
{
   char buf[512];
   /* Spaces and slashes in the full name should be sanitized to
    * underscores by compose_naming_sanitize. */
   g_use_full_folder_names = 1;
   int ok = compose_iter_build_output_path(
      buf, (int) sizeof(buf),
      "C:\\out", COMPOSE_CATEGORY_NPC,
      "Cain", "Deckard Cain",
      "NU", "", 0);
   TEST_ASSERT_EQUAL_INT(1, ok);
   /* "Deckard Cain" -> "Deckard_Cain" */
   TEST_ASSERT_EQUAL_STRING(
      "C:\\out\\NPCs\\Deckard_Cain\\CainNU_dir0.png", buf);
}

static void test_output_path_falls_back_to_code_when_full_missing(void)
{
   char buf[512];
   g_use_full_folder_names = 1;
   int ok = compose_iter_build_output_path(
      buf, (int) sizeof(buf),
      "C:\\out", COMPOSE_CATEGORY_OBJECT,
      "TownPortal", NULL,
      "NU", "", 0);
   TEST_ASSERT_EQUAL_INT(1, ok);
   TEST_ASSERT_EQUAL_STRING(
      "C:\\out\\Objects\\TownPortal\\TownPortalNU_dir0.png", buf);
}

static void test_output_path_rejects_bad_args(void)
{
   char buf[512];
   buf[0] = 'X';
   TEST_ASSERT_EQUAL_INT(0, compose_iter_build_output_path(
      NULL, 0,
      "C:\\out", COMPOSE_CATEGORY_PLAYER_CHAR, "NE", NULL,
      "WL", "HTH", 0));
   /* root NULL */
   TEST_ASSERT_EQUAL_INT(0, compose_iter_build_output_path(
      buf, (int) sizeof(buf),
      NULL, COMPOSE_CATEGORY_PLAYER_CHAR, "NE", NULL,
      "WL", "HTH", 0));
   TEST_ASSERT_EQUAL_INT(0, buf[0]);
   /* token NULL */
   TEST_ASSERT_EQUAL_INT(0, compose_iter_build_output_path(
      buf, (int) sizeof(buf),
      "C:\\out", COMPOSE_CATEGORY_PLAYER_CHAR, NULL, NULL,
      "WL", "HTH", 0));
   /* mode NULL */
   TEST_ASSERT_EQUAL_INT(0, compose_iter_build_output_path(
      buf, (int) sizeof(buf),
      "C:\\out", COMPOSE_CATEGORY_PLAYER_CHAR, "NE", NULL,
      NULL, "HTH", 0));
   /* negative direction */
   TEST_ASSERT_EQUAL_INT(0, compose_iter_build_output_path(
      buf, (int) sizeof(buf),
      "C:\\out", COMPOSE_CATEGORY_PLAYER_CHAR, "NE", NULL,
      "WL", "HTH", -1));
}

static void test_output_dir_omits_filename(void)
{
   char buf[512];
   int ok = compose_iter_build_output_dir(
      buf, (int) sizeof(buf),
      "C:\\out", COMPOSE_CATEGORY_PLAYER_CHAR,
      "NE", "Necromancer");
   TEST_ASSERT_EQUAL_INT(1, ok);
   TEST_ASSERT_EQUAL_STRING("C:\\out\\Player_Characters\\NE", buf);
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_default_lists_are_nonempty_and_terminated);
   RUN_TEST(test_default_modes_include_idle_and_walk);
   RUN_TEST(test_player_classes_include_necromancer);
   RUN_TEST(test_category_base_paths);
   RUN_TEST(test_category_skin);
   RUN_TEST(test_category_folder);
   RUN_TEST(test_output_path_codes_only);
   RUN_TEST(test_output_path_full_folder_names);
   RUN_TEST(test_output_path_no_wclass);
   RUN_TEST(test_output_path_full_name_with_unsafe_chars);
   RUN_TEST(test_output_path_falls_back_to_code_when_full_missing);
   RUN_TEST(test_output_path_rejects_bad_args);
   RUN_TEST(test_output_dir_omits_filename);
   return UNITY_END();
}
