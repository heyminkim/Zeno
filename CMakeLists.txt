cmake_minimum_required(VERSION 3.14)
project(Zeno LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)

if(APPLE)
    set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl" CACHE PATH "Path to OpenSSL")
endif()

find_package(OpenSSL REQUIRED)

option(RARRAY "Build zeno filter on resizable array" OFF)
option(VMEM "Build zeno filter on virtual memory" OFF)
option(RSQF "Build rank and select quotient filter" OFF)
option(WIDENING "Build filter on the widening regime" OFF)
option(BUILD_TESTS "Build unit tests" OFF)

if(RARRAY AND VMEM)
    message(FATAL_ERROR "Options RARRAY and VMEM are mutually exclusive. Please enable only one. ")
endif()

if(RSQF AND (RARRAY OR VMEM))
    message(FATAL_ERROR "Options RSQF is not compatible with RARRAY or VMEM. ")
endif()

if(NOT RARRAY AND NOT VMEM AND FIXED)
    message("Option FIXED will not take effect unless options RARRAY or VMEM are enabled. ")
endif()

if(RARRAY)
    add_definitions(-DRARRAY)
endif()

if(VMEM)
    add_definitions(-DVMEM)
endif()

if(RSQF)
    add_definitions(-DRSQF)
endif()

if(WIDENING)
    add_definitions(-DWIDENING)
endif()

add_library(zeno INTERFACE)
target_include_directories(zeno INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(BITHACKING_COMPILE_FLAGS -Ofast -march=native -D__SSE4_2_)
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BITHACKING_COMPILE_FLAGS -pg -march=native -D__SSE4_2_)
endif()
target_compile_options(zeno INTERFACE ${BITHACKING_COMPILE_FLAGS})

find_package(Threads REQUIRED)
target_link_libraries(zeno INTERFACE Threads::Threads)

target_link_libraries(zeno INTERFACE OpenSSL::SSL)

add_subdirectory(bench)
add_subdirectory(examples)
add_subdirectory(evaluations)
