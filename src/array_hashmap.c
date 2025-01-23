#include "array_hashmap.h"
#ifdef THREAD_SAFETY
#include <pthread.h>
#endif
#include <stdlib.h>
#include <string.h>

typedef struct hashmap {
    char *map;
    int32_t map_size;
    double max_load;
    int32_t now_in_map;
    int32_t elem_size;
    int32_t data_size;
    add_hash_t add_hash;
    add_cmp_t add_cmp;
    find_hash_t find_hash;
    find_cmp_t find_cmp;
#ifdef THREAD_SAFETY
    pthread_rwlock_t rwlock;
#endif
} hashmap_t;

typedef struct __attribute__((packed)) elem {
    int32_t next;
    char data;
} elem_t;

enum next { elem_empty = -2, elem_last = -1 };

#define get_add_hash(data) (map_struct->add_hash(data) % map_struct->map_size)
#define get_elem(index) ((elem_t *)&map_struct->map[index * map_struct->elem_size])

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
    map_struct->max_load = max_load;
    map_struct->data_size = type_size;
    map_struct->elem_size = type_size + sizeof(elem_t) - sizeof(char);
    map_struct->add_hash = NULL;
    map_struct->add_cmp = NULL;
    map_struct->find_hash = NULL;
    map_struct->find_cmp = NULL;
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
        elem_t *elem = get_elem(i);
        elem->next = elem_empty;
    }

    return (array_hashmap_t)map_struct;
}

void array_hashmap_set_func(array_hashmap_t map_struct_c, add_hash_t add_hash, add_cmp_t add_cmp,
                            find_hash_t find_hash, find_cmp_t find_cmp)
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

    add_elem_index = get_add_hash(add_elem_data);
    check_elem = get_elem(add_elem_index);
    check_elem_data = &check_elem->data;

    if (check_elem->next == elem_empty) {
        if (map_struct->now_in_map < map_struct->map_size * map_struct->max_load) {
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
        check_elem_index = get_add_hash(check_elem_data);

        if (check_elem_index == add_elem_index) {
            list_prev_elem_index = 0;
            list_elem_index = check_elem_index;
            list_elem = NULL;
            list_elem_data = NULL;

            do {
                list_elem = get_elem(list_elem_index);
                list_elem_data = &list_elem->data;

                if (map_struct->add_cmp(add_elem_data, list_elem_data)) {
                    if (on_already_in) {
                        if (on_already_in == array_hashmap_add_new) {
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
            list_elem = get_elem(list_elem_index);

            if (map_struct->now_in_map < map_struct->map_size * map_struct->max_load) {
                new_elem_index = list_elem_index;
                new_elem = NULL;

                do {
                    new_elem_index = (new_elem_index + 1) % map_struct->map_size;
                    new_elem = get_elem(new_elem_index);
                } while (new_elem->next != elem_empty);

                new_elem->next = elem_last;
                memcpy(&new_elem->data, add_elem_data, map_struct->data_size);
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
            if (map_struct->now_in_map < map_struct->map_size * map_struct->max_load) {
                list_elem = get_elem(check_elem_index);
                while (list_elem->next != add_elem_index) {
                    list_elem = get_elem(list_elem->next);
                }

                new_elem_index = add_elem_index;
                new_elem = NULL;

                do {
                    new_elem_index = (new_elem_index + 1) % map_struct->map_size;
                    new_elem = get_elem(new_elem_index);
                } while (new_elem->next != elem_empty);

                memcpy(new_elem, check_elem, map_struct->elem_size);
                list_elem->next = new_elem_index;

                check_elem->next = elem_last;
                memcpy(&check_elem->data, add_elem_data, map_struct->data_size);

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

int32_t array_hashmap_find_elem(array_hashmap_t map_struct_c, const void *find_elem, void *res_elem)
{
    int32_t find_elem_hash = 0;
    elem_t *on_hash_elem = NULL;

    int32_t collision_chain_hash = 0;
    elem_t *collision_chain_elem = NULL;

    hashmap_t *map_struct = NULL;
    map_struct = (hashmap_t *)map_struct_c;
    if (!map_struct || !find_elem) {
        return array_hashmap_empty_args;
    }

    if (!map_struct->find_hash || !map_struct->find_cmp) {
        return array_hashmap_empty_funcs;
    }

#ifdef THREAD_SAFETY
    pthread_rwlock_rdlock(&map_struct->rwlock);
#endif

    find_elem_hash = map_struct->find_hash(find_elem) % map_struct->map_size;
    on_hash_elem = get_elem(find_elem_hash);

    if (on_hash_elem->next == elem_empty) {
#ifdef THREAD_SAFETY
        pthread_rwlock_unlock(&map_struct->rwlock);
#endif
        return array_hashmap_elem_not_finded;
    }

    collision_chain_hash = find_elem_hash;
    while (collision_chain_hash != elem_last) {
        collision_chain_elem = get_elem(collision_chain_hash);
        if (map_struct->find_cmp(find_elem, &collision_chain_elem->data)) {
            if (res_elem) {
                memcpy(res_elem, &collision_chain_elem->data, map_struct->data_size);
            }
#ifdef THREAD_SAFETY
            pthread_rwlock_unlock(&map_struct->rwlock);
#endif
            return array_hashmap_elem_finded;
        }

        collision_chain_hash = collision_chain_elem->next;
    }

#ifdef THREAD_SAFETY
    pthread_rwlock_unlock(&map_struct->rwlock);
#endif
    return array_hashmap_elem_not_finded;
}

/*int32_t array_hashmap_del_elem(array_hashmap_t map_struct_c, const void *del_elem, void *res_elem)
{
    hashmap_t *map_struct = NULL;
    map_struct = (hashmap_t *)map_struct_c;
    if (!map_struct || !del_elem) {
        return array_hashmap_empty_args;
    }

    if (!map_struct->find_hash || !map_struct->find_cmp) {
        return array_hashmap_empty_funcs;
    }

    pthread_rwlock_wrlock(&map_struct->rwlock);

    int32_t del_elem_hash = 0;
    del_elem_hash = map_struct->find_hash(del_elem) % map_struct->map_size;
    elem_t *on_hash_elem = NULL;
    int32_t offset = del_elem_hash * map_struct->elem_size;
    on_hash_elem = (elem_t *)&map_struct->map[offset];

    if (on_hash_elem->next == elem_empty) {
        pthread_rwlock_unlock(&map_struct->rwlock);
        return array_hashmap_elem_not_deled;
    }

    int32_t prev_elem_hash = elem_last;
    int32_t collision_chain_hash = del_elem_hash;
    while (collision_chain_hash != elem_last) {
        elem_t *collision_chain_elem = NULL;
        int32_t offset = collision_chain_hash * map_struct->elem_size;
        collision_chain_elem = (elem_t *)&map_struct->map[offset];
        if (map_struct->find_cmp(del_elem, &collision_chain_elem->data)) {
            if (res_elem) {
                memcpy(res_elem, &collision_chain_elem->data, map_struct->data_size);
            }

            if (collision_chain_elem->next == elem_last) {
                if (prev_elem_hash != elem_last) {
                    elem_t *prev_elem = NULL;
                    int32_t offset = prev_elem_hash * map_struct->elem_size;
                    prev_elem = (elem_t *)&map_struct->map[offset];

                    prev_elem->next = elem_last;
                }

                collision_chain_elem->next = elem_empty;
            } else {
                int32_t next_elem_hash = collision_chain_elem->next;
                elem_t *next_elem = NULL;
                int32_t offset = next_elem_hash * map_struct->elem_size;
                next_elem = (elem_t *)&map_struct->map[offset];

                memcpy(collision_chain_elem, next_elem, map_struct->elem_size);

                next_elem->next = elem_empty;
            }

            map_struct->now_in_map--;

            pthread_rwlock_unlock(&map_struct->rwlock);
            return array_hashmap_elem_deled;
        }

        prev_elem_hash = collision_chain_hash;
        collision_chain_hash = collision_chain_elem->next;
    }

    pthread_rwlock_unlock(&map_struct->rwlock);
    return array_hashmap_elem_not_deled;
}

int32_t array_hashmap_del_elem_by_func(array_hashmap_t map_struct_c,
                                       int32_t (*decide)(const void *))
{
    hashmap_t *map_struct = NULL;
    map_struct = (hashmap_t *)map_struct_c;
    if (!map_struct || !decide) {
        return array_hashmap_empty_args;
    }

    pthread_rwlock_wrlock(&map_struct->rwlock);

    int32_t del_count = 0;
    for (int32_t i = 0; i < map_struct->map_size; i++) {
        elem_t *elem = NULL;
        int32_t offset = i * map_struct->elem_size;
        elem = (elem_t *)&map_struct->map[offset];
        if (elem->next == elem_empty) {
            continue;
        }

        int32_t elem_hash = map_struct->add_hash(&elem->data) % map_struct->map_size;
        if (elem_hash != i) {
            continue;
        }

        int32_t prev_elem_hash = elem_last;
        int32_t collision_chain_hash = elem_hash;
        while (collision_chain_hash != elem_last) {
            elem_t *collision_chain_elem = NULL;
            int32_t offset = collision_chain_hash * map_struct->elem_size;
            collision_chain_elem = (elem_t *)&map_struct->map[offset];
            if (decide(&collision_chain_elem->data)) {
                if (collision_chain_elem->next == elem_last) {
                    if (prev_elem_hash != elem_last) {
                        elem_t *prev_elem = NULL;
                        int32_t offset = prev_elem_hash * map_struct->elem_size;
                        prev_elem = (elem_t *)&map_struct->map[offset];
                        prev_elem->next = elem_last;
                    }

                    collision_chain_elem->next = elem_empty;
                    collision_chain_hash = elem_last;
                } else {
                    int32_t next_elem_hash = collision_chain_elem->next;
                    int32_t offset = next_elem_hash * map_struct->elem_size;
                    elem_t *next_elem = (elem_t *)&map_struct->map[offset];

                    memcpy(collision_chain_elem, next_elem, map_struct->elem_size);

                    next_elem->next = elem_empty;
                }

                del_count++;
                map_struct->now_in_map--;
            } else {
                prev_elem_hash = collision_chain_hash;
                collision_chain_hash = collision_chain_elem->next;
            }
        }
    }

    pthread_rwlock_unlock(&map_struct->rwlock);
    return del_count;
}*/

void array_hashmap_del(array_hashmap_t map_struct_c)
{
    hashmap_t *map_struct = NULL;
    map_struct = (hashmap_t *)map_struct_c;
    if (!map_struct) {
        return;
    }

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
