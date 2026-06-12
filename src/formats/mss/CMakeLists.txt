set(DENIGMA_FORMAT_MSS_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/mss.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mss_converter.cpp
)

add_denigma_internal_library(denigma_format_mss ${DENIGMA_FORMAT_MSS_SOURCES})
# MSS measures text and parses XML, so textmetrics and pugixml belong here. Keep
# them PRIVATE so consumers of other format libraries do not inherit them.
target_link_libraries(denigma_format_mss
    PUBLIC
        denigma_core
    PRIVATE
        denigma_format_enigmaxml
        denigma_textmetrics
        denigma_utils
        pugixml
        musx
)
# This opt-in is local because only this adapter constructs pugi-backed musx
# documents. Do not move MUSX_USE_PUGIXML to the musx target.
target_compile_definitions(denigma_format_mss PRIVATE MUSX_USE_PUGIXML)
