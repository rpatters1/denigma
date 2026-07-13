# The dependencies of each util library are deliberately kept as light as possible.
# Be wary of adding more dependencies, as they bubble up the dependency chain.

add_denigma_internal_library(denigma_utf8
    ${CMAKE_CURRENT_LIST_DIR}/utf8_iterator.cpp
)

add_denigma_internal_library(denigma_font_names
    ${CMAKE_CURRENT_LIST_DIR}/font_names.cpp
)

add_denigma_internal_library(denigma_smufl_support MUSX_PCH
    ${CMAKE_CURRENT_LIST_DIR}/smufl_support.cpp
)
target_link_libraries(denigma_smufl_support
    PUBLIC
        musx
    PRIVATE
        denigma_utf8
        nlohmann_json::nlohmann_json
        smufl_mapping
)

add_denigma_internal_library(denigma_textmetrics MUSX_PCH
    ${CMAKE_CURRENT_LIST_DIR}/textmetrics.cpp
)
include("${PROJECT_SOURCE_DIR}/cmake/TextMetricsDependencies.cmake")
# Text metrics brings platform/font resolver dependencies. Link it directly from
# converters that measure text, such as SVG and MSS; do not add it to
# denigma_utils or MNX/EnigmaXML will inherit the whole font stack.
target_link_libraries(denigma_textmetrics
    PUBLIC
        denigma_core
        musx
    PRIVATE
        denigma_font_names
        denigma_smufl_support
        ${_denigma_textmetrics_libs}
)
target_include_directories(denigma_textmetrics PRIVATE ${_denigma_textmetrics_include_dirs})
target_compile_definitions(denigma_textmetrics PRIVATE ${_denigma_textmetrics_defines})

add_denigma_internal_library(denigma_zip MUSX_PCH
    ${CMAKE_CURRENT_LIST_DIR}/ziputils.cpp
)
# Zip support depends on minizip/zlib and pugixml for container metadata. Keep
# it separate so pure XML/string converters do not pull archive support.
target_link_libraries(denigma_zip
    PUBLIC
        denigma_core
        denigma_io
        pugixml
    PRIVATE
        denigma_minizip
        ${_denigma_zlib_target}
)

add_library(denigma_utils INTERFACE)
# This is a compatibility aggregate for existing internal code. Treat additions
# here as high risk: anything linked here follows every converter that uses
# denigma_utils. Prefer linking the narrow util library at the call site.
target_link_libraries(denigma_utils
    INTERFACE
        denigma_font_names
        denigma_smufl_support
        denigma_utf8
        denigma_zip
)
