cmake_minimum_required(VERSION 3.20)
project(d2-ds1-edit C)

# Options
option(USE_SOFTWARE_RENDERER "Force software rendering (no GPU)" OFF)

# Source files
set(SOURCES
    Sources/anim.c
    Sources/animdata.c
    Sources/dc6info.c
    Sources/dccinfo.c
    Sources/ds1misc.c
    Sources/ds1save.c
    Sources/dt1misc.c
    Sources/dt1_draw.c
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
)

# Platform-specific configuration
if(WIN32)
    target_compile_definitions(ds1edit PRIVATE WIN32 USE_CONSOLE)

    # Allegro 4 (temporary - will be replaced with Allegro 5)
    target_include_directories(ds1edit PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/allegro-4.4.3.1/include
        ${CMAKE_SOURCE_DIR}/third_party/allegro-build/include
    )
    target_link_libraries(ds1edit PRIVATE
        ${CMAKE_SOURCE_DIR}/bin/allegro-4.4.2-monolith-md.lib
        user32 gdi32 ole32 dinput8 ddraw dxguid winmm dsound
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

# Unit tests
enable_testing()
add_subdirectory(test)
