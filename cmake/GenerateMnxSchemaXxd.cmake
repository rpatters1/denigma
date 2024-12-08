# GenerateMnxSchemaXxd.cmake

# Define the paths
set(MNX_SCHEMA_JSON "${CMAKE_BINARY_DIR}/third_party/mnx-schema.json")
set(GENERATED_MNX_XXD "${GENERATED_DIR}/mnx_schema.xxd")
set(MNX_SCHEMA_URL "https://w3c.github.io/mnx/docs/mnx-schema.json")

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/third_party")

# Only download if the file doesn't exist
if(NOT EXISTS "${MNX_SCHEMA_JSON}")
    message(STATUS "Downloading ${MNX_SCHEMA_URL}...")
    execute_process(
        COMMAND curl --fail --location --output "${MNX_SCHEMA_JSON}" "${MNX_SCHEMA_URL}"
        RESULT_VARIABLE download_result
        OUTPUT_QUIET
        ERROR_QUIET
    )
    
    if(NOT download_result EQUAL 0)
        message(FATAL_ERROR "Failed to download ${MNX_SCHEMA_URL}")
    else()
        message(STATUS "Downloaded ${MNX_SCHEMA_URL} to ${MNX_SCHEMA_JSON}")
    endif()
else()
    message(STATUS "Schema file already exists; skipping download.")
endif()

# Step 2: Convert mnx-schema.json to mnx_schema.xxd
add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/generated/mnx_schema.xxd"
    COMMAND ${CMAKE_COMMAND} -E echo "Generating mnx_schema.xxd..."
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GENERATED_DIR}"
    COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_BINARY_DIR}/third_party"
            xxd -i "mnx-schema.json" > "${GENERATED_MNX_XXD}"
    DEPENDS "${MNX_SCHEMA_JSON}"
    COMMENT "Converting mnx-schema.json to mnx_schema.xxd"
    VERBATIM
)

# Step 3: Add the generated file as a dependency for your target
add_custom_target(
    GenerateMnxSchemaXxd ALL
    DEPENDS ${GENERATED_MNX_XXD}
)
