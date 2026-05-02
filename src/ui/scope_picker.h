#ifndef _SCOPE_PICKER_H_
#define _SCOPE_PICKER_H_

// Modal scope picker for the unified export action. Shown after the
// user has chosen an asset type (from export_type_picker). Lets the
// user pick:
//
//   - "All <type> assets"      — recursive Data\ scan, type-filtered
//   - "Current area's assets"  — uses the loaded DS1's lvltype/lvlprest
//   - "Choose folder..."       — caller opens its native folder picker
//   - "Type custom pattern..." — opens text_input_modal_show inline
//   - any user-defined preset from [export_presets] in Ds1edit.ini
//     (auto-filtered by the chosen type)

typedef enum SCOPE_KIND_E
{
   SCOPE_KIND_NONE    = 0,
   SCOPE_KIND_ALL     = 1,
   SCOPE_KIND_AREA    = 2,
   SCOPE_KIND_FOLDER  = 3,
   SCOPE_KIND_PATTERN = 4
} SCOPE_KIND_E;

typedef struct SCOPE_RESULT_S
{
   SCOPE_KIND_E kind;
   /* Valid only when kind == SCOPE_KIND_PATTERN. Holds either the
    * user-typed custom pattern or the matched preset's pattern. */
   char pattern[256];
} SCOPE_RESULT_S;

// Show the picker. type_filter is the type chosen by the user (one of
// "all"/"dt1"/"dc6"/"dcc") and is used to:
//   - label the "All <type> assets" entry
//   - filter the preset list (presets whose type does not match are
//     hidden; presets typed "all" always appear)
//
// area_available is the visibility flag for the "Current area's assets"
// entry: when 0, the entry renders greyed-out and cannot be selected.
//
// Returns 1 if the user confirmed a selection (out is populated), 0 on
// cancel (Esc / clicking outside / X-out the dialog).
int scope_picker_choose(const char *title,
                        const char *type_filter,
                        int area_available,
                        SCOPE_RESULT_S *out);

#endif
