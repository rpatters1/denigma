# Minimum version of CMake required
cmake_minimum_required(VERSION 3.15)

# Project name and version
project(denigma VERSION 0.1)

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
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
    message(STATUS "No build type specified. Defaulting to Release.")
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
endif()

if(MSVC)
    add_compile_options(/wd5051) # don't care about C++20 attributes
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

# Set output directory for compiled executable
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(BUILD_TESTING OFF CACHE BOOL "Disable testing for external dependencies" FORCE)

# Fetch EzGz
FetchContent_Declare(
    EzGz
    GIT_REPOSITORY https://github.com/Dugy/EzGz.git
    GIT_TAG 9229cb8fabe28470eb0b74798cadcdbdb5e72c6d
    SOURCE_SUBDIR not_a_directory # this is a hack to prevent FetchContent_Declare from running the fetched content cmake.
    # see https://stackoverflow.com/questions/79261625/cmake-fetchcontent-with-header-only-project/79261858#79261858
)
FetchContent_MakeAvailable(EzGz)

# Fetch pugixml
FetchContent_Declare(
    pugixml
    URL https://github.com/zeux/pugixml/releases/download/v1.14/pugixml-1.14.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(pugixml)

# Fetch miniz-cpp
FetchContent_Declare(
    miniz-cpp
    GIT_REPOSITORY https://github.com/tfussell/miniz-cpp.git
    GIT_TAG        052335e4f7773368df07b26d2baedb0e6d4dbd38
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR not_a_directory # this is a hack to prevent FetchContent_Declare from running the fetched content cmake.
    # see https://stackoverflow.com/questions/79261625/cmake-fetchcontent-with-header-only-project/79261858#79261858
)
FetchContent_MakeAvailable(miniz-cpp)

# Define the version of nlohmann_json to use
set(NLOHMANN_JSON_VERSION v3.11.3)

# Fetch nlohmann_json
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/${NLOHMANN_JSON_VERSION}/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d  # Verify the hash matches the tarball
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# Fetch json_schema_validator from your fork
set(nlohmann_json_VERSION ${NLOHMANN_JSON_VERSION})
FetchContent_Declare(
    json_schema_validator
    GIT_REPOSITORY https://github.com/rpatters1/json-schema-validator.git # use my fork unless PR#338 is merged to upstream
    GIT_TAG        010eba651fc2469f73ff132108747638e14f8cb0
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(json_schema_validator)

add_custom_target(PrintConfig ALL
    COMMAND ${CMAKE_COMMAND} -E echo "Active build configuration: $<CONFIG>"
)

include("${CMAKE_SOURCE_DIR}/cmake/GenerateMnxSchemaXxd.cmake")

# Add executable target
add_executable(denigma
    src/main.cpp
    src/denigma.cpp
    src/export/export.cpp
    src/export/enigmaxml.cpp
    src/export/mnx.cpp
    src/export/mss.cpp
    src/massage/massage.cpp
    src/massage/musicxml.cpp
    src/utils/ziputils.cpp
)

# For the denigma target specifically
set_target_properties(denigma PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build/${CMAKE_BUILD_TYPE}
)

# Compile definitions
#set(MUSX_USE_TINYXML2 ON CACHE BOOL "Disable tinyxml2 parsing classes" FORCE)
#set(MUSX_USE_RAPIDXML ON CACHE BOOL "Enable rapidxml parsing classes" FORCE)
set(MUSX_USE_PUGIXML ON CACHE BOOL "Enable pugixml parsing classes" FORCE)

# temporary for reference and debugging
set(MUSX_DISPLAY_NODE_NAMES ON CACHE BOOL "Write node names to std::cout as they are processed" FORCE)
#set(MUSX_THROW_ON_UNKNOWN_XML ON CACHE BOOL "Enable throwing on unknown XML" FORCE)
#set(MUSX_THROW_ON_INTEGRITY_CHECK_FAIL ON CACHE BOOL "Enable throwing integrity check failures" FORCE)

#Include directories for your source files
add_subdirectory(submodules/musx_object_model)

# Ensure the include directories are added
add_dependencies(denigma GenerateMnxSchemaXxd)
target_include_directories(denigma PRIVATE  "${FETCHCONTENT_BASE_DIR}/ezgz-src")
target_include_directories(denigma PRIVATE  "${FETCHCONTENT_BASE_DIR}/miniz-cpp-src")
target_include_directories(denigma PRIVATE ${GENERATED_DIR})
target_include_directories(denigma PRIVATE ${MUSX_OBJECT_MODEL_DIR})
target_include_directories(denigma PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Ensure the libraries are added
target_link_libraries(denigma PRIVATE pugixml)
target_link_libraries(denigma PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(denigma PRIVATE nlohmann_json_schema_validator)
target_link_libraries(denigma PRIVATE musx)

# Define an interface library for precompiled headers
add_library(denigma_pch INTERFACE)

# Add Precompiled Headers (PCH) for the test suite
target_precompile_headers(denigma_pch INTERFACE
    <algorithm>
    <functional>
    <string>
    <memory>
    "${pugixml_SOURCE_DIR}/src/pugixml.hpp"
    "${MUSX_ROOT_DIR}/src/musx/musx.h"
)

# Link the interface library to denigma
target_link_libraries(denigma PUBLIC denigma_pch)

# Link the interface library to musx
target_link_libraries(musx PRIVATE denigma_pch)

function(copy_to_deploy target config)
    set(DEPLOY_DIR ${CMAKE_BINARY_DIR}/../deploy)

    # Add a custom command restricted to the given configuration
    add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPLOY_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${target}> ${DEPLOY_DIR}
        COMMENT "Copying ${config} build of ${target} to deploy directory"
        VERBATIM
    )
endfunction()

if (CMAKE_BUILD_TYPE STREQUAL "Release" OR "$<CONFIG>" STREQUAL "Release")
    copy_to_deploy(denigma "Release")
endif()
