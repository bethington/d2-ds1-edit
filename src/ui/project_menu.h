#ifndef _PROJECT_MENU_H_
#define _PROJECT_MENU_H_

// The "Project: <name>" indicator itself lives in preview.c's bottom status
// row next to [Objects] / Mode, so the whole HUD shares one row system and
// nothing overlaps the top header bar.

// Run the New or Open Project flow. Returns 1 if a project is now open.
int project_menu_new_project(void);
int project_menu_open_project(void);

// Close the project, and run the asset export flow. These block while native
// file dialogs are up, as the rest of the editor's modal UI does.
void project_menu_close_project(void);
void project_menu_export(void);

#endif
