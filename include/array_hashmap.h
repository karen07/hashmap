#ifndef __ARRAY_HASHMAP__
#define __ARRAY_HASHMAP__

#include <stdint.h>

#define THREAD_SAFETY

#define array_hashmap_save_new 1
#define array_hashmap_save_old 0
#define array_hashmap_save_new_func (on_already_in_t)1
#define array_hashmap_save_old_func (on_already_in_t)0

#define array_hashmap_del_by_func 1
#define array_hashmap_not_del_by_func 0

typedef int32_t array_hashmap_bool;
typedef uint32_t array_hashmap_hash;
typedef int32_t array_hashmap_deled_count;
typedef const void *array_hashmap_t;

typedef array_hashmap_hash (*add_hash_t)(const void *add_elem_data);
typedef array_hashmap_bool (*add_cmp_t)(const void *add_elem_data, const void *hashmap_elem_data);
typedef array_hashmap_hash (*find_hash_t)(const void *find_elem_data);
typedef array_hashmap_bool (*find_cmp_t)(const void *find_elem_data, const void *hashmap_elem_data);
typedef array_hashmap_hash (*del_hash_t)(const void *del_elem_data);
typedef array_hashmap_bool (*del_cmp_t)(const void *del_elem_data, const void *hashmap_elem_data);
typedef array_hashmap_bool (*on_already_in_t)(const void *add_elem_data,
                                              const void *hashmap_elem_data);
typedef array_hashmap_bool (*del_func_t)(const void *del_elem_data);

typedef enum array_hashmap_ret {
    array_hashmap_empty_args = -3,
    array_hashmap_empty_funcs = -2,
    array_hashmap_full = -1,
    array_hashmap_elem_added = 1,
    array_hashmap_elem_already_in = 0,
    array_hashmap_elem_finded = 1,
    array_hashmap_elem_not_finded = 0,
    array_hashmap_elem_deled = 1,
    array_hashmap_elem_not_deled = 0
} array_hashmap_ret_t;

array_hashmap_t array_hashmap_init(int32_t hashmap_size, double max_load, int32_t type_size);
void array_hashmap_del(array_hashmap_t *);

void array_hashmap_set_func(array_hashmap_t, add_hash_t, add_cmp_t, find_hash_t, find_cmp_t,
                            del_hash_t, del_cmp_t);

int32_t array_hashmap_now_in_map(array_hashmap_t map_struct_c);
array_hashmap_bool array_hashmap_is_thread_safety(array_hashmap_t map_struct_c);

array_hashmap_ret_t array_hashmap_add_elem(array_hashmap_t, const void *add_elem_data,
                                           void *res_elem_data, on_already_in_t);
array_hashmap_ret_t array_hashmap_find_elem(array_hashmap_t, const void *find_elem_data,
                                            void *res_elem_data);
array_hashmap_ret_t array_hashmap_del_elem(array_hashmap_t, const void *del_elem_data,
                                           void *res_elem_data);
array_hashmap_deled_count array_hashmap_del_elem_by_func(array_hashmap_t, del_func_t);

#endif
