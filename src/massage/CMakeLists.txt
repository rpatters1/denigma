set(DENIGMA_MASSAGE_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/massage.cpp
    ${CMAKE_CURRENT_LIST_DIR}/musicxml.cpp
)

add_denigma_internal_library(denigma_massage ${DENIGMA_MASSAGE_SOURCES})
target_link_libraries(denigma_massage
    PUBLIC
        denigma_core
    PRIVATE
        denigma_format_enigmaxml
        denigma_utils
        pugixml
        musx
)

if(denigma_BUILD_TESTING)
    add_denigma_internal_test_library(denigma_massage_test ${DENIGMA_MASSAGE_SOURCES})
    target_link_libraries(denigma_massage_test
        PUBLIC
            denigma_core_test
        PRIVATE
            denigma_format_enigmaxml_test
            denigma_utils
            pugixml
            musx
    )
endif()
