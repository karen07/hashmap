#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef __ARRAY_HASHMAP__
#define __ARRAY_HASHMAP__

typedef struct array_hashmap {
    char* map;
    int32_t map_size;
    double max_load;
    int32_t now_in_map;
    int32_t type_size;
    uint32_t (*add_hash)(const void*);
    int32_t (*add_cmp)(const void*, const void*);
    uint32_t (*find_hash)(const void*);
    int32_t (*find_cmp)(const void*, const void*);
    int32_t iter_flag;
    pthread_rwlock_t rwlock;
} array_hashmap_t;

typedef struct array_hashmap_elem {
    int32_t next;
    char data[];
} array_hashmap_elem_t;

typedef struct array_hashmap_iter {
    int32_t next;
    array_hashmap_t* map_struct;
} array_hashmap_iter_t;

const array_hashmap_t* init_array_hashmap(int32_t map_size, double max_load, int32_t type_size);
void del_array_hashmap(const array_hashmap_t* map_struct);

void array_hashmap_set_func(const array_hashmap_t* map_struct, uint32_t (*)(const void*), int32_t (*)(const void*, const void*), uint32_t (*)(const void*), int32_t (*)(const void*, const void*));

int32_t array_hashmap_add_elem(const array_hashmap_t* map_struct, const void* add_elem, void* res_elem, int32_t (*)(const void*, const void*));
int32_t array_hashmap_find_elem(const array_hashmap_t* map_struct_c, const void* find_elem, void* res_elem);
int32_t array_hashmap_del_elem(const array_hashmap_t* map_struct, const void* del_elem, void* res_elem);

const array_hashmap_iter_t* array_hashmap_get_iter(const array_hashmap_t* map_struct);
int32_t array_hashmap_next(const array_hashmap_iter_t* iter, void* next_elem);
void array_hashmap_del_iter(const array_hashmap_iter_t* iter);

int32_t array_hashmap_del_elem_by_func(const array_hashmap_t* map_struct, int32_t (*)(const void*));

int32_t array_hashmap_get_size(const array_hashmap_t* map_struct);

#endif
