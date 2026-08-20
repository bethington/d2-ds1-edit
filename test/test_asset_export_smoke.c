#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"

#include "unity.h"

#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>

#include "core/asset_export.h"

#ifndef TEST_BUILD_DIR
#define TEST_BUILD_DIR "."
#endif

#ifndef DS1EDIT_BIN_EXE
#ifdef WIN32
#define DS1EDIT_BIN_EXE "bin\\ds1edit.exe"
#else
#define DS1EDIT_BIN_EXE "bin/ds1edit"
#endif
#endif

void test_asset_export_stub_reset(void);
void test_asset_export_stub_set_mpq_file(const char *path,
                                         const unsigned char *data,
                                         long len);
int test_asset_export_stub_get_misc_load_count(void);
int test_asset_export_stub_get_dt1_add_count(void);

static const unsigned char tiny_dc6_fixture[] = {
   0x06, 0x00, 0x00, 0x00,
   0x01, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x01, 0x00, 0x00, 0x00,
   0x01, 0x00, 0x00, 0x00,
   0x1c, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x01, 0x00, 0x00, 0x00,
   0x01, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x03, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x01, 0x2a, 0x80
};

static unsigned char valid_dt1_header_fixture[276 + 96];
static const unsigned char invalid_dt1_header_fixture[] = {
   0x04, 0x00, 0x00, 0x00,
   0x01, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00
};

static void build_valid_dt1_header_fixture(void)
{
   memset(valid_dt1_header_fixture, 0, sizeof(valid_dt1_header_fixture));
   * (int32_t *) (valid_dt1_header_fixture + 0) = 7;
   * (int32_t *) (valid_dt1_header_fixture + 4) = 6;
   * (int32_t *) (valid_dt1_header_fixture + 268) = 1;
   * (int32_t *) (valid_dt1_header_fixture + 272) = 276;
}

static int file_exists_nonempty(const char *path)
{
   FILE *fp;
   long size;

   fp = fopen(path, "rb");
   if (fp == NULL)
      return 0;

   fseek(fp, 0, SEEK_END);
   size = ftell(fp);
   fclose(fp);
   return size > 0;
}

static void ensure_dir(const char *path)
{
   DS1_MKDIR(path);
}

static void init_allegro_once(void)
{
   static int initialized = 0;

   if (initialized)
      return;

   TEST_ASSERT_TRUE(al_init());
   TEST_ASSERT_TRUE(al_init_image_addon());
   initialized = 1;
}

void setUp(void)
{
   test_asset_export_stub_reset();
}

void tearDown(void)
{
}

static void test_dc6_fixture_exports_png(void)
{
   char out_root[512];
   char out_path[768];
   int exported;

   init_allegro_once();

   snprintf(out_root, sizeof(out_root), "%s\\asset_export_smoke_out", TEST_BUILD_DIR);
   ensure_dir(out_root);

   exported = asset_export_dc6_buffer_png(
      "Data\\Test\\tiny.dc6",
      tiny_dc6_fixture,
      (long) sizeof(tiny_dc6_fixture),
      out_root
   );

   TEST_ASSERT_EQUAL_INT(1, exported);

   snprintf(out_path, sizeof(out_path), "%s\\Data\\Test\\tiny.png", out_root);
   TEST_ASSERT_TRUE(file_exists_nonempty(out_path));
}

static void test_dcc_cli_smoke_if_configured(void)
{
   const char *asset_path;
   char out_root[512];
   char out_path[1024];
   char command[2048];
   int rc;

   asset_path = getenv("DS1EDIT_DCC_SMOKE_ASSET");
   if (asset_path == NULL || asset_path[0] == 0)
      TEST_IGNORE_MESSAGE("Set DS1EDIT_DCC_SMOKE_ASSET to run the DCC smoke export.");
   if (!file_exists_nonempty(DS1EDIT_BIN_EXE))
      TEST_IGNORE_MESSAGE("bin\\ds1edit.exe is not available for the DCC smoke export.");

   snprintf(out_root, sizeof(out_root), "%s\\asset_export_dcc_cli", TEST_BUILD_DIR);
   ensure_dir(out_root);

   snprintf(command, sizeof(command),
      "\"%s\" --export-asset \"%s\" \"%s\" -force_pal 1 > NUL 2>&1",
      DS1EDIT_BIN_EXE,
      asset_path,
      out_root);
   rc = system(command);
   TEST_ASSERT_EQUAL_INT(0, rc);

   snprintf(out_path, sizeof(out_path), "%s\\%s\\frame_000.png", out_root, asset_path);
   TEST_ASSERT_TRUE(file_exists_nonempty(out_path));
}

static void test_prefix_filter_matches_segments_only(void)
{
   TEST_ASSERT_TRUE(asset_export_filter_matches_prefix(
      "Data\\Global\\Tiles\\Act1\\Town\\floor.dt1",
      "Data\\Global\\Tiles\\Act1\\Town"));
   TEST_ASSERT_TRUE(asset_export_filter_matches_prefix(
      "Data/Global/Tiles/Act1/Town/floor.dt1",
      "Data\\Global\\Tiles\\Act1\\Town\\"));
   TEST_ASSERT_FALSE(asset_export_filter_matches_prefix(
      "Data\\Global\\Tiles\\Act1\\TownExtra\\floor.dt1",
      "Data\\Global\\Tiles\\Act1\\Town"));
}

static void test_type_filter_matches_supported_extensions_only(void)
{
   TEST_ASSERT_TRUE(asset_export_filter_matches_type(
      "Data\\Global\\Tiles\\Act1\\Town\\floor.dt1",
      "dt1"));
   TEST_ASSERT_FALSE(asset_export_filter_matches_type(
      "Data\\Global\\Excel\\bad.dt1",
      "dt1"));
   TEST_ASSERT_TRUE(asset_export_filter_matches_type(
      "Data\\Global\\Monsters\\Fallen\\idle.dcc",
      "all"));
   TEST_ASSERT_TRUE(asset_export_filter_matches_type(
      "Data\\Global\\ui\\panel.dc6",
      NULL));
   TEST_ASSERT_FALSE(asset_export_filter_matches_type(
      "Data\\Global\\ui\\panel.dc6",
      "dt1"));
   TEST_ASSERT_FALSE(asset_export_filter_matches_type(
      "Data\\Global\\Excel\\Levels.txt",
      "all"));
}

static void test_guess_palette_act_from_asset_path(void)
{
   TEST_ASSERT_EQUAL_INT(2, asset_export_guess_palette_act(
      "Data\\Global\\Tiles\\Act2\\BigCliff\\CliffMesa.dt1"));
   TEST_ASSERT_EQUAL_INT(2, asset_export_guess_palette_act(
      "Act2\\Arcane\\Sanctuary.dt1"));
   TEST_ASSERT_EQUAL_INT(5, asset_export_guess_palette_act(
      "Data\\Global\\Tiles\\Expansion\\Siege\\ground.dt1"));
   TEST_ASSERT_EQUAL_INT(0, asset_export_guess_palette_act(
      "Data\\Global\\Excel\\Levels.txt"));
}

static void test_dt1_header_bounds_validation(void)
{
   build_valid_dt1_header_fixture();

   TEST_ASSERT_TRUE(asset_export_dt1_header_looks_valid(
      valid_dt1_header_fixture,
      (long) sizeof(valid_dt1_header_fixture)));

   * (int32_t *) (valid_dt1_header_fixture + 268) = 2;
   TEST_ASSERT_FALSE(asset_export_dt1_header_looks_valid(
      valid_dt1_header_fixture,
      (long) sizeof(valid_dt1_header_fixture)));

   * (int32_t *) (valid_dt1_header_fixture + 268) = 1;
   * (int32_t *) (valid_dt1_header_fixture + 272) = 277;
   TEST_ASSERT_FALSE(asset_export_dt1_header_looks_valid(
      valid_dt1_header_fixture,
      (long) sizeof(valid_dt1_header_fixture)));
}

static void test_invalid_dt1_discovery_is_rejected_and_cached(void)
{
   static char filename_table[MPQTYPES_MAX_PATH * 2];
   static char identify_table[2];
   const char *invalid_path = "Data\\Global\\Tiles\\Act1\\Bad\\invalid.dt1";
   int exported;

   memset(filename_table, 0, sizeof(filename_table));
   memset(identify_table, 0, sizeof(identify_table));
   strcpy(filename_table, invalid_path);
   strcpy(filename_table + MPQTYPES_MAX_PATH, invalid_path);
   identify_table[0] = 0x1;
   identify_table[1] = 0x1;

   glb_mpq_struct[0].is_open = TRUE;
   glb_mpq_struct[0].count_files = 2;
   glb_mpq_struct[0].filename_table = filename_table;
   glb_mpq_struct[0].identify_table = identify_table;

   test_asset_export_stub_set_mpq_file(
      invalid_path,
      invalid_dt1_header_fixture,
      (long) sizeof(invalid_dt1_header_fixture));

   exported = asset_export_prefix_png("Data", "dt1", TEST_BUILD_DIR);

   TEST_ASSERT_EQUAL_INT(0, exported);
   TEST_ASSERT_EQUAL_INT(1, test_asset_export_stub_get_misc_load_count());
   TEST_ASSERT_EQUAL_INT(0, test_asset_export_stub_get_dt1_add_count());
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_prefix_filter_matches_segments_only);
   RUN_TEST(test_type_filter_matches_supported_extensions_only);
   RUN_TEST(test_guess_palette_act_from_asset_path);
   RUN_TEST(test_dt1_header_bounds_validation);
   RUN_TEST(test_invalid_dt1_discovery_is_rejected_and_cached);
   RUN_TEST(test_dc6_fixture_exports_png);
   RUN_TEST(test_dcc_cli_smoke_if_configured);
   return UNITY_END();
}
