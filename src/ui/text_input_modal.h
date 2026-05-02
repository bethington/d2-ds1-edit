#ifndef _TEXT_INPUT_MODAL_H_
#define _TEXT_INPUT_MODAL_H_

// Show a centered modal dialog with a single-line text input field.
// Drives the existing Allegro event loop until the user clicks OK,
// presses Enter, or cancels (Esc / clicks Cancel).
//
// title          - Title bar text (displayed at top of the modal).
// prompt         - One-line prompt label rendered above the input field.
// default_text   - Initial value of the input field. NULL or "" for empty.
// out_buf        - Caller-supplied buffer to receive the entered string
//                  on OK. Untouched on Cancel.
// out_cap        - Capacity of out_buf in bytes (must be > 0). The
//                  result is always NUL-terminated and never exceeds
//                  out_cap - 1 characters.
//
// Returns 1 if the user confirmed (Enter / OK click) with a non-empty
// string. Returns 0 on cancel or if the user submitted an empty string.
//
// Supported editing: typing printable ASCII, Backspace, Delete, Left
// and Right arrows, Home, End, Ctrl+V (paste). No selection model in
// v1 — Shift-arrow does not extend a selection. Cursor blinks at ~2 Hz.
int text_input_modal_show(const char *title,
                          const char *prompt,
                          const char *default_text,
                          char *out_buf,
                          int out_cap);

#endif
