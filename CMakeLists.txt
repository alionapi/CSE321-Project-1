cmake_minimum_required(VERSION 3.16)

project(dbpa1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Warnings: clang on macOS and gcc both accept these.
add_compile_options(-Wall -Wextra)

# Main experiment driver
add_executable(btree_project
        src/main.cpp
        src/common.h
        src/csv_loader.h
        src/btree.h
        src/bstar_tree.h
        src/bplus_tree.h)

add_executable(test_correctness
        src/test_correctness.cpp
        src/common.h
        src/btree.h
        src/bstar_tree.h
        src/bplus_tree.h)


add_executable(gen_data
        src/gen_data.cpp)
