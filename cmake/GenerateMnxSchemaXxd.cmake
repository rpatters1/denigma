# GenerateMnxSchemaXxd.cmake

# Define the paths
set(MNX_SCHEMA_DIR "${CMAKE_SOURCE_DIR}/schema")
set(MNX_SCHEMA_JSON "${MNX_SCHEMA_DIR}/mnx-schema.json")
set(GENERATED_MNX_XXD "${GENERATED_DIR}/mnx_schema.xxd")

# Step 2: Convert mnx-schema.json to mnx_schema.xxd
add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/generated/mnx_schema.xxd"
    COMMAND ${CMAKE_COMMAND} -E echo "Generating mnx_schema.xxd..."
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GENERATED_DIR}"
    COMMAND ${CMAKE_COMMAND} -E chdir "${MNX_SCHEMA_DIR}"
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
