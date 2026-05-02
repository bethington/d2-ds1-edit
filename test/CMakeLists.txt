# Unity test framework
add_library(unity STATIC unity/unity.c)
target_include_directories(unity PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/unity)

# Placeholder test (validates the test infrastructure)
add_executable(test_placeholder test_placeholder.c)
target_link_libraries(test_placeholder PRIVATE unity)
add_test(NAME placeholder COMMAND test_placeholder)

# Export helpers: screenshot naming regression checks
add_executable(test_export_common test_export_common.c ../src/core/export_common.c)
target_link_libraries(test_export_common PRIVATE unity)
target_include_directories(test_export_common PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME export_common COMMAND test_export_common)

# Glob matcher (used by export preset patterns)
add_executable(test_glob_match test_glob_match.c ../src/core/glob_match.c)
target_link_libraries(test_glob_match PRIVATE unity)
target_include_directories(test_glob_match PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME glob_match COMMAND test_glob_match)

# Export presets parser ([export_presets] section in Ds1edit.ini)
add_executable(test_export_presets test_export_presets.c ../src/core/export_presets.c)
target_link_libraries(test_export_presets PRIVATE unity)
target_include_directories(test_export_presets PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME export_presets COMMAND test_export_presets)

# DC6 header inspection (single-frame check for export filter)
add_executable(test_dc6_header test_dc6_header.c ../src/core/dc6_header.c)
target_link_libraries(test_dc6_header PRIVATE unity)
target_include_directories(test_dc6_header PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME dc6_header COMMAND test_dc6_header)

# APNG writer (used by character composition exports)
add_executable(test_apng_writer test_apng_writer.c ../src/core/apng_writer.c)
target_link_libraries(test_apng_writer PRIVATE unity ZLIB::ZLIB)
target_include_directories(test_apng_writer PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME apng_writer COMMAND test_apng_writer)

# animdata speed -> frame delay converter (pure math, no globals)
add_executable(test_animdata_speed test_animdata_speed.c ../src/core/animdata_math.c)
target_link_libraries(test_animdata_speed PRIVATE unity)
target_include_directories(test_animdata_speed PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME animdata_speed COMMAND test_animdata_speed)

# Compose naming: class code -> full name + filename sanitization
add_executable(test_compose_naming test_compose_naming.c ../src/core/compose_naming.c)
target_link_libraries(test_compose_naming PRIVATE unity)
target_include_directories(test_compose_naming PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME compose_naming COMMAND test_compose_naming)

# Compose index: MonStats / Objects TXT parsers (pure functions tested
# against synthetic fixtures; the MPQ-loading glue is integration-tested
# at runtime).
add_executable(test_compose_index test_compose_index.c ../src/core/compose_index.c)
target_link_libraries(test_compose_index PRIVATE unity)
target_include_directories(test_compose_index PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME compose_index COMMAND test_compose_index)

# Compose mode/weapon presets parser
add_executable(test_compose_presets test_compose_presets.c ../src/core/compose_presets.c)
target_link_libraries(test_compose_presets PRIVATE unity)
target_include_directories(test_compose_presets PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME compose_presets COMMAND test_compose_presets)

# Compose COF byte parser (offline composition; pure logic)
add_executable(test_compose_cof test_compose_cof.c ../src/core/compose_cof.c)
target_link_libraries(test_compose_cof PRIVATE unity)
target_include_directories(test_compose_cof PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME compose_cof COMMAND test_compose_cof)

# Compose DCC path builder (pure string formatting)
add_executable(test_compose_dcc_path test_compose_dcc_path.c ../src/core/compose_dcc_path.c)
target_link_libraries(test_compose_dcc_path PRIVATE unity)
target_include_directories(test_compose_dcc_path PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME compose_dcc_path COMMAND test_compose_dcc_path)

# Compose COF path builder
add_executable(test_compose_cof_path test_compose_cof_path.c ../src/core/compose_cof_path.c)
target_link_libraries(test_compose_cof_path PRIVATE unity)
target_include_directories(test_compose_cof_path PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME compose_cof_path COMMAND test_compose_cof_path)

# Compose canvas/bbox calculator (pure math)
add_executable(test_compose_bbox test_compose_bbox.c ../src/core/compose_bbox.c)
target_link_libraries(test_compose_bbox PRIVATE unity)
target_include_directories(test_compose_bbox PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME compose_bbox COMMAND test_compose_bbox)

add_executable(test_asset_export_smoke
    test_asset_export_smoke.c
    test_asset_export_stubs.c
    ../src/core/asset_export.c
    ../src/core/dc6_header.c
    ../src/core/glob_match.c
)
target_link_libraries(test_asset_export_smoke PRIVATE unity ${ALLEGRO5_LIB} ${ALLEGRO5_IMAGE_LIB})
target_include_directories(test_asset_export_smoke PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/mpq
    ${ALLEGRO5_INCLUDE_DIR}
)
target_compile_definitions(test_asset_export_smoke PRIVATE
    TEST_BUILD_DIR="$<TARGET_FILE_DIR:test_asset_export_smoke>"
    DS1EDIT_BIN_EXE="${CMAKE_SOURCE_DIR}/bin/ds1edit.exe"
)
add_test(NAME asset_export_smoke COMMAND test_asset_export_smoke)

# Phase 1: Palette abstraction layer tests
add_executable(test_palette test_palette.c ../src/core/palette.c)
target_link_libraries(test_palette PRIVATE unity)
target_include_directories(test_palette PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/mpq
)
add_test(NAME palette COMMAND test_palette)

add_executable(test_area_metadata test_area_metadata.c ../src/core/area_metadata.c)
target_link_libraries(test_area_metadata PRIVATE unity)
target_include_directories(test_area_metadata PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME area_metadata COMMAND test_area_metadata)

# Phase 1: RGBA cache tests (needs Allegro 5 for cache_tile_to_a5_bitmap)
add_executable(test_rgba_cache test_rgba_cache.c ../src/core/rgba_cache.c)
target_link_libraries(test_rgba_cache PRIVATE unity ${ALLEGRO5_LIB})
target_include_directories(test_rgba_cache PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/mpq
    ${ALLEGRO5_INCLUDE_DIR}
)
add_test(NAME rgba_cache COMMAND test_rgba_cache)

# Copy Allegro DLLs next to test executables so they can find them at runtime
if(WIN32 AND DEFINED ALLEGRO5_BIN_DIR AND EXISTS "${ALLEGRO5_BIN_DIR}")
    add_custom_command(TARGET test_rgba_cache POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${ALLEGRO5_BIN_DIR}"
            "$<TARGET_FILE_DIR:test_rgba_cache>"
        COMMENT "Copying Allegro DLLs to test output directory"
    )
endif()

if(WIN32 AND DEFINED ALLEGRO5_BIN_DIR AND EXISTS "${ALLEGRO5_BIN_DIR}")
    add_custom_command(TARGET test_asset_export_smoke POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${ALLEGRO5_BIN_DIR}"
            "$<TARGET_FILE_DIR:test_asset_export_smoke>"
        COMMENT "Copying Allegro DLLs for test_asset_export_smoke"
    )
endif()

# Phase 2: DT1 sub-tile decoder tests (no Allegro dependency)
add_executable(test_dt1_decode test_dt1_decode.c ../src/core/dt1_decode.c)
target_link_libraries(test_dt1_decode PRIVATE unity)
target_include_directories(test_dt1_decode PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME dt1_decode COMMAND test_dt1_decode)

# MPQ integration Phase 3: preset index reverse lookup
add_executable(test_mpq_index
    test_mpq_index.c
    ../src/core/mpq_index_query.c
)
target_link_libraries(test_mpq_index PRIVATE unity)
target_include_directories(test_mpq_index PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME mpq_index COMMAND test_mpq_index)

# MPQ integration Phase 2a: preferences + project model
add_executable(test_project
    test_project.c
    ../src/core/preferences.c
    ../src/core/project.c
)
target_link_libraries(test_project PRIVATE unity ${ALLEGRO5_LIB})
target_include_directories(test_project PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/mpq
    ${ALLEGRO5_INCLUDE_DIR}
)
target_compile_definitions(test_project PRIVATE DS1EDIT_VERSION_STR="${PROJECT_VERSION}")
add_test(NAME project COMMAND test_project)

if(WIN32 AND DEFINED ALLEGRO5_BIN_DIR AND EXISTS "${ALLEGRO5_BIN_DIR}")
    add_custom_command(TARGET test_project POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${ALLEGRO5_BIN_DIR}"
            "$<TARGET_FILE_DIR:test_project>"
        COMMENT "Copying Allegro DLLs for test_project"
    )
endif()
