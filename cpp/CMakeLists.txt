cmake_minimum_required(VERSION 3.28)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" SWISSQUOTE_VERSION_RAW)
string(STRIP "${SWISSQUOTE_VERSION_RAW}" SWISSQUOTE_VERSION)

project(swissquote VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SWISSQUOTE_BUILD_TESTS "Build Catch2 tests" ON)
option(SWISSQUOTE_BUILD_BENCHMARKS "Build Catch2 benchmarks" OFF)
option(SWISSQUOTE_BUILD_CODEC_BENCHMARK "Build standalone codec benchmark" OFF)

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.15.1
)
FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.4.2
)
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.18.3
)

FetchContent_MakeAvailable(Catch2 spdlog CLI11 tomlplusplus httplib)


find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    add_compile_definitions(SWISSQUOTE_HAVE_OPENSSL)
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/swissquote/version.hpp"
    @ONLY
)

add_subdirectory(src/libswissquote)

if(SWISSQUOTE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(SWISSQUOTE_BUILD_BENCHMARKS OR SWISSQUOTE_BUILD_CODEC_BENCHMARK)
    add_subdirectory(benchmarks)
endif()

add_executable(swissquote-feed src/feed/main.cpp)
target_link_libraries(swissquote-feed PRIVATE swissquote_lib CLI11::CLI11)

add_executable(swissquote-writer src/writer/main.cpp)
target_link_libraries(swissquote-writer PRIVATE swissquote_lib CLI11::CLI11)

install(TARGETS swissquote-feed swissquote-writer RUNTIME DESTINATION bin)
