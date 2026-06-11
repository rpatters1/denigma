set(DENIGMA_UTILS_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/textmetrics.cpp
    ${CMAKE_CURRENT_LIST_DIR}/smufl_support.cpp
    ${CMAKE_CURRENT_LIST_DIR}/utf8_iterator.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ziputils.cpp
)

add_denigma_internal_library(denigma_utils ${DENIGMA_UTILS_SOURCES})
target_link_libraries(denigma_utils
    PUBLIC
        denigma_core
        musx
        pugixml
    PRIVATE
        denigma_minizip
        ${_denigma_zlib_target}
        nlohmann_json::nlohmann_json
        pugixml
        smufl_mapping
        ${DENIGMA_TEXT_METRICS_LIBS}
)
target_include_directories(denigma_utils PRIVATE ${DENIGMA_TEXT_METRICS_INCLUDE_DIRS})
target_compile_definitions(denigma_utils PRIVATE ${DENIGMA_TEXT_METRICS_DEFINES})
