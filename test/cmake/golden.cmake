# Local-only tests that need a vanilla 1.13c runtime (tier `check`).
#
#   cmake -S . -B tmp/build-presets ... -DDS1EDIT_GOLDEN_RUNTIME=C:/.../tmp/regen/rt
#
# DS1EDIT_GOLDEN_RUNTIME names an owned runtime directory: ds1edit[.exe], its
# DLLs, assets/*.ini, and a ds1edit.ini pointing at the game archives (never
# the repository's own bin/ds1edit.ini). The tests run the executable that is
# IN that directory, which this build tree does not rebuild: refresh it when
# the code under test changes.
#
#   golden_core       scripts/run_golden_tests.py --core, exact match
#                     (map .ini files from the runtime's assets/ if present,
#                     else the repository's assets/)
#   display_selftest  a windowed `--selftest` (30 frames), window off-screen
#
# Without a usable runtime both are registered as a skip stub that prints
# "SKIPPED ... not configured: ..." and matches SKIP_REGULAR_EXPRESSION, so
# CTest reports "Not Run (Skipped)" with the reason and still exits 0. They are
# never silent passes, and never failures: a machine without game data cannot
# run them and says so.

get_filename_component(_ds1_golden_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

function(_ds1_skipped_test name reason)
    add_test(NAME ${name}
        COMMAND ${CMAKE_COMMAND} -E echo "SKIPPED ${name}: ${reason}")
    set_tests_properties(${name} PROPERTIES
        LABELS "check;local"
        SKIP_REGULAR_EXPRESSION "SKIPPED ${name}:")
endfunction()

set(_ds1_gr_reason "")
if(NOT DS1EDIT_GOLDEN_RUNTIME)
    set(_ds1_gr_reason "not configured (pass -DDS1EDIT_GOLDEN_RUNTIME=<runtime dir with ds1edit, assets/ and a vanilla 1.13c ds1edit.ini>)")
elseif(NOT Python3_Interpreter_FOUND)
    set(_ds1_gr_reason "not configured (Python 3 not found)")
else()
    if(WIN32)
        set(_ds1_gr_exe "${DS1EDIT_GOLDEN_RUNTIME}/ds1edit.exe")
    else()
        set(_ds1_gr_exe "${DS1EDIT_GOLDEN_RUNTIME}/ds1edit")
    endif()
    if(NOT EXISTS "${_ds1_gr_exe}" OR NOT IS_DIRECTORY "${DS1EDIT_GOLDEN_RUNTIME}/assets")
        set(_ds1_gr_reason "not configured (DS1EDIT_GOLDEN_RUNTIME=${DS1EDIT_GOLDEN_RUNTIME} has no ds1edit executable or assets/)")
    endif()
endif()

if(_ds1_gr_reason)
    _ds1_skipped_test(golden_core "${_ds1_gr_reason}")
    _ds1_skipped_test(display_selftest "${_ds1_gr_reason}")
    if(DS1EDIT_GOLDEN_RUNTIME)
        message(WARNING "golden/display tests: ${_ds1_gr_reason}")
    endif()
else()
    # The map .ini files are source-tree data. A runtime that carries its own
    # copy (assets/act1_town.ini) is used as is; one that only has the editor
    # UI assets falls back to the repository's assets/.
    if(EXISTS "${DS1EDIT_GOLDEN_RUNTIME}/assets/act1_town.ini")
        set(_ds1_gr_assets "${DS1EDIT_GOLDEN_RUNTIME}/assets")
    else()
        set(_ds1_gr_assets "${_ds1_golden_root}/assets")
    endif()
    add_test(NAME golden_core
        COMMAND ${Python3_EXECUTABLE} scripts/run_golden_tests.py --core
            --exe "${_ds1_gr_exe}"
            --assets-dir "${_ds1_gr_assets}"
            --artifacts-dir "${CMAKE_CURRENT_BINARY_DIR}/golden-runs"
        WORKING_DIRECTORY "${_ds1_golden_root}")
    set_tests_properties(golden_core PROPERTIES
        LABELS "check;local" TIMEOUT 900
        ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1;DS1EDIT_TEST_OFFSCREEN=1")

    add_test(NAME display_selftest
        COMMAND "${_ds1_gr_exe}" "${_ds1_gr_assets}/act5_town.ini" --selftest=30
        WORKING_DIRECTORY "${DS1EDIT_GOLDEN_RUNTIME}")
    set_tests_properties(display_selftest PROPERTIES
        LABELS "check;display;local" TIMEOUT 300)
endif()
