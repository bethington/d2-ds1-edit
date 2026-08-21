/* Derived from win_ds1edit by Paul Siramy (originally iniread.h and inicreat.h).
 * See NOTICE at the repository root for attribution and license status. */

#ifndef _CONFIG_H_

#define _CONFIG_H_

/* The config file's name, spelled once so the code that opens it and the
 * build rules that copy it into bin/ cannot drift apart. Case matters:
 * on a case-sensitive filesystem a lowercase literal here does not match
 * the Ds1edit.ini that CMake installs and that README tells users to
 * create, so the editor never sees its own config. */
#define DS1EDIT_INI_NAME "Ds1edit.ini"

void ini_create (char * ininame);
void ini_read   (char * ininame);

/* Parse a zoom string ("1:1", "1:4", ...) into a ZOOM_E, or -1. */
int config_zoom_from_string(const char *str);

#endif
