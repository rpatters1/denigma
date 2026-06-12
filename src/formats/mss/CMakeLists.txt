set(DENIGMA_FORMAT_MSS_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/mss.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mss_converter.cpp
)

add_denigma_internal_library(denigma_mss ${DENIGMA_FORMAT_MSS_SOURCES})
add_library(denigma::mss ALIAS denigma_mss)
add_library(denigma_format_mss ALIAS denigma_mss)
# MSS measures text and parses XML, so textmetrics and pugixml belong here. Keep
# them PRIVATE so consumers of other format libraries do not inherit them.
target_link_libraries(denigma_mss
    PUBLIC
        denigma_core
    PRIVATE
        denigma_format_enigmaxml
        denigma_textmetrics
        denigma_utils
        pugixml
        musx
)
