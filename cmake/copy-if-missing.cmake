# copy-if-missing.cmake — Copy SRC to DST only if DST does not exist.
# Usage: cmake -DSRC=source -DDST=dest -P copy-if-missing.cmake
if(NOT EXISTS "${DST}")
    file(COPY "${SRC}" DESTINATION "${DST}/..")
    get_filename_component(SRC_NAME "${SRC}" NAME)
    get_filename_component(DST_DIR "${DST}" DIRECTORY)
    file(RENAME "${DST_DIR}/${SRC_NAME}" "${DST}")
    message(STATUS "Created ${DST} from ${SRC}")
endif()
