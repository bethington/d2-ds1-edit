# clean-bin.cmake — Remove bin/ contents except Ds1edit.ini (user config)
# Usage: cmake -P cmake/clean-bin.cmake  (run from project root)

set(BIN_DIR "${CMAKE_CURRENT_LIST_DIR}/../bin")

if(NOT EXISTS "${BIN_DIR}")
    message(STATUS "bin/ does not exist, nothing to clean")
    return()
endif()

# Back up Ds1edit.ini if it exists
set(HAS_INI FALSE)
if(EXISTS "${BIN_DIR}/Ds1edit.ini")
    file(COPY "${BIN_DIR}/Ds1edit.ini" DESTINATION "${CMAKE_CURRENT_LIST_DIR}/../")
    set(HAS_INI TRUE)
    message(STATUS "Backed up Ds1edit.ini")
endif()

# Remove entire bin/ directory
file(REMOVE_RECURSE "${BIN_DIR}")
message(STATUS "Removed bin/")

# Recreate bin/ and restore Ds1edit.ini
file(MAKE_DIRECTORY "${BIN_DIR}")
if(HAS_INI)
    file(COPY "${CMAKE_CURRENT_LIST_DIR}/../Ds1edit.ini" DESTINATION "${BIN_DIR}")
    file(REMOVE "${CMAKE_CURRENT_LIST_DIR}/../Ds1edit.ini")
    message(STATUS "Restored Ds1edit.ini")
endif()

message(STATUS "bin/ cleaned. Run a build to repopulate.")
