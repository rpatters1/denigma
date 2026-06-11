set(DENIGMA_FORMAT_MSS_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/mss.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mss_converter.cpp
)

add_denigma_internal_library(denigma_format_mss ${DENIGMA_FORMAT_MSS_SOURCES})
target_link_libraries(denigma_format_mss
    PUBLIC
        denigma_core
    PRIVATE
        denigma_format_enigmaxml
        denigma_utils
        pugixml
        musx
)
target_compile_definitions(denigma_format_mss PRIVATE MUSX_USE_PUGIXML)
