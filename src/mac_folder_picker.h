/* mac_folder_picker.h -- the folder picker the editor uses on macOS.
 * See mac_folder_picker.m for why it replaces Allegro's there. */

#ifndef MAC_FOLDER_PICKER_H
#define MAC_FOLDER_PICKER_H

/* Ask for a folder with `message` shown in the panel, starting in `initial`
   (NULL or "" for the panel's own choice). Returns 1 and the folder's path in
   `out` when one was picked, 0 on cancel or when the path doesn't fit. */
int ds1_mac_pick_folder(const char *message, const char *initial,
                        char *out, int out_cap);

#endif
