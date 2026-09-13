add_denigma_internal_library(denigma_gap_report
    ${CMAKE_CURRENT_LIST_DIR}/gap_report.cpp
)
add_library(denigma::gap-report ALIAS denigma_gap_report)
target_link_libraries(denigma_gap_report
    PUBLIC denigma_conversion
    PRIVATE nlohmann_json::nlohmann_json
)