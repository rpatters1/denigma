# WebAssembly module exposing the MUSX inspection and conversion entry points
# through a small C ABI. Denigma Online consumes the denigma.js/denigma.wasm pair
# this target emits, so the exported function list below is that site's contract.

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "denigma_BUILD_WASM requires the Emscripten toolchain (configure with emcmake).")
endif()

add_executable(denigma_wasm denigma_wasm.cpp)
target_compile_options(denigma_wasm PRIVATE ${DENIGMA_WARNING_OPTIONS})
target_link_libraries(denigma_wasm PRIVATE
    denigma::enigmaxml
    denigma::gap-report
    denigma::mnx
    denigma::musicxml
    denigma_internal_deps
)

set(DENIGMA_WASM_OUTPUT_DIR "${PROJECT_BINARY_DIR}/wasm")
set_target_properties(denigma_wasm PROPERTIES
    OUTPUT_NAME denigma
    SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY "${DENIGMA_WASM_OUTPUT_DIR}"
)

set(_denigma_wasm_exports
    _denigma_malloc
    _denigma_free
    _denigma_inspect
    _denigma_convert
    _denigma_result_destroy
    _denigma_result_success
    _denigma_result_diagnostic_count
    _denigma_result_diagnostic_severity
    _denigma_result_diagnostic_message
    _denigma_result_score_name
    _denigma_result_score_page_width_mm
    _denigma_result_score_page_height_mm
    _denigma_result_score_spatium_mm
    _denigma_result_score_has_page_margins
    _denigma_result_score_page_margin_top_sp
    _denigma_result_score_page_margin_bottom_sp
    _denigma_result_score_page_margin_left_sp
    _denigma_result_score_page_margin_right_sp
    _denigma_result_part_count
    _denigma_result_part_id
    _denigma_result_part_name
    _denigma_result_part_output_index
    _denigma_result_part_page_width_mm
    _denigma_result_part_page_height_mm
    _denigma_result_part_spatium_mm
    _denigma_result_part_has_page_margins
    _denigma_result_part_page_margin_top_sp
    _denigma_result_part_page_margin_bottom_sp
    _denigma_result_part_page_margin_left_sp
    _denigma_result_part_page_margin_right_sp
    _denigma_result_output_count
    _denigma_result_output_name
    _denigma_result_output_data
    _denigma_result_output_size
    _denigma_result_output_index
    _denigma_result_gap_report_data
    _denigma_result_gap_report_size
    _denigma_version
    _denigma_commit
)
list(JOIN _denigma_wasm_exports "," _denigma_wasm_exports)

target_compile_options(denigma_wasm PRIVATE -Oz)
target_link_options(denigma_wasm PRIVATE
    -Oz
    "-sMODULARIZE=1"
    "-sEXPORT_ES6=1"
    "-sENVIRONMENT=web,worker,node"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sFILESYSTEM=0"
    "-sASSERTIONS=0"
    "-sEXPORTED_FUNCTIONS=${_denigma_wasm_exports}"
    "-sEXPORTED_RUNTIME_METHODS=HEAPU8,UTF8ToString"
)
unset(_denigma_wasm_exports)
