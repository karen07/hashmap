#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef __ARRAY_HASHMAP__
#define __ARRAY_HASHMAP__

typedef enum operations {
    hashmap_add,
    hashmap_find,
    hashmap_replace,
    hashmap_del
} operations_t;

typedef enum next_val {
    hashmap_empty = -2,
    hashmap_alone = -1
} next_val_t;

typedef enum operations_val {
    hashmap_ok = 1,
    hashmap_no_elem = -1,
    hashmap_iter_process = -2,
    hashmap_no_free_mem = -3,
    hashmap_elem_already_in = -4,
    hashmap_null_point = -5
} operations_val_t;

typedef struct array_hashmap {
    char* map;
    int32_t map_size;
    double max_load;
    int32_t now_in_map;
    int32_t type_size;
    uint32_t (*hash_func[4])(const void*);
    int32_t (*cmp_func[4])(const void*, const void*);
    int32_t iter_flag;
    pthread_rwlock_t rwlock;
    pthread_rwlock_t iterlock;
} array_hashmap_t;

typedef struct array_hashmap_elem {
    next_val_t next;
    char data[];
} array_hashmap_elem_t;

typedef struct array_hashmap_iter {
    next_val_t next;
    array_hashmap_t* map_struct;
} array_hashmap_iter_t;

const array_hashmap_t* init_array_hashmap(int32_t map_size, double max_load, int32_t type_size, uint32_t (*)(const void*), int32_t (*)(const void*, const void*));
void array_hashmap_set_find_funcs(const array_hashmap_t* map_struct_c, uint32_t (*)(const void*), int32_t (*)(const void*, const void*));
void array_hashmap_set_del_funcs(const array_hashmap_t* map_struct_c, uint32_t (*)(const void*), int32_t (*)(const void*, const void*));
int32_t array_hashmap_get_size(const array_hashmap_t* map_struct_c);
void del_array_hashmap(const array_hashmap_t* map_struct_c);

int32_t array_hashmap_add_elem(const array_hashmap_t* map_struct_c, const void* add_elem, void* res_elem);
int32_t array_hashmap_repl_elem(const array_hashmap_t* map_struct_c, const void* repl_elem, void* res_elem);
int32_t array_hashmap_find_elem(const array_hashmap_t* map_struct_c, const void* find_elem, void* res_elem);
int32_t array_hashmap_del_elem(const array_hashmap_t* map_struct_c, const void* del_elem, void* res_elem);

int32_t array_hashmap_add_elem_wait(const array_hashmap_t* map_struct_c, const void* add_elem, void* res_elem);
int32_t array_hashmap_repl_elem_wait(const array_hashmap_t* map_struct_c, const void* repl_elem, void* res_elem);
int32_t array_hashmap_find_elem_wait(const array_hashmap_t* map_struct_c, const void* find_elem, void* res_elem);
int32_t array_hashmap_del_elem_wait(const array_hashmap_t* map_struct_c, const void* del_elem, void* res_elem);

int32_t array_hashmap_del_elem_by_func(const array_hashmap_t* map_struct_c, int32_t (*)(const void*));

const array_hashmap_iter_t* array_hashmap_get_iter(const array_hashmap_t* map_struct_c);
int32_t array_hashmap_next(const array_hashmap_iter_t* iter, void* next_elem);
void array_hashmap_del_iter(const array_hashmap_iter_t* iter);

#endif
