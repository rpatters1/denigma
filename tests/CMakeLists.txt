# Only configure tests if denigma_BUILD_TESTING is ON
if(denigma_BUILD_TESTING)

    # Enable testing in this directory
    enable_testing()

    # Collect all .cpp files in the source directory
    file(GLOB DENIGMA_SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp"
                            "${CMAKE_SOURCE_DIR}/src/export/*.cpp"
                            "${CMAKE_SOURCE_DIR}/src/massage/*.cpp"
                            "${CMAKE_SOURCE_DIR}/src/utils/*.cpp")

    # Add an executable for the test suite
    add_executable(denigma_tests
        denigmatests.cpp
        test_export.cpp
        test_logging.cpp
        test_massage.cpp
        test_options.cpp
        mnx/test_beams.cpp
        mnx/test_global.cpp
        mnx/test_layouts.cpp
        mnx/test_ottavas.cpp
        mnx/test_parts.cpp
        mnx/test_schema.cpp
        mnx/test_scores.cpp
        ${DENIGMA_SOURCES}
    )

    if (CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
        # Extra warnings for Clang/AppleClang
        target_compile_options(denigma_tests PRIVATE -Wall -Wextra -Werror -Wpedantic)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        # Extra warnings for MSVC
        target_compile_options(denigma_tests PRIVATE /W4 /WX)
    endif()

    # Set the test app's output directory
    set_target_properties(denigma_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    )

    # Include the necessary directories
    target_include_directories(denigma_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src       # Source files
        ${CMAKE_CURRENT_SOURCE_DIR}   # base test directory
        ${FETCHCONTENT_BASE_DIR}/ezgz-src
        ${FETCHCONTENT_BASE_DIR}/miniz-cpp-src
        ${GENERATED_DIR}              # Generated files
    )

    # Link libraries used by denigma
    target_link_libraries(denigma_tests PRIVATE
        denigma_pch                   # Precompiled headers
        pugixml
        mnxdom
        musx
        smufl_mapping
    )

    target_compile_definitions(denigma_tests PRIVATE
        MUSX_TEST_DATA_PATH="${musx_SOURCE_DIR}/tests/data"
    )

    # Define testing-specific preprocessor macro
    target_compile_definitions(denigma_tests PRIVATE DENIGMA_TEST)

    # Add test dependencies
    add_dependencies(denigma_tests GenerateMnxSchemaXxd)

    # Add Google Test as a dependency
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/release-1.12.1.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(googletest)

    # Link Google Test libraries
    target_link_libraries(denigma_tests PRIVATE gmock gtest_main)

    # Discover tests
    include(GoogleTest)
    gtest_discover_tests(denigma_tests
        DISCOVERY_TIMEOUT 60 # Set timeout to 60 seconds
        PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data
            ENVIRONMENT "GTEST_COLOR=1"
        EXTRA_ARGS --gtest_color=yes
    )

endif()
