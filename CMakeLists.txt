# Minimum version of CMake required
cmake_minimum_required(VERSION 3.22)

# Project name and version
project(denigma VERSION 3.1.0)

# Specify C++ standard and minimum macOS version
set(DENIGMA_CXX_STANDARD "23" CACHE STRING "C++ standard used to build Denigma; must be at least 20")
set_property(CACHE DENIGMA_CXX_STANDARD PROPERTY STRINGS 20 23 26)
if(NOT DENIGMA_CXX_STANDARD MATCHES "^[0-9]+$")
    message(FATAL_ERROR "DENIGMA_CXX_STANDARD must be numeric")
endif()
if(DENIGMA_CXX_STANDARD LESS 20)
    message(FATAL_ERROR "DENIGMA_CXX_STANDARD must be at least 20")
endif()
set(CMAKE_CXX_STANDARD ${DENIGMA_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Prevent in-source builds
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Please use a separate directory for building.\n"
                        "For example:\n"
                        "  cmake -S . -B build\n"
                        "  cmake --build build"
    )
endif()

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    #set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
    #message(STATUS "No build type specified. Defaulting to Release.")
endif()

if (NOT CMAKE_CONFIGURATION_TYPES) # Only applies to single-config generators
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/${CMAKE_BUILD_TYPE})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/${CMAKE_BUILD_TYPE})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/${CMAKE_BUILD_TYPE})
endif()

message(STATUS "C++ Standard: ${DENIGMA_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Configution Types: ${CMAKE_CONFIGURATION_TYPES}")
message(STATUS "Generator: ${CMAKE_GENERATOR}")

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        # Build for both arm64 and x86_64 in Release mode
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures for Release" FORCE)
    else()
        # Use CMAKE_SYSTEM_PROCESSOR for Debug mode to match the current architecture
        if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
            set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "macOS architecture for Debug" FORCE)
        elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "amd64")
            set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "macOS architecture for Debug" FORCE)
        else()
            message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
        endif()
    endif()
    message(STATUS "macOS Deployment Target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
    message(STATUS "macOS Architectures: ${CMAKE_OSX_ARCHITECTURES}")
endif()

if(MSVC)
    add_compile_options(/Zc:__cplusplus) # force MSVC to report correct c++ version
    add_compile_options(/DNOMINMAX) # do not use min and max macros from Win32
    add_compile_options(/bigobj) # stoopid Microsoft can't handle industrial size without holding its hand
    add_compile_options(/Zc:preprocessor) # so stoopid we have to do this in 2026
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
    # Surface mnxdom warnings and fail fast so we can fix them upstream.
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

add_compile_definitions(
    DENIGMA_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    DENIGMA_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    DENIGMA_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    DENIGMA_VERSION="${PROJECT_VERSION}"
)

include(FetchContent)

set(BUILD_SHARED_LIBS OFF)

# Define output directory for executables
set(OUTPUT_DIR "${PROJECT_BINARY_DIR}/build")

#define other directories
set(GENERATED_DIR "${PROJECT_BINARY_DIR}/generated")
set(SMUFL_METADATA_DIR "${CMAKE_CURRENT_LIST_DIR}/smufl")

# Set output directory for compiled executable
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})

# Save the current state and disable deprecation warnings
set(_original_WARN_DEPRECATED ${CMAKE_WARN_DEPRECATED})
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "Suppress deprecation warnings for external projects" FORCE)

# Disable building tests for pugixml
set(PUGIXML_BUILD_TESTS OFF CACHE BOOL "Do not build tests for pugixml")
# Fetch pugixml
FetchContent_Declare(
    pugixml
    URL https://github.com/zeux/pugixml/releases/download/v1.15/pugixml-1.15.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(pugixml)

# Fetch zlib
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "Do not build zlib examples")
FetchContent_Declare(
    zlib
    URL https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz
    URL_HASH SHA256=9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(zlib)

if(TARGET zlib)
    set_target_properties(zlib PROPERTIES
        EXCLUDE_FROM_ALL TRUE
        EXCLUDE_FROM_DEFAULT_BUILD TRUE
    )
endif()

if(TARGET zlibstatic)
    set(_denigma_zlib_target zlibstatic)
elseif(TARGET zlib)
    set(_denigma_zlib_target zlib)
elseif(TARGET ZLIB::ZLIB)
    set(_denigma_zlib_target ZLIB::ZLIB)
else()
    message(FATAL_ERROR "Unable to determine zlib target name after FetchContent_MakeAvailable(zlib).")
endif()
message(STATUS "Using zlib target: ${_denigma_zlib_target}")

set(_denigma_minizip_sources
    "${zlib_SOURCE_DIR}/contrib/minizip/ioapi.c"
    "${zlib_SOURCE_DIR}/contrib/minizip/unzip.c"
    "${zlib_SOURCE_DIR}/contrib/minizip/zip.c"
)
if(WIN32)
    list(APPEND _denigma_minizip_sources "${zlib_SOURCE_DIR}/contrib/minizip/iowin32.c")
endif()

add_library(denigma_minizip STATIC ${_denigma_minizip_sources})
target_include_directories(denigma_minizip PUBLIC
    "${zlib_SOURCE_DIR}"
    "${zlib_SOURCE_DIR}/contrib/minizip"
)
target_link_libraries(denigma_minizip PUBLIC ${_denigma_zlib_target})

include(cmake/Dependencies.cmake) # GitHub branches/tags for MNX and MUSX

set(DENIGMA_TEXT_METRICS_DEFINES "")
set(DENIGMA_TEXT_METRICS_LIBS "")
set(DENIGMA_TEXT_METRICS_INCLUDE_DIRS "")

# Build FreeType from source so we do not require external package installation.
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "Disable HarfBuzz support in FreeType" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "Disable Brotli support in FreeType" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "Disable bzip2 support in FreeType" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "Disable libpng support in FreeType" FORCE)
set(FT_DISABLE_ZLIB ON CACHE BOOL "Disable zlib support in FreeType" FORCE)
set(FT_DISABLE_TESTS ON CACHE BOOL "Disable FreeType tests" FORCE)
set(FT_REQUIRE_ZLIB OFF CACHE BOOL "Do not require zlib for FreeType" FORCE)

FetchContent_Declare(
    freetype
    URL https://download.savannah.gnu.org/releases/freetype/freetype-2.14.1.tar.xz
    URL_HASH SHA256=32427e8c471ac095853212a37aef816c60b42052d4d9e48230bab3bdf2936ccc
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(freetype)

if(TARGET freetype)
    set(_denigma_freetype_target freetype)
elseif(TARGET Freetype::Freetype)
    set(_denigma_freetype_target Freetype::Freetype)
else()
    message(FATAL_ERROR "Unable to determine FreeType target name after FetchContent_MakeAvailable(freetype).")
endif()
message(STATUS "Using FreeType target: ${_denigma_freetype_target}")
list(APPEND DENIGMA_TEXT_METRICS_DEFINES DENIGMA_USE_FREETYPE=1)
list(APPEND DENIGMA_TEXT_METRICS_LIBS ${_denigma_freetype_target})
if(TARGET freetype AND CMAKE_C_COMPILER_ID MATCHES "AppleClang|Clang")
    target_compile_options(freetype PRIVATE -Wno-macro-redefined)
endif()

if(UNIX AND NOT APPLE)
    find_path(DENIGMA_FONTCONFIG_INCLUDE_DIR fontconfig/fontconfig.h)
    find_library(DENIGMA_FONTCONFIG_LIBRARY NAMES fontconfig)

    if(DENIGMA_FONTCONFIG_INCLUDE_DIR AND DENIGMA_FONTCONFIG_LIBRARY)
        list(APPEND DENIGMA_TEXT_METRICS_DEFINES DENIGMA_USE_FONTCONFIG=1)
        list(APPEND DENIGMA_TEXT_METRICS_LIBS ${DENIGMA_FONTCONFIG_LIBRARY})
        list(APPEND DENIGMA_TEXT_METRICS_INCLUDE_DIRS ${DENIGMA_FONTCONFIG_INCLUDE_DIR})
        message(STATUS "Text metrics resolver enabled: fontconfig (find_path/find_library)")
        message(STATUS "  fontconfig include: ${DENIGMA_FONTCONFIG_INCLUDE_DIR}")
        message(STATUS "  fontconfig library: ${DENIGMA_FONTCONFIG_LIBRARY}")
    else()
        message(WARNING "Text metrics resolver disabled: fontconfig not found (find_path/find_library). Install libfontconfig1-dev to enable it.")
        message(STATUS "  fontconfig include: ${DENIGMA_FONTCONFIG_INCLUDE_DIR}")
        message(STATUS "  fontconfig library: ${DENIGMA_FONTCONFIG_LIBRARY}")
        message(STATUS "Text metrics resolver fallback: FreeType directory-index lookup")
    endif()
elseif(APPLE)
    find_library(DENIGMA_CORETEXT_FRAMEWORK CoreText REQUIRED)
    find_library(DENIGMA_COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    list(APPEND DENIGMA_TEXT_METRICS_DEFINES DENIGMA_USE_CORETEXT=1)
    list(APPEND DENIGMA_TEXT_METRICS_LIBS ${DENIGMA_CORETEXT_FRAMEWORK} ${DENIGMA_COREFOUNDATION_FRAMEWORK})
    message(STATUS "Text metrics resolver enabled: CoreText")
elseif(WIN32)
    list(APPEND DENIGMA_TEXT_METRICS_DEFINES DENIGMA_USE_DIRECTWRITE=1)
    list(APPEND DENIGMA_TEXT_METRICS_LIBS dwrite)
    message(STATUS "Text metrics resolver enabled: DirectWrite")
endif()

# Define a cache variable for the local Musx C++ DOM path relative to the source directory.
set(MUSX_LOCAL_PATH "" CACHE STRING "Path to local Musx C++ DOM checkout relative to the source directory (if provided)")

if(MUSX_LOCAL_PATH)
    # Combine the source directory with the relative path to get an absolute path.
    get_filename_component(MUSX_LOCAL_PATH_ABS "${PROJECT_SOURCE_DIR}/${MUSX_LOCAL_PATH}" ABSOLUTE)
    if(EXISTS "${MUSX_LOCAL_PATH_ABS}/CMakeLists.txt")
        message(STATUS "Local Musx C++ DOM found at ${MUSX_LOCAL_PATH_ABS}")
        # Set the FetchContent source directory to the computed absolute path.
        set(FETCHCONTENT_SOURCE_DIR_MUSX "${MUSX_LOCAL_PATH_ABS}" CACHE INTERNAL "")
    else()
        message(FATAL_ERROR "Local Musx C++ DOM not found at ${MUSX_LOCAL_PATH_ABS}")
    endif()
else()
    message(STATUS "Using GitHub for MUSX C++ DOM (${MUSXDOM_GIT_TAG_OR_BRANCH})")
endif()

# temporary for reference and debugging
set(MUSX_DISPLAY_NODE_NAMES ON CACHE BOOL "Write node names to std::cout as they are processed" FORCE)
#set(MUSX_THROW_ON_UNKNOWN_XML ON CACHE BOOL "Enable throwing on unknown XML" FORCE)
#set(MUSX_THROW_ON_INTEGRITY_CHECK_FAIL ON CACHE BOOL "Enable throwing integrity check failures" FORCE)

# Disable building tests for musx
set(musx_BUILD_TESTING OFF CACHE BOOL "Do not build tests for musx")
# Fetch musx
FetchContent_Declare(
    musx
    GIT_REPOSITORY https://github.com/rpatters1/musxdom.git
    GIT_TAG        ${MUSXDOM_GIT_TAG_OR_BRANCH}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(musx)

FetchContent_GetProperties(musx)
message(STATUS "Musx Source Directory: ${musx_SOURCE_DIR}")

# Define a cache variable for the local MNX C++ DOM path relative to the source directory.
set(MNXDOM_LOCAL_PATH "" CACHE STRING "Path to local MNX C++ DOM checkout relative to the source directory (if provided)")

if(MNXDOM_LOCAL_PATH)
    # Combine the source directory with the relative path to get an absolute path.
    get_filename_component(MNXDOM_LOCAL_PATH_ABS "${PROJECT_SOURCE_DIR}/${MNXDOM_LOCAL_PATH}" ABSOLUTE)
    if(EXISTS "${MNXDOM_LOCAL_PATH_ABS}/CMakeLists.txt")
        message(STATUS "Local MNX C++ DOM found at ${MNXDOM_LOCAL_PATH_ABS}")
        # Set the FetchContent source directory to the computed absolute path.
        set(FETCHCONTENT_SOURCE_DIR_MNXDOM "${MNXDOM_LOCAL_PATH_ABS}" CACHE INTERNAL "")
    else()
        message(FATAL_ERROR "Local MNX C++ DOM not found at ${MNXDOM_LOCAL_PATH_ABS}")
    endif()
else()
    message(STATUS "Using GitHub for MNX C++ DOM (${MNXDOM_GIT_TAG_OR_BRANCH})")
endif()

option(mnxdom_BUILD_TESTING "Enable testing mnxdom" OFF)
# Fetch mnxdom
FetchContent_Declare(
    mnxdom
    GIT_REPOSITORY https://github.com/rpatters1/mnxdom.git
    GIT_TAG        ${MNXDOM_GIT_TAG_OR_BRANCH}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(mnxdom)

FetchContent_GetProperties(mnxdom)
message(STATUS "mnxdom Source Directory: ${mnxdom_SOURCE_DIR}")

# Define a cache variable for the local Smufl Mapping Library path relative to the source directory.
set(SMUFL_MAPPING_LOCAL_PATH "" CACHE STRING "Path to local Smufl Mapping Library checkout relative to the source directory (if provided)")

if(SMUFL_MAPPING_LOCAL_PATH)
    # Combine the source directory with the relative path to get an absolute path.
    get_filename_component(SMUFL_MAPPING_LOCAL_PATH_ABS "${PROJECT_SOURCE_DIR}/${SMUFL_MAPPING_LOCAL_PATH}" ABSOLUTE)
    if(EXISTS "${SMUFL_MAPPING_LOCAL_PATH_ABS}/CMakeLists.txt")
        message(STATUS "Local Smufl Mapping Library found at ${SMUFL_MAPPING_LOCAL_PATH_ABS}")
        # Set the FetchContent source directory to the computed absolute path.
        set(FETCHCONTENT_SOURCE_DIR_SMUFL_MAPPING "${SMUFL_MAPPING_LOCAL_PATH_ABS}" CACHE INTERNAL "")
    else()
        message(FATAL_ERROR "Local Smufl Mapping Library not found at ${SMUFL_MAPPING_LOCAL_PATH_ABS}")
    endif()
else()
    message(STATUS "Using GitHub for Smufl Mapping Library (${SMUFL_MAPPING_GIT_TAG_OR_BRANCH})")
endif()

# Disable building tests for smufl_mapping
set(smufl_mapping_BUILD_TESTING OFF CACHE BOOL "Do not build tests for smufl_mapping")
# Fetch musx
FetchContent_Declare(
    smufl_mapping
    GIT_REPOSITORY https://github.com/rpatters1/smufl-mapping.git
    GIT_TAG        ${SMUFL_MAPPING_GIT_TAG_OR_BRANCH}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(smufl_mapping)

FetchContent_GetProperties(smufl_mapping)
message(STATUS "Smufl Mapping Source Directory: ${smufl_mapping_SOURCE_DIR}")

add_custom_target(PrintConfig ALL
    COMMAND ${CMAKE_COMMAND} -E echo "Active build configuration: $<CONFIG>"
)

include("${PROJECT_SOURCE_DIR}/cmake/GenerateLicenseXxd.cmake")

add_library(denigma_conversion INTERFACE)
add_library(denigma::conversion ALIAS denigma_conversion)
target_include_directories(denigma_conversion INTERFACE
    ${PROJECT_SOURCE_DIR}/include
)
target_compile_features(denigma_conversion INTERFACE cxx_std_20)

# Define an interface library for internal include paths and precompiled headers.
add_library(denigma_internal_deps INTERFACE)
target_include_directories(denigma_internal_deps INTERFACE
    ${GENERATED_DIR}
    ${PROJECT_SOURCE_DIR}/src
)

# Define a concrete precompiled-header host. CMake propagates INTERFACE PCH
# settings as a header list, which makes every consumer build its own PCH. The
# internal libraries below reuse this target's compiled PCH where their compile
# definitions stay compatible with it.
add_library(denigma_common_pch STATIC
    ${PROJECT_SOURCE_DIR}/src/common_pch.cpp
)
target_link_libraries(denigma_common_pch PRIVATE
    denigma_conversion
    musx
)
target_include_directories(denigma_common_pch PRIVATE
    ${GENERATED_DIR}
    ${PROJECT_SOURCE_DIR}/src
)
target_precompile_headers(denigma_common_pch PRIVATE
    <algorithm>
    <functional>
    <string>
    <memory>
    <musx/musx.h>
)

# Internal library targets are still intended to become externally consumable
# conversion libraries. Keep dependencies on each target precise; avoid using
# this helper as a reason to hide broad dependency aggregates in every library.
function(add_denigma_internal_library target_name)
    cmake_parse_arguments(DENIGMA_INTERNAL_LIBRARY "NO_COMMON_PCH" "" "" ${ARGN})
    add_library(${target_name} STATIC ${DENIGMA_INTERNAL_LIBRARY_UNPARSED_ARGUMENTS})
    target_link_libraries(${target_name}
        PUBLIC
            denigma_conversion
        PRIVATE
            denigma_internal_deps
    )
    if(NOT DENIGMA_INTERNAL_LIBRARY_NO_COMMON_PCH)
        target_precompile_headers(${target_name} REUSE_FROM denigma_common_pch)
    endif()
endfunction()

function(add_denigma_internal_test_library target_name)
    add_denigma_internal_library(${target_name} NO_COMMON_PCH ${ARGN})
    target_compile_definitions(${target_name} PRIVATE DENIGMA_TEST)
endfunction()

option(denigma_BUILD_TESTING "Build the Denigma test suite" ON)

add_subdirectory(src)

# Add executable target
add_executable(denigma
    src/main.cpp
    src/about.cpp
)

# For the denigma target specifically
if (NOT CMAKE_CONFIGURATION_TYPES) # Only applies to single-config generators
    set_target_properties(denigma PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/build/${CMAKE_BUILD_TYPE}
    )
endif()

# Ensure the include directories are added
add_dependencies(denigma GenerateLicenseXxd)
target_link_libraries(denigma PRIVATE
    denigma_export
    denigma_massage
    denigma_internal_deps
)

set(DEPLOY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/denigma)

if(CMAKE_GENERATOR MATCHES "^Visual Studio")
    add_custom_command(
        TARGET denigma
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPLOY_DIR}
        COMMAND cmd.exe /c if "$(Configuration)" == "Release" echo Copying Release build of denigma to deploy directory
        COMMAND cmd.exe /c if "$(Configuration)" == "Release" "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:denigma>" "${DEPLOY_DIR}"
    )
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Configuring POST_BUILD command for Release.")
    add_custom_command(
        TARGET denigma
        POST_BUILD
        COMMAND echo Copying Release build of denigma to deploy directory
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPLOY_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:denigma> ${DEPLOY_DIR}
        VERBATIM
    )
endif()

if(denigma_BUILD_TESTING)
    message(STATUS "Configuring tests for denigma_BUILD_TESTING.")
    add_subdirectory(tests)
else()
    message(STATUS "Testing not enabled for denigma_BUILD_TESTING.")
endif()

option(denigma_BUILD_DOCS "Build the Denigma public API Doxygen site as part of the default build" OFF)
find_package(Doxygen QUIET)

if(DOXYGEN_FOUND)
    configure_file(
        ${PROJECT_SOURCE_DIR}/docs/Doxyfile.in
        ${PROJECT_BINARY_DIR}/Doxyfile
        @ONLY
    )

    set(_denigma_docs_all)
    if(denigma_BUILD_DOCS)
        set(_denigma_docs_all ALL)
    endif()

    add_custom_target(denigma_docs ${_denigma_docs_all}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/docs/api
        COMMAND ${DOXYGEN_EXECUTABLE} ${PROJECT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Generating Denigma public API documentation"
        VERBATIM
    )
elseif(denigma_BUILD_DOCS)
    message(FATAL_ERROR "denigma_BUILD_DOCS is ON, but Doxygen was not found.")
endif()
