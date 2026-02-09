cmake_minimum_required(VERSION 3.13)

project(hashmap)

add_compile_options(-Wall -Wextra -Werror -Wpedantic)
add_link_options()
include_directories(include)

if(CMAKE_BUILD_TYPE MATCHES "Debug_ASan")
    add_compile_options(-Og -g -fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-g -fsanitize=address)
endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug_MSan")
    add_compile_options(-Og -g -fsanitize=memory -fno-omit-frame-pointer)
    add_link_options(-g -fsanitize=memory)
endif()

file(GLOB SRC_LIB "src/*.c")
add_library(${PROJECT_NAME} STATIC ${SRC_LIB})
target_compile_options(${PROJECT_NAME} PRIVATE -std=gnu89)

add_library(hashmap_threadsafe STATIC ${SRC_LIB})
target_compile_options(hashmap_threadsafe PRIVATE -std=gnu89 -DTHREAD_SAFETY)
set_target_properties(hashmap_threadsafe PROPERTIES EXCLUDE_FROM_ALL TRUE)

find_program(CLANGFORMAT clang-format)
if(CLANGFORMAT)
    add_custom_command(
        TARGET ${PROJECT_NAME}
        PRE_BUILD
        COMMAND clang-format -i ${CMAKE_CURRENT_SOURCE_DIR}/include/*
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*)
endif()

project(hashmap_test)

file(GLOB SRC "test/*.c")
add_executable(hashmap_test ${SRC})
target_include_directories(hashmap_test PRIVATE include)
target_link_libraries(hashmap_test hashmap_threadsafe)
set_target_properties(hashmap_test PROPERTIES EXCLUDE_FROM_ALL TRUE)

find_program(CLANGFORMAT clang-format)
if(CLANGFORMAT)
    add_custom_command(
        TARGET hashmap_test
        PRE_BUILD
        COMMAND clang-format -i ${CMAKE_CURRENT_SOURCE_DIR}/test/*)
endif()
