/*
 * Install and Project Diablo 2 detection (src/core/d2install.c), every
 * precedence step, run through an injected registry reader and file probe.
 * Nothing here reads the machine's registry or game folders, so the results
 * are identical on Windows, Linux and macOS.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity/unity.h"
#include "platform.h"
#include "structs.h"
#include "core/d2install.h"

CONFIG_S glb_config;   /* normally defined in globals of the application */

void setUp(void) {}
void tearDown(void) {}

#define FK_MAX 40

typedef struct
{
   const char *files[FK_MAX];           /* full paths that "exist" */
   int         nfiles;
   const char *hkcu, *hklm, *wow64;     /* Blizzard InstallPath values */
   struct { D2REG_ROOT root; const char *name; const char *loc; } un[8];
   int         nun;
   const char *common[6];
} FAKE;

static const char *norm(const char *s)
{
   static char buf[8][700];
   static int n;
   char *b = buf[n++ & 7];
   snprintf(b, 700, "%s", s);
   ds1_path_normalize(b);
   return b;
}

static int fk_streq_nocase(const char *a, const char *b)
{
   for (; *a && *b; a++, b++)
   {
      char x = *a, y = *b;
      if (x >= 'A' && x <= 'Z') x = (char)(x + 32);
      if (y >= 'A' && y <= 'Z') y = (char)(y + 32);
      if (x != y) return 0;
   }
   return *a == 0 && *b == 0;
}

static int fk_find_file(void *ctx, const char *dir, const char *name,
                        char *out, size_t cap)
{
   FAKE *f = (FAKE *) ctx;
   char want[700];
   int i;
   snprintf(want, sizeof(want), "%s" DS1_SEP_STR "%s", dir, name);
   ds1_path_normalize(want);
   for (i = 0; i < f->nfiles; i++)
      if (fk_streq_nocase(want, norm(f->files[i])))
      {
         snprintf(out, cap, "%s", norm(f->files[i]));
         return 1;
      }
   return 0;
}

static int fk_reg_install_path(void *ctx, D2REG_ROOT root, char *out, int cap)
{
   FAKE *f = (FAKE *) ctx;
   const char *v = root == D2REG_HKCU ? f->hkcu
                 : root == D2REG_HKLM_NATIVE ? f->hklm : f->wow64;
   if (v == NULL) return 0;
   snprintf(out, (size_t) cap, "%s", v);
   return 1;
}

static int fk_reg_uninstall(void *ctx, D2REG_ROOT root, const char *name,
                            char *out, int cap)
{
   FAKE *f = (FAKE *) ctx;
   int i;
   for (i = 0; i < f->nun; i++)
      if (f->un[i].root == root && strcmp(f->un[i].name, name) == 0)
      {
         snprintf(out, (size_t) cap, "%s", f->un[i].loc);
         return 1;
      }
   return 0;
}

static D2INSTALL_ENV fk_env(FAKE *f, int with_registry)
{
   D2INSTALL_ENV e;
   memset(&e, 0, sizeof(e));
   e.ctx = f;
   e.find_file = fk_find_file;
   if (with_registry)
   {
      e.reg_install_path = fk_reg_install_path;
      e.reg_uninstall_location = fk_reg_uninstall;
   }
   e.common_paths = f->common;
   return e;
}

static void fk_add(FAKE *f, const char *path) { f->files[f->nfiles++] = path; }
static void fk_un(FAKE *f, D2REG_ROOT r, const char *name, const char *loc)
{
   f->un[f->nun].root = r; f->un[f->nun].name = name; f->un[f->nun].loc = loc;
   f->nun++;
}

/* A fake machine with a D2 in several places at once; tests take things away. */
static void fk_everywhere(FAKE *f)
{
   memset(f, 0, sizeof(*f));
   f->hkcu  = "C:\\ByHkcu";
   f->hklm  = "C:\\ByHklm";
   f->wow64 = "C:\\ByWow\\";
   fk_un(f, D2REG_HKCU, "Diablo II", "C:\\ByUnHkcu\\");
   fk_un(f, D2REG_HKLM_NATIVE, "Diablo II", "C:\\ByUnHklm\\");
   fk_un(f, D2REG_HKLM_WOW64, "Diablo II", "C:\\ByUnWow\\");
   f->common[0] = "C:\\ByCommon";
   fk_add(f, "C:\\ByIni\\d2data.mpq");
   fk_add(f, "C:\\ByHkcu\\d2data.mpq");
   fk_add(f, "C:\\ByHklm\\d2data.mpq");
   fk_add(f, "C:\\ByWow\\d2data.mpq");
   fk_add(f, "C:\\ByUnHkcu\\d2exp.mpq");
   fk_add(f, "C:\\ByUnHklm\\d2data.mpq");
   fk_add(f, "C:\\ByUnWow\\d2data.mpq");
   fk_add(f, "C:\\ByCommon\\d2data.mpq");
}

static void expect_d2(const D2INSTALL_ENV *e, const char *ini,
                      const char *want, D2INSTALL_SRC src)
{
   char out[600];
   D2INSTALL_SRC got = D2SRC_NONE;
   TEST_ASSERT_TRUE(d2install_find_d2(e, ini, out, sizeof(out), &got));
   TEST_ASSERT_EQUAL_STRING(norm(want), out);
   TEST_ASSERT_EQUAL_INT(src, got);
}

void test_detect_ini_beats_every_other_source(void)
{
   FAKE f; D2INSTALL_ENV e;
   fk_everywhere(&f); e = fk_env(&f, 1);
   expect_d2(&e, "C:\\ByIni", "C:\\ByIni", D2SRC_INI);
}

void test_detect_precedence_chain_step_by_step(void)
{
   FAKE f; D2INSTALL_ENV e;
   fk_everywhere(&f); e = fk_env(&f, 1);
   expect_d2(&e, NULL, "C:\\ByHkcu", D2SRC_REG_HKCU);

   f.hkcu = NULL;
   expect_d2(&e, "", "C:\\ByHklm", D2SRC_REG_HKLM);

   f.hklm = NULL;
   expect_d2(&e, NULL, "C:\\ByWow", D2SRC_REG_WOW64);

   f.wow64 = NULL;
   /* the HKCU Uninstall entry holds only d2exp.mpq: still a valid install */
   expect_d2(&e, NULL, "C:\\ByUnHkcu", D2SRC_UNINSTALL);

   f.un[0].name = "something else";
   expect_d2(&e, NULL, "C:\\ByUnHklm", D2SRC_UNINSTALL);
   f.un[1].name = "something else";
   expect_d2(&e, NULL, "C:\\ByUnWow", D2SRC_UNINSTALL);
   f.un[2].name = "something else";
   expect_d2(&e, NULL, "C:\\ByCommon", D2SRC_COMMON);
}

void test_detect_reports_nothing_when_nothing_is_there(void)
{
   FAKE f; D2INSTALL_ENV e; char out[600] = "x"; D2INSTALL_SRC s;
   memset(&f, 0, sizeof(f)); e = fk_env(&f, 1);
   TEST_ASSERT_FALSE(d2install_find_d2(&e, NULL, out, sizeof(out), &s));
   TEST_ASSERT_EQUAL_STRING("", out);
   TEST_ASSERT_EQUAL_INT(D2SRC_NONE, s);
}

void test_detect_strips_trailing_separators(void)
{
   FAKE f; D2INSTALL_ENV e;
   memset(&f, 0, sizeof(f));
   f.hklm = "C:\\Diablo2\\";
   fk_add(&f, "C:\\Diablo2\\d2data.mpq");
   e = fk_env(&f, 1);
   expect_d2(&e, NULL, "C:\\Diablo2", D2SRC_REG_HKLM);

   f.hklm = "C:\\Diablo2\\\\";
   expect_d2(&e, NULL, "C:\\Diablo2", D2SRC_REG_HKLM);

   f.hklm = NULL;
   expect_d2(&e, "C:\\Diablo2\\", "C:\\Diablo2", D2SRC_INI);
}

void test_detect_invalid_registry_path_falls_through(void)
{
   FAKE f; D2INSTALL_ENV e;
   memset(&f, 0, sizeof(f));
   f.hkcu = "C:\\Gone";                         /* no archives there */
   f.hklm = "C:\\AlsoGone";
   fk_add(&f, "C:\\Real\\d2data.mpq");
   f.wow64 = "C:\\Real";
   e = fk_env(&f, 1);
   expect_d2(&e, NULL, "C:\\Real", D2SRC_REG_WOW64);

   f.wow64 = "C:\\Gone";
   fk_un(&f, D2REG_HKLM_WOW64, "Diablo II", "C:\\Gone\\");
   fk_un(&f, D2REG_HKCU, "Diablo II", "C:\\Real\\");
   expect_d2(&e, NULL, "C:\\Real", D2SRC_UNINSTALL);
}

void test_detect_ini_is_authoritative_even_when_invalid(void)
{
   FAKE f; D2INSTALL_ENV e;
   fk_everywhere(&f); e = fk_env(&f, 1);
   /* a typo in d2_install must not be papered over by another install */
   expect_d2(&e, "C:\\Typo", "C:\\Typo", D2SRC_INI);
}

void test_detect_without_registry_probes_common_paths_only(void)
{
   FAKE f; D2INSTALL_ENV e;
   fk_everywhere(&f);
   e = fk_env(&f, 0);                           /* Linux / macOS shape */
   expect_d2(&e, NULL, "C:\\ByCommon", D2SRC_COMMON);
}

void test_detect_archive_names_match_regardless_of_case(void)
{
   FAKE f; D2INSTALL_ENV e;
   memset(&f, 0, sizeof(f));
   f.hkcu = "C:\\Diablo2";
   fk_add(&f, "C:\\Diablo2\\D2DATA.MPQ");
   e = fk_env(&f, 1);
   expect_d2(&e, NULL, "C:\\Diablo2", D2SRC_REG_HKCU);
}

static void expect_pd2(const D2INSTALL_ENV *e, const char *ini, const char *d2,
                       const char *want, D2INSTALL_SRC src)
{
   char out[600];
   D2INSTALL_SRC got = D2SRC_NONE;
   TEST_ASSERT_TRUE(d2install_find_pd2(e, ini, d2, out, sizeof(out), &got));
   TEST_ASSERT_EQUAL_STRING(norm(want), out);
   TEST_ASSERT_EQUAL_INT(src, got);
}

void test_pd2_ini_then_uninstall_then_sibling(void)
{
   FAKE f; D2INSTALL_ENV e;
   memset(&f, 0, sizeof(f));
   fk_add(&f, "D:\\Mine\\pd2data.mpq");
   fk_add(&f, "C:\\UnPd2\\ProjectD2\\pd2data.mpq");
   fk_add(&f, "C:\\Diablo2\\ProjectD2\\pd2data.mpq");
   fk_un(&f, D2REG_HKLM_NATIVE, "Project Diablo 2", "C:\\UnPd2\\");
   e = fk_env(&f, 1);

   expect_pd2(&e, "D:\\Mine", "C:\\Diablo2", "D:\\Mine", D2SRC_INI);
   expect_pd2(&e, NULL, "C:\\Diablo2", "C:\\UnPd2\\ProjectD2", D2SRC_UNINSTALL);

   f.un[0].name = "other";
   expect_pd2(&e, "", "C:\\Diablo2", "C:\\Diablo2\\ProjectD2", D2SRC_SIBLING);
}

void test_pd2_uninstall_checks_all_three_roots(void)
{
   FAKE f; D2INSTALL_ENV e;
   D2REG_ROOT roots[3] = { D2REG_HKCU, D2REG_HKLM_NATIVE, D2REG_HKLM_WOW64 };
   int i;
   for (i = 0; i < 3; i++)
   {
      memset(&f, 0, sizeof(f));
      fk_add(&f, "C:\\P\\ProjectD2\\pd2data.mpq");
      fk_un(&f, roots[i], "Project Diablo 2", "C:\\P");
      e = fk_env(&f, 1);
      expect_pd2(&e, NULL, NULL, "C:\\P\\ProjectD2", D2SRC_UNINSTALL);
   }
}

void test_pd2_folder_without_pd2data_is_rejected_and_falls_through(void)
{
   FAKE f; D2INSTALL_ENV e; char out[600]; D2INSTALL_SRC s;
   memset(&f, 0, sizeof(f));
   fk_add(&f, "D:\\Mine\\pd2maps.mpq");                       /* ini: no pd2data */
   fk_add(&f, "C:\\UnPd2\\ProjectD2\\pd2assets.mpq");         /* uninstall: none */
   fk_un(&f, D2REG_HKCU, "Project Diablo 2", "C:\\UnPd2");
   e = fk_env(&f, 1);
   TEST_ASSERT_FALSE(d2install_find_pd2(&e, "D:\\Mine", "C:\\Diablo2",
                                        out, sizeof(out), &s));
   TEST_ASSERT_EQUAL_STRING("", out);

   fk_add(&f, "C:\\Diablo2\\ProjectD2\\pd2data.mpq");
   expect_pd2(&e, "D:\\Mine", "C:\\Diablo2", "C:\\Diablo2\\ProjectD2", D2SRC_SIBLING);
}

void test_pd2_without_a_d2_dir_has_no_sibling_to_try(void)
{
   FAKE f; D2INSTALL_ENV e; char out[600]; D2INSTALL_SRC s;
   memset(&f, 0, sizeof(f)); e = fk_env(&f, 1);
   TEST_ASSERT_FALSE(d2install_find_pd2(&e, NULL, "", out, sizeof(out), &s));
   TEST_ASSERT_FALSE(d2install_find_pd2(&e, NULL, NULL, out, sizeof(out), &s));
}

/* A complete fake PD2 machine for the resolver tests. */
static void fk_pd2_machine(FAKE *f)
{
   memset(f, 0, sizeof(*f));
   f->hklm = "C:\\Diablo2\\";
   fk_add(f, "C:\\Diablo2\\Patch_D2.mpq");
   fk_add(f, "C:\\Diablo2\\d2exp.mpq");
   fk_add(f, "C:\\Diablo2\\d2data.mpq");
   fk_add(f, "C:\\Diablo2\\d2char.mpq");
   fk_add(f, "C:\\Diablo2\\ProjectD2\\pd2data.mpq");
   fk_add(f, "C:\\Diablo2\\ProjectD2\\pd2maps.mpq");
   fk_add(f, "C:\\Diablo2\\ProjectD2\\pd2assets.mpq");
   fk_add(f, "C:\\Diablo2\\ProjectD2\\pd2monchars.mpq");
   fk_add(f, "C:\\Diablo2\\ProjectD2\\patch_d2.mpq");
   /* shipped by PD2 but not read by the editor */
   fk_add(f, "C:\\Diablo2\\ProjectD2\\D2MultiRes.mpq");
   fk_add(f, "C:\\Diablo2\\ProjectD2\\SGD2FreeRes.mpq");
   fk_add(f, "C:\\Diablo2\\ProjectD2\\d2gl.mpq");
   fk_add(f, "C:\\Diablo2\\ProjectD2\\ver-IX86-1.mpq");
}

static void expect_chain(const D2INSTALL_RESULT *r, int slot, const char *path)
{
   TEST_ASSERT_EQUAL_STRING(norm(path), r->mpq[slot]);
}

void test_resolve_auto_orders_pd2_first_then_the_vanilla_chain(void)
{
   FAKE f; D2INSTALL_ENV e; D2INSTALL_CONFIG c; D2INSTALL_RESULT r;
   int i;
   fk_pd2_machine(&f); e = fk_env(&f, 1);
   memset(&c, 0, sizeof(c));
   d2install_resolve(&e, &c, &r);

   TEST_ASSERT_EQUAL_STRING(norm("C:\\Diablo2"), r.d2_dir);
   TEST_ASSERT_EQUAL_INT(D2SRC_REG_HKLM, r.d2_source);
   TEST_ASSERT_EQUAL_STRING(norm("C:\\Diablo2\\ProjectD2"), r.pd2_dir);
   TEST_ASSERT_TRUE(r.pd2_active);
   TEST_ASSERT_FALSE(r.pd2_required_missing);
   TEST_ASSERT_EQUAL_INT(9, r.filled);

   expect_chain(&r, 0, "C:\\Diablo2\\ProjectD2\\pd2data.mpq");
   expect_chain(&r, 1, "C:\\Diablo2\\ProjectD2\\pd2maps.mpq");
   expect_chain(&r, 2, "C:\\Diablo2\\ProjectD2\\pd2assets.mpq");
   expect_chain(&r, 3, "C:\\Diablo2\\ProjectD2\\pd2monchars.mpq");
   expect_chain(&r, 4, "C:\\Diablo2\\ProjectD2\\patch_d2.mpq");
   expect_chain(&r, MPQ_SLOT_PATCH_D2, "C:\\Diablo2\\Patch_D2.mpq");
   expect_chain(&r, MPQ_SLOT_D2EXP, "C:\\Diablo2\\d2exp.mpq");
   expect_chain(&r, MPQ_SLOT_D2DATA, "C:\\Diablo2\\d2data.mpq");
   expect_chain(&r, MPQ_SLOT_D2CHAR, "C:\\Diablo2\\d2char.mpq");

   /* the extras PD2 ships are never put in the chain */
   for (i = 0; i < MAX_MPQ_FILE; i++)
   {
      TEST_ASSERT_NULL(strstr(r.mpq[i], "D2MultiRes"));
      TEST_ASSERT_NULL(strstr(r.mpq[i], "SGD2FreeRes"));
      TEST_ASSERT_NULL(strstr(r.mpq[i], "d2gl"));
      TEST_ASSERT_NULL(strstr(r.mpq[i], "ver-IX86"));
   }
}

void test_resolve_use_pd2_no_never_adds_pd2(void)
{
   FAKE f; D2INSTALL_ENV e; D2INSTALL_CONFIG c; D2INSTALL_RESULT r;
   int i;
   fk_pd2_machine(&f); e = fk_env(&f, 1);
   memset(&c, 0, sizeof(c));
   c.use_pd2 = D2_USE_PD2_NO;
   d2install_resolve(&e, &c, &r);
   TEST_ASSERT_FALSE(r.pd2_active);
   TEST_ASSERT_EQUAL_STRING("", r.pd2_dir);
   TEST_ASSERT_EQUAL_INT(4, r.filled);
   for (i = 0; i < MAX_PD2_MPQ; i++)
      TEST_ASSERT_EQUAL_STRING("", r.mpq[i]);
   expect_chain(&r, MPQ_SLOT_D2DATA, "C:\\Diablo2\\d2data.mpq");
}

void test_resolve_use_pd2_no_ignores_even_an_ini_pd2_dir(void)
{
   FAKE f; D2INSTALL_ENV e; D2INSTALL_CONFIG c; D2INSTALL_RESULT r;
   fk_pd2_machine(&f); e = fk_env(&f, 1);
   memset(&c, 0, sizeof(c));
   c.use_pd2 = D2_USE_PD2_NO;
   c.pd2_dir = "C:\\Diablo2\\ProjectD2";
   d2install_resolve(&e, &c, &r);
   TEST_ASSERT_FALSE(r.pd2_active);
   TEST_ASSERT_EQUAL_STRING("", r.mpq[0]);
}

void test_resolve_use_pd2_yes_without_pd2_flags_the_error(void)
{
   FAKE f; D2INSTALL_ENV e; D2INSTALL_CONFIG c; D2INSTALL_RESULT r;
   memset(&f, 0, sizeof(f));
   f.hklm = "C:\\Diablo2";
   fk_add(&f, "C:\\Diablo2\\d2data.mpq");
   e = fk_env(&f, 1);
   memset(&c, 0, sizeof(c));
   c.use_pd2 = D2_USE_PD2_YES;
   d2install_resolve(&e, &c, &r);
   TEST_ASSERT_FALSE(r.pd2_active);
   TEST_ASSERT_TRUE(r.pd2_required_missing);
   TEST_ASSERT_EQUAL_INT(1, r.filled);          /* still a working vanilla chain */

   c.use_pd2 = D2_USE_PD2_AUTO;                 /* auto: absent is not an error */
   d2install_resolve(&e, &c, &r);
   TEST_ASSERT_FALSE(r.pd2_required_missing);
}

void test_resolve_use_pd2_yes_with_pd2_is_active(void)
{
   FAKE f; D2INSTALL_ENV e; D2INSTALL_CONFIG c; D2INSTALL_RESULT r;
   fk_pd2_machine(&f); e = fk_env(&f, 1);
   memset(&c, 0, sizeof(c));
   c.use_pd2 = D2_USE_PD2_YES;
   d2install_resolve(&e, &c, &r);
   TEST_ASSERT_TRUE(r.pd2_active);
   TEST_ASSERT_FALSE(r.pd2_required_missing);
}

void test_resolve_pd2_archives_that_are_missing_are_skipped(void)
{
   FAKE f; D2INSTALL_ENV e; D2INSTALL_CONFIG c; D2INSTALL_RESULT r;
   memset(&f, 0, sizeof(f));
   f.hklm = "C:\\Diablo2";
   fk_add(&f, "C:\\Diablo2\\d2data.mpq");
   fk_add(&f, "C:\\Diablo2\\ProjectD2\\pd2data.mpq");
   fk_add(&f, "C:\\Diablo2\\ProjectD2\\pd2assets.mpq");
   e = fk_env(&f, 1);
   memset(&c, 0, sizeof(c));
   d2install_resolve(&e, &c, &r);
   expect_chain(&r, 0, "C:\\Diablo2\\ProjectD2\\pd2data.mpq");
   TEST_ASSERT_EQUAL_STRING("", r.mpq[1]);      /* maps: absent, slot kept empty */
   expect_chain(&r, 2, "C:\\Diablo2\\ProjectD2\\pd2assets.mpq");
   TEST_ASSERT_EQUAL_STRING("", r.mpq[3]);
   TEST_ASSERT_EQUAL_STRING("", r.mpq[4]);
   TEST_ASSERT_EQUAL_INT(3, r.filled);
}

void test_resolve_explicit_ini_archives_are_left_to_the_ini(void)
{
   FAKE f; D2INSTALL_ENV e; D2INSTALL_CONFIG c; D2INSTALL_RESULT r;
   fk_pd2_machine(&f); e = fk_env(&f, 1);
   memset(&c, 0, sizeof(c));
   c.explicit_mpq[MPQ_SLOT_D2DATA] = "D:\\pinned\\d2data.mpq";
   c.explicit_mpq[MPQ_SLOT_PATCH_D2] = "D:\\pinned\\patch_d2.mpq";
   d2install_resolve(&e, &c, &r);
   TEST_ASSERT_EQUAL_STRING("", r.mpq[MPQ_SLOT_D2DATA]);
   TEST_ASSERT_EQUAL_STRING("", r.mpq[MPQ_SLOT_PATCH_D2]);
   expect_chain(&r, MPQ_SLOT_D2EXP, "C:\\Diablo2\\d2exp.mpq");
   expect_chain(&r, 0, "C:\\Diablo2\\ProjectD2\\pd2data.mpq");
   TEST_ASSERT_EQUAL_INT(7, r.filled);
}

void test_resolve_ini_d2_install_wins_over_detection(void)
{
   FAKE f; D2INSTALL_ENV e; D2INSTALL_CONFIG c; D2INSTALL_RESULT r;
   fk_pd2_machine(&f);
   fk_add(&f, "E:\\Other\\d2data.mpq");
   fk_add(&f, "E:\\Other\\ProjectD2\\pd2data.mpq");
   e = fk_env(&f, 1);
   memset(&c, 0, sizeof(c));
   c.d2_install = "E:\\Other";
   d2install_resolve(&e, &c, &r);
   TEST_ASSERT_EQUAL_INT(D2SRC_INI, r.d2_source);
   expect_chain(&r, MPQ_SLOT_D2DATA, "E:\\Other\\d2data.mpq");
   expect_chain(&r, 0, "E:\\Other\\ProjectD2\\pd2data.mpq");   /* sibling of the ini install */
}

void test_resolve_with_no_install_at_all_is_empty(void)
{
   FAKE f; D2INSTALL_ENV e; D2INSTALL_CONFIG c; D2INSTALL_RESULT r;
   memset(&f, 0, sizeof(f)); e = fk_env(&f, 1);
   memset(&c, 0, sizeof(c));
   d2install_resolve(&e, &c, &r);
   TEST_ASSERT_EQUAL_INT(0, r.filled);
   TEST_ASSERT_EQUAL_STRING("", r.d2_dir);
   TEST_ASSERT_FALSE(r.pd2_active);
}

void test_resolve_pd2_via_uninstall_when_it_is_not_next_to_d2(void)
{
   FAKE f; D2INSTALL_ENV e; D2INSTALL_CONFIG c; D2INSTALL_RESULT r;
   memset(&f, 0, sizeof(f));
   f.hklm = "C:\\Diablo2";
   fk_add(&f, "C:\\Diablo2\\d2data.mpq");
   fk_add(&f, "D:\\Games\\PD2\\ProjectD2\\pd2data.mpq");
   fk_un(&f, D2REG_HKCU, "Project Diablo 2", "D:\\Games\\PD2\\");
   e = fk_env(&f, 1);
   memset(&c, 0, sizeof(c));
   d2install_resolve(&e, &c, &r);
   TEST_ASSERT_TRUE(r.pd2_active);
   TEST_ASSERT_EQUAL_INT(D2SRC_UNINSTALL, r.pd2_source);
   expect_chain(&r, 0, "D:\\Games\\PD2\\ProjectD2\\pd2data.mpq");
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_detect_ini_beats_every_other_source);
   RUN_TEST(test_detect_precedence_chain_step_by_step);
   RUN_TEST(test_detect_reports_nothing_when_nothing_is_there);
   RUN_TEST(test_detect_strips_trailing_separators);
   RUN_TEST(test_detect_invalid_registry_path_falls_through);
   RUN_TEST(test_detect_ini_is_authoritative_even_when_invalid);
   RUN_TEST(test_detect_without_registry_probes_common_paths_only);
   RUN_TEST(test_detect_archive_names_match_regardless_of_case);
   RUN_TEST(test_pd2_ini_then_uninstall_then_sibling);
   RUN_TEST(test_pd2_uninstall_checks_all_three_roots);
   RUN_TEST(test_pd2_folder_without_pd2data_is_rejected_and_falls_through);
   RUN_TEST(test_pd2_without_a_d2_dir_has_no_sibling_to_try);
   RUN_TEST(test_resolve_auto_orders_pd2_first_then_the_vanilla_chain);
   RUN_TEST(test_resolve_use_pd2_no_never_adds_pd2);
   RUN_TEST(test_resolve_use_pd2_no_ignores_even_an_ini_pd2_dir);
   RUN_TEST(test_resolve_use_pd2_yes_without_pd2_flags_the_error);
   RUN_TEST(test_resolve_use_pd2_yes_with_pd2_is_active);
   RUN_TEST(test_resolve_pd2_archives_that_are_missing_are_skipped);
   RUN_TEST(test_resolve_explicit_ini_archives_are_left_to_the_ini);
   RUN_TEST(test_resolve_ini_d2_install_wins_over_detection);
   RUN_TEST(test_resolve_with_no_install_at_all_is_empty);
   RUN_TEST(test_resolve_pd2_via_uninstall_when_it_is_not_next_to_d2);
   return UNITY_END();
}
