# Minimum version of CMake required
cmake_minimum_required(VERSION 3.15)

# Project name and version
project(denigma VERSION 0.1)

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
#set(NLOHMANN_JSON_DIR "${CMAKE_SOURCE_DIR}/submodules/nlohmann_json")
set(JSON_SCHEMA_VALIDATOR_DIR "${CMAKE_SOURCE_DIR}/submodules/json_schema_validator")
set(MUSX_OBJECT_MODEL_DIR "${CMAKE_SOURCE_DIR}/submodules/musx_object_model/src")
set(RAPIDXML_DIR "${CMAKE_SOURCE_DIR}/third_party/rapidxml")

# Set output directory for compiled executable
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(BUILD_TESTING OFF CACHE BOOL "Disable testing for external dependencies" FORCE)

# Fetch EzGz
FetchContent_Declare(
  EzGz
  GIT_REPOSITORY https://github.com/Dugy/EzGz.git
  GIT_TAG 9229cb8fabe28470eb0b74798cadcdbdb5e72c6d
  SOURCE_SUBDIR not_a_directory # this is a hack to prevent FetchContent_Declare from the fetched content cmake.
  # see https://stackoverflow.com/questions/79261625/cmake-fetchcontent-with-header-only-project/79261858#79261858
)
FetchContent_MakeAvailable(EzGz)

# Fetch tinyxml2
FetchContent_Declare(
    tinyxml2
    URL https://github.com/leethomason/tinyxml2/archive/refs/tags/9.0.0.tar.gz
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(tinyxml2)

# Fetch miniz-cpp
FetchContent_Declare(
  miniz-cpp
  GIT_REPOSITORY https://github.com/tfussell/miniz-cpp.git
  GIT_TAG        052335e4f7773368df07b26d2baedb0e6d4dbd38
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_SUBDIR not_a_directory # this is a hack to prevent FetchContent_Declare from the fetched content cmake.
  # see https://stackoverflow.com/questions/79261625/cmake-fetchcontent-with-header-only-project/79261858#79261858
)
FetchContent_MakeAvailable(miniz-cpp)

# Define the version of nlohmann_json to use
set(NLOHMANN_JSON_VERSION v3.11.2)

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

# Include directories for your source files
#add_subdirectory(${NLOHMANN_JSON_DIR})
#add_subdirectory(${JSON_SCHEMA_VALIDATOR_DIR})

include("${CMAKE_SOURCE_DIR}/cmake/GenerateMnxSchemaXxd.cmake")

# Add executable target
add_executable(denigma
    src/main.cpp
    src/enigmaxml/enigmaxml.cpp
    src/mnx/mnx.cpp
    src/mss/mss.cpp
)

# Compile definitions
target_compile_definitions(denigma PRIVATE MUSX_USE_TINYXML2)
target_compile_definitions(denigma PRIVATE MUSX_USE_RAPIDXML)
 # temporary for reference
target_compile_definitions(denigma PRIVATE MUSX_DISPLAY_NODE_NAMES)

# Ensure the include directories are added
add_dependencies(denigma GenerateMnxSchemaXxd)
target_include_directories(denigma PRIVATE ${RAPIDXML_DIR})
target_include_directories(denigma PRIVATE  "${FETCHCONTENT_BASE_DIR}/ezgz-src")
target_include_directories(denigma PRIVATE  "${FETCHCONTENT_BASE_DIR}/miniz-cpp-src")
target_include_directories(denigma PRIVATE ${GENERATED_DIR})
target_include_directories(denigma PRIVATE ${MUSX_OBJECT_MODEL_DIR})
target_include_directories(denigma PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Ensure the libraries are added
target_link_libraries(denigma PRIVATE tinyxml2)
target_link_libraries(denigma PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(denigma PRIVATE nlohmann_json_schema_validator)
