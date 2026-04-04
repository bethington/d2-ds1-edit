cmake_minimum_required(VERSION 3.20)
project(d2-ds1-edit VERSION 1.0.0 LANGUAGES C)

# Pass version to source code
add_compile_definitions(
    DS1EDIT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    DS1EDIT_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    DS1EDIT_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    DS1EDIT_VERSION_STR="${PROJECT_VERSION}"
)

# Options
option(USE_SOFTWARE_RENDERER "Force software rendering (no GPU)" OFF)

# Find Allegro 5 (installed via vcpkg or system)
# vcpkg toolchain is set via CMakePresets.json or -DCMAKE_TOOLCHAIN_FILE
find_path(ALLEGRO5_INCLUDE_DIR allegro5/allegro.h)
find_library(ALLEGRO5_LIB allegro)
find_library(ALLEGRO5_IMAGE_LIB allegro_image)
find_library(ALLEGRO5_FONT_LIB allegro_font)
find_library(ALLEGRO5_PRIMITIVES_LIB allegro_primitives)
find_library(ALLEGRO5_MAIN_LIB allegro_main)

if(NOT ALLEGRO5_INCLUDE_DIR OR NOT ALLEGRO5_LIB)
    message(FATAL_ERROR
        "Allegro 5 not found. Install via vcpkg:\n"
        "  vcpkg install allegro5[core,image,font,primitives]:x86-windows\n"
        "Then configure with:\n"
        "  cmake --preset default")
endif()

# Source files
set(SOURCES
    Sources/a5_globals.c
    Sources/anim.c
    Sources/animdata.c
    Sources/dc6info.c
    Sources/dccinfo.c
    Sources/ds1misc.c
    Sources/ds1save.c
    Sources/dt1misc.c
    Sources/dt1_draw.c
    Sources/dt1_decode.c
    Sources/editobj.c
    Sources/editpath.c
    Sources/edittile.c
    Sources/error.c
    Sources/gfx_custom.c
    Sources/inicreat.c
    Sources/iniread.c
    Sources/interfac.c
    Sources/main.c
    Sources/misc.c
    Sources/msg_quit.c
    Sources/msg_save.c
    Sources/palette.c
    Sources/rgba_cache.c
    Sources/txtread.c
    Sources/undo.c
    Sources/wBits.c
    Sources/wEdit.c
    Sources/wMsg.c
    Sources/wPreview.c
    Sources/mpq/Dcl_tbl.c
    Sources/mpq/Explode.c
    Sources/mpq/MpqView.c
    Sources/mpq/Wav_unp.c
)

# Main executable
add_executable(ds1edit ${SOURCES})

target_include_directories(ds1edit PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Sources
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
        user32 gdi32 ole32 winmm shlwapi
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

# Output to bin/ directory
set_target_properties(ds1edit PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin
    OUTPUT_NAME "ds1edit"
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
set(CPACK_GENERATOR "ZIP")

# Install rules for packaging
install(TARGETS ds1edit RUNTIME DESTINATION .)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/bin/data/ DESTINATION data)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/bin/assets/
    DESTINATION assets
    FILES_MATCHING PATTERN "*.ini"
)
install(FILES ${CMAKE_SOURCE_DIR}/bin/Ds1edit.ini.sample DESTINATION .)
install(FILES ${CMAKE_SOURCE_DIR}/LICENSE DESTINATION .)

# Install required DLLs
if(WIN32 AND EXISTS "${ALLEGRO5_BIN_DIR}")
    # Only install the DLLs we actually need
    set(REQUIRED_DLLS
        allegro-5.2.dll
        allegro_image-5.2.dll
        allegro_font-5.2.dll
        allegro_primitives-5.2.dll
        allegro_main-5.2.dll
    )
    foreach(DLL_NAME ${REQUIRED_DLLS})
        if(EXISTS "${ALLEGRO5_BIN_DIR}/${DLL_NAME}")
            install(FILES "${ALLEGRO5_BIN_DIR}/${DLL_NAME}" DESTINATION .)
        endif()
    endforeach()
    # Image format dependencies
    file(GLOB IMAGE_DLLS
        "${ALLEGRO5_BIN_DIR}/libpng*.dll"
        "${ALLEGRO5_BIN_DIR}/zlib*.dll"
        "${ALLEGRO5_BIN_DIR}/libjpeg*.dll"
        "${ALLEGRO5_BIN_DIR}/libwebp*.dll"
        "${ALLEGRO5_BIN_DIR}/libsharpyuv*.dll"
        "${ALLEGRO5_BIN_DIR}/turbojpeg*.dll"
    )
    install(FILES ${IMAGE_DLLS} DESTINATION .)
endif()

include(CPack)
