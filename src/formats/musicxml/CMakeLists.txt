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

set(MX_API ON CACHE BOOL "Build the mx::api/mx::impl product library" FORCE)
set(MX_CORE_DEV OFF CACHE BOOL "Build mx core development test targets" FORCE)
set(MX_COVERAGE OFF CACHE BOOL "Instrument mx with coverage flags" FORCE)

get_directory_property(_denigma_musicxml_compile_options COMPILE_OPTIONS)
set_directory_properties(PROPERTIES COMPILE_OPTIONS "")
FetchContent_MakeAvailable(mx)
set_directory_properties(PROPERTIES COMPILE_OPTIONS "${_denigma_musicxml_compile_options}")
unset(_denigma_musicxml_compile_options)

FetchContent_GetProperties(mx)
message(STATUS "mx MusicXML DOM Source Directory: ${mx_SOURCE_DIR}")

if(TARGET mx_core)
    set_target_properties(mx_core PROPERTIES UNITY_BUILD ON)
endif()
if(TARGET mx)
    set_target_properties(mx PROPERTIES UNITY_BUILD ON)
endif()

foreach(_denigma_mx_non_product_target IN ITEMS
        mxtest-core
        mxtest-core-dev
        mxtest-validate
        mxtest
        mxtest-api-roundtrip
        mxread
        mxwrite
        mxhide)
    if(TARGET ${_denigma_mx_non_product_target})
        set_target_properties(${_denigma_mx_non_product_target} PROPERTIES
            EXCLUDE_FROM_ALL TRUE
            EXCLUDE_FROM_DEFAULT_BUILD TRUE
        )
    endif()
endforeach()
unset(_denigma_mx_non_product_target)

function(denigma_read_pugixml_version out_var header_path)
    unset(_denigma_pugixml_version)
    if(EXISTS "${header_path}")
        file(STRINGS "${header_path}" _denigma_pugixml_version_line
            REGEX "^[ \t]*#[ \t]*define[ \t]+PUGIXML_VERSION[ \t]+[0-9]+"
        )
        if(_denigma_pugixml_version_line MATCHES "PUGIXML_VERSION[ \t]+([0-9]+)")
            set(_denigma_pugixml_version "${CMAKE_MATCH_1}")
        endif()
    endif()
    set(${out_var} "${_denigma_pugixml_version}" PARENT_SCOPE)
endfunction()

denigma_read_pugixml_version(
    _denigma_pugixml_version
    "${pugixml_SOURCE_DIR}/src/pugixml.hpp"
)
denigma_read_pugixml_version(
    _mx_vendored_pugixml_version
    "${mx_SOURCE_DIR}/src/private/pugixml/pugixml.hpp"
)
if(_denigma_pugixml_version
        AND _mx_vendored_pugixml_version
        AND _denigma_pugixml_version LESS _mx_vendored_pugixml_version)
    message(WARNING
        "Denigma pugixml (${_denigma_pugixml_version}) is older than "
        "mx vendored pugixml (${_mx_vendored_pugixml_version})."
    )
endif()

set(DENIGMA_FORMAT_MUSICXML_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/musicxml.cpp
    ${CMAKE_CURRENT_LIST_DIR}/musicxml_articulations.cpp
    ${CMAKE_CURRENT_LIST_DIR}/musicxml_converter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/musicxml_defaults.cpp
    ${CMAKE_CURRENT_LIST_DIR}/musicxml_enums.cpp
    ${CMAKE_CURRENT_LIST_DIR}/musicxml_formatted_text.cpp
    ${CMAKE_CURRENT_LIST_DIR}/musicxml_instruments.cpp
    ${CMAKE_CURRENT_LIST_DIR}/musicxml_mapping.cpp
    ${CMAKE_CURRENT_LIST_DIR}/musicxml_measures.cpp
    ${CMAKE_CURRENT_LIST_DIR}/musicxml_metadata.cpp
    ${CMAKE_CURRENT_LIST_DIR}/musicxml_notes.cpp
    ${CMAKE_CURRENT_LIST_DIR}/musicxml_parts.cpp
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
        denigma_font_names
        mx
        musx
)
