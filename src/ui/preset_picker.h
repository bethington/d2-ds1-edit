#ifndef _PRESET_PICKER_H_
#define _PRESET_PICKER_H_

// Opens a modal "Open Preset" finder that filters the preset
// index (LvlPrest rows joined with LvlTypes/Levels) in real time and
// loads the selected DS1 on Enter. Escape closes with no effect.
//
// Called from the command table (Ctrl+Shift+P or the File menu).
void preset_picker_open(void);

#endif
