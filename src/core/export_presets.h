#ifndef _EXPORT_PRESETS_H_
#define _EXPORT_PRESETS_H_

// User-defined export presets parsed from the [export_presets] section of
// Ds1edit.ini. Each line is "name = type | pattern", where:
//   name    is the preset key (left of =)
//   type    is one of: all, dt1, dc6, dcc (case-insensitive; stored lowercase)
//   pattern is a glob pattern matched against virtual asset paths
//
// Pattern syntax is documented in core/glob_match.h.

#define EXPORT_PRESET_NAME_MAX        64
#define EXPORT_PRESET_TYPE_MAX        16
#define EXPORT_PRESET_PATTERN_MAX    256
#define EXPORT_PRESETS_MAX           256

typedef struct EXPORT_PRESET_S
{
   char name[EXPORT_PRESET_NAME_MAX];
   char type[EXPORT_PRESET_TYPE_MAX];
   char pattern[EXPORT_PRESET_PATTERN_MAX];
} EXPORT_PRESET_S;

// Pure parse function, exposed for unit testing. Parses one INI line's
// (key, value) pair into a preset struct. Returns 1 on success, 0 on
// failure (out is unchanged on failure).
//
// Failure cases: NULL inputs, empty name, missing '|', invalid type,
// empty pattern.
int export_preset_parse(const char *name, const char *value,
                        EXPORT_PRESET_S *out);

// Module state: the parsed preset array. Populated by config loading.
int export_presets_count(void);
const EXPORT_PRESET_S * export_presets_at(int idx);

// Append a preset to the in-memory array. Returns 1 on success, 0 if the
// array is full or the preset is malformed (any field empty).
int export_presets_add(const EXPORT_PRESET_S *preset);

// Reset the preset array to empty. Idempotent. Call before reloading
// config so a re-read of Ds1edit.ini does not duplicate entries.
void export_presets_reset(void);

#endif
