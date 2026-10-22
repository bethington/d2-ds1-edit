# Sanitizer and coverage lanes (robustness package E, task 2).
#
# Both options are OFF by default and change nothing for ordinary builds. They
# are for the dedicated instrumented lanes only: an instrumented build is never
# a Release performance build (scripts/equivalence/benchmarks.py refuses the
# build identity it records for one).
#
#   DS1EDIT_ENABLE_SANITIZERS  GCC/Clang (Linux, macOS): AddressSanitizer +
#                              UndefinedBehaviorSanitizer, frame pointers kept,
#                              UB made fatal. MSVC (19.28+): AddressSanitizer
#                              only (MSVC has no UBSan).
#   DS1EDIT_ENABLE_COVERAGE    GCC: --coverage (read with `gcov --json-format`).
#                              Clang/AppleClang: source-based coverage (read
#                              with `llvm-cov export`). Always -O0.
#
# Unsupported combinations stop the configure with an explicit message rather
# than silently producing an uninstrumented build that would be mistaken for
# a passing lane.

option(DS1EDIT_ENABLE_SANITIZERS
    "Build every target with AddressSanitizer/UBSan (instrumented lane only)" OFF)
option(DS1EDIT_ENABLE_COVERAGE
    "Build every target with coverage instrumentation (instrumented lane only)" OFF)

if(DS1EDIT_ENABLE_SANITIZERS AND DS1EDIT_ENABLE_COVERAGE)
    message(FATAL_ERROR
        "DS1EDIT_ENABLE_SANITIZERS and DS1EDIT_ENABLE_COVERAGE are mutually "
        "exclusive: sanitizer runtimes distort coverage and coverage "
        "instrumentation distorts sanitizer reports. Use two build trees.")
endif()

# MSVC frontends: cl.exe is "MSVC"; clang-cl reports "Clang" with MSVC set.
set(_ds1_cc "${CMAKE_C_COMPILER_ID}")
if(_ds1_cc STREQUAL "AppleClang")
    set(_ds1_cc "Clang")
endif()

if(DS1EDIT_ENABLE_SANITIZERS)
    if(_ds1_cc STREQUAL "MSVC")
        if(CMAKE_C_COMPILER_VERSION VERSION_LESS 19.28)
            message(FATAL_ERROR
                "DS1EDIT_ENABLE_SANITIZERS needs MSVC 19.28 (VS 2019 16.9) or "
                "newer for /fsanitize=address; found ${CMAKE_C_COMPILER_VERSION}.")
        endif()
        message(STATUS "Sanitizers: MSVC AddressSanitizer (no UBSan on MSVC)")
        add_compile_options(/fsanitize=address /Zi)
        # ASan is incompatible with the runtime checks and incremental linking
        # that the default Debug flags enable.
        foreach(_cfg DEBUG RELWITHDEBINFO)
            string(REGEX REPLACE "/RTC[1csu]+" "" CMAKE_C_FLAGS_${_cfg} "${CMAKE_C_FLAGS_${_cfg}}")
        endforeach()
        add_link_options(/INCREMENTAL:NO /DEBUG)
    elseif(_ds1_cc STREQUAL "GNU" OR _ds1_cc STREQUAL "Clang")
        if(MINGW OR (WIN32 AND NOT MSVC))
            message(FATAL_ERROR
                "DS1EDIT_ENABLE_SANITIZERS is not supported with ${CMAKE_C_COMPILER_ID} "
                "on Windows (no ASan/UBSan runtime for MinGW). Use MSVC "
                "(AddressSanitizer) or run the Linux lane.")
        endif()
        message(STATUS "Sanitizers: address,undefined (${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION})")
        add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined
                            -fno-omit-frame-pointer -g)
        add_link_options(-fsanitize=address,undefined)
    else()
        message(FATAL_ERROR
            "DS1EDIT_ENABLE_SANITIZERS: unsupported compiler '${CMAKE_C_COMPILER_ID}'. "
            "Supported: GCC, Clang, AppleClang, MSVC 19.28+.")
    endif()
endif()

if(DS1EDIT_ENABLE_COVERAGE)
    if(CMAKE_BUILD_TYPE MATCHES "^(Release|MinSizeRel|RelWithDebInfo)$")
        message(FATAL_ERROR
            "DS1EDIT_ENABLE_COVERAGE with CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}: "
            "optimization merges and deletes lines, so coverage would be "
            "misleading. Configure with -DCMAKE_BUILD_TYPE=Debug.")
    endif()
    if(_ds1_cc STREQUAL "GNU")
        message(STATUS "Coverage: gcov (${CMAKE_C_COMPILER_VERSION}); read with gcov --json-format")
        add_compile_options(--coverage -O0 -g)
        add_link_options(--coverage)
    elseif(_ds1_cc STREQUAL "Clang" AND NOT MSVC)
        message(STATUS "Coverage: llvm source-based (${CMAKE_C_COMPILER_VERSION}); read with llvm-cov export")
        add_compile_options(-fprofile-instr-generate -fcoverage-mapping -O0 -g)
        add_link_options(-fprofile-instr-generate)
    else()
        message(FATAL_ERROR
            "DS1EDIT_ENABLE_COVERAGE: unsupported compiler '${CMAKE_C_COMPILER_ID}' "
            "(MSVC and clang-cl have no supported machine-readable coverage "
            "format here). Use GCC (gcov) or Clang (llvm-cov) on Linux/macOS.")
    endif()
endif()
unset(_ds1_cc)
