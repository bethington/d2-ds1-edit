#ifndef _COMPOSE_PRESET_PICKER_H_
#define _COMPOSE_PRESET_PICKER_H_

#include "core/compose_presets.h"

// Multi-select picker for compose-mode presets (mode codes or weapon-
// class codes). Always shows a synthetic "All" entry at the top
// followed by the presets parsed from the relevant Ds1edit.ini
// section. Per Q2/Q3 of the planning doc, the "All" entry is a
// graceful fallback so even an empty/malformed preset list yields a
// usable picker.
//
// Caller picks WHICH preset table by passing in compose_mode_presets_*
// or compose_weapon_presets_* pointers. The compose_mode_picker_show
// and compose_weapon_picker_show wrappers below do that for you.

#define COMPOSE_SELECTED_CODES_MAX (COMPOSE_PRESET_CODES_MAX * 8)

typedef struct COMPOSE_PRESET_SELECTION_S
{
   int  use_all;                                                /* 1 = include every code the discovery loop finds */
   int  code_count;
   char codes[COMPOSE_SELECTED_CODES_MAX][COMPOSE_PRESET_CODE_MAX];
} COMPOSE_PRESET_SELECTION_S;

// Show the picker. Returns 1 on confirm with `out` populated, 0 on
// cancel.
//
//   title              modal title bar text
//   all_label          label for the synthetic "All" entry (e.g.
//                      "All modes" or "All weapons")
//   preset_count       count of presets to enumerate
//   preset_at(idx)     accessor; pass compose_mode_presets_at or
//                      compose_weapon_presets_at directly
int compose_preset_picker_show(const char *title,
                               const char *all_label,
                               int preset_count,
                               const COMPOSE_PRESET_S *(*preset_at)(int),
                               COMPOSE_PRESET_SELECTION_S *out);

// Convenience wrapper: shows the mode-preset picker.
int compose_mode_picker_show(COMPOSE_PRESET_SELECTION_S *out);

// Convenience wrapper: shows the weapon-preset picker.
int compose_weapon_picker_show(COMPOSE_PRESET_SELECTION_S *out);

#endif
