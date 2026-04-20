#ifndef _MPQ_INDEX_H_
#define _MPQ_INDEX_H_

// Eager, queryable index built from LvlPrest.txt + Levels.txt + LvlTypes.txt.
// Produced once at startup (or on project switch) so the preset picker can
// enumerate presets without a TXT scan per click, and DS1 -> (type, def)
// resolution is O(scan-the-index) with all the data pre-joined.
//
// This complements -- does not replace -- read_lvlprest_txt() /
// read_lvltypes_txt(). Those still do the per-DS1 side effects (loading
// referenced DT1s into glb_ds1[...].dt1_idx). The index is purely read-only
// metadata.

#define MPQ_INDEX_PRESET_FILES_MAX   6
#define MPQ_INDEX_PATH_LEN         128
#define MPQ_INDEX_NAME_LEN          64
#define MPQ_INDEX_TYPE_NAME_LEN     64

typedef struct PRESET_ENTRY_S
{
   // From LvlPrest.txt
   int  def;                                              // "Def" (primary key)
   int  level_id;                                         // "LevelId"
   char name[MPQ_INDEX_NAME_LEN];                         // "Name"
   int  ds1_count;
   char ds1_files[MPQ_INDEX_PRESET_FILES_MAX][MPQ_INDEX_PATH_LEN];

   // Joined from Levels.txt (via level_id) and LvlTypes.txt (via level_type)
   int  level_type;                                       // Levels."LevelType"
   int  act;                                              // Levels."Act" (1..5, 0 if unknown)
   char type_name[MPQ_INDEX_TYPE_NAME_LEN];               // LvlTypes."Name"
} PRESET_ENTRY_S;

// Build the index from the three D2 tables. Returns the number of entries
// indexed, or -1 on failure. Idempotent: destroys and rebuilds. Must be
// called after MPQs are open (uses the existing txt_read_in_mem pipeline).
int  mpq_index_build(void);

// Release all state. Safe to call when nothing is built.
void mpq_index_destroy(void);

// Is the index populated and queryable?
int  mpq_index_is_ready(void);

// Flat enumeration for the picker.
int  mpq_index_preset_count(void);
const PRESET_ENTRY_S * mpq_index_preset_at(int idx);

// Reverse lookup: basename (e.g. "denent.ds1") -> preset. Case-insensitive.
// Returns NULL if no preset references this DS1 in any File1..File6 slot.
// If `out_file_slot` is non-NULL, receives the 0-based slot (0..5) the DS1
// was matched in.
const PRESET_ENTRY_S * mpq_index_find_by_ds1_name(const char *ds1_basename,
                                                  int *out_file_slot);

// Pure variant of the reverse lookup, exposed so unit tests can exercise
// the scan/match logic without having to populate the global index first.
const PRESET_ENTRY_S * mpq_index_find_by_ds1_name_in(const PRESET_ENTRY_S *arr,
                                                     int count,
                                                     const char *ds1_basename,
                                                     int *out_file_slot);

#endif
