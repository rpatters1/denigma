# Minimum version of CMake required
cmake_minimum_required(VERSION 3.15)

# Project name and version
project(musx2mnx VERSION 0.1)

# Specify C++ standard and minimum macOS version
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15)
endif()

if(MSVC)
    add_compile_options(/wd5051) # don't care about C++20 attributes
    add_compile_options(/Zc:__cplusplus) # force MSVC to report correct c++ version
endif()

add_compile_definitions(
    MUSX2MNX_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    MUSX2MNX_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    MUSX2MNX_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    MUSX2MNX_VERSION="${PROJECT_VERSION}"
)

set(BUILD_SHARED_LIBS OFF)

# Define output directory for executables
set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/build")

#define other directories
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(MINIZ_DIR "${CMAKE_SOURCE_DIR}/submodules/miniz")
set(EZGZ_DIR "${CMAKE_SOURCE_DIR}/submodules/ezgz")
set(NLOHMANN_JSON_DIR "${CMAKE_SOURCE_DIR}/submodules/nlohmann_json")
set(JSON_SCHEMA_VALIDATOR_DIR "${CMAKE_SOURCE_DIR}/submodules/json_schema_validator")

# Set output directory for compiled executable
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})

# Include directories for your source files
include_directories(src)
add_subdirectory(${NLOHMANN_JSON_DIR})
add_subdirectory(${JSON_SCHEMA_VALIDATOR_DIR})

include("${CMAKE_SOURCE_DIR}/cmake/GenerateMnxSchemaXxd.cmake")

# Add executable target
add_executable(musx2mnx src/main.cpp)

# Ensure the include directories ar added
add_dependencies(musx2mnx GenerateMnxSchemaXxd)
target_include_directories(musx2mnx PRIVATE ${GENERATED_DIR})
target_include_directories(musx2mnx PRIVATE ${EZGZ_DIR})
target_include_directories(musx2mnx PRIVATE ${MINIZ_DIR})
target_link_libraries(musx2mnx PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(musx2mnx PRIVATE nlohmann_json_schema_validator)
