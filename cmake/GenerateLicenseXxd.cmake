# GenerateLicenseXxd.cmake


# Define parallel lists for file paths and their corresponding prefixes
set(LICENSE_FILES
    "${CMAKE_SOURCE_DIR}/LICENSE"
    "${musx_SOURCE_DIR}/LICENSE"
    "${ezgz_SOURCE_DIR}/LICENSE"
    "${pugixml_SOURCE_DIR}/LICENSE.md"
    "${miniz-cpp_SOURCE_DIR}/LICENSE.md"
)

set(LICENSE_PREFIXES
    denigma
    musx
    ezgz
    pugixml
    miniz_cpp
)

# Ensure both lists have the same length
list(LENGTH LICENSE_FILES FILES_LENGTH)
list(LENGTH LICENSE_PREFIXES PREFIXES_LENGTH)
if(NOT FILES_LENGTH EQUAL PREFIXES_LENGTH)
    message(FATAL_ERROR "LICENSE_FILES and LICENSE_PREFIXES must have the same length.")
endif()
math(EXPR LAST_INDEX "${FILES_LENGTH} - 1")

# Generate the corresponding xxd files
foreach(INDEX RANGE 0 ${LAST_INDEX})
    list(GET LICENSE_FILES ${INDEX} LICENSE_FILE)
    list(GET LICENSE_PREFIXES ${INDEX} SAFE_NAME_PREFIX)

    set(GENERATED_XXD "${GENERATED_DIR}/${SAFE_NAME_PREFIX}_license.xxd")
    
    # Extract the directory of the license file
    get_filename_component(LICENSE_DIR "${LICENSE_FILE}" DIRECTORY)
    get_filename_component(LICENSE_FILENAME "${LICENSE_FILE}" NAME)
    message(STATUS "License directory: ${LICENSE_DIR}, Name: ${LICENSE_FILENAME}, Prefix: ${SAFE_NAME_PREFIX}")

    add_custom_command(
        OUTPUT "${GENERATED_XXD}"
        COMMAND ${CMAKE_COMMAND} -E echo "Generating ${SAFE_NAME_PREFIX}_license.xxd..."
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GENERATED_DIR}"
        COMMAND ${CMAKE_COMMAND} -E chdir "${LICENSE_DIR}" xxd -i "${LICENSE_FILENAME}" > "${GENERATED_XXD}"
        DEPENDS "${LICENSE_FILE}"
        COMMENT "Converting ${LICENSE_FILE} to ${SAFE_NAME_PREFIX}_license.xxd"
        VERBATIM
    )

    # Add each generated file as a dependency of the overall target
    list(APPEND GENERATED_XXD_FILES "${GENERATED_XXD}")
endforeach()

# Create a custom target for all generated xxd files
add_custom_target(
    GenerateLicenseXxd ALL
    DEPENDS ${GENERATED_XXD_FILES}
    COMMENT "Generating all license xxd files"
)
