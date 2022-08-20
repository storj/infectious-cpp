# Copyright (c) 2021-2022 Storj Labs, Inc.
# See LICENSE for copying information.

cmake_minimum_required(VERSION 3.16)
project(
    infectious_cpp
    DESCRIPTION "Forward Error Correction in C++20"
    LANGUAGES CXX
    VERSION 0.2)

#set(CMAKE_DEBUG_TARGET_PROPERTIES INCLUDE_DIRECTORIES)
#set(CMAKE_VERBOSE_MAKEFILE on)

add_compile_options(-Wall -Wextra -pedantic -Werror)

set(CMAKE_CXX_CLANG_TIDY "clang-tidy")

add_subdirectory(src)

# Only do the following if this is the main project, and not included in something else
if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(CMAKE_CXX_STANDARD 20)

    set_property(GLOBAL PROPERTY USE_FOLDERS ON)

    # Testing only available if this is the main app
    enable_testing()
endif()

# Testing only available if this is the main app
# Emergency override MODERN_CMAKE_BUILD_TESTING provided as well
if((CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME OR MODERN_CMAKE_BUILD_TESTING)
        AND BUILD_TESTING)
    add_subdirectory(tests)
endif()
