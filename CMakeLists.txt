cmake_minimum_required(VERSION 3.10)

project(hashmap)

include_directories(include)
file(GLOB SRC_LIB "src/*.c")
add_library(${PROJECT_NAME} STATIC ${SRC_LIB})

find_program(CLANGFORMAT clang-format)
if(CLANGFORMAT)
    add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
        COMMAND clang-format -i ${CMAKE_CURRENT_SOURCE_DIR}/include/* ${CMAKE_CURRENT_SOURCE_DIR}/src/*
    )
endif()

project(hashmap_test)

add_compile_options(-Wall -Wextra -Werror -Wpedantic -std=gnu89)
add_link_options()
include_directories(include)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    # -fsanitize=memory
    add_compile_options(-Og -g -fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

file(GLOB SRC "test/*.c")
add_executable(${PROJECT_NAME} ${SRC})
target_link_libraries(${PROJECT_NAME} hashmap)

install(TARGETS ${PROJECT_NAME} DESTINATION /usr/bin)

find_program(CLANGFORMAT clang-format)
if(CLANGFORMAT)
    add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
        COMMAND clang-format -i ${CMAKE_CURRENT_SOURCE_DIR}/test/*
    )
endif()
