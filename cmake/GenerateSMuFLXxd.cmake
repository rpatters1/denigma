# GenerateMnxSchemaXxd.cmake

# Define the paths
set(SMUFL_GLYPHNAMES_JSON "${SMUFL_METADATA_DIR}/smufl_glyphnames.zip")
set(GENERATED_GLYPHNAMES_XXD "${GENERATED_DIR}/smufl_glyphnames.xxd")

# Step 2: Convert smufl_glyphnames.zip to smufl_glyphnames.xxd
add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/generated/smufl_glyphnames.xxd"
    COMMAND ${CMAKE_COMMAND} -E echo "Generating smufl_glyphnames.xxd..."
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GENERATED_DIR}"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMUFL_METADATA_DIR}"
            xxd -i "smufl_glyphnames.zip" > "${GENERATED_GLYPHNAMES_XXD}"
    DEPENDS "${SMUFL_GLYPHNAMES_JSON}"
    COMMENT "Converting smufl_glyphnames.zip to smufl_glyphnames.xxd"
    VERBATIM
)

# Step 3: Add the generated file as a dependency for your target
add_custom_target(
    GenerateSMuFLXxd ALL
    DEPENDS ${GENERATED_GLYPHNAMES_XXD}
)
