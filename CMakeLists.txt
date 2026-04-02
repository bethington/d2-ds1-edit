cmake_minimum_required(VERSION 3.20)
project(d2-ds1-edit C)

# Options
option(USE_SOFTWARE_RENDERER "Force software rendering (no GPU)" OFF)

# vcpkg integration
set(VCPKG_ROOT "C:/vcpkg" CACHE PATH "vcpkg root directory")
if(EXISTS "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

# Find Allegro 5 packages (installed via vcpkg)
find_package(PkgConfig QUIET)

# Allegro 5 - find via vcpkg installed paths
set(ALLEGRO5_ROOT "${VCPKG_ROOT}/installed/x86-windows" CACHE PATH "Allegro 5 installation root")

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
    ${ALLEGRO5_ROOT}/include
)

# Platform-specific configuration
if(WIN32)
    target_compile_definitions(ds1edit PRIVATE WIN32 USE_CONSOLE)

    # Allegro 5 libraries
    target_link_directories(ds1edit PRIVATE ${ALLEGRO5_ROOT}/lib)
    target_link_libraries(ds1edit PRIVATE
        allegro
        allegro_image
        allegro_font
        allegro_primitives
        allegro_main
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

# Copy Allegro 5 DLLs to output directory
if(WIN32 AND EXISTS "${ALLEGRO5_ROOT}/bin")
    file(GLOB ALLEGRO5_DLLS "${ALLEGRO5_ROOT}/bin/*.dll")
    foreach(DLL ${ALLEGRO5_DLLS})
        get_filename_component(DLL_NAME ${DLL} NAME)
        configure_file(${DLL} ${CMAKE_SOURCE_DIR}/bin/${DLL_NAME} COPYONLY)
    endforeach()
endif()

# Unit tests
enable_testing()
add_subdirectory(test)
