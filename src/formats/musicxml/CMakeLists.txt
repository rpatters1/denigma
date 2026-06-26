set(DENIGMA_FORMAT_MUSICXML_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/musicxml.cpp
)

add_denigma_internal_library(denigma_musicxml MUSX_PCH ${DENIGMA_FORMAT_MUSICXML_SOURCES})
add_library(denigma::musicxml ALIAS denigma_musicxml)
add_library(denigma_format_musicxml ALIAS denigma_musicxml)
# MusicXML export will use mx::api for the target DOM. This target is currently
# a dependency probe/stub so FetchContent and link issues surface before the
# converter implementation is added.
target_link_libraries(denigma_musicxml
    PUBLIC
        denigma_core
    PRIVATE
        denigma_format_enigmaxml
        denigma_mx
        musx
)
