#ifndef _MULTI_SELECT_PICKER_H_
#define _MULTI_SELECT_PICKER_H_

// Generic multi-select modal: shows a list of items each with a
// checkbox; user toggles individual items with Space, navigates with
// Up/Down, confirms with Enter, cancels with Esc. Mouse: click a row
// to toggle.
//
// Used by the compose-mode flow's mode preset picker and weapon
// preset picker. Both pass in a list of D2 codes (e.g. "NU", "WL",
// "A1", "DT") and receive back which the user selected.
//
// Special: an "All" toggle row at the top is implied by passing
// items[0].is_all_toggle = 1. Toggling "All" sets/clears every
// other item; toggling a specific item clears the "All" flag if
// not all items are now selected.

#define MULTI_SELECT_LABEL_MAX 96
#define MULTI_SELECT_MAX_ITEMS 64

typedef struct MULTI_SELECT_ITEM_S
{
   char label[MULTI_SELECT_LABEL_MAX];
   int  selected;       /* 0 = unchecked, 1 = checked */
   int  is_all_toggle;  /* 1 if this is the special "All" row */
} MULTI_SELECT_ITEM_S;

// Show the picker. items is in/out: caller initializes label + initial
// selected state for each, picker updates selected state on confirm.
// Returns 1 on confirm, 0 on cancel. If 0, items[].selected is
// untouched.
int multi_select_picker_show(const char *title,
                             MULTI_SELECT_ITEM_S *items, int item_count);

#endif
