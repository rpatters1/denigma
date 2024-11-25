# Minimum version of CMake required
cmake_minimum_required(VERSION 3.15)

# Project name and version
project(musx2mnx VERSION 1.0)

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
set(MINIZ_DIR "submodules/miniz")
set(EZGZ_DIR "submodules/ezgz")

# Set output directory for compiled executable
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})

# Include directories for your source files
include_directories(src)

# Add executable target
add_executable(musx2mnx src/main.cpp)

# Ensure the include directories ar added
target_include_directories(musx2mnx PRIVATE ${EZGZ_DIR})
target_include_directories(musx2mnx PRIVATE ${MINIZ_DIR})
