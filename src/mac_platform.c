/* mac_platform.c -- macOS-only host integration.
 *
 * Two things the editor needs from macOS that no other platform asks for:
 *
 *   1. Input Monitoring. Allegro's macOS mouse driver (src/macosx/qzmouse.m,
 *      osx_init_mouse) counts buttons by enumerating HID devices and fails
 *      when it finds no mouse-class device. Under TCC a process that has not
 *      been granted Input Monitoring cannot see the trackpad or mouse at all,
 *      so al_install_mouse() returns false and startup dies -- on a machine
 *      whose mouse obviously works. IOHIDRequestAccess raises the system
 *      prompt so the user can grant it by name.
 *
 *   2. Working directory. Every resource path here is relative to the
 *      process CWD ("assets/editor/", "Ds1edit.ini"). Launched from a .app
 *      the CWD is "/", so the editor must step back to the directory holding
 *      the bundle -- which is bin/, the layout the build already produces.
 *
 * IOHIDCheckAccess/IOHIDRequestAccess exist in IOKit from 10.15 on but are
 * not declared in the public SDK headers, so they are declared here.
 */

#ifdef __APPLE__

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <mach-o/dyld.h>
#include "platform.h"

/* kIOHIDRequestTypeListenEvent == 1. Returns: 0 granted, 1 denied,
   2 not-determined. */
extern int IOHIDCheckAccess(int request_type);
extern int IOHIDRequestAccess(int request_type);

#define DS1_HID_LISTEN_EVENT 1
#define DS1_HID_GRANTED      0
#define DS1_HID_DENIED       1

int ds1_mac_request_input_monitoring(void)
{
   int access = IOHIDCheckAccess(DS1_HID_LISTEN_EVENT);

   if (access == DS1_HID_GRANTED)
      return 1;

   /* Not determined: ask, which raises the prompt. The grant does not apply
      to a process already running, so a first-run user still has to restart
      -- but they get the prompt instead of a bare failure. Denied: asking
      again does nothing, System Settings is the only way back. */
   if (access != DS1_HID_DENIED)
      IOHIDRequestAccess(DS1_HID_LISTEN_EVENT);

   return 0;
}

void ds1_mac_fix_working_directory(void)
{
   char exe_path[4096];
   uint32_t size = sizeof(exe_path);
   char *marker;

   if (_NSGetExecutablePath(exe_path, &size) != 0)
      return;

   /* .../ds1edit.app/Contents/MacOS/ds1edit -> the directory holding the
      bundle. Not in a bundle: leave the CWD exactly as the shell set it. */
   marker = strstr(exe_path, ".app/Contents/MacOS/");
   if (marker == NULL)
      return;

   *marker = '\0';
   marker = strrchr(exe_path, '/');
   if (marker == NULL)
      return;
   *marker = '\0';

   if (chdir(exe_path) != 0)
      fprintf(stderr, "macOS: could not chdir to \"%s\"\n", exe_path);
}

#endif /* __APPLE__ */
