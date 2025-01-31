# Hashmap
Fixed size hash map over array in C language for any data type. Thread safety and extremely fast.
![alt text](hashmap.svg)
## Link
You can add hashmap to your project as CMake subdirectory or compile hashmap and link lib to your project.
To build the thread safety version, you must pass define `THREAD_SAFETY`.
## Description
This hash map use list over array on collision. Hash map structure add `int32_t next` to input type.
## Usage
All functions usage examples in [test.c](test/test.c).
