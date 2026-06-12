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

add_denigma_internal_library(denigma_format_mnx ${DENIGMA_FORMAT_MNX_SOURCES})
# MNX needs mnxdom/json and classifiers, but it does not use text metrics.
# Keep denigma_textmetrics out of this target unless MNX starts measuring glyphs.
target_link_libraries(denigma_format_mnx
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
# This opt-in is local because only mnx.cpp instantiates musx::xml::pugi::Document.
# Do not re-enable MUSX_USE_PUGIXML globally on musx.
target_compile_definitions(denigma_format_mnx PRIVATE MUSX_USE_PUGIXML)
