# Package C: map, editing and persistence integrity.
#
# These targets link the real DS1 reader/writer (src/core/ds1.c,
# src/core/ds1_save_io.c), the real editing and undo code (src/editor/*) and
# the real project path logic. test/support provides the editor's globals, an
# owned temp directory per run, an independent DS1 fixture encoder, and
# documented stubs for hooks outside the code under test (archive lookup,
# view-port bookkeeping, tile-graphics release, object-array growth).

set(DS1EDIT_PKGC_SUPPORT
    ${CMAKE_CURRENT_SOURCE_DIR}/support/test_tmp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/support/editor_test_state.c
    ${CMAKE_CURRENT_SOURCE_DIR}/support/ds1_test_stubs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/support/ds1_fixture.c
    ${CMAKE_CURRENT_SOURCE_DIR}/support/ds1_expect.c
)

# Production sources every map-integrity target needs: ds1_read() calls into
# objects.c, which reaches paths.c, selection_logic.c and the input layer.
set(DS1EDIT_PKGC_PRODUCTION
    ${CMAKE_SOURCE_DIR}/src/core/ds1.c
    ${CMAKE_SOURCE_DIR}/src/core/ds1_save_io.c
    ${CMAKE_SOURCE_DIR}/src/core/project.c
    ${CMAKE_SOURCE_DIR}/src/editor/objects.c
    ${CMAKE_SOURCE_DIR}/src/editor/paths.c
    ${CMAKE_SOURCE_DIR}/src/editor/undo.c
    ${CMAKE_SOURCE_DIR}/src/editor/tiles.c
    ${CMAKE_SOURCE_DIR}/src/editor/selection_logic.c
    ${CMAKE_SOURCE_DIR}/src/ui/input.c
    ${CMAKE_SOURCE_DIR}/src/globals.c
    ${CMAKE_SOURCE_DIR}/src/core/palette.c
)

function(ds1edit_pkgc_test name source)
    add_executable(${name} ${source} ${DS1EDIT_PKGC_SUPPORT}
        ${DS1EDIT_PKGC_PRODUCTION} ${ARGN})
    target_link_libraries(${name} PRIVATE unity ds1edit_allegro)
    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/mpq
        ${ALLEGRO5_INCLUDE_DIR}
    )
    list(APPEND DS1EDIT_ALLEGRO_TESTS ${name})
    set(DS1EDIT_ALLEGRO_TESTS ${DS1EDIT_ALLEGRO_TESTS} PARENT_SCOPE)
endfunction()

ds1edit_pkgc_test(test_ds1_format test_ds1_format.c)
add_test(NAME ds1_format COMMAND test_ds1_format)
set_tests_properties(ds1_format PROPERTIES LABELS "portable")

ds1edit_pkgc_test(test_ds1_save test_ds1_save.c)
add_test(NAME ds1_save COMMAND test_ds1_save)
set_tests_properties(ds1_save PROPERTIES LABELS "portable")

ds1edit_pkgc_test(test_editor_undo test_editor_undo.c)
add_test(NAME editor_undo COMMAND test_editor_undo)
set_tests_properties(editor_undo PROPERTIES LABELS "portable")

ds1edit_pkgc_test(test_object_path_edits test_object_path_edits.c)
add_test(NAME object_path_edits COMMAND test_object_path_edits)
set_tests_properties(object_path_edits PROPERTIES LABELS "portable")

# Snapshot probe: dumps what the real reader made of a DS1 (schema
# ds1edit.map/1) and re-saves through the checked writer. Used by the Python
# case runner below and, later, by the original-loader comparison (task 4).
ds1edit_pkgc_test(test_ds1_reference_probe test_ds1_reference_probe.c)

# Generated fixtures -> real reader/writer -> canonical comparison. The
# probe is required here, so a missing or broken probe fails instead of
# skipping.
if(Python3_Interpreter_FOUND)
    add_test(NAME ds1_cases
        COMMAND ${Python3_EXECUTABLE} -m unittest -v test.python.test_ds1_cases
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(ds1_cases PROPERTIES
        LABELS "portable"
        ENVIRONMENT "DS1EDIT_PROBE_EXE=$<TARGET_FILE:test_ds1_reference_probe>;DS1EDIT_REQUIRE_PROBE=1")
endif()

# Project copy-on-save paths through the checked writer, and the overlay
# reader the project depends on (src/mpq is linked for mod_load_in_mem only).
ds1edit_pkgc_test(test_resource_resolution test_resource_resolution.c
    ${CMAKE_SOURCE_DIR}/src/mpq/MpqView.c
    ${CMAKE_SOURCE_DIR}/src/mpq/Explode.c
    ${CMAKE_SOURCE_DIR}/src/mpq/Dcl_tbl.c
    ${CMAKE_SOURCE_DIR}/src/mpq/Wav_unp.c
    ${CMAKE_SOURCE_DIR}/src/platform.c)
target_link_libraries(test_resource_resolution PRIVATE ZLIB::ZLIB)
add_test(NAME resource_resolution COMMAND test_resource_resolution)
set_tests_properties(resource_resolution PROPERTIES LABELS "portable")

# Installation resolution: explicit archive paths, partial/missing installs,
# name case, separators. Placeholder archives only; detection never runs.
add_executable(test_d2install test_d2install.c
    ${CMAKE_SOURCE_DIR}/src/core/d2install.c
    ${CMAKE_SOURCE_DIR}/src/platform.c
    ${CMAKE_CURRENT_SOURCE_DIR}/support/test_tmp.c)
target_link_libraries(test_d2install PRIVATE unity ds1edit_allegro)
target_include_directories(test_d2install PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/mpq
    ${ALLEGRO5_INCLUDE_DIR})
if(WIN32)
    target_link_libraries(test_d2install PRIVATE advapi32)
endif()
list(APPEND DS1EDIT_ALLEGRO_TESTS test_d2install)
add_test(NAME d2install COMMAND test_d2install)
set_tests_properties(d2install PROPERTIES LABELS "portable")
