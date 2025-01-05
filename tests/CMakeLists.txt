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
        options.cpp
        #test/test_export.cpp
        #test/test_massage.cpp
        #test/test_utils.cpp
        ${DENIGMA_SOURCES}
    )

    if (CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
        # Extra warnings for Clang/AppleClang
        target_compile_options(denigma_tests PRIVATE -Wall -Wextra -Werror -pedantic)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        # Extra warnings for MSVC
        target_compile_options(denigma_tests PRIVATE /W4 /WX)
    endif()

    # don't care about C++20 attributes
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang|AppleClang")
        target_compile_options(denigma_tests PRIVATE -Wno-c++20-extensions)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(denigma_tests PRIVATE -Wno-attributes)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(denigma_tests PRIVATE /wd5051) # Example warning code
    endif()

    # Set the test app's output directory
    if (NOT CMAKE_CONFIGURATION_TYPES) # For single-config generators
        set_target_properties(denigma_tests PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/${CMAKE_BUILD_TYPE}
        )
    endif()

    # Include the necessary directories
    target_include_directories(denigma_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src       # Source files
        ${FETCHCONTENT_BASE_DIR}/ezgz-src
        ${FETCHCONTENT_BASE_DIR}/miniz-cpp-src
        ${MUSX_OBJECT_MODEL_DIR}
        ${GENERATED_DIR}              # Generated files
    )

    # Link libraries used by denigma
    target_link_libraries(denigma_tests PRIVATE
        denigma_pch                   # Precompiled headers
        pugixml
        nlohmann_json::nlohmann_json
        nlohmann_json_schema_validator
        musx
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
    target_link_libraries(denigma_tests PRIVATE gtest gmock gtest_main)

    # Discover tests
    include(GoogleTest)
    gtest_discover_tests(denigma_tests
        DISCOVERY_TIMEOUT 60 # Set timeout to 60 seconds
        PROPERTIES
            ENVIRONMENT "GTEST_COLOR=1"
        EXTRA_ARGS --gtest_color=yes
    )

endif()
