set(DENIGMA_CORE_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/denigma.cpp
    ${CMAKE_CURRENT_LIST_DIR}/finale_options.cpp
)

# Core is the base dependency for every converter. Do not add parser backends,
# archive readers, text metrics, json, or format-specific libraries here unless
# every external consumer should inherit them.
add_denigma_internal_library(denigma_core MUSX_PCH ${DENIGMA_CORE_SOURCES})
target_link_libraries(denigma_core PUBLIC musx)

if(denigma_BUILD_TESTING)
    add_denigma_internal_test_library(denigma_core_test ${DENIGMA_CORE_SOURCES})
    target_link_libraries(denigma_core_test PUBLIC musx)
endif()
