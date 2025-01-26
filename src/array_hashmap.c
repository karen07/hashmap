#include "array_hashmap.h"
#ifdef THREAD_SAFETY
#include <pthread.h>
#endif
#include <stdlib.h>
#include <string.h>

typedef struct hashmap {
    char *map;
    int32_t map_size;
    int32_t max_size;
    int32_t now_in_map;
    int32_t elem_size;
    int32_t data_size;
    add_hash_t add_hash;
    add_cmp_t add_cmp;
    find_hash_t find_hash;
    find_cmp_t find_cmp;
    del_hash_t del_hash;
    del_cmp_t del_cmp;
#ifdef THREAD_SAFETY
    pthread_rwlock_t rwlock;
#endif
} hashmap_t;

typedef struct __attribute__((packed)) elem {
    int32_t next;
    char data;
} elem_t;

enum next { elem_empty = -2, elem_last = -1 };

#define index_add(data) (map_struct->add_hash(data) % map_struct->map_size)
#define index_find(data) (map_struct->find_hash(data) % map_struct->map_size)
#define index_del(data) (map_struct->del_hash(data) % map_struct->map_size)
#define elem_i(index) ((elem_t *)&map_struct->map[index * map_struct->elem_size])

array_hashmap_t array_hashmap_init(int32_t map_size, double max_load, int32_t type_size)
{
    char *map = NULL;
    int32_t i = 0;

    hashmap_t *map_struct = NULL;

    if (map_size <= 0) {
        return NULL;
    }

    if (type_size <= 0) {
        return NULL;
    }

    if (max_load <= 0) {
        return NULL;
    }

    if (max_load > 1.0) {
        return NULL;
    }

    map_struct = malloc(sizeof(hashmap_t));
    if (!map_struct) {
        return NULL;
    }

    map_struct->map_size = map_size;
    map_struct->max_size = map_size * max_load;
    map_struct->data_size = type_size;
    map_struct->elem_size = type_size + sizeof(elem_t) - sizeof(char);
    map_struct->add_hash = NULL;
    map_struct->add_cmp = NULL;
    map_struct->find_hash = NULL;
    map_struct->find_cmp = NULL;
    map_struct->del_hash = NULL;
    map_struct->del_cmp = NULL;
    map_struct->now_in_map = 0;

    map = malloc(map_struct->map_size * map_struct->elem_size);
    if (!map) {
        free(map_struct);
        return NULL;
    }
    map_struct->map = map;

#ifdef THREAD_SAFETY
    if (pthread_rwlock_init(&map_struct->rwlock, NULL)) {
        free(map);
        free(map_struct);
        return NULL;
    }
#endif

    for (i = 0; i < map_struct->map_size; i++) {
        elem_t *elem = elem_i(i);
        elem->next = elem_empty;
    }

    return (array_hashmap_t)map_struct;
}

void array_hashmap_set_func(array_hashmap_t map_struct_c, add_hash_t add_hash, add_cmp_t add_cmp,
                            find_hash_t find_hash, find_cmp_t find_cmp, del_hash_t del_hash,
                            del_cmp_t del_cmp)
{
    hashmap_t *map_struct = NULL;
    map_struct = (hashmap_t *)map_struct_c;
    if (!map_struct) {
        return;
    }

#ifdef THREAD_SAFETY
    pthread_rwlock_wrlock(&map_struct->rwlock);
#endif

    map_struct->add_hash = add_hash;
    map_struct->add_cmp = add_cmp;

    map_struct->find_hash = find_hash;
    map_struct->find_cmp = find_cmp;

    map_struct->del_hash = del_hash;
    map_struct->del_cmp = del_cmp;

#ifdef THREAD_SAFETY
    pthread_rwlock_unlock(&map_struct->rwlock);
#endif
}

int32_t array_hashmap_get_size(array_hashmap_t map_struct_c)
{
    hashmap_t *map_struct = NULL;
    map_struct = (hashmap_t *)map_struct_c;
    if (!map_struct) {
        return array_hashmap_empty_args;
    }

    return map_struct->now_in_map;
}

int32_t array_hashmap_add_elem(array_hashmap_t map_struct_c, const void *add_elem_data,
                               void *res_elem_data, on_already_in_t on_already_in)
{
    int32_t add_elem_index = 0;

    int32_t check_elem_index = 0;
    elem_t *check_elem = NULL;
    void *check_elem_data = NULL;

    int32_t list_prev_elem_index = 0;

    int32_t list_elem_index = 0;
    elem_t *list_elem = NULL;
    void *list_elem_data = NULL;

    int32_t new_elem_index = 0;
    elem_t *new_elem = NULL;
    void *new_elem_data = NULL;

    hashmap_t *map_struct = NULL;
    map_struct = (hashmap_t *)map_struct_c;
    if (!map_struct || !add_elem_data) {
        return array_hashmap_empty_args;
    }

    if (!map_struct->add_hash || !map_struct->add_cmp) {
        return array_hashmap_empty_funcs;
    }

#ifdef THREAD_SAFETY
    pthread_rwlock_wrlock(&map_struct->rwlock);
#endif

    add_elem_index = index_add(add_elem_data);
    check_elem = elem_i(add_elem_index);
    check_elem_data = &check_elem->data;

    if (check_elem->next == elem_empty) {
        if (map_struct->now_in_map < map_struct->max_size) {
            check_elem->next = elem_last;
            memcpy(check_elem_data, add_elem_data, map_struct->data_size);

            map_struct->now_in_map++;

#ifdef THREAD_SAFETY
            pthread_rwlock_unlock(&map_struct->rwlock);
#endif
            return array_hashmap_elem_added;
        } else {
#ifdef THREAD_SAFETY
            pthread_rwlock_unlock(&map_struct->rwlock);
#endif
            return array_hashmap_full;
        }
    } else {
        check_elem_index = index_add(check_elem_data);

        if (check_elem_index == add_elem_index) {
            list_prev_elem_index = 0;
            list_elem_index = check_elem_index;
            list_elem = NULL;
            list_elem_data = NULL;

            do {
                list_elem = elem_i(list_elem_index);
                list_elem_data = &list_elem->data;

                if (map_struct->add_cmp(add_elem_data, list_elem_data)) {
                    if (on_already_in) {
                        if (on_already_in == array_hashmap_save_new_func) {
                            memcpy(list_elem_data, add_elem_data, map_struct->data_size);
                        } else {
                            if (on_already_in(add_elem_data, list_elem_data)) {
                                memcpy(list_elem_data, add_elem_data, map_struct->data_size);
                            }
                        }
                    }
                    if (res_elem_data) {
                        memcpy(res_elem_data, list_elem_data, map_struct->data_size);
                    }
#ifdef THREAD_SAFETY
                    pthread_rwlock_unlock(&map_struct->rwlock);
#endif
                    return array_hashmap_elem_already_in;
                }

                list_prev_elem_index = list_elem_index;
                list_elem_index = list_elem->next;
            } while (list_elem_index != elem_last);

            list_elem_index = list_prev_elem_index;
            list_elem = elem_i(list_elem_index);

            if (map_struct->now_in_map < map_struct->max_size) {
                new_elem_index = list_elem_index;
                new_elem = NULL;

                do {
                    new_elem_index = (new_elem_index + 1) % map_struct->map_size;
                    new_elem = elem_i(new_elem_index);
                } while (new_elem->next != elem_empty);

                new_elem->next = elem_last;
                new_elem_data = &new_elem->data;
                memcpy(new_elem_data, add_elem_data, map_struct->data_size);
                list_elem->next = new_elem_index;

                map_struct->now_in_map++;

#ifdef THREAD_SAFETY
                pthread_rwlock_unlock(&map_struct->rwlock);
#endif
                return array_hashmap_elem_added;
            } else {
#ifdef THREAD_SAFETY
                pthread_rwlock_unlock(&map_struct->rwlock);
#endif
                return array_hashmap_full;
            }
        } else {
            if (map_struct->now_in_map < map_struct->max_size) {
                list_elem = elem_i(check_elem_index);
                while (list_elem->next != add_elem_index) {
                    list_elem = elem_i(list_elem->next);
                }

                new_elem_index = add_elem_index;
                new_elem = NULL;

                do {
                    new_elem_index = (new_elem_index + 1) % map_struct->map_size;
                    new_elem = elem_i(new_elem_index);
                } while (new_elem->next != elem_empty);

                memcpy(new_elem, check_elem, map_struct->elem_size);
                list_elem->next = new_elem_index;

                check_elem->next = elem_last;
                memcpy(check_elem_data, add_elem_data, map_struct->data_size);

                map_struct->now_in_map++;

#ifdef THREAD_SAFETY
                pthread_rwlock_unlock(&map_struct->rwlock);
#endif
                return array_hashmap_elem_added;
            } else {
#ifdef THREAD_SAFETY
                pthread_rwlock_unlock(&map_struct->rwlock);
#endif
                return array_hashmap_full;
            }
        }
    }
}

int32_t array_hashmap_find_elem(array_hashmap_t map_struct_c, const void *find_elem_data,
                                void *res_elem_data)
{
    int32_t find_elem_index = 0;
    elem_t *find_elem = NULL;

    int32_t list_elem_index = 0;
    elem_t *list_elem = NULL;
    void *list_elem_data = NULL;

    hashmap_t *map_struct = NULL;
    map_struct = (hashmap_t *)map_struct_c;
    if (!map_struct || !find_elem_data) {
        return array_hashmap_empty_args;
    }

    if (!map_struct->find_hash || !map_struct->find_cmp) {
        return array_hashmap_empty_funcs;
    }

#ifdef THREAD_SAFETY
    pthread_rwlock_rdlock(&map_struct->rwlock);
#endif

    find_elem_index = index_find(find_elem_data);
    find_elem = elem_i(find_elem_index);

    if (find_elem->next == elem_empty) {
#ifdef THREAD_SAFETY
        pthread_rwlock_unlock(&map_struct->rwlock);
#endif
        return array_hashmap_elem_not_finded;
    }

    list_elem_index = find_elem_index;
    while (list_elem_index != elem_last) {
        list_elem = elem_i(list_elem_index);
        list_elem_data = &list_elem->data;
        if (map_struct->find_cmp(find_elem_data, list_elem_data)) {
            if (res_elem_data) {
                memcpy(res_elem_data, list_elem_data, map_struct->data_size);
            }
#ifdef THREAD_SAFETY
            pthread_rwlock_unlock(&map_struct->rwlock);
#endif
            return array_hashmap_elem_finded;
        }

        list_elem_index = list_elem->next;
    }

#ifdef THREAD_SAFETY
    pthread_rwlock_unlock(&map_struct->rwlock);
#endif
    return array_hashmap_elem_not_finded;
}

int32_t array_hashmap_del_elem(array_hashmap_t map_struct_c, const void *del_elem_data,
                               void *res_elem_data)
{
    int32_t del_elem_index = 0;
    elem_t *del_elem = NULL;

    int32_t list_prev_elem_index = 0;
    elem_t *list_prev_elem = NULL;

    int32_t list_next_elem_index = 0;
    elem_t *list_next_elem = NULL;

    int32_t list_elem_index = 0;
    elem_t *list_elem = NULL;
    void *list_elem_data = NULL;

    hashmap_t *map_struct = NULL;
    map_struct = (hashmap_t *)map_struct_c;
    if (!map_struct || !del_elem_data) {
        return array_hashmap_empty_args;
    }

    if (!map_struct->del_hash || !map_struct->del_cmp) {
        return array_hashmap_empty_funcs;
    }

#ifdef THREAD_SAFETY
    pthread_rwlock_wrlock(&map_struct->rwlock);
#endif

    del_elem_index = index_del(del_elem_data);
    del_elem = elem_i(del_elem_index);

    if (del_elem->next == elem_empty) {
#ifdef THREAD_SAFETY
        pthread_rwlock_unlock(&map_struct->rwlock);
#endif
        return array_hashmap_elem_not_deled;
    }

    list_prev_elem_index = elem_last;
    list_elem_index = del_elem_index;
    while (list_elem_index != elem_last) {
        list_elem = elem_i(list_elem_index);
        list_elem_data = &list_elem->data;
        if (map_struct->del_cmp(del_elem_data, list_elem_data)) {
            if (res_elem_data) {
                memcpy(res_elem_data, list_elem_data, map_struct->data_size);
            }

            if (list_elem->next == elem_last) {
                if (list_prev_elem_index != elem_last) {
                    list_prev_elem = elem_i(list_prev_elem_index);
                    list_prev_elem->next = elem_last;
                }

                list_elem->next = elem_empty;
            } else {
                list_next_elem_index = list_elem->next;
                list_next_elem = elem_i(list_next_elem_index);

                memcpy(list_elem, list_next_elem, map_struct->elem_size);

                list_next_elem->next = elem_empty;
            }

            map_struct->now_in_map--;
#ifdef THREAD_SAFETY
            pthread_rwlock_unlock(&map_struct->rwlock);
#endif
            return array_hashmap_elem_deled;
        }

        list_prev_elem_index = list_elem_index;
        list_elem_index = list_elem->next;
    }

#ifdef THREAD_SAFETY
    pthread_rwlock_unlock(&map_struct->rwlock);
#endif
    return array_hashmap_elem_not_deled;
}

int32_t array_hashmap_del_elem_by_func(array_hashmap_t map_struct_c, del_func_t del_func)
{
    int32_t del_count = 0;
    int32_t i = 0;

    int32_t elem_index = 0;
    elem_t *elem = NULL;
    void *elem_data = NULL;

    int32_t list_prev_elem_index = 0;
    elem_t *list_prev_elem = NULL;

    int32_t list_next_elem_index = 0;
    elem_t *list_next_elem = NULL;

    int32_t list_elem_index = 0;
    elem_t *list_elem = NULL;
    void *list_elem_data = NULL;

    hashmap_t *map_struct = NULL;
    map_struct = (hashmap_t *)map_struct_c;
    if (!map_struct || !del_func) {
        return array_hashmap_empty_args;
    }

#ifdef THREAD_SAFETY
    pthread_rwlock_wrlock(&map_struct->rwlock);
#endif

    for (i = 0; i < map_struct->map_size; i++) {
        elem = elem_i(i);
        if (elem->next == elem_empty) {
            continue;
        }

        elem_data = &elem->data;

        elem_index = index_add(elem_data);
        if (elem_index != i) {
            continue;
        }

        list_prev_elem_index = elem_last;
        list_elem_index = elem_index;
        while (list_elem_index != elem_last) {
            list_elem = elem_i(list_elem_index);
            list_elem_data = &list_elem->data;
            if (del_func(list_elem_data)) {
                if (list_elem->next == elem_last) {
                    if (list_prev_elem_index != elem_last) {
                        list_prev_elem = elem_i(list_prev_elem_index);
                        list_prev_elem->next = elem_last;
                    }

                    list_elem->next = elem_empty;
                    list_elem_index = elem_last;
                } else {
                    list_next_elem_index = list_elem->next;
                    list_next_elem = elem_i(list_next_elem_index);

                    memcpy(list_elem, list_next_elem, map_struct->elem_size);

                    list_next_elem->next = elem_empty;
                }

                del_count++;
                map_struct->now_in_map--;
            } else {
                list_prev_elem_index = list_elem_index;
                list_elem_index = list_elem->next;
            }
        }
    }

#ifdef THREAD_SAFETY
    pthread_rwlock_unlock(&map_struct->rwlock);
#endif
    return del_count;
}

void array_hashmap_del(array_hashmap_t *map_struct_c)
{
    hashmap_t *map_struct = NULL;
    map_struct = (hashmap_t *)(*map_struct_c);
    if (!map_struct) {
        return;
    }

    *map_struct_c = NULL;

#ifdef THREAD_SAFETY
    pthread_rwlock_wrlock(&map_struct->rwlock);
#endif

    free(map_struct->map);

#ifdef THREAD_SAFETY
    pthread_rwlock_unlock(&map_struct->rwlock);
    pthread_rwlock_destroy(&map_struct->rwlock);
#endif
    free(map_struct);
}
