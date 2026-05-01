#ifndef _UPSCALE_MODE_PICKER_H_
#define _UPSCALE_MODE_PICKER_H_

typedef enum UPSCALE_MODE_E
{
   UPSCALE_MODE_NONE = 0,
   UPSCALE_MODE_2X,
   UPSCALE_MODE_4X
} UPSCALE_MODE_E;

// Show a small modal picker for export upscale mode.
// Returns one of UPSCALE_MODE_NONE/2X/4X, or -1 on cancel.
int upscale_mode_picker_choose(const char *title, int remote_enabled);

#endif