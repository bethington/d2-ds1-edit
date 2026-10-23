# Package B: reference identity and the palette nearest-colour equivalence pilot.
#
# test_palette_reference_probe links the real src/core/palette.c and reports
# palette_find_closest() for authored palettes/queries. The replay test compares
# its answers with the original D2CMP.dll 1.13c results captured from a live
# member (test/equivalence/fixtures/palette-nearest-capture.json). The replay
# needs only Python and the probe: no game install, MPQs, or fleet access.

get_filename_component(_ds1_eq_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(_ds1_eq_dir "${_ds1_eq_root}/test/equivalence")

add_executable(test_palette_reference_probe
    test_palette_reference_probe.c
    ${CMAKE_SOURCE_DIR}/src/core/palette.c
)
target_include_directories(test_palette_reference_probe PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Malformed invocation must fail (no arguments -> usage, nonzero exit).
add_test(NAME equivalence_palette_probe_usage COMMAND test_palette_reference_probe)
set_tests_properties(equivalence_palette_probe_usage PROPERTIES
    WILL_FAIL TRUE LABELS portable TIMEOUT 30)

if(NOT Python3_Interpreter_FOUND)
    set(_script "${CMAKE_CURRENT_BINARY_DIR}/equivalence_palette_replay.cmake")
    file(WRITE "${_script}" "message(FATAL_ERROR [==[Python 3 was not found when this build was configured; equivalence_palette_replay cannot run.]==])\n")
    add_test(NAME equivalence_palette_replay COMMAND ${CMAKE_COMMAND} -P "${_script}")
else()
    add_test(NAME equivalence_palette_replay
        COMMAND ${Python3_EXECUTABLE} -m scripts.equivalence.replay
            --contract "${_ds1_eq_dir}/contracts/palette-nearest-113c.json"
            --baseline "${_ds1_eq_dir}/fixtures/palette-nearest-baseline.json"
            --capture "${_ds1_eq_dir}/fixtures/palette-nearest-capture.json"
            --probe "$<TARGET_FILE:test_palette_reference_probe>"
            --out-dir "${CMAKE_CURRENT_BINARY_DIR}/equivalence/palette-nearest-113c"
            --build-config "$<CONFIG>"
            --compiler "${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}"
        WORKING_DIRECTORY "${_ds1_eq_root}")
    set_tests_properties(equivalence_palette_replay PROPERTIES
        ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
endif()
set_tests_properties(equivalence_palette_replay PROPERTIES LABELS portable TIMEOUT 120)
