#ifndef __ARRAY_HASHMAP__
#define __ARRAY_HASHMAP__

#include <stdint.h>

typedef const void *array_hashmap_t;

typedef uint32_t (*add_hash_t)(const void *add_elem);
typedef int32_t (*add_cmp_t)(const void *add_elem, const void *hashmap_elem);
typedef uint32_t (*find_hash_t)(const void *find_elem);
typedef int32_t (*find_cmp_t)(const void *find_elem, const void *hashmap_elem);
typedef int32_t (*on_already_in_t)(const void *add_elem, const void *old_elem);
typedef int32_t (*del_func_t)(const void *elem);

array_hashmap_t array_hashmap_init(int32_t hashmap_size, double max_load, int32_t type_size);
void array_hashmap_del(array_hashmap_t);

void array_hashmap_set_func(array_hashmap_t, add_hash_t, add_cmp_t, find_hash_t, find_cmp_t);

int32_t array_hashmap_add_elem(array_hashmap_t, const void *add_elem, void *res_elem,
                               on_already_in_t);
int32_t array_hashmap_find_elem(array_hashmap_t, const void *find_elem, void *res_elem);
int32_t array_hashmap_del_elem(array_hashmap_t, const void *del_elem, void *res_elem);
int32_t array_hashmap_del_elem_by_func(array_hashmap_t, del_func_t);

int32_t array_hashmap_get_size(array_hashmap_t);

#define array_hashmap_add_new (on_already_in_t)1
#define array_hashmap_save_old (on_already_in_t)0

enum array_hashmap_ret {
    array_hashmap_empty_args = -3,
    array_hashmap_empty_funcs = -2,
    array_hashmap_full = -1,
    array_hashmap_elem_added = 1,
    array_hashmap_elem_already_in = 0,
    array_hashmap_elem_finded = 1,
    array_hashmap_elem_not_finded = 0,
    array_hashmap_elem_deled = 1,
    array_hashmap_elem_not_deled = 0
};

#endif
