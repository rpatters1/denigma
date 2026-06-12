set(DENIGMA_EXPORT_COMMAND_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/export.cpp
)

add_denigma_internal_library(denigma_export ${DENIGMA_EXPORT_COMMAND_SOURCES})
# Export is a CLI orchestration layer and intentionally aggregates all format
# libraries. External projects that need one converter should link that converter
# directly instead of linking denigma_export.
target_link_libraries(denigma_export PUBLIC
    denigma_core
    denigma_format_enigmaxml
    denigma_format_mnx
    denigma_format_mss
    denigma_format_svg
)

if(denigma_BUILD_TESTING)
    add_denigma_internal_test_library(denigma_export_test ${DENIGMA_EXPORT_COMMAND_SOURCES})
    target_link_libraries(denigma_export_test PUBLIC
        denigma_core_test
        denigma_format_enigmaxml
        denigma_format_mnx
        denigma_format_mss
        denigma_format_svg
    )
endif()
