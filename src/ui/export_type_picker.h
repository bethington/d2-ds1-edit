#ifndef _EXPORT_TYPE_PICKER_H_
#define _EXPORT_TYPE_PICKER_H_

// Show a small modal picker for supported export types.
// Returns one of: "all", "dt1", "dc6", "dcc", or NULL on cancel.
const char *export_type_picker_choose(const char *title);

#endif