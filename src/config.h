/* Derived from win_ds1edit by Paul Siramy (originally iniread.h and inicreat.h).
 * See NOTICE at the repository root for attribution and license status. */

#ifndef _CONFIG_H_

#define _CONFIG_H_

void ini_create (char * ininame);
void ini_read   (char * ininame);

/* Parse a zoom string ("1:1", "1:4", ...) into a ZOOM_E, or -1. */
int config_zoom_from_string(const char *str);

#endif
