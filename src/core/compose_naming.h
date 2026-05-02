#ifndef _COMPOSE_NAMING_H_
#define _COMPOSE_NAMING_H_

// Naming helpers for character / monster / NPC / object composition
// outputs. The per-Q9b decision: when use_full_folder_names is YES,
// folder names use full descriptive names (e.g. "Necromancer",
// "Andariel"); filenames keep the compact 2-char-code form. Mode and
// weapon-class codes are short already and aren't expanded.
//
// Player class names are hardcoded (the 7 codes are stable). Monster /
// NPC / object names are looked up from monstats.txt / objects.txt;
// that wiring lives in compose_index and is not part of this header.

// Returns the full descriptive name for a player class code, or NULL
// if the code does not match any of the 7 D2 classes. Comparison is
// case-insensitive. Examples: "NE" -> "Necromancer", "ne" ->
// "Necromancer", "ZZ" -> NULL.
const char * compose_naming_class_name(const char *code);

// Sanitize a string for filesystem use:
//   - Any character not in [A-Za-z0-9_] is replaced with '_'.
//   - Runs of consecutive underscores collapse to a single underscore.
//   - Leading and trailing underscores are stripped.
//   - Empty input or all-non-printable input produces "_".
// Result is always NUL-terminated and never exceeds out_cap - 1
// characters. Returns 1 on success, 0 on bad arguments.
int compose_naming_sanitize(const char *src, char *out_buf, int out_cap);

#endif
