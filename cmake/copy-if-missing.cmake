# copy-if-missing.cmake -- Copy SRC to DST only if DST does not exist.
# Usage: cmake -DSRC=source -DDST=dest -P copy-if-missing.cmake
#
# Note: DESTINATION must be a real parent directory. Passing "${DST}/.."
# makes CMake materialise DST itself as a directory in order to resolve the
# "..", after which the RENAME below fails with "Is a directory".
if(NOT EXISTS "${DST}")
    get_filename_component(DST_DIR "${DST}" DIRECTORY)
    get_filename_component(DST_NAME "${DST}" NAME)
    get_filename_component(SRC_NAME "${SRC}" NAME)

    file(MAKE_DIRECTORY "${DST_DIR}")
    file(COPY "${SRC}" DESTINATION "${DST_DIR}")

    if(NOT SRC_NAME STREQUAL DST_NAME)
        file(RENAME "${DST_DIR}/${SRC_NAME}" "${DST}")
    endif()
    message(STATUS "Created ${DST} from ${SRC}")
endif()
