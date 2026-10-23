# Python unit tests (test/python/test_*.py), included from test/CMakeLists.txt
# after find_package(Python3 COMPONENTS Interpreter).
#
# Each file becomes its own CTest, py_<name> for test_<name>.py, run as
#   python -m unittest -v test.python.test_<name>
# from the repository root so that both `scripts.` and `test.python.` import.
# They use authored fixtures and mocked subprocesses only: no game data, no
# real renders, so they carry the `portable` label and run on every platform.
#
# A missing interpreter, or no test files at all, registers a test that FAILS
# with an explanation. Configure still succeeds, but the absence cannot pass
# for a green run.

get_filename_component(_ds1_repo_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
file(GLOB _ds1_py_tests CONFIGURE_DEPENDS
    "${_ds1_repo_root}/test/python/test_*.py")
list(SORT _ds1_py_tests)

function(_ds1_failing_python_test name reason)
    set(_script "${CMAKE_CURRENT_BINARY_DIR}/${name}.cmake")
    file(WRITE "${_script}" "message(FATAL_ERROR [==[${reason}]==])\n")
    add_test(NAME ${name} COMMAND ${CMAKE_COMMAND} -P "${_script}")
    set_tests_properties(${name} PROPERTIES LABELS portable TIMEOUT 60)
endfunction()

if(NOT Python3_Interpreter_FOUND)
    message(WARNING "Python 3 not found: the Python unit tests will be "
                    "registered as the failing test py_unavailable.")
    _ds1_failing_python_test(py_unavailable
        "Python 3 was not found when this build was configured, so the Python unit tests in test/python could not run. Install Python 3 and the packages in test/python/requirements.txt (pip install -r test/python/requirements.txt), then re-run CMake configure.")
elseif(NOT _ds1_py_tests)
    _ds1_failing_python_test(py_missing
        "No test/python/test_*.py files were found; the Python unit tests are missing from this checkout.")
else()
    foreach(_file IN LISTS _ds1_py_tests)
        get_filename_component(_stem "${_file}" NAME_WE)
        string(REGEX REPLACE "^test_" "" _name "${_stem}")
        add_test(NAME py_${_name}
            COMMAND ${Python3_EXECUTABLE} -m unittest -v test.python.${_stem}
            WORKING_DIRECTORY "${_ds1_repo_root}")
        set_tests_properties(py_${_name} PROPERTIES
            LABELS portable
            TIMEOUT 60
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
    endforeach()
    # Lanes that drive a native probe get its path; without it they fail
    # rather than skip.
    if(TARGET test_ds1_reference_probe AND TEST py_ds1_cases)
        set_tests_properties(py_ds1_cases PROPERTIES ENVIRONMENT
            "PYTHONDONTWRITEBYTECODE=1;DS1EDIT_PROBE_EXE=$<TARGET_FILE:test_ds1_reference_probe>")
    endif()
endif()
