cmake_minimum_required(VERSION 2.6)

include_directories(include) 
add_library(hashmap STATIC array_hashmap.c)
