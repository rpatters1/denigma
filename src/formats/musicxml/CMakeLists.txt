include(FetchContent)

# Define a cache variable for the local mx MusicXML DOM path relative to the source directory.
set(MX_LOCAL_PATH "" CACHE STRING "Path to local mx MusicXML DOM checkout relative to the source directory (if provided)")
set(_denigma_mx_git_repository "git@github.com:webern/mx.git")

if(MX_LOCAL_PATH)
    # Combine the source directory with the relative path to get an absolute path.
    get_filename_component(MX_LOCAL_PATH_ABS "${PROJECT_SOURCE_DIR}/${MX_LOCAL_PATH}" ABSOLUTE)
    if(EXISTS "${MX_LOCAL_PATH_ABS}/CMakeLists.txt")
        message(STATUS "Local mx MusicXML DOM found at ${MX_LOCAL_PATH_ABS}")
        # Set the FetchContent source directory to the computed absolute path.
        set(FETCHCONTENT_SOURCE_DIR_MX "${MX_LOCAL_PATH_ABS}" CACHE INTERNAL "")
    else()
        message(FATAL_ERROR "Local mx MusicXML DOM not found at ${MX_LOCAL_PATH_ABS}")
    endif()
else()
    message(STATUS "Using GitHub for mx MusicXML DOM (${MX_GIT_TAG_OR_BRANCH})")
endif()

FetchContent_Declare(
    mx
    GIT_REPOSITORY ${_denigma_mx_git_repository}
    GIT_TAG        ${MX_GIT_TAG_OR_BRANCH}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

get_directory_property(_denigma_musicxml_compile_options COMPILE_OPTIONS)
set_directory_properties(PROPERTIES COMPILE_OPTIONS "")
FetchContent_MakeAvailable(mx)
set_directory_properties(PROPERTIES COMPILE_OPTIONS "${_denigma_musicxml_compile_options}")
unset(_denigma_musicxml_compile_options)

FetchContent_GetProperties(mx)
message(STATUS "mx MusicXML DOM Source Directory: ${mx_SOURCE_DIR}")

set(DENIGMA_FORMAT_MUSICXML_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/musicxml.cpp
)

add_denigma_internal_library(denigma_musicxml MUSX_PCH ${DENIGMA_FORMAT_MUSICXML_SOURCES})
add_library(denigma::musicxml ALIAS denigma_musicxml)
add_library(denigma_format_musicxml ALIAS denigma_musicxml)
# MusicXML export will use mx::api for the target DOM. This target is currently
# a dependency probe/stub so FetchContent and link issues surface before the
# converter implementation is added.
target_link_libraries(denigma_musicxml
    PUBLIC
        denigma_core
    PRIVATE
        denigma_format_enigmaxml
        mx
        musx
)
