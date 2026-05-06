find_package(OpenSSL REQUIRED)
find_package(OpenMP REQUIRED)
find_package(Threads REQUIRED)

include(CheckCXXCompilerFlag)

check_cxx_compiler_flag("-march=core-avx2" HAS_MARCH_CORE_AVX2)
check_cxx_compiler_flag("-mavx2" HAS_MAVX2)
check_cxx_compiler_flag("-mfma" HAS_MFMA)

# Configure build type
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(BITHACKING_COMPILE_FLAGS -Ofast -march=native)
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BITHACKING_COMPILE_FLAGS -pg -march=native)
endif()

# Include Bamboo
if (HAS_MARCH_CORE_AVX2 AND HAS_MAVX2 AND HAS_MFMA)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/include/bamboofilters/src)
endif()

# Include LDCF
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/include/LDCF)

# Include CQF
# file(GLOB CQF_SOURCES
#     ${CMAKE_CURRENT_SOURCE_DIR}/include/cqf/src/*.c
# )
# add_library(cqf STATIC ${CQF_SOURCES})
# target_include_directories(cqf PUBLIC
#     ${CMAKE_CURRENT_SOURCE_DIR}/include/cqf/include
# )
# target_compile_options(cqf PRIVATE ${BITHACKING_COMPILE_FLAGS})

# exp 1
add_executable(exp_fractional_expansion exp_fractional_expansion.cc)
add_executable(exp_spaceamp exp_spaceamp.cc)
# exp 2
add_executable(exp_inplace_expansion exp_inplace_expansion.cc)
# exp 3
add_executable(exp_widening exp_widening.cc)
# exp 4
add_executable(exp_directory exp_directory.cc)
# exp 5
add_executable(exp_contract exp_contract.cc)
# exp 6
add_executable(exp_concurrency exp_concurrency.cc)
add_executable(exp_concurrent_rw exp_concurrent_rw.cc)
# exp 7
add_executable(exp_overall exp_overall.cc)
# exp 8
add_executable(exp_wiredtiger exp_wiredtiger.cc)

if(BUILD_WIREDTIGER)
    target_link_libraries(exp_wiredtiger PRIVATE wiredtiger)
    add_dependencies(exp_wiredtiger build_wiredtiger)
endif()

file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

# Link common libraries
foreach(src ${SOURCES})
    get_filename_component(exe_name ${src} NAME_WE)

    target_include_directories(${exe_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../util
    )

    target_link_libraries(${exe_name} PRIVATE
        Threads::Threads
        OpenSSL::SSL
    )

    target_compile_options(${exe_name} PRIVATE ${BITHACKING_COMPILE_FLAGS})
endforeach()


target_link_libraries(exp_overall PRIVATE
    OpenMP::OpenMP_CXX
    your_library
)

if (HAS_MARCH_CORE_AVX2 AND HAS_MAVX2 AND HAS_MFMA)
    target_link_libraries(exp_overall PRIVATE
        header          # for bamboo filter
        hash            # for bamboo filter
    )
    target_compile_options(exp_overall PRIVATE -march=native -mavx2 -mfma)
endif()

target_link_libraries(exp_concurrency PRIVATE Threads::Threads)
target_link_libraries(exp_concurrent_rw PRIVATE Threads::Threads)
