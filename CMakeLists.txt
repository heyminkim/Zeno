cmake_minimum_required(VERSION 3.19)
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
option(FASTRESIZE "Build filter with fast resizing" OFF)
option(BUILD_TESTS "Build unit tests" OFF)
option(BUILD_WIREDTIGER "Build wiredtiger benchmark" OFF)

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

if(FASTRESIZE)
    add_definitions(-DFASTRESIZE)
endif()

set(
    LOCKSLOTS
    4096
    CACHE
    STRING
    "Number of slots to lock"
)

add_library(zeno INTERFACE)
target_include_directories(zeno INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
add_compile_definitions(LOCKSLOTS=${LOCKSLOTS})

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

if(BUILD_WIREDTIGER)
    add_custom_target(build_wiredtiger
        COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/build_wiredtiger.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Building WiredTiger"
    )

    set(WT_ROOT ${CMAKE_SOURCE_DIR}/bench/wiredtiger)
    add_library(wiredtiger SHARED IMPORTED GLOBAL)
    set_target_properties(wiredtiger PROPERTIES
        IMPORTED_LOCATION ${WT_ROOT}/build/libwiredtiger.so
        INTERFACE_INCLUDE_DIRECTORIES ${WT_ROOT}/build/include
    )
endif()


# if(BUILD_WIREDTIGER)
#     set(WT_ROOT ${CMAKE_SOURCE_DIR}/bench/wiredtiger)
#     set(WT_LIB ${WT_ROOT}/build/libwiredtiger.so) 

#     add_custom_command(
#         OUTPUT ${WT_LIB}
#         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/build_wiredtiger.sh
#         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
#         COMMENT "Building WiredTiger (Generating libwiredtiger.so)"
#         VERBATIM
#     )

#     add_custom_target(build_wiredtiger DEPENDS ${WT_LIB})

#     add_library(wiredtiger SHARED IMPORTED GLOBAL)
#     set_target_properties(wiredtiger PROPERTIES
#         IMPORTED_LOCATION ${WT_LIB}
#         INTERFACE_INCLUDE_DIRECTORIES ${WT_ROOT}/build/include
#     )

#     if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.19.0") 
#         add_dependencies(wiredtiger build_wiredtiger)
#     endif()

# endif()