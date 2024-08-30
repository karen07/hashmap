cmake_minimum_required(VERSION 3.6)
add_compile_options(-Wall -Wextra -Werror)
include_directories(include) 
add_library(hashmap STATIC src/array_hashmap.c)
