set(DENIGMA_FORMAT_ENIGMAXML_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/enigmaxml.cpp
    ${CMAKE_CURRENT_LIST_DIR}/enigmaxml_converter.cpp
)

add_denigma_internal_library(denigma_format_enigmaxml ${DENIGMA_FORMAT_ENIGMAXML_SOURCES})
# This adapter handles musx archive extraction and EnigmaXML pass-through. Keep
# MNX/MSS/SVG/json/textmetrics dependencies out of this target.
target_link_libraries(denigma_format_enigmaxml
    PUBLIC
        denigma_core
    PRIVATE
        denigma_utils
        denigma_minizip
        ${_denigma_zlib_target}
        musx
)
