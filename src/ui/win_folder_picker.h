#ifndef _WIN_FOLDER_PICKER_H_
#define _WIN_FOLDER_PICKER_H_

// Windows-only folder picker built on IFileOpenDialog with FOS_PICKFOLDERS.
// Uses the modern (Win 7+) shell dialog, which shows a prominent
// "New folder" button in the toolbar -- unlike Allegro's default path
// through SHBrowseForFolderW, where the equivalent button is small and
// easy to miss.
//
// Inputs and outputs are UTF-8. `initial_path` may be NULL.
// Returns 1 if the user selected a folder, 0 on cancel or error.

#ifdef WIN32
int win_pick_folder(const char *title, const char *initial_path,
                    char *out_utf8, int out_cap);
#endif

#endif
