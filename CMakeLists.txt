cmake_minimum_required(VERSION 3.21)  # RUNTIME_DEPENDENCY_SET
project(d2-ds1-edit VERSION 2.0.0 LANGUAGES C)

# Pass version to source code
add_compile_definitions(
    DS1EDIT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    DS1EDIT_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    DS1EDIT_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    DS1EDIT_VERSION_STR="${PROJECT_VERSION}"
)

# Options
option(USE_SOFTWARE_RENDERER "Force software rendering (no GPU)" OFF)
option(DS1EDIT_PERF_LOG "Enable per-frame perf logging to stderr and perf_log.csv" OFF)

# Find Allegro 5.
# Windows: vcpkg (toolchain set via CMakePresets.json or -DCMAKE_TOOLCHAIN_FILE).
# Linux/macOS: pkg-config against the distro / Homebrew packages, falling back
# to a bare find_library sweep if the .pc files are missing.
if(NOT WIN32)
    find_package(PkgConfig QUIET)
endif()

if(PKG_CONFIG_FOUND AND NOT WIN32)
    pkg_check_modules(ALLEGRO5 QUIET allegro-5)
    pkg_check_modules(ALLEGRO5_IMAGE QUIET allegro_image-5)
    pkg_check_modules(ALLEGRO5_FONT QUIET allegro_font-5)
    pkg_check_modules(ALLEGRO5_PRIMITIVES QUIET allegro_primitives-5)
    pkg_check_modules(ALLEGRO5_DIALOG QUIET allegro_dialog-5)
    pkg_check_modules(ALLEGRO5_MAIN QUIET allegro_main-5)
endif()

if(ALLEGRO5_FOUND)
    # pkg-config path: carry its -I / -L / -l sets straight through.
    set(ALLEGRO5_INCLUDE_DIR ${ALLEGRO5_INCLUDE_DIRS})
else()
    find_path(ALLEGRO5_INCLUDE_DIR allegro5/allegro.h)
    find_library(ALLEGRO5_LIB allegro)
    find_library(ALLEGRO5_IMAGE_LIB allegro_image)
    find_library(ALLEGRO5_FONT_LIB allegro_font)
    find_library(ALLEGRO5_PRIMITIVES_LIB allegro_primitives)
    find_library(ALLEGRO5_DIALOG_LIB allegro_dialog)
    # allegro_main exists on Windows and macOS but not on Linux.
    find_library(ALLEGRO5_MAIN_LIB allegro_main)

    if(NOT ALLEGRO5_INCLUDE_DIR OR NOT ALLEGRO5_LIB)
        if(WIN32)
            message(FATAL_ERROR
                "Allegro 5 not found. Install via vcpkg, then configure with:\n"
                "  cmake --preset default")
        elseif(APPLE)
            message(FATAL_ERROR
                "Allegro 5 not found. Install via Homebrew:\n"
                "  brew install allegro pkg-config\n"
                "Then configure with:  cmake --preset macos")
        else()
            message(FATAL_ERROR
                "Allegro 5 not found. On Debian/Ubuntu:\n"
                "  sudo apt install liballegro5-dev liballegro-image5-dev "
                "liballegro-ttf5-dev liballegro-dialog5-dev zlib1g-dev\n"
                "Then configure with:  cmake --preset linux")
        endif()
    endif()
endif()

# Allegro's "magic main" on macOS: allegro5/platform/alosx.h rewrites our
# main() to _al_mangled_main and liballegro_main supplies the real main(),
# which then calls back into ours. That callback is resolved through the flat
# namespace, and macOS has used a two-level namespace since 10.1 -- so the
# dylib cannot see the executable's symbol and startup dies with
#   dyld: symbol not found in flat namespace '__al_mangled_main'
#
# Turning the rename off and dropping the wrapper keeps a single ordinary
# main(), which is what every other platform here already does. This has to be
# a directory-scope definition so the test targets agree with the main binary:
# a TU that mangles while the link supplies no wrapper fails with "_main"
# undefined.
if(APPLE)
    add_compile_definitions(ALLEGRO_NO_MAGIC_MAIN)
    set(ALLEGRO5_MAIN_LIBRARIES "")
    set(ALLEGRO5_MAIN_LIB "")
endif()

# Normalised Allegro handle. Discovery populates two different variable
# families (pkg-config's *_LIBRARIES vs find_library's *_LIB), and the test
# targets need the same set the main binary gets, so wrap it once here.
add_library(ds1edit_allegro INTERFACE)
if(ALLEGRO5_FOUND)
    target_include_directories(ds1edit_allegro INTERFACE
        ${ALLEGRO5_INCLUDE_DIRS}
        ${ALLEGRO5_IMAGE_INCLUDE_DIRS}
        ${ALLEGRO5_FONT_INCLUDE_DIRS}
        ${ALLEGRO5_PRIMITIVES_INCLUDE_DIRS}
        ${ALLEGRO5_DIALOG_INCLUDE_DIRS})
    target_link_directories(ds1edit_allegro INTERFACE
        ${ALLEGRO5_LIBRARY_DIRS}
        ${ALLEGRO5_IMAGE_LIBRARY_DIRS}
        ${ALLEGRO5_FONT_LIBRARY_DIRS}
        ${ALLEGRO5_PRIMITIVES_LIBRARY_DIRS}
        ${ALLEGRO5_DIALOG_LIBRARY_DIRS})
    target_link_libraries(ds1edit_allegro INTERFACE
        ${ALLEGRO5_LIBRARIES}
        ${ALLEGRO5_IMAGE_LIBRARIES}
        ${ALLEGRO5_FONT_LIBRARIES}
        ${ALLEGRO5_PRIMITIVES_LIBRARIES}
        ${ALLEGRO5_DIALOG_LIBRARIES}
        ${ALLEGRO5_MAIN_LIBRARIES})
else()
    target_include_directories(ds1edit_allegro INTERFACE ${ALLEGRO5_INCLUDE_DIR})
    target_link_libraries(ds1edit_allegro INTERFACE
        ${ALLEGRO5_LIB}
        ${ALLEGRO5_IMAGE_LIB}
        ${ALLEGRO5_FONT_LIB}
        ${ALLEGRO5_PRIMITIVES_LIB}
        ${ALLEGRO5_DIALOG_LIB})
    if(ALLEGRO5_MAIN_LIB)
        target_link_libraries(ds1edit_allegro INTERFACE ${ALLEGRO5_MAIN_LIB})
    endif()
endif()

# Source files
set(SOURCES
    src/main.c
    src/globals.c
    src/config.c
    src/error.c
    src/misc.c
    src/platform.c
    # cli/
    src/cli/cli.c
    # core/
    src/core/apng_writer.c
    src/core/compose_apng.c
    src/core/compose_bbox.c
    src/core/compose_blit.c
    src/core/compose_cof.c
    src/core/compose_cof_path.c
    src/core/compose_dcc_path.c
    src/core/compose_index.c
    src/core/compose_index_build.c
    src/core/compose_iter.c
    src/core/compose_iter_config.c
    src/core/compose_iter_probe.c
    src/core/compose_naming.c
    src/core/compose_palette.c
    src/core/compose_palette_index.c
    src/core/compose_presets.c
    src/core/compose_render.c
    src/core/compose_scale.c
    src/core/monstats2.c
    src/core/monstats2_build.c
    src/core/area_browser.c
    src/core/area_metadata.c
    src/core/d2install.c
    src/core/ds1.c
    src/core/ds1_manager.c
    src/core/asset_export.c
    src/core/upscale.c
    src/core/dt1.c
    src/core/dt1_draw.c
    src/core/dt1_decode.c
    src/core/cof.c
    src/core/animdata.c
    src/core/animdata_math.c
    src/core/dcc.c
    src/core/dc6.c
    src/core/dc6_header.c
    src/core/export_common.c
    src/core/export_presets.c
    src/core/export_progress.c
    src/core/glob_match.c
    src/core/mpq_index.c
    src/core/mpq_index_query.c
    src/core/palette.c
    src/core/preferences.c
    src/core/project.c
    src/core/rgba_cache.c
    src/core/txtread.c
    # render/
    src/render/preview.c
    src/render/gfx.c
    # editor/
    src/editor/tiles.c
    src/editor/objects.c
    src/editor/paths.c
    src/editor/undo.c
    # ui/
    src/ui/interface.c
    src/ui/dialogs.c
    src/ui/edit_window.c
    src/ui/bits_window.c
    src/ui/props_panel.c
    src/ui/tile_picker.c
    src/ui/project_menu.c
    src/ui/export_type_picker.c
    src/ui/upscale_mode_picker.c
    src/ui/compose_category_picker.c
    src/ui/compose_mode_modal.c
    src/ui/compose_preset_picker.c
    src/ui/multi_select_picker.c
    src/ui/preset_picker.c
    src/ui/scope_picker.c
    src/ui/text_input_modal.c
    src/ui/win_folder_picker.c
    # mpq/
    src/mpq/Dcl_tbl.c
    src/mpq/Explode.c
    src/mpq/MpqView.c
    src/mpq/Wav_unp.c
)

# zlib (for the APNG writer's compress2/crc32; transitively a dep of
# Allegro 5's image addon, but we want it as a direct dep so calls to
# core/apng_writer.c link cleanly).
find_package(ZLIB REQUIRED)

# Main executable
add_executable(ds1edit ${SOURCES})

target_include_directories(ds1edit PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/mpq
    ${ALLEGRO5_INCLUDE_DIR}
)

# Platform-specific configuration
if(WIN32)
    target_compile_definitions(ds1edit PRIVATE WIN32 USE_CONSOLE)

    target_link_libraries(ds1edit PRIVATE
        ${ALLEGRO5_LIB}
        ${ALLEGRO5_IMAGE_LIB}
        ${ALLEGRO5_FONT_LIB}
        ${ALLEGRO5_PRIMITIVES_LIB}
        ${ALLEGRO5_MAIN_LIB}
        ${ALLEGRO5_DIALOG_LIB}
        ZLIB::ZLIB
        user32 gdi32 ole32 winmm shlwapi advapi32 comctl32 comdlg32 shell32 uuid
    )

    # Whole-program optimization for Release builds
    target_compile_options(ds1edit PRIVATE
        $<$<CONFIG:Release>:/GL>
        $<$<CONFIG:RelWithDebInfo>:/GL>
    )
    target_link_options(ds1edit PRIVATE
        $<$<CONFIG:Release>:/LTCG>
        $<$<CONFIG:RelWithDebInfo>:/LTCG>
    )
else()
    # POSIX. The WIN32 define is deliberately absent -- it is what every
    # #ifdef WIN32 in src/ keys off, so leaving it unset selects the POSIX
    # branches throughout.
    target_compile_definitions(ds1edit PRIVATE USE_CONSOLE _GNU_SOURCE)

    if(ALLEGRO5_FOUND)
        # pkg-config gave us full -l / -L sets for each addon.
        target_include_directories(ds1edit PRIVATE
            ${ALLEGRO5_INCLUDE_DIRS}
            ${ALLEGRO5_IMAGE_INCLUDE_DIRS}
            ${ALLEGRO5_FONT_INCLUDE_DIRS}
            ${ALLEGRO5_PRIMITIVES_INCLUDE_DIRS}
            ${ALLEGRO5_DIALOG_INCLUDE_DIRS}
        )
        target_link_directories(ds1edit PRIVATE
            ${ALLEGRO5_LIBRARY_DIRS}
            ${ALLEGRO5_IMAGE_LIBRARY_DIRS}
            ${ALLEGRO5_FONT_LIBRARY_DIRS}
            ${ALLEGRO5_PRIMITIVES_LIBRARY_DIRS}
            ${ALLEGRO5_DIALOG_LIBRARY_DIRS}
        )
        target_link_libraries(ds1edit PRIVATE
            ${ALLEGRO5_LIBRARIES}
            ${ALLEGRO5_IMAGE_LIBRARIES}
            ${ALLEGRO5_FONT_LIBRARIES}
            ${ALLEGRO5_PRIMITIVES_LIBRARIES}
            ${ALLEGRO5_DIALOG_LIBRARIES}
            ${ALLEGRO5_MAIN_LIBRARIES}
            ZLIB::ZLIB
            m
        )
    else()
        target_link_libraries(ds1edit PRIVATE
            ${ALLEGRO5_LIB}
            ${ALLEGRO5_IMAGE_LIB}
            ${ALLEGRO5_FONT_LIB}
            ${ALLEGRO5_PRIMITIVES_LIB}
            ${ALLEGRO5_DIALOG_LIB}
            ZLIB::ZLIB
            m
        )
        # Present on macOS, absent on Linux -- link only if it was found.
        if(ALLEGRO5_MAIN_LIB)
            target_link_libraries(ds1edit PRIVATE ${ALLEGRO5_MAIN_LIB})
        endif()
    endif()

    target_compile_options(ds1edit PRIVATE
        -Wall
        # The codebase predates a lot of -W pedantry; keep the build readable
        # rather than drowning the real errors.
        -Wno-unused-but-set-variable
        -Wno-unused-result
        -Wno-format-truncation
    )
endif()

# Debug/Release definitions
target_compile_definitions(ds1edit PRIVATE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
    $<$<CONFIG:RelWithDebInfo>:NDEBUG>
)

if(USE_SOFTWARE_RENDERER)
    target_compile_definitions(ds1edit PRIVATE DS1EDIT_SOFTWARE_RENDERER)
endif()

if(DS1EDIT_PERF_LOG)
    target_compile_definitions(ds1edit PRIVATE DS1EDIT_PERF_LOG)
endif()

# Output to bin/ directory
set_target_properties(ds1edit PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_SOURCE_DIR}/bin
    OUTPUT_NAME "ds1edit"
)

# Clean bin/ target — removes everything except Ds1edit.ini (user config)
add_custom_target(clean-bin
    COMMAND ${CMAKE_COMMAND} -E echo "Cleaning bin/ (preserving Ds1edit.ini)..."
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/cmake/clean-bin.cmake"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Removing bin/ contents except Ds1edit.ini"
)

# Copy runtime data files to bin/ at build time
add_custom_command(TARGET ds1edit POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/data" "${CMAKE_SOURCE_DIR}/bin/data"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets" "${CMAKE_SOURCE_DIR}/bin/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/examples" "${CMAKE_SOURCE_DIR}/bin/examples"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Ds1edit.ini.sample" "${CMAKE_SOURCE_DIR}/bin/Ds1edit.ini.sample"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/allegro5.cfg" "${CMAKE_SOURCE_DIR}/bin/allegro5.cfg"
    COMMAND ${CMAKE_COMMAND}
        -DSRC=${CMAKE_SOURCE_DIR}/Ds1edit.ini.sample
        -DDST=${CMAKE_SOURCE_DIR}/bin/Ds1edit.ini
        -P ${CMAKE_SOURCE_DIR}/cmake/copy-if-missing.cmake
    COMMENT "Copying data, assets, and examples to bin/"
)

# Copy required DLLs to output directory (vcpkg puts them next to the libs)
if(WIN32)
    get_filename_component(ALLEGRO5_BIN_DIR "${ALLEGRO5_LIB}" DIRECTORY)
    get_filename_component(ALLEGRO5_BIN_DIR "${ALLEGRO5_BIN_DIR}/../bin" ABSOLUTE)
    if(EXISTS "${ALLEGRO5_BIN_DIR}")
        file(GLOB ALLEGRO5_DLLS "${ALLEGRO5_BIN_DIR}/*.dll")
        foreach(DLL ${ALLEGRO5_DLLS})
            get_filename_component(DLL_NAME ${DLL} NAME)
            configure_file(${DLL} ${CMAKE_SOURCE_DIR}/bin/${DLL_NAME} COPYONLY)
        endforeach()
    endif()
endif()

# Unit tests
enable_testing()
add_subdirectory(test)

# ---- Packaging (CPack) ----
set(CPACK_PACKAGE_NAME "DS1Edit")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Diablo II DS1 Map Editor")
set(CPACK_PACKAGE_VENDOR "bethington")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
if(WIN32)
    set(CPACK_GENERATOR "ZIP")
    set(CPACK_SYSTEM_NAME "win32")
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_SYSTEM_NAME "macos-${CMAKE_SYSTEM_PROCESSOR}")
else()
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_SYSTEM_NAME "linux-${CMAKE_SYSTEM_PROCESSOR}")
endif()

# Install rules for packaging
if(NOT WIN32)
    install(TARGETS ds1edit RUNTIME DESTINATION .)
endif()
# data/ minus the game-derived and generated caches. pal*.bin are verbatim
# Blizzard palettes the editor extracts from the user's own MPQs at runtime,
# cmap*.bin are computed from them, and gamma.dat is now generated in code --
# none of the three belong in a redistributable package.
install(DIRECTORY ${CMAKE_SOURCE_DIR}/data/
    DESTINATION data
    PATTERN "pal*.bin"  EXCLUDE
    PATTERN "cmap*.bin" EXCLUDE
    PATTERN "gamma.dat" EXCLUDE
    PATTERN "Tmp"       EXCLUDE
)
# Area configs plus the editor's own interface images (assets/ui/). The .ini
# filter used to be the whole rule, back when the UI images lived in pcx/ and
# had an install rule of their own.
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/
    DESTINATION assets
    FILES_MATCHING
        PATTERN "*.ini"
        PATTERN "*.png"
)
install(FILES ${CMAKE_SOURCE_DIR}/Ds1edit.ini.sample DESTINATION .)
install(FILES ${CMAKE_SOURCE_DIR}/LICENSE DESTINATION .)
install(FILES ${CMAKE_SOURCE_DIR}/NOTICE DESTINATION .)
install(FILES ${CMAKE_SOURCE_DIR}/README.md DESTINATION .)

# Install the runtime DLLs.
#
# History: this was a hand-maintained list plus a few globs, which silently
# missed vcpkg transitives and shipped a package that would not start. The fix
# for that globbed *.dll instead, which worked but dragged in 32 libraries the
# editor never loads -- FLAC, OpenAL, Theora, Vorbis and friends -- roughly
# 7 MB of dead weight.
#
# Let CMake walk the actual import tables instead, so the package carries the
# real closure and stays correct when the dependency set changes.
if(WIN32)
    # RUNTIME_DEPENDENCY_SET has to precede the artifact options.
    install(TARGETS ds1edit
        RUNTIME_DEPENDENCY_SET ds1edit_deps
        RUNTIME DESTINATION .)

    install(RUNTIME_DEPENDENCY_SET ds1edit_deps
        DIRECTORIES "${ALLEGRO5_BIN_DIR}"
        PRE_EXCLUDE_REGEXES  "api-ms-win-.*" "ext-ms-.*"
        POST_EXCLUDE_REGEXES ".*[Ss]ystem32.*" ".*[Ww]indows.*"
        DESTINATION .)
endif()

include(CPack)
