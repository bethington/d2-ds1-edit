# Test tiers: which CTest entries a typical change should run.
#
#   fast   every portable unit suite; no display, no game data, no long
#          campaigns. Target: under two minutes.
#   check  fast + the tests that need a display, a local runtime or a longer
#          campaign slice: golden_core, display_selftest, adversarial_slice
#          and (when another branch registers it) parity_quick.
#   full   everything registered, including adversarial_core, perf_fps
#          (DS1EDIT_GAME_DIR) and the pd2 lane (DS1EDIT_PD2_DIR).
#
# Scheme (CTest LABELS; see BUILDING.md "Test tiers"):
#   fast     added HERE to every test that carries none of the labels below,
#            so a new unit test is fast by default.
#   check    set on the beyond-fast entries. A test labelled `parity_quick`
#            gets `check` added here, so the headless software-renderer parity
#            test slots in just by labelling itself.
#   slow     the full adversarial campaign (full tier only).
#   display  creates a window; DS1EDIT_TEST_OFFSCREEN=1 is set here for it.
#   local    needs a local runtime (DS1EDIT_GOLDEN_RUNTIME) or game data.
#   perf, pd2   the existing local-only lanes.
# `portable` is unchanged and is what CI selects (-L portable).
# Presets: CMakePresets.json test presets fast / check / full per family.
#
# Include last: it inspects every test the other packages registered.

get_property(_ds1_all_tests DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY TESTS)
foreach(_t IN LISTS _ds1_all_tests)
    get_test_property(${_t} LABELS _labels)
    if(NOT _labels)
        set(_labels "")
    endif()
    if("parity_quick" IN_LIST _labels AND NOT "check" IN_LIST _labels)
        list(APPEND _labels check)
        set_tests_properties(${_t} PROPERTIES LABELS "${_labels}")
    endif()
    if("display" IN_LIST _labels)
        set_property(TEST ${_t} APPEND PROPERTY ENVIRONMENT "DS1EDIT_TEST_OFFSCREEN=1")
    endif()
    set(_beyond_fast FALSE)
    foreach(_l slow local display perf pd2 check parity_quick)
        if(_l IN_LIST _labels)
            set(_beyond_fast TRUE)
        endif()
    endforeach()
    if(NOT _beyond_fast)
        list(APPEND _labels fast)
        set_tests_properties(${_t} PROPERTIES LABELS "${_labels}")
    endif()
endforeach()
