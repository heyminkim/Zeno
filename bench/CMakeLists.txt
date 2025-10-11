find_package(OpenSSL REQUIRED)
find_package(OpenMP REQUIRED)
find_package(Threads REQUIRED)

include(CheckCXXCompilerFlag)

check_cxx_compiler_flag("-march=core-avx2" HAS_MARCH_CORE_AVX2)
check_cxx_compiler_flag("-mavx2" HAS_MAVX2)
check_cxx_compiler_flag("-mfma" HAS_MFMA)

# Include Bamboo
if (HAS_MARCH_CORE_AVX2 AND HAS_MAVX2 AND HAS_MFMA)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/include/bamboofilters/src)
endif()

# Include E2CF
file(GLOB E2CF_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/include/E2CF/src/*.cpp")
add_library(E2CF STATIC ${E2CF_SOURCES})
target_include_directories(E2CF PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include/E2CF/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include/E2CF
)
target_link_libraries(E2CF PRIVATE OpenSSL::SSL OpenSSL::Crypto)

# Include LDCF
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/include/LDCF)

add_executable(exp_fractional_expansion exp_fractional_expansion.cc)
add_executable(exp_inplace_expansion exp_inplace_expansion.cc)
add_executable(exp_fe_aleph exp_fractional_expansion.cc)
add_executable(exp_bamboo exp_bamboo.cc)
add_executable(exp_insert_spaceamp exp_insert_spaceamp.cc)
add_executable(exp_concurrency exp_concurrency.cc)
add_executable(exp_concurrent_rw exp_concurrent_rw.cc)
add_executable(exp_stretching_overall exp_stretching_overall.cc)
add_executable(exp_stretching_overall_niv exp_stretching_overall_niv.cc)
add_executable(exp_fpr_writeamp exp_fpr_writeamp.cc)
add_executable(exp_delete_size exp_delete_size.cc)
add_executable(exp_void exp_void.cc)
add_executable(fractional_expansion fractional_expansion.cc)

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
        OpenSSL::SSL
    )
endforeach()


target_link_libraries(exp_bamboo PRIVATE
    OpenMP::OpenMP_CXX
    your_library    # for LDCF
    E2CF
)

if (HAS_MARCH_CORE_AVX2 AND HAS_MAVX2 AND HAS_MFMA)
    target_link_libraries(exp_bamboo PRIVATE
        header          # for bamboo filter
        hash            # for bamboo filter
    )
    target_compile_options(exp_bamboo PRIVATE -march=core-avx2 -mavx2 -mfma)
endif()

target_link_libraries(exp_concurrency PRIVATE Threads::Threads)
target_link_libraries(exp_concurrent_rw PRIVATE Threads::Threads)
