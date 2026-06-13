set(DENIGMA_FORMAT_MNX_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/mnx.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mnx_converter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mnx_enums.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mnx_formatted_text.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mnx_global.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mnx_layouts.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mnx_mapping.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mnx_markings.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mnx_parts.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mnx_sequences.cpp
)

add_denigma_internal_library(denigma_mnx MUSX_PCH ${DENIGMA_FORMAT_MNX_SOURCES})
add_library(denigma::mnx ALIAS denigma_mnx)
add_library(denigma_format_mnx ALIAS denigma_mnx)
# MNX needs mnxdom/json and classifiers, but it does not use text metrics.
# Keep denigma_textmetrics out of this target unless MNX starts measuring glyphs.
target_link_libraries(denigma_mnx
    PUBLIC
        denigma_core
        denigma_classify
        denigma_utils
        mnxdom
        musx
    PRIVATE
        denigma_format_enigmaxml
        pugixml
)
