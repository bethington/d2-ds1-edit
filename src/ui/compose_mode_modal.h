#ifndef _COMPOSE_MODE_MODAL_H_
#define _COMPOSE_MODE_MODAL_H_

// "Compose mode?" follow-up modal that appears after the user picks
// DCC or All from the type picker (per Q5b of the planning doc).
// Shows a centered yes/no prompt with the Yes button as the default
// (Enter confirms YES). Esc returns 0 (cancel).
//
// Returns:
//   1 = user picked YES (compose mode on)
//   2 = user picked NO  (raw export mode)
//   0 = user cancelled (Esc / Display close)
int compose_mode_modal_show(void);

#endif
