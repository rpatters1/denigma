# Only configure tests if denigma_BUILD_TESTING is ON
if(denigma_BUILD_TESTING)

    # Enable testing in this directory
    enable_testing()

    function(target_add_denigma_test_warnings target_name)
        if (CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
            # Extra warnings for Clang/AppleClang
            target_compile_options(${target_name} PRIVATE -Wno-c++20-extensions -Wall -Wextra -Werror -Wpedantic)
        elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
            # Extra warnings for MSVC
            target_compile_options(${target_name} PRIVATE /W4 /WX)
        endif()
    endfunction()

    # Add an executable for the test suite
    add_executable(denigma_tests
        denigmatests.cpp
        xml_compare.cpp
        test_articulations.cpp
        test_clefs.cpp
        test_enigmaxml_converter.cpp
        test_export.cpp
        test_dynamics.cpp
        test_logging.cpp
        test_massage.cpp
        test_mss_converter.cpp
        test_options.cpp
        test_svg_converter.cpp
        test_typed_converter_options.cpp
        test_jumps.cpp
        mnx/test_beams.cpp
        mnx/test_converter.cpp
        mnx/test_formatted_text.cpp
        mnx/test_global.cpp
        mnx/test_layouts.cpp
        mnx/test_ottavas.cpp
        mnx/test_parts.cpp
        mnx/test_schema.cpp
        mnx/test_scores.cpp
        mnx/test_sequences.cpp
        ${CMAKE_SOURCE_DIR}/src/main.cpp
        ${CMAKE_SOURCE_DIR}/src/about.cpp
    )

    target_add_denigma_test_warnings(denigma_tests)

    # Set the test app's output directory
    set_target_properties(denigma_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    )

    # Include the necessary directories
    target_include_directories(denigma_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src       # Source files
        ${CMAKE_CURRENT_SOURCE_DIR}   # base test directory
        ${GENERATED_DIR}              # Generated files
        "${zlib_SOURCE_DIR}"          # zlib.h included by minizip headers
        "${zlib_SOURCE_DIR}/contrib/minizip"
    )

    # Tests intentionally aggregate broad dependencies. Do not copy this
    # dependency shape into production converter libs.
    target_link_libraries(denigma_tests PRIVATE
        denigma_export_test
        denigma_massage_test
        denigma_core_test
        denigma_utils
        denigma_internal_deps
        pugixml
    )

    target_compile_definitions(denigma_tests PRIVATE
        MUSX_TEST_DATA_PATH="${musx_SOURCE_DIR}/tests/data"
    )

    # Define testing-specific preprocessor macro
    target_compile_definitions(denigma_tests PRIVATE DENIGMA_TEST)

    add_library(denigma_tests_pch STATIC
        ${CMAKE_SOURCE_DIR}/src/common_pch.cpp
    )
    target_add_denigma_test_warnings(denigma_tests_pch)
    target_include_directories(denigma_tests_pch PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${GENERATED_DIR}
        "${zlib_SOURCE_DIR}"
        "${zlib_SOURCE_DIR}/contrib/minizip"
    )
    target_link_libraries(denigma_tests_pch PRIVATE
        denigma_export_test
        denigma_massage_test
        denigma_core_test
        denigma_utils
        denigma_internal_deps
        pugixml
    )
    target_compile_definitions(denigma_tests_pch PRIVATE
        MUSX_TEST_DATA_PATH="${musx_SOURCE_DIR}/tests/data"
        DENIGMA_TEST
    )
    target_precompile_headers(denigma_tests_pch PRIVATE
        <musx/musx.h>
    )
    target_precompile_headers(denigma_tests REUSE_FROM denigma_tests_pch)

    # Add test dependencies
    add_dependencies(denigma_tests GenerateLicenseXxd)

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
