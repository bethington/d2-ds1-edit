# Sanitizer lane control (release-core: control.sanitizer.detector).
#
# Registered only in an instrumented configuration
# (-DDS1EDIT_ENABLE_SANITIZERS=ON). test_sanitizer_detector overflows a heap
# block on purpose; the CTest passes only when the sanitizer's report names the
# overflow (PASS_REGULAR_EXPRESSION overrides the exit status), so a build that
# is not really instrumented fails here instead of vouching for sanitizer.core.
# The `portable` label puts it in the sanitizer job's `ctest -L portable` run.

if(DS1EDIT_ENABLE_SANITIZERS)
    add_executable(test_sanitizer_detector test_sanitizer_detector.c)
    add_test(NAME sanitizer_detector_control COMMAND test_sanitizer_detector)
    set_tests_properties(sanitizer_detector_control PROPERTIES
        LABELS portable
        TIMEOUT 60
        # Keep the report on stderr and make sure the process aborts on it.
        ENVIRONMENT "ASAN_OPTIONS=abort_on_error=0:detect_leaks=0"
        PASS_REGULAR_EXPRESSION "AddressSanitizer: heap-buffer-overflow")
endif()
