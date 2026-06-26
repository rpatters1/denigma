# Configure dependencies used only by denigma_textmetrics.

set(_denigma_textmetrics_defines "")
set(_denigma_textmetrics_libs "")
set(_denigma_textmetrics_include_dirs "")

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

list(APPEND _denigma_textmetrics_defines DENIGMA_USE_FREETYPE=1)
list(APPEND _denigma_textmetrics_libs ${_denigma_freetype_target})

if(TARGET freetype AND CMAKE_C_COMPILER_ID MATCHES "AppleClang|Clang")
    target_compile_options(freetype PRIVATE -Wno-macro-redefined)
endif()

if(DEFINED freetype_SOURCE_DIR AND EXISTS "${freetype_SOURCE_DIR}/docs/FTL.TXT")
    set_property(GLOBAL PROPERTY DENIGMA_FREETYPE_LICENSE_FILE "${freetype_SOURCE_DIR}/docs/FTL.TXT")
endif()

if(UNIX AND NOT APPLE)
    find_path(DENIGMA_FONTCONFIG_INCLUDE_DIR fontconfig/fontconfig.h)
    find_library(DENIGMA_FONTCONFIG_LIBRARY NAMES fontconfig)

    if(DENIGMA_FONTCONFIG_INCLUDE_DIR AND DENIGMA_FONTCONFIG_LIBRARY)
        list(APPEND _denigma_textmetrics_defines DENIGMA_USE_FONTCONFIG=1)
        list(APPEND _denigma_textmetrics_libs ${DENIGMA_FONTCONFIG_LIBRARY})
        list(APPEND _denigma_textmetrics_include_dirs ${DENIGMA_FONTCONFIG_INCLUDE_DIR})
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
    list(APPEND _denigma_textmetrics_defines DENIGMA_USE_CORETEXT=1)
    list(APPEND _denigma_textmetrics_libs ${DENIGMA_CORETEXT_FRAMEWORK} ${DENIGMA_COREFOUNDATION_FRAMEWORK})
    message(STATUS "Text metrics resolver enabled: CoreText")
elseif(WIN32)
    list(APPEND _denigma_textmetrics_defines DENIGMA_USE_DIRECTWRITE=1)
    list(APPEND _denigma_textmetrics_libs dwrite)
    message(STATUS "Text metrics resolver enabled: DirectWrite")
endif()
