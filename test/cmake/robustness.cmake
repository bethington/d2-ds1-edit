# Package E: robustness. Deterministic malformed-input campaigns against the
# real production probes.
#
# test_adversarial_probe covers the formats that have no other CLI probe (COF
# via compose_cof_parse, AnimData via animdata_load). The CTest below runs
# scripts/equivalence/adversarial.py with the committed adversarial-core
# profile: authored valid inputs corrupted at every header/record boundary and
# with length/count/offset extremes, each case in a bounded subprocess (10 s,
# 1 MiB). A crash, timeout, out-of-root file or a silent accept of a required
# section cut fails the test; the reproducing input and command are kept
# under <build>/test/adversarial/failures/.
#
# Reuses ds1edit_visual_test() from visuals.cmake (included first).

set(_core ${CMAKE_SOURCE_DIR}/src/core)

if(COMMAND ds1edit_visual_test)
    ds1edit_visual_test(test_adversarial_probe ALLEGRO SOURCES
        test_adversarial_probe.c ${_core}/compose_cof.c ${_core}/animdata.c
        ${_core}/animdata_math.c ${CMAKE_CURRENT_SOURCE_DIR}/visual_test_support.c
        ${CMAKE_SOURCE_DIR}/src/globals.c)
    # ds1edit_visual_test() appends to its own scope; publish it here too.
    list(APPEND DS1EDIT_ALLEGRO_TESTS test_adversarial_probe)
endif()

if(NOT Python3_Interpreter_FOUND)
    set(_script "${CMAKE_CURRENT_BINARY_DIR}/adversarial_core.cmake")
    file(WRITE "${_script}" "message(FATAL_ERROR [==[Python 3 was not found when this build was configured; adversarial_core cannot run.]==])\n")
    add_test(NAME adversarial_core COMMAND ${CMAKE_COMMAND} -P "${_script}")
elseif(TARGET test_adversarial_probe AND TARGET test_ds1_reference_probe
       AND TARGET test_visual_reference_probe)
    add_test(NAME adversarial_core
        COMMAND ${Python3_EXECUTABLE} -m scripts.equivalence.adversarial run
            --profile ${CMAKE_SOURCE_DIR}/test/equivalence/profiles/adversarial-core.json
            --probe-ds1 $<TARGET_FILE:test_ds1_reference_probe>
            --probe-visual $<TARGET_FILE:test_visual_reference_probe>
            --probe-robust $<TARGET_FILE:test_adversarial_probe>
            --out-dir ${CMAKE_CURRENT_BINARY_DIR}/adversarial
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(adversarial_core PROPERTIES
        ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

    # Tier `check`: a fixed-seed slice of the same campaign (all controls plus
    # a reproducible sample of mutations, ~500 cases) so a typical change gets
    # real malformed-input coverage without the full campaign's runtime. The
    # seed is fixed on purpose: the slice is identical on every run, so a
    # failure reproduces from its case id. adversarial_core is the full run.
    set(DS1EDIT_ADVERSARIAL_SLICE_CASES 500 CACHE STRING
        "Cases in the adversarial_slice test (tier check)")
    set(DS1EDIT_ADVERSARIAL_SLICE_SEED 20261023 CACHE STRING
        "Seed of the adversarial_slice test (fixed so runs are comparable)")
    add_test(NAME adversarial_slice
        COMMAND ${Python3_EXECUTABLE} -m scripts.equivalence.adversarial run
            --profile ${CMAKE_SOURCE_DIR}/test/equivalence/profiles/adversarial-core.json
            --probe-ds1 $<TARGET_FILE:test_ds1_reference_probe>
            --probe-visual $<TARGET_FILE:test_visual_reference_probe>
            --probe-robust $<TARGET_FILE:test_adversarial_probe>
            --max-cases ${DS1EDIT_ADVERSARIAL_SLICE_CASES}
            --seed ${DS1EDIT_ADVERSARIAL_SLICE_SEED}
            --out-dir ${CMAKE_CURRENT_BINARY_DIR}/adversarial-slice
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(adversarial_slice PROPERTIES
        LABELS "portable;check" TIMEOUT 900
        ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
else()
    set(_script "${CMAKE_CURRENT_BINARY_DIR}/adversarial_core.cmake")
    file(WRITE "${_script}" "message(FATAL_ERROR [==[A probe target (test_ds1_reference_probe, test_visual_reference_probe) is missing from this configuration; adversarial_core cannot run.]==])\n")
    add_test(NAME adversarial_core COMMAND ${CMAKE_COMMAND} -P "${_script}")
endif()
set_tests_properties(adversarial_core PROPERTIES LABELS "portable;slow" TIMEOUT 1800)

# Resource-lifetime and minimized-defect regressions. ds1.c is #included into
# the test's own translation unit behind a tracking allocator, so it is taken
# out of the shared production list here.
if(DEFINED DS1EDIT_PKGC_PRODUCTION)
    set(_prod ${DS1EDIT_PKGC_PRODUCTION})
    list(REMOVE_ITEM _prod ${CMAKE_SOURCE_DIR}/src/core/ds1.c)
    add_executable(test_adversarial_regressions test_adversarial_regressions.c
        ${DS1EDIT_PKGC_SUPPORT} ${_prod})
    target_link_libraries(test_adversarial_regressions PRIVATE unity ds1edit_allegro)
    target_include_directories(test_adversarial_regressions PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/mpq
        ${ALLEGRO5_INCLUDE_DIR})
    list(APPEND DS1EDIT_ALLEGRO_TESTS test_adversarial_regressions)
    add_test(NAME adversarial_regressions COMMAND test_adversarial_regressions)
    set_tests_properties(adversarial_regressions PROPERTIES LABELS portable TIMEOUT 120)
endif()

# Minimized sprite/tile decoder defects found by the campaign.
if(COMMAND ds1edit_visual_test)
    ds1edit_visual_test(test_adversarial_codecs ALLEGRO SOURCES test_adversarial_codecs.c
        ${_core}/dcc.c ${_core}/index_frame.c ${_core}/dt1.c ${_core}/dt1_decode.c
        ${_core}/dt1_draw.c ${_core}/rgba_cache.c
        ${CMAKE_CURRENT_SOURCE_DIR}/visual_test_support.c ${CMAKE_SOURCE_DIR}/src/globals.c)
    list(APPEND DS1EDIT_ALLEGRO_TESTS test_adversarial_codecs)
    add_test(NAME adversarial_codecs COMMAND test_adversarial_codecs)
    set_tests_properties(adversarial_codecs PROPERTIES LABELS portable TIMEOUT 120)
endif()
