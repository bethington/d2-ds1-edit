#ifndef _PRESET_PICKER_H_
#define _PRESET_PICKER_H_

// Ctrl+Shift+P opens a modal "Open Preset" finder that filters the preset
// index (LvlPrest rows joined with LvlTypes/Levels) in real time and
// loads the selected DS1 on Enter. Escape closes with no effect.
//
// Poll from the main loop every frame; no-op unless the shortcut is hit.
void preset_picker_handle_shortcut(void);

#endif
