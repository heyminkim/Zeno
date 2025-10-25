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

# Add benchmark executable for benchLDCF
add_executable(benchLDCF benchmarks/benchLDCF.cpp)
target_link_libraries(benchLDCF your_library)