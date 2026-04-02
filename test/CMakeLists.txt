# Unity test framework
add_library(unity STATIC unity/unity.c)
target_include_directories(unity PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/unity)

# Placeholder test (validates the test infrastructure)
add_executable(test_placeholder test_placeholder.c)
target_link_libraries(test_placeholder PRIVATE unity)
add_test(NAME placeholder COMMAND test_placeholder)

# Future test targets (uncomment as each phase is implemented):
# add_executable(test_palette test_palette.c ../Sources/palette.c)
# target_link_libraries(test_palette PRIVATE unity)
# target_include_directories(test_palette PRIVATE ${CMAKE_SOURCE_DIR}/Sources)
# add_test(NAME palette COMMAND test_palette)
