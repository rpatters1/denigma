set(DENIGMA_FORMAT_MSS_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/../../export/mss.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../export/mss_converter.cpp
)

add_denigma_internal_library(denigma_format_mss ${DENIGMA_FORMAT_MSS_SOURCES})
target_link_libraries(denigma_format_mss
    PUBLIC
        denigma_core
    PRIVATE
        denigma_utils
        pugixml
        musx
)

if(denigma_BUILD_TESTING)
    add_denigma_internal_test_library(denigma_format_mss_test ${DENIGMA_FORMAT_MSS_SOURCES})
    target_link_libraries(denigma_format_mss_test
        PUBLIC
            denigma_core_test
        PRIVATE
            denigma_utils
            pugixml
            musx
    )
endif()
