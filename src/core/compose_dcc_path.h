#ifndef _COMPOSE_DCC_PATH_H_
#define _COMPOSE_DCC_PATH_H_

// Build the virtual asset path for one DCC layer file used by the
// composer. D2 stores per-layer DCCs at well-known locations:
//
//   <base>\<token>\<layer_code>\<token><layer_code><skin><mode><wclass>.dcc
//
// Examples:
//   data\global\chars\NE\HD\NEHDLITNUHTH.dcc
//     (Necromancer, head, light skin, idle, bare hands)
//   data\global\chars\SO\TR\SOTRMEDWL1HS.dcc
//     (Sorceress, torso, medium skin, walk, 1H sword)
//   data\global\monsters\AN\HD\ANHDNUHTH.dcc
//     (Andariel, head, no skin variant, idle, bare hands)
//
// The 16 layer slots have well-known 2-char codes:
//   0=HD 1=TR 2=LG 3=RA 4=LA 5=RH 6=LH 7=SH 8..15=S1..S8

#define COMPOSE_DCC_PATH_LAYER_COUNT 16

// Returns the 2-char layer code for a slot index 0..15. Returns NULL
// for out-of-range slots.
const char * compose_dcc_path_layer_code(int slot);

// Build the DCC path for one layer of one (token, mode, weapon)
// tuple. Writes a NUL-terminated path of at most out_cap-1 chars into
// out_buf. Returns 1 on success, 0 on bad arguments or buffer too
// small.
//
//   base       Root of the asset tree, e.g. "data\\global\\chars".
//   token      Asset code, e.g. "NE", "AN", "TownPortal".
//   slot       Layer slot 0..15 (HD..S8).
//   skin       Skin variant code (e.g. "LIT", "MED"). May be NULL or
//              empty for assets with no skin variant (most monsters).
//   mode       D2 mode code (e.g. "NU", "WL", "A1").
//   wclass     Weapon-class code (e.g. "HTH", "1HS", "BOW"). May be
//              NULL or empty for layers whose weapon_class field in
//              the COF is blank.
int compose_dcc_path_build(char *out_buf, int out_cap,
                           const char *base,
                           const char *token,
                           int slot,
                           const char *skin,
                           const char *mode,
                           const char *wclass);

#endif
