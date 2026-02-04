# Minimum version of CMake required
cmake_minimum_required(VERSION 3.22)

# Project name and version
project(denigma VERSION 3.0.1)

# Specify C++ standard and minimum macOS version
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Prevent in-source builds
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
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
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE})
endif()

message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
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
set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/build")

#define other directories
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(SMUFL_METADATA_DIR "${CMAKE_CURRENT_LIST_DIR}/smufl")

# Set output directory for compiled executable
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(BUILD_TESTING OFF CACHE BOOL "Disable testing for external dependencies" FORCE)

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
target_include_directories(denigma_minizip PUBLIC "${zlib_SOURCE_DIR}/contrib/minizip")
target_link_libraries(denigma_minizip PUBLIC ${_denigma_zlib_target})

include(cmake/Dependencies.cmake) # GitHub branches/tags for MNX and MUSX

# Define a cache variable for the local Musx C++ DOM path relative to the source directory.
set(MUSX_LOCAL_PATH "" CACHE STRING "Path to local Musx C++ DOM checkout relative to the source directory (if provided)")

if(MUSX_LOCAL_PATH)
    # Combine the source directory with the relative path to get an absolute path.
    get_filename_component(MUSX_LOCAL_PATH_ABS "${CMAKE_SOURCE_DIR}/${MUSX_LOCAL_PATH}" ABSOLUTE)
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

# Compile definitions
#set(MUSX_USE_TINYXML2 ON CACHE BOOL "Disable tinyxml2 parsing classes" FORCE)
#set(MUSX_USE_RAPIDXML ON CACHE BOOL "Enable rapidxml parsing classes" FORCE)
set(MUSX_USE_PUGIXML ON CACHE BOOL "Enable pugixml parsing classes" FORCE)

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
    get_filename_component(MNXDOM_LOCAL_PATH_ABS "${CMAKE_SOURCE_DIR}/${MNXDOM_LOCAL_PATH}" ABSOLUTE)
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
    get_filename_component(SMUFL_MAPPING_LOCAL_PATH_ABS "${CMAKE_SOURCE_DIR}/${SMUFL_MAPPING_LOCAL_PATH}" ABSOLUTE)
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

include("${CMAKE_SOURCE_DIR}/cmake/GenerateLicenseXxd.cmake")

# Add executable target
add_executable(denigma
    src/main.cpp
    src/about.cpp
    src/denigma.cpp
    src/export/export.cpp
    src/export/enigmaxml.cpp
    src/export/mnx.cpp
    src/export/mnx_enums.cpp
    src/export/mnx_global.cpp
    src/export/mnx_layouts.cpp
    src/export/mnx_parts.cpp
    src/export/mnx_sequences.cpp
    src/export/mss.cpp
    src/export/mnx_mapping.cpp
    src/massage/massage.cpp
    src/massage/musicxml.cpp
    src/utils/smufl_support.cpp
    src/utils/ziputils.cpp
)

# For the denigma target specifically
if (NOT CMAKE_CONFIGURATION_TYPES) # Only applies to single-config generators
    set_target_properties(denigma PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build/${CMAKE_BUILD_TYPE}
    )
endif()

# Ensure the include directories are added
add_dependencies(denigma GenerateLicenseXxd)
target_include_directories(denigma PRIVATE ${GENERATED_DIR})
target_include_directories(denigma PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Ensure the libraries are added
target_link_libraries(denigma PRIVATE
    denigma_minizip
    ${_denigma_zlib_target}
    pugixml
    mnxdom
    musx
    smufl_mapping
)

if (CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
    # Keep strict warnings for denigma code without applying them to fetched dependencies.
    target_compile_options(denigma PRIVATE -Wno-c++20-extensions -Wall -Wextra -Wpedantic -Werror)
endif()

# Define an interface library for precompiled headers
add_library(denigma_pch INTERFACE)

# Add Precompiled Headers (PCH) for the test suite
target_precompile_headers(denigma_pch INTERFACE
    <algorithm>
    <functional>
    <string>
    <memory>
    "${pugixml_SOURCE_DIR}/src/pugixml.hpp"
    "${musx_SOURCE_DIR}/src/musx/musx.h"
    "${mnxdom_SOURCE_DIR}/src/mnxdom.h"
    "${smufl_mapping_SOURCE_DIR}/src/smufl_mapping.h"
)

# Link the interface library to denigma
target_link_libraries(denigma PUBLIC denigma_pch)

set(DEPLOY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/denigma)

if(MSVC)
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

option(denigma_BUILD_TESTING "Build the Denigma test suite" ON)

if(denigma_BUILD_TESTING)
    message(STATUS "Configuring tests for denigma_BUILD_TESTING.")
    add_subdirectory(tests)
else()
    message(STATUS "Testing not enabled for denigma_BUILD_TESTING.")
endif()

get_target_property(defs musx INTERFACE_COMPILE_DEFINITIONS)
message(STATUS "musx INTERFACE definitions: ${defs}")

get_target_property(defs denigma INTERFACE_COMPILE_DEFINITIONS)
message(STATUS "denigma INTERFACE definitions: ${defs}")
