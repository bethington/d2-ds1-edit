/* mac_folder_picker.m -- the folder picker the editor uses on macOS.
 *
 * Allegro's folder chooser is an NSOpenPanel titled "Open" with an "Open"
 * button, no New Folder button, and no room for the editor's title, which macOS
 * doesn't draw on panels. New Project told the user to click "New folder", a
 * button that wasn't there, so a project needed a folder made beforehand in
 * Finder. This panel can make folders, shows the editor's text as its message
 * and says Select, which is what that text names.
 *
 * Allegro runs the editor on its own thread, and AppKit panels belong to the
 * main thread, so the panel is run there and this call waits for it.
 */
#import <Cocoa/Cocoa.h>
#include <string.h>
#include "mac_folder_picker.h"

int ds1_mac_pick_folder(const char *message, const char *initial,
                        char *out, int out_cap)
{
   __block int picked = 0;
   dispatch_block_t run = ^{
      @autoreleasepool
      {
         NSOpenPanel *panel = [NSOpenPanel openPanel];
         const char *path;

         panel.canChooseFiles = NO;
         panel.canChooseDirectories = YES;
         panel.canCreateDirectories = YES;
         panel.allowsMultipleSelection = NO;
         panel.prompt = @"Select";
         if (message != NULL && message[0] != '\0')
            panel.message = [NSString stringWithUTF8String:message];
         if (initial != NULL && initial[0] != '\0')
            panel.directoryURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:initial]
                                            isDirectory:YES];

         if ([panel runModal] != NSModalResponseOK || panel.URL == nil)
            return;
         path = panel.URL.fileSystemRepresentation;
         if (path == NULL || out == NULL || (int) strlen(path) >= out_cap)
            return;
         memcpy(out, path, strlen(path) + 1);
         picked = 1;
      }
   };

   if ([NSThread isMainThread])
      run();
   else
      dispatch_sync(dispatch_get_main_queue(), run);
   return picked;
}
