# Unity test framework
add_library(unity STATIC unity/unity.c)
target_include_directories(unity PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/unity)

# Placeholder test (validates the test infrastructure)
add_executable(test_placeholder test_placeholder.c)
target_link_libraries(test_placeholder PRIVATE unity)
add_test(NAME placeholder COMMAND test_placeholder)

# Phase 1: Palette abstraction layer tests
add_executable(test_palette test_palette.c ../src/palette.c)
target_link_libraries(test_palette PRIVATE unity)
target_include_directories(test_palette PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME palette COMMAND test_palette)

# Phase 1: RGBA cache tests (needs Allegro 5 for cache_tile_to_a5_bitmap)
add_executable(test_rgba_cache test_rgba_cache.c ../src/rgba_cache.c)
target_link_libraries(test_rgba_cache PRIVATE unity ${ALLEGRO5_LIB})
target_include_directories(test_rgba_cache PRIVATE
    ${CMAKE_SOURCE_DIR}/src
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

# Phase 2: DT1 sub-tile decoder tests (no Allegro dependency)
add_executable(test_dt1_decode test_dt1_decode.c ../src/dt1_decode.c)
target_link_libraries(test_dt1_decode PRIVATE unity)
target_include_directories(test_dt1_decode PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME dt1_decode COMMAND test_dt1_decode)
