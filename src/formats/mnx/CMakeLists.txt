set(MNXDOM_LOCAL_PATH "" CACHE STRING "Path to local MNX C++ DOM checkout relative to the source directory (if provided)")
set(_denigma_mnxdom_git_repository "https://github.com/rpatters1/mnxdom.git")
include("${PROJECT_SOURCE_DIR}/cmake/GitRefUtils.cmake")

if(MNXDOM_LOCAL_PATH)
    get_filename_component(MNXDOM_LOCAL_PATH_ABS "${PROJECT_SOURCE_DIR}/${MNXDOM_LOCAL_PATH}" ABSOLUTE)
    if(EXISTS "${MNXDOM_LOCAL_PATH_ABS}/CMakeLists.txt")
        message(STATUS "Local MNX C++ DOM found at ${MNXDOM_LOCAL_PATH_ABS}")
        set(FETCHCONTENT_SOURCE_DIR_MNXDOM "${MNXDOM_LOCAL_PATH_ABS}" CACHE INTERNAL "")
        if(GIT_FOUND)
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
                WORKING_DIRECTORY "${MNXDOM_LOCAL_PATH_ABS}"
                OUTPUT_VARIABLE _denigma_mnxdom_resolved_git_commit
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
            if(_denigma_mnxdom_resolved_git_commit)
                set(MNXDOM_GIT_COMMIT "${_denigma_mnxdom_resolved_git_commit}" CACHE STRING "mnxdom source control revision to embed in provenance" FORCE)
            endif()
        endif()
    else()
        message(FATAL_ERROR "Local MNX C++ DOM not found at ${MNXDOM_LOCAL_PATH_ABS}")
    endif()
else()
    message(STATUS "Using GitHub for MNX C++ DOM (${MNXDOM_GIT_TAG_OR_BRANCH})")
    denigma_resolve_git_ref_commit(_denigma_mnxdom_resolved_git_commit "${_denigma_mnxdom_git_repository}" "${MNXDOM_GIT_TAG_OR_BRANCH}")
    if(_denigma_mnxdom_resolved_git_commit)
        set(MNXDOM_GIT_COMMIT "${_denigma_mnxdom_resolved_git_commit}" CACHE STRING "mnxdom source control revision to embed in provenance" FORCE)
    endif()
endif()
if(MNXDOM_GIT_COMMIT)
    message(STATUS "mnxdom Git Commit: ${MNXDOM_GIT_COMMIT}")
else()
    message(WARNING "Unable to resolve mnxdom Git commit from ${MNXDOM_GIT_TAG_OR_BRANCH}")
endif()

option(mnxdom_BUILD_TESTING "Enable testing mnxdom" OFF)
FetchContent_Declare(
    mnxdom
    GIT_REPOSITORY ${_denigma_mnxdom_git_repository}
    GIT_TAG        ${MNXDOM_GIT_TAG_OR_BRANCH}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(mnxdom)

FetchContent_GetProperties(mnxdom)
message(STATUS "mnxdom Source Directory: ${mnxdom_SOURCE_DIR}")

set(DENIGMA_FORMAT_MNX_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/mnx.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mnx_converter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mnx_enums.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mnx_expressions.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mnx_formatted_text.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mnx_global.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mnx_layouts.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mnx_mapping.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mnx_markings.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mnx_parts.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mnx_sequences.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mnx_smartshapes.cpp
)

add_denigma_internal_library(denigma_mnx MUSX_PCH ${DENIGMA_FORMAT_MNX_SOURCES})
add_library(denigma::mnx ALIAS denigma_mnx)
add_library(denigma_format_mnx ALIAS denigma_mnx)
# MNX needs mnxdom/json and classifiers, but it does not use text metrics.
# Keep denigma_textmetrics out of this target unless MNX starts measuring glyphs.
target_link_libraries(denigma_mnx
    PUBLIC
        denigma_core
        denigma_classify
        denigma_utils
        musx
    PRIVATE
        denigma_format_enigmaxml
        mnxdom
        pugixml
)
