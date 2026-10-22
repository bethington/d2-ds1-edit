# Local-only frame-rate test. Needs a display, a GPU and a vanilla 1.13c
# install, so it is NOT labelled `portable` and registers only when the
# configure step is given the game directory:
#
#   cmake -S . -B build -DDS1EDIT_GAME_DIR=F:/D2VersionChanger/VersionChanger/LoD/1.13c
#   ctest --test-dir build -C Release -L perf --output-on-failure
#
# See BUILDING.md ("Frame-rate test") for how to read the table it prints.

get_filename_component(_ds1_perf_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

if(DS1EDIT_GAME_DIR AND Python3_Interpreter_FOUND AND TARGET ds1edit)
    add_test(NAME perf_fps
        COMMAND ${Python3_EXECUTABLE} scripts/perf/fps_test.py
            --exe "$<TARGET_FILE:ds1edit>"
            --game-dir "${DS1EDIT_GAME_DIR}"
            --out-dir "${CMAKE_CURRENT_BINARY_DIR}/perf-fps"
        WORKING_DIRECTORY "${_ds1_perf_root}")
    set_tests_properties(perf_fps PROPERTIES
        LABELS perf
        TIMEOUT 3600
        ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
elseif(DS1EDIT_GAME_DIR)
    message(STATUS "perf_fps not registered: needs Python 3 and the ds1edit target")
endif()
