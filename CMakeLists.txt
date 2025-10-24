cmake_minimum_required(VERSION 3.14)
project(Zeno LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)

if(APPLE)
    set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl" CACHE PATH "Path to OpenSSL")
endif()

find_package(OpenSSL REQUIRED)

option(WIDENING "Build filter on the widening regime" OFF)

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
