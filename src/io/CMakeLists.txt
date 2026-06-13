set(DENIGMA_IO_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/random_access_reader.cpp
)

# Keep IO limited to byte/file access abstractions. XML parsing, zip extraction,
# and format conversion belong in the targets that actually need them.
add_denigma_internal_library(denigma_io ${DENIGMA_IO_SOURCES})
