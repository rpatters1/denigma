#!/usr/bin/env cmake -P

# CMake arguments up to '-P' (ignore these)
set(i "1")
set(COMMAND_ARG "")
set(CONFIG_ARG "")
while(i LESS "${CMAKE_ARGC}")
	if("${CMAKE_ARGV${i}}" STREQUAL "clean")
		set(COMMAND_ARG "${CMAKE_ARGV${i}}")
    elseif("${CMAKE_ARGV${i}}" STREQUAL "Release")
        set(CONFIG_ARG "${CMAKE_ARGV${i}}")
    elseif("${CMAKE_ARGV${i}}" STREQUAL "Debug")
        set(CONFIG_ARG "${CMAKE_ARGV${i}}")
    endif()
    math(EXPR i "${i} + 1") # next argument
endwhile()

if(CONFIG_ARG STREQUAL "")
    set(CONGIG_ARG "Release")
endif()

# Define the build directory
set(BUILD_DIR "${CMAKE_BINARY_DIR}/build")

# If the command is "clean", clean the build directory
if(COMMAND_ARG STREQUAL "clean")
    message(STATUS "Cleaning build directory...")
    file(REMOVE_RECURSE ${BUILD_DIR})
    return()  # Exit the script after cleaning
endif()

# Check if the system supports multi-config
set(MULTI_CONFIG_FOUND "FALSE")
if(EXISTS ${BUILD_DIR}/CMakeCache.txt)
    file(READ "${BUILD_DIR}/CMakeCache.txt" CMAKE_CACHE_CONTENTS)
    string(FIND "${CMAKE_CACHE_CONTENTS}" "CMAKE_CONFIGURATION_TYPES" MULTI_CONFIG_FOUND_INDEX)
    if(MULTI_CONFIG_FOUND_INDEX GREATER -1)
        set(MULTI_CONFIG_FOUND "TRUE")
    endif()
endif()

# Configure the project for single-config systems or if CMakeCache is missing
if(NOT MULTI_CONFIG_FOUND OR NOT EXISTS ${BUILD_DIR}/CMakeCache.txt)
    message(STATUS "Configuring project for configuration: ${CONFIG_ARG}")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -G Ninja  -S ${CMAKE_SOURCE_DIR} -B ${BUILD_DIR} -DCMAKE_BUILD_TYPE=${CONFIG_ARG}
    )
endif()

# Build the project with the appropriate configuration
if(MULTI_CONFIG_FOUND)
    message(STATUS "Detected multi-config system. Building with configuration: ${CONFIG_ARG}")
    execute_process(COMMAND ${CMAKE_COMMAND} --build ${BUILD_DIR} --config ${CONFIG_ARG})
else()
    message(STATUS "Detected single-config system. Building with configuration: ${CONFIG_ARG}")
    execute_process(COMMAND ${CMAKE_COMMAND} --build ${BUILD_DIR})
endif()
