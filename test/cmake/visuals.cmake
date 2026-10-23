# Package D: visual and export correctness.
#
# Every target here links the real production decoders, lookups and writers
# and feeds them authored bytes (inline, or the generated fixtures under
# test/equivalence/fixtures). test/visual_test_support.c supplies the editor
# globals and an in-memory archive fetch; it decodes nothing itself.

set(DS1EDIT_VISUAL_FIXTURES ${CMAKE_SOURCE_DIR}/test/equivalence/fixtures)

# asset_export.c decodes sprites through the shared raw seam.
if(TARGET test_asset_export_smoke)
    target_sources(test_asset_export_smoke PRIVATE
        ${CMAKE_SOURCE_DIR}/src/core/index_frame.c
        ${CMAKE_SOURCE_DIR}/src/core/dcc.c)
    target_compile_definitions(test_asset_export_smoke PRIVATE
        DS1EDIT_VISUAL_FIXTURES="${DS1EDIT_VISUAL_FIXTURES}"
        DS1EDIT_TEST_OUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/visual_out")
endif()

function(ds1edit_visual_test name)
    cmake_parse_arguments(V "ALLEGRO" "" "SOURCES" ${ARGN})
    add_executable(${name} ${V_SOURCES})
    target_include_directories(${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/mpq
        ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(${name} PRIVATE
        DS1EDIT_DECODER_FIXTURES="${DS1EDIT_VISUAL_FIXTURES}/decoders"
        DS1EDIT_VISUAL_FIXTURES="${DS1EDIT_VISUAL_FIXTURES}"
        DS1EDIT_TEST_OUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/visual_out")
    if(V_ALLEGRO)
        target_link_libraries(${name} PRIVATE unity ds1edit_allegro ZLIB::ZLIB)
        target_include_directories(${name} PRIVATE ${ALLEGRO5_INCLUDE_DIR})
        list(APPEND DS1EDIT_ALLEGRO_TESTS ${name})
        set(DS1EDIT_ALLEGRO_TESTS ${DS1EDIT_ALLEGRO_TESTS} PARENT_SCOPE)
    else()
        target_link_libraries(${name} PRIVATE unity)
    endif()
endfunction()

macro(ds1edit_visual_add_test test_name target)
    add_test(NAME ${test_name} COMMAND ${target} ${ARGN})
    set_tests_properties(${test_name} PROPERTIES LABELS portable TIMEOUT 120)
endmacro()

set(_vsup ${CMAKE_CURRENT_SOURCE_DIR}/visual_test_support.c)
set(_core ${CMAKE_SOURCE_DIR}/src/core)

# --- task 2: decoders ------------------------------------------------------
ds1edit_visual_test(test_dc6_decode SOURCES test_dc6_decode.c ${_core}/index_frame.c)
ds1edit_visual_add_test(dc6_decode test_dc6_decode)

set(_vglobals ${CMAKE_SOURCE_DIR}/src/globals.c)
set(_dt1_sources ${_core}/dt1.c ${_core}/dt1_decode.c ${_core}/dt1_draw.c
    ${_core}/rgba_cache.c)

ds1edit_visual_test(test_dcc_decode ALLEGRO SOURCES test_dcc_decode.c
    ${_core}/dcc.c ${_core}/index_frame.c ${_vsup} ${_vglobals})
ds1edit_visual_add_test(dcc_decode test_dcc_decode)

# The reference probe: decode one asset with the real decoders and write raw
# index frames (see the file header). The CTest below drives it over every
# authored decoder fixture and compares with the independent encoder's intent.
ds1edit_visual_test(test_visual_reference_probe ALLEGRO SOURCES
    test_visual_reference_probe.c ${_core}/dcc.c ${_core}/index_frame.c
    ${_dt1_sources} ${_vsup} ${_vglobals})
if(Python3_Interpreter_FOUND)
    add_test(NAME visual_reference_probe_decoder_cases
        COMMAND ${Python3_EXECUTABLE} -m scripts.equivalence.decoder_cases verify
            --probe $<TARGET_FILE:test_visual_reference_probe>
            --dll-dir ${CMAKE_CURRENT_BINARY_DIR}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(visual_reference_probe_decoder_cases PROPERTIES
        LABELS portable TIMEOUT 120 ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
endif()

# --- task 1: palette, tile keys, AnimData -----------------------------------
ds1edit_visual_test(test_animdata_lookup ALLEGRO SOURCES test_animdata_lookup.c
    ${_core}/animdata.c ${_core}/animdata_math.c ${_vsup} ${_vglobals})
ds1edit_visual_add_test(animdata_lookup test_animdata_lookup)

ds1edit_visual_test(test_tile_properties ALLEGRO SOURCES test_tile_properties.c
    ${_core}/ds1.c test_tile_properties_stubs.c ${_vsup} ${_vglobals})
ds1edit_visual_add_test(tile_properties test_tile_properties)

ds1edit_visual_test(test_dt1_load ALLEGRO SOURCES test_dt1_load.c
    ${_dt1_sources} ${_vsup} ${_vglobals})
ds1edit_visual_add_test(dt1_load test_dt1_load)

# --- task 3: composition and exports -----------------------------------------
set(_compose_sources ${_core}/compose_render.c ${_core}/compose_cof.c
    ${_core}/compose_cof_path.c ${_core}/compose_dcc_path.c
    ${_core}/compose_bbox.c ${_core}/compose_blit.c ${_core}/dcc.c
    ${_core}/index_frame.c)

ds1edit_visual_test(test_compose_render ALLEGRO SOURCES test_compose_render.c
    ${_compose_sources} ${_core}/compose_apng.c ${_core}/compose_scale.c
    ${_core}/apng_writer.c ${_core}/animdata.c ${_core}/animdata_math.c
    ${_vsup} ${_vglobals})
ds1edit_visual_add_test(compose_render test_compose_render)

# The exports above are decoded independently (Pillow + a chunk reader) and
# compared with expectations computed from the authored inputs.
set_tests_properties(compose_render PROPERTIES FIXTURES_SETUP visual_exports)
if(TEST asset_export_smoke)
    set_tests_properties(asset_export_smoke PROPERTIES FIXTURES_SETUP visual_exports)
endif()
if(Python3_Interpreter_FOUND)
    add_test(NAME visual_export_images
        COMMAND ${Python3_EXECUTABLE} -m scripts.equivalence.export_cases verify
            --out-dir ${CMAKE_CURRENT_BINARY_DIR}/visual_out
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(visual_export_images PROPERTIES LABELS portable
        TIMEOUT 120 FIXTURES_REQUIRED visual_exports
        ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
endif()

ds1edit_visual_test(test_export_progress ALLEGRO SOURCES test_export_progress.c
    ${_core}/export_progress.c ${_vsup} ${_vglobals})
ds1edit_visual_add_test(export_progress test_export_progress)
