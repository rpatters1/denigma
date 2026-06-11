set(DENIGMA_FORMAT_ENIGMAXML_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/../../export/enigmaxml.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../export/enigmaxml_converter.cpp
)

add_denigma_internal_library(denigma_format_enigmaxml ${DENIGMA_FORMAT_ENIGMAXML_SOURCES})
target_link_libraries(denigma_format_enigmaxml
    PUBLIC
        denigma_core
    PRIVATE
        denigma_utils
        denigma_minizip
        ${_denigma_zlib_target}
        musx
)

if(denigma_BUILD_TESTING)
    add_denigma_internal_test_library(denigma_format_enigmaxml_test ${DENIGMA_FORMAT_ENIGMAXML_SOURCES})
    target_link_libraries(denigma_format_enigmaxml_test
        PUBLIC
            denigma_core_test
        PRIVATE
            denigma_utils
            denigma_minizip
            ${_denigma_zlib_target}
            musx
    )
endif()
