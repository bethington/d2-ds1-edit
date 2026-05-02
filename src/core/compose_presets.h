#ifndef _COMPOSE_PRESETS_H_
#define _COMPOSE_PRESETS_H_

// User-defined named lists of D2 mode codes (NU, WL, A1, DT, ...) and
// weapon-class codes (HTH, 1HS, BOW, ...) for the unified compose
// flow's multi-select pickers. Per Q2/Q3 of the planning doc, the
// pickers also always offer a hardcoded "All" entry as a fallback,
// so an empty or missing config is graceful.
//
// INI format (in Ds1edit.ini):
//
//   [char_mode_presets]
//   idle_only       = NU
//   idle_walk       = NU, WL
//   standard_combat = NU, WL, A1, DT
//
//   [char_weapon_presets]
//   bare_hands  = HTH
//   melee       = HTH, 1HS, 2HS
//   ...

#define COMPOSE_PRESET_NAME_MAX  64
#define COMPOSE_PRESET_CODE_MAX  16
#define COMPOSE_PRESET_CODES_MAX 32
#define COMPOSE_PRESETS_MAX      64

typedef struct COMPOSE_PRESET_S
{
   char name[COMPOSE_PRESET_NAME_MAX];
   int  code_count;
   char codes[COMPOSE_PRESET_CODES_MAX][COMPOSE_PRESET_CODE_MAX];
} COMPOSE_PRESET_S;

// Pure parse function, exposed for unit testing. Parses one INI line's
// (key, comma-separated-value) pair into a preset. Trims whitespace
// around each code and uppercases each. Returns 1 on success, 0 on
// failure (NULL inputs, empty name, empty value list).
int compose_preset_parse(const char *name, const char *value,
                         COMPOSE_PRESET_S *out);

// Mode preset registry.
int  compose_mode_presets_count(void);
const COMPOSE_PRESET_S * compose_mode_presets_at(int idx);
int  compose_mode_presets_add(const COMPOSE_PRESET_S *preset);
void compose_mode_presets_reset(void);

// Weapon preset registry.
int  compose_weapon_presets_count(void);
const COMPOSE_PRESET_S * compose_weapon_presets_at(int idx);
int  compose_weapon_presets_add(const COMPOSE_PRESET_S *preset);
void compose_weapon_presets_reset(void);

#endif
