set(DENIGMA_FORMAT_MNX_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/../../export/mnx.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../export/mnx_converter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../export/mnx_enums.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../export/mnx_formatted_text.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../export/mnx_global.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../export/mnx_layouts.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../export/mnx_mapping.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../export/mnx_markings.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../export/mnx_parts.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../export/mnx_sequences.cpp
)

add_denigma_internal_library(denigma_format_mnx ${DENIGMA_FORMAT_MNX_SOURCES})
target_link_libraries(denigma_format_mnx
    PUBLIC
        denigma_core
        denigma_classify
        denigma_utils
        mnxdom
        musx
    PRIVATE
        denigma_format_enigmaxml
)

if(denigma_BUILD_TESTING)
    add_denigma_internal_test_library(denigma_format_mnx_test ${DENIGMA_FORMAT_MNX_SOURCES})
    target_link_libraries(denigma_format_mnx_test
        PUBLIC
            denigma_core_test
            denigma_classify
            denigma_utils
            mnxdom
            musx
        PRIVATE
            denigma_format_enigmaxml_test
    )
endif()
