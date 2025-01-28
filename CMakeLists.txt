cmake_minimum_required(VERSION 3.10)

project(hashmap)

include_directories(include)
add_library(hashmap STATIC src/array_hashmap.c)

project(hashmap_test)

add_compile_options(-Wall -Wextra -Werror -Wpedantic -std=gnu89)

add_executable(hashmap_test test/test.c)
target_link_libraries(hashmap_test hashmap)

find_program(CLANGFORMAT clang-format)
if(CLANGFORMAT)
    add_custom_command(TARGET hashmap_test PRE_BUILD
        COMMAND clang-format -i ${CMAKE_CURRENT_SOURCE_DIR}/test/*
    )
    add_custom_command(TARGET hashmap PRE_BUILD
        COMMAND clang-format -i ${CMAKE_CURRENT_SOURCE_DIR}/include/* ${CMAKE_CURRENT_SOURCE_DIR}/src/*
    )
endif()
