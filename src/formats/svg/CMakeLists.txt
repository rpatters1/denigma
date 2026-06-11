set(DENIGMA_FORMAT_SVG_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/svg.cpp
    ${CMAKE_CURRENT_LIST_DIR}/svg_converter.cpp
)

add_denigma_internal_library(denigma_format_svg ${DENIGMA_FORMAT_SVG_SOURCES})
target_link_libraries(denigma_format_svg
    PUBLIC
        denigma_core
    PRIVATE
        denigma_format_enigmaxml
        denigma_utils
        musx
)

if(denigma_BUILD_TESTING)
    add_denigma_internal_test_library(denigma_format_svg_test ${DENIGMA_FORMAT_SVG_SOURCES})
    target_link_libraries(denigma_format_svg_test
        PUBLIC
            denigma_core_test
        PRIVATE
            denigma_format_enigmaxml_test
            denigma_utils
            musx
    )
endif()
