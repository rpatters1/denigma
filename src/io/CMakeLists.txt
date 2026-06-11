set(DENIGMA_IO_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/random_access_reader.cpp
)

add_denigma_internal_library(denigma_io ${DENIGMA_IO_SOURCES})
