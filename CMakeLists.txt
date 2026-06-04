cmake_minimum_required(VERSION 3.15)
project(cvmpp CXX)

# Set C++17 as the target language standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Configure compiler flags for warnings and optimizations based on the toolchain
if(MSVC)
    add_compile_options(/W4 /O2)
else()
    add_compile_options(-Wall -Wextra -O2)
endif()

# Define the executable binary targets and primary source entry point
add_executable(cvmpp src/main.cpp)

# Specify the include directories for internal header file resolution
target_include_directories(cvmpp PRIVATE src)