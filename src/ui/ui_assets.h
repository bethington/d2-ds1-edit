#ifndef DS1EDIT_UI_ASSETS_H
#define DS1EDIT_UI_ASSETS_H

/* Where the editor's own interface images live, relative to the working
 * directory. Buttons, tabs, cursors, the walkable-info bits, and the two
 * nine-slice window frames.
 *
 * This was "pcx/" until the Allegro 5 port replaced the last .pcx with a
 * .png; the name outlived the format by some years. Paths are built from
 * UI_DIR rather than spelled out at each call site so the directory can move
 * again without another sweep through src/.
 */
#define UI_DIR              "assets/ui/"
#define UI_FRAME_PREVIEW    UI_DIR "frame_preview/"   /* preview window chrome */
#define UI_FRAME_TILEWIN    UI_DIR "frame_tilewin/"   /* tile window chrome    */

#endif /* DS1EDIT_UI_ASSETS_H */
