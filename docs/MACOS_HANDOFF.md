# macOS Port and Startup Debugging Handoff

## Status

`ds1edit` now builds and launches successfully on Apple Silicon macOS using Homebrew Allegro 5.2.11. The application reads the configured Diablo II MPQ directory, opens all four archives, extracts the palettes and text tables, builds the preset index, and remains running with its editor window open.

The final verified executable is:

```text
/Users/dylanknuth/src/d2-ds1-edit/bin/ds1edit
```

The configured MPQ directory used during verification is:

```text
/Users/dylanknuth/d2_english_mpq
```

The successful launch loaded 1,092 presets and built 35 area-browser groups. The process remained alive after startup instead of exiting with `SIGTRAP`, rejecting the MPQs, or crashing during palette extraction.

## Original problem

The project was originally developed for Windows. On macOS, launching from the `bin` directory initially failed with:

```text
No Mod Directory and no MPQ files available : it can't work.
```

The MPQ location was configured in `ds1edit.ini`, but the program did not honor it correctly. This was caused by Windows-oriented path assumptions, including separator handling, path joining, configuration lookup behavior, and assumptions about where the process was launched.

After the path handling was corrected, startup advanced far enough to print the configured archives:

```text
d2char   = /Users/dylanknuth/d2_english_mpq/d2char.mpq
d2data   = /Users/dylanknuth/d2_english_mpq/d2data.mpq
d2exp    = /Users/dylanknuth/d2_english_mpq/d2exp.mpq
patch_d2 = /Users/dylanknuth/d2_english_mpq/patch_d2.mpq
```

However, the process then terminated immediately after:

```text
building gamma correction tables
zsh: trace trap ./ds1edit
```

That visible line was initially misleading. Gamma generation was not the cause; it was simply the last flushed output before the actual crash.

## Work completed, in chronological order

### 1. Made configuration and paths portable

The first set of changes removed Windows-only assumptions from configuration and resource path handling.

The relevant working-tree files from this pass include:

```text
CMakeLists.txt
src/config.c
src/core/area_browser.c
src/core/compose_apng.c
src/core/compose_iter.c
src/core/d2install.c
src/core/ds1_manager.c
src/main.c
src/mpq/MpqView.c
src/ui/project_menu.c
```

The important behavioral changes were:

- MPQ and mod-directory values read from configuration are normalized for the current platform.
- Paths are joined using the platform separator instead of embedding Windows backslashes in filesystem paths.
- Windows-style paths remain usable as Diablo II archive member names where backslashes are part of the MPQ's internal naming convention.
- The editor can locate `ds1edit.ini` correctly when launched from `bin` on a case-sensitive or case-preserving macOS filesystem.
- The configured Diablo II installation directory can be seeded from editor preferences and then used to populate the four expected MPQ slots.
- MPQ filenames are resolved as `patch_d2.mpq`, `d2exp.mpq`, `d2data.mpq`, and `d2char.mpq` beneath the configured installation directory.
- Resource, project, area-browser, composition, and temporary paths use the shared cross-platform path helpers instead of manually appending Windows separators.

The resulting startup diagnostics confirmed that configuration was now being respected:

```text
d2install: seeded from preferences </Users/dylanknuth/d2_english_mpq>
d2install: using configured path </Users/dylanknuth/d2_english_mpq>
d2install: slot 0 -> /Users/dylanknuth/d2_english_mpq/patch_d2.mpq
d2install: slot 1 -> /Users/dylanknuth/d2_english_mpq/d2exp.mpq
d2install: slot 2 -> /Users/dylanknuth/d2_english_mpq/d2data.mpq
d2install: slot 3 -> /Users/dylanknuth/d2_english_mpq/d2char.mpq
```

At this point, the original "No Mod Directory and no MPQ files available" error was resolved.

### 2. Fixed a macOS compile failure in APNG composition

`src/core/compose_apng.c` used a path-separator macro as though it were a compile-time string literal. That worked with the old Windows assumptions but did not compile with the cross-platform separator definition.

The fix was to include the platform path declarations and pass the separator as a normal `%s` formatting argument when constructing frame filenames. This preserved the intended output naming while allowing the source to compile on macOS.

After this change, the main CMake build completed. The compiler still emitted warnings from legacy code, but there were no build-stopping errors.

### 3. Added display creation fallback behavior

The display initialization in `src/main.c` was consolidated into a helper that attempts sensible alternatives:

1. The configured fullscreen mode is attempted first when fullscreen is requested.
2. A normal resizable window is attempted if the preferred mode fails.
3. A fixed-size window is attempted as a final fallback.

This improved failure behavior for macOS display configurations, although it did not resolve the `trace trap`. The process was crashing inside the first call to Allegro's display backend before any fallback could return.

### 4. Proved gamma generation was not crashing

Because the terminal always stopped after `building gamma correction tables`, temporary progress diagnostics were added to `misc_read_gamma()`.

The diagnostics printed:

- The number and total size of gamma tables.
- The start of each gamma curve.
- Progress at selected values in every 256-byte table.
- A final completion marker.

All 41 curves, numbered 0 through 40, completed successfully. A separate minimal C program also verified that the same `pow()` calculation worked on the machine.

The temporary gamma diagnostics were removed after they established that the crash happened later in startup. The normal single `building gamma correction tables` message remains.

### 5. Captured the macOS `SIGTRAP` with LLDB

AddressSanitizer did not report a memory violation, because the termination was a deliberate libdispatch breakpoint rather than heap corruption. An initial LLDB attempt inside the restricted environment could not retain the process. LLDB was then run with permission to control the launched GUI process.

The useful backtrace was:

```text
stop reason = EXC_BREAKPOINT
frame #0: libdispatch.dylib`__DISPATCH_WAIT_FOR_QUEUE__
frame #1: libdispatch.dylib`_dispatch_sync_f_slow
frame #2: liballegro.5.2.dylib`create_display_win
frame #3: liballegro.5.2.dylib`al_create_display
frame #4: ds1edit`ds1edit_try_create_display
frame #5: ds1edit`main
```

This established the exact root cause: Allegro's macOS backend was synchronously dispatching display creation to the Cocoa main queue while `ds1edit` itself was already executing on that queue. macOS detects that self-deadlock and deliberately executes a breakpoint instruction, which zsh reports as `trace trap`.

The build defines both `USE_CONSOLE` and `ALLEGRO_NO_MAGIC_MAIN`. On macOS, disabling Allegro's normal entry-point handling means a traditional `main()` runs on the Cocoa main thread. Allegro expects to own that thread and run the application's actual callback on a worker thread.

### 6. Fixed the Allegro macOS entry point

`src/main.c` now uses a macOS-specific wrapper:

```c
#ifdef __APPLE__
static int ds1edit_main(int argc, char *argv[]);

int main(int argc, char *argv[])
{
   return al_run_main(argc, argv, ds1edit_main);
}

static int ds1edit_main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
   /* existing application body */
}
```

This approach was chosen instead of removing the global `ALLEGRO_NO_MAGIC_MAIN` definition because the same compile configuration is also used by test executables with their own `main()` functions. Moving the definition selectively among every application and test target would have been broader and easier to regress.

`al_run_main()` gives Allegro control of the macOS application thread and invokes `ds1edit_main()` in the execution context expected by the Cocoa backend. Windows and other platforms retain the original entry-point behavior.

After this change, display creation succeeded and startup advanced to opening the archives.

### 7. Diagnosed the "not a valid MPQ archive" error

Once the display trap was fixed, the next launch failed with:

```text
Error: File '/Users/dylanknuth/d2_english_mpq/patch_d2.mpq' is not a valid MPQ archive
```

The files were inspected directly before changing the MPQ reader. They were confirmed to be regular MPQ archives rather than directories, aliases, empty files, or partial placeholders.

Observed file information:

```text
patch_d2.mpq  3,915,305 bytes
d2exp.mpq     250,156,780 bytes
d2data.mpq    267,642,202 bytes
d2char.mpq    263,369,609 bytes
```

The first archive began with the expected bytes:

```text
4d 50 51 1a 20 00 00 00
```

The system `file` utility identified all four files as MoPaQ archives. This proved that the failure was in the bundled reader, not the configured data.

### 8. Corrected Windows integer-width assumptions in the MPQ reader

The bundled MPQ implementation is legacy "Stormless MPQ" code dating to 2000. Its `src/mpq/mpqtypes.h` defined archive types like this:

```c
#define UInt32 unsigned long
#define SInt32 long
#define DWORD unsigned long
```

That assumes the Windows data model, where `long` is 32-bit. Modern 64-bit macOS uses the LP64 data model, where `long` is 64-bit.

This broke the reader immediately. `test_prepare_archive()` used `sizeof(DWORD)` while scanning for the MPQ header, so it read eight bytes at a time while comparing against four-byte constants. Even valid archives could not match correctly.

`src/mpq/mpqtypes.h` now includes `<stdint.h>` and defines the archive types explicitly:

```c
typedef uint8_t  UInt8;
typedef uint16_t UInt16;
typedef int16_t  SInt16;
typedef uint32_t UInt32;
typedef int32_t  SInt32;
typedef uint32_t DWORD;
```

This is the correct model for the on-disk MPQ format and preserves the original Windows sizes on every host platform.

After rebuilding, all four archives opened successfully:

```text
opening mpq 0 : /Users/dylanknuth/d2_english_mpq/patch_d2.mpq
opening mpq 1 : /Users/dylanknuth/d2_english_mpq/d2exp.mpq
opening mpq 2 : /Users/dylanknuth/d2_english_mpq/d2data.mpq
opening mpq 3 : /Users/dylanknuth/d2_english_mpq/d2char.mpq
```

### 9. Diagnosed the palette-loading segmentation fault

With archive headers and tables decoded correctly, startup reached:

```text
loading palettes
```

It then terminated with exit status 139, indicating `SIGSEGV`.

The LLDB backtrace was:

```text
stop reason = EXC_BAD_ACCESS
frame #0: ds1edit`explode
frame #1: ds1edit`ExtractToMem
frame #2: ds1edit`mpq_batch_load_in_mem
frame #3: ds1edit`misc_load_mpq_file
frame #4: ds1edit`ds1edit_load_palettes
frame #5: ds1edit`ds1edit_main
```

The fault occurred while extracting:

```text
Data\Global\Palette\Act1\Pal.pl2
```

The palette data uses PKWARE DCL compression, so startup entered the bundled `explode` implementation in `src/mpq/Explode.c`.

### 10. Removed pointer truncation from PKWARE decompression

The legacy `explode` implementation contained two independent 32-bit pointer assumptions.

First, it stored function pointers and the callback context inside four-byte slots in its work buffer:

```c
*((UInt32 *) (work_buff + 0x16)) = (UInt32) read_data;
*((UInt32 *) (work_buff + 0x1A)) = (UInt32) write_data;
*((void **) (work_buff + 0x12)) = param;
```

This cannot represent 64-bit function or data pointers. Expanding those fields in place was not safe because their historical offsets overlap when pointers are eight bytes wide.

The callbacks and context are now held as native file-static pointers:

```c
static read_data_proc  *explode_read_data;
static write_data_proc *explode_write_data;
static void            *explode_param;
```

The decompressor assigns them at the beginning of `explode()` and uses them directly when it needs more input or emits decompressed output. The decompression path is already serialized by the editor's global MPQ state, so this preserves the existing non-reentrant behavior without truncation.

Second, the dictionary-copy code converted a live destination pointer to `UInt32`, subtracted a distance, and converted it back to a pointer:

```c
s = (UInt8 *)((UInt32)d - (UInt32)s);
```

On Apple Silicon this discarded the high 32 bits of the address, producing the invalid address reported by LLDB.

The distance is now kept as a 16-bit numeric value returned by `__explode_3()`, and the source pointer is calculated with native pointer arithmetic:

```c
distance = __explode_3(buf, (UInt16) result);
s = d - distance;
```

This fixed palette decompression without changing the DCL stream format or dictionary behavior.

### 11. Rebuilt and verified the complete startup path

The final build command was:

```sh
cd /Users/dylanknuth/src/d2-ds1-edit
cmake --build build -j 4
```

The final launch command was:

```sh
cd /Users/dylanknuth/src/d2-ds1-edit/bin
./ds1edit
```

The process remained running after the verification timeout, confirming that it had reached the normal editor event loop.

The successful output included all five palettes:

```text
Data\Global\Palette\Act1\Pal.pl2 ... ok
Data\Global\Palette\Act2\Pal.pl2 ... ok
Data\Global\Palette\Act3\Pal.pl2 ... ok
Data\Global\Palette\Act4\Pal.pl2 ... ok
Data\Global\Palette\Act5\Pal.pl2 ... ok
```

It then loaded the core text tables:

```text
Data\Global\Excel\Objects.txt
Data\Global\Excel\LvlPrest.txt
Data\Global\Excel\LvlTypes.txt
Data\Global\Excel\Levels.txt
```

The final indexing output was:

```text
mpq_index: built 1092 presets
[area browser] 35 groups built
[area browser] 616 map(s) present in the archives but named by no table,
filed under "Unlisted"
```

## Files most directly responsible for the successful macOS launch

### `src/main.c`

- Locates configuration with macOS-compatible path handling.
- Uses portable joins for runtime assets and files.
- Adds display-mode fallbacks.
- Routes the macOS executable through `al_run_main()`.

### `src/config.c`

- Normalizes configured MPQ and mod-directory paths for the host platform.
- Preserves valid absolute macOS paths read from the INI file.

### `src/core/d2install.c`

- Uses the configured or preference-seeded Diablo II directory.
- Resolves the four expected archive slots beneath that directory.
- Emits diagnostics showing exactly which directory and archive path are selected.

### `src/mpq/mpqtypes.h`

- Replaces Windows-size assumptions with fixed-width integer types.
- Ensures MPQ headers, offsets, hash entries, block entries, and CRC values remain 32-bit.

### `src/mpq/Explode.c`

- Stores callbacks as native pointers instead of four-byte integers.
- Uses native pointer arithmetic for dictionary copies.
- Allows PKWARE-compressed palette resources to extract on Apple Silicon.

### `src/mpq/MpqView.c`

- Participates in the cross-platform filesystem path work.
- Uses the corrected fixed-width MPQ types for header parsing, table decoding, and extraction.

### `src/core/compose_apng.c`

- Constructs output frame paths with the platform separator as a formatting argument.
- Fixes the macOS build failure caused by treating the separator as a literal macro.

### Other path-related files

The area browser, composition iterator, DS1 manager, and project menu were updated as part of the same path-portability pass so that features reached after startup do not recreate Windows-only paths.

## Build notes

The build succeeds, but it still prints legacy warnings. These warnings were not startup blockers and were not broadly cleaned up as part of this focused repair.

Notable warning categories include:

- `ALLEGRO_NO_MAGIC_MAIN` is defined both by the project and indirectly through `USE_CONSOLE`.
- Several old `printf` format strings use `%li` with values that are currently `int`.
- Some functions pass `char **` where an API declares `void **`.
- There are unused local variables in legacy paths.
- `src/config.c` contains an integer cast from a stored `void *` default value.
- Some rendering code still mixes `long *` with `int32_t *`.
- The old WAVE decompression paths still contain casts from pointers to `UInt32`.

The final two categories deserve attention during future 64-bit work. They were not exercised by the verified startup path, but they may fail when using features or archive resources that reach those code paths.

## Remaining technical risks

### WAVE decompression pointer casts

`src/mpq/MpqView.c` still emits warnings in calls such as `ExtWavUnp1`, `ExtWavUnp2`, and `ExtWavUnp3`, where native pointers are cast to `UInt32`. This is the same general class of problem that caused the DCL palette crash.

Startup does not use those routines, so they were left outside the immediate fix. If ds1edit extracts WAVE-compressed MPQ members, those functions should be ported to accept pointers or `uintptr_t` rather than 32-bit integers.

### Other uses of `long`

Any structure that maps directly onto Diablo II binary data should use `int32_t` or `uint32_t`, not `long`, because `long` differs between Win64 and macOS. Compiler warnings involving `long *` and `int32_t *` should be treated as potential binary-layout defects, not merely cosmetic warnings.

### Global MPQ and DCL state

The MPQ reader and decompressor are global and non-reentrant. The callback fix preserves that model. If archive extraction is later moved onto multiple threads, the DCL callbacks and context should become an explicit per-call context structure rather than file-static state.

### OpenGL capability messages

Under LLDB, Allegro printed many messages about OpenGL symbols unavailable in Apple's deprecated OpenGL framework. These were capability probes, not the cause of the crash. The editor still created a usable display and completed startup.

### Shell `.gitconfig` lock messages

Some terminal commands in the restricted development environment printed:

```text
error: could not lock config file /Users/dylanknuth/.gitconfig: Operation not permitted
```

These messages came from the shell or environment setup around commands. They were unrelated to ds1edit, MPQ parsing, Allegro, or the application launch.

## Diagnostic attempts and lessons

An AddressSanitizer build was created with:

```sh
cmake -S . -B build-asan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS='-g -O1 -fsanitize=address' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address'
cmake --build build-asan -j 4
```

ASan did not report the original `SIGTRAP` because libdispatch intentionally trapped a queue self-deadlock. LLDB was required to identify that failure.

The project sets a shared runtime output directory, so the ASan build also wrote its executable to `bin/ds1edit`, temporarily replacing the normal build's binary. Rebuilding the regular `build` directory restored the normal optimized executable. Future diagnostic build directories should use a distinct runtime output location if both binaries need to coexist.

The debugging sequence demonstrated that each newly visible failure was a separate issue:

```text
INI/path failure
    -> configuration and path normalization
SIGTRAP after gamma message
    -> Allegro/Cocoa main-thread deadlock
"not a valid MPQ archive"
    -> 64-bit DWORD definition
SIGSEGV while loading palettes
    -> 64-bit pointer truncation in DCL explode
successful editor startup
```

## Reproduction and verification procedure

From a fresh shell:

```sh
cd /Users/dylanknuth/src/d2-ds1-edit
cmake -S . -B build
cmake --build build -j 4
cd bin
./ds1edit
```

Expected behavior:

- The INI file is found and loaded.
- The configured path `/Users/dylanknuth/d2_english_mpq` is printed.
- All four MPQ slots point to files beneath that directory.
- Gamma tables build without a trap.
- A window is created.
- All four MPQs open without an invalid-archive error.
- Five palettes are extracted successfully.
- `Objects.txt`, `LvlPrest.txt`, `LvlTypes.txt`, and `Levels.txt` load.
- The preset and area-browser indexes are built.
- The process remains alive in the editor event loop.

If a future regression stops at one of those checkpoints, use the chronology above to distinguish configuration, display-threading, archive-layout, and decompression failures.

## Repository state and handoff caution

The working tree contained pre-existing and port-related modifications while this work was performed. No destructive Git commands were used, unrelated changes were not reverted, and no commit was created as part of this debugging session.

Before committing, review and group the current changes deliberately. At minimum, keep the macOS entry-point fix, fixed-width MPQ types, and DCL pointer fixes together because they form the verified startup chain. The broader path-portability edits can be committed with them or separated into an earlier commit if the diff cleanly supports that history.
