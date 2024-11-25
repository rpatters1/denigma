#!/usr/bin/env cmake -P

# CMake arguments up to '-P' (ignore these)
set(i "1")
set(COMMAND_ARG "")
while(i LESS "${CMAKE_ARGC}")
	if("${CMAKE_ARGV${i}}" STREQUAL "clean")
		set(COMMAND_ARG "${CMAKE_ARGV${i}}")
        break() # done with CMake arguments
    endif()
    math(EXPR i "${i} + 1") # next argument
endwhile()

# Define the build directory
set(BUILD_DIR "${CMAKE_BINARY_DIR}/build")

# If the command is "clean", clean the build directory
if(COMMAND_ARG STREQUAL "clean")
    message(STATUS "Cleaning build directory...")
    file(REMOVE_RECURSE ${BUILD_DIR})
    return()  # Exit the script after cleaning
endif()

# Check if the build directory exists (implies CMake was run before)
if(NOT EXISTS ${BUILD_DIR}/CMakeCache.txt)
    message(STATUS "CMake cache not found, configuring project...")
    execute_process(COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${BUILD_DIR})
endif()

# Build the project
message(STATUS "Building project...")
execute_process(COMMAND ${CMAKE_COMMAND} --build ${BUILD_DIR})
