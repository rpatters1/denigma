set(DENIGMA_FORMAT_SVG_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/svg.cpp
    ${CMAKE_CURRENT_LIST_DIR}/svg_converter.cpp
)

add_denigma_internal_library(denigma_svg MUSX_PCH ${DENIGMA_FORMAT_SVG_SOURCES})
add_library(denigma::svg ALIAS denigma_svg)
add_library(denigma_format_svg ALIAS denigma_svg)
# SVG measures text and parses musx through the pugi backend. Keep textmetrics
# and pugixml PRIVATE so non-SVG consumers do not inherit font/XML backends.
target_link_libraries(denigma_svg
    PUBLIC
        denigma_core
    PRIVATE
        denigma_format_enigmaxml
        denigma_textmetrics
        denigma_utils
        musx
        pugixml
)
