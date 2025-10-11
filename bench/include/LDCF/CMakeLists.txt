cmake_minimum_required(VERSION 3.10)
project(LDCF_Project)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CLANG_TIDY "clang-tidy")
if(CLANG_TIDY)
    message(STATUS "clang-tidy found: ${CLANG_TIDY}")
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY}")
else()
    message(STATUS "clang-tidy not found")
endif()

# Add source files
add_library(your_library
    src/CF.cpp
    src/LDCF.cpp
)

# Include directories
target_include_directories(your_library PUBLIC src)

# Add test executable
add_executable(test_CF test/test_CF.cpp)
target_link_libraries(test_CF gtest gtest_main your_library)

# Add test executable
add_executable(test_LDCF test/test_LDCF.cpp)
target_link_libraries(test_LDCF gtest gtest_main your_library)

# Add test executable
add_executable(test_bucket test/test_bucket.cpp)
target_link_libraries(test_bucket gtest gtest_main your_library)

# Add tests to CTest
add_test(NAME TestCF COMMAND test_CF)
add_test(NAME TestLDCF COMMAND test_LDCF)
add_test(NAME TestBucket COMMAND test_bucket)

# Add benchmark executable for benchLDCF
add_executable(benchLDCF benchmarks/benchLDCF.cpp)
target_link_libraries(benchLDCF your_library)