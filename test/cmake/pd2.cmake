# Local-only Project Diablo 2 compatibility lane (label `pd2`).
#
# Registered ONLY when the cache variable DS1EDIT_PD2_DIR names a ProjectD2
# folder, so a default configure (and every CI job) has no pd2 test at all.
# The lane reads PD2 art that is not redistributable; it writes under tmp/pd2/
# (run-<ts>/ per run, baseline/ for the local render baseline) and nothing it
# produces may be committed. See scripts/equivalence/pd2_lane.py and the
# "Project Diablo 2 lane" section of BUILDING.md.
#
#   cmake -S . -B tmp/build-pd2 ... -DDS1EDIT_PD2_DIR=C:/Diablo2/ProjectD2
#   ctest --test-dir tmp/build-pd2 -C Debug -L pd2 --output-on-failure
#
# Paths are explicit, never auto-detected. DS1EDIT_PD2_BASE_DIR is the vanilla
# Diablo II folder (default: the parent of DS1EDIT_PD2_DIR).

if(NOT DS1EDIT_PD2_DIR)
    return()
endif()

if(DEFINED ENV{CI} OR DEFINED ENV{GITHUB_ACTIONS})
    message(STATUS "pd2 lane: skipped, DS1EDIT_PD2_DIR is set but this is a CI environment")
    return()
endif()

if(NOT Python3_Interpreter_FOUND)
    message(WARNING "pd2 lane: Python 3 not found, the pd2 tests are not registered")
    return()
endif()

if(NOT EXISTS "${DS1EDIT_PD2_DIR}/pd2data.mpq")
    message(FATAL_ERROR "DS1EDIT_PD2_DIR=${DS1EDIT_PD2_DIR} has no pd2data.mpq")
endif()

if(NOT DS1EDIT_PD2_BASE_DIR)
    get_filename_component(DS1EDIT_PD2_BASE_DIR "${DS1EDIT_PD2_DIR}/.." ABSOLUTE)
endif()

if(NOT (TARGET ds1edit AND TARGET test_ds1_reference_probe
        AND TARGET test_visual_reference_probe AND TARGET test_adversarial_probe))
    message(FATAL_ERROR "pd2 lane: a probe target is missing from this configuration")
endif()

set(_pd2_common
    --pd2-dir ${DS1EDIT_PD2_DIR}
    --d2-dir ${DS1EDIT_PD2_BASE_DIR}
    --exe $<TARGET_FILE:ds1edit>
    --probe-ds1 $<TARGET_FILE:test_ds1_reference_probe>
    --probe-visual $<TARGET_FILE:test_visual_reference_probe>
    --probe-robust $<TARGET_FILE:test_adversarial_probe>
    --out-root ${CMAKE_SOURCE_DIR}/tmp/pd2)
if(EXISTS "${CMAKE_SOURCE_DIR}/tmp/equivalence/baseline-corpus-113c.json")
    list(APPEND _pd2_common
        --manifest ${CMAKE_SOURCE_DIR}/tmp/equivalence/baseline-corpus-113c.json)
endif()

foreach(_part inventory sweep render)
    add_test(NAME pd2_${_part}
        COMMAND ${Python3_EXECUTABLE} -m scripts.equivalence.pd2_lane ${_part} ${_pd2_common}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(pd2_${_part} PROPERTIES
        LABELS pd2
        TIMEOUT 7200
        RESOURCE_LOCK pd2_lane
        ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
endforeach()
