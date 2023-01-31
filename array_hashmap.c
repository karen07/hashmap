#include "array_hashmap.h"

const array_hashmap_t* init_array_hashmap(int32_t map_size, double max_load, int32_t type_size)
{
    array_hashmap_t* map_struct = malloc(sizeof(array_hashmap_t));
    if (!map_struct) {
        return NULL;
    }

    map_struct->map_size = map_size;
    map_struct->max_load = max_load;
    map_struct->type_size = type_size + sizeof(array_hashmap_elem_t);
    map_struct->add_hash = NULL;
    map_struct->add_cmp = NULL;
    map_struct->find_hash = NULL;
    map_struct->find_cmp = NULL;
    map_struct->now_in_map = 0;
    map_struct->iter_flag = 0;

    char* map = malloc(map_struct->map_size * map_struct->type_size);
    if (!map) {
        free(map_struct);
        return NULL;
    }
    map_struct->map = map;

    if (pthread_rwlock_init(&map_struct->rwlock, NULL)) {
        free(map_struct);
        return NULL;
    }

    for (int32_t i = 0; i < map_struct->map_size; i++) {
        array_hashmap_elem_t* elem = (array_hashmap_elem_t*)&map_struct->map[i * map_struct->type_size];
        elem->next = -2;
    }

    return map_struct;
}

void array_hashmap_set_func(const array_hashmap_t* map_struct_c, uint32_t (*add_hash)(const void*), int32_t (*add_cmp)(const void*, const void*), uint32_t (*find_hash)(const void*), int32_t (*find_cmp)(const void*, const void*))
{
    array_hashmap_t* map_struct = (array_hashmap_t*)map_struct_c;

    if (!map_struct) {
        return;
    }

    pthread_rwlock_wrlock(&map_struct->rwlock);

    map_struct->add_hash = add_hash;
    map_struct->add_cmp = add_cmp;

    map_struct->find_hash = find_hash;
    map_struct->find_cmp = find_cmp;

    pthread_rwlock_unlock(&map_struct->rwlock);
}

int32_t array_hashmap_get_size(const array_hashmap_t* map_struct)
{
    if (!map_struct) {
        return -1;
    }

    return map_struct->now_in_map;
}

int32_t array_hashmap_add_elem(const array_hashmap_t* map_struct_c, const void* add_elem, void* res_elem, int32_t (*on_collision)(const void*, const void*))
{
    array_hashmap_t* map_struct = (array_hashmap_t*)map_struct_c;

    if (!map_struct || !add_elem) {
        return -2;
    }

    if (!map_struct->add_hash || !map_struct->add_cmp) {
        return -2;
    }

    pthread_rwlock_wrlock(&map_struct->rwlock);

    int32_t add_elem_hash = map_struct->add_hash(add_elem) % map_struct->map_size;
    array_hashmap_elem_t* on_hash_elem = (array_hashmap_elem_t*)&map_struct->map[add_elem_hash * map_struct->type_size];

    if (on_hash_elem->next == -2) {
        if (map_struct->now_in_map < map_struct->map_size * map_struct->max_load) {
            on_hash_elem->next = -1;
            memcpy(&on_hash_elem->data, add_elem, map_struct->type_size - sizeof(array_hashmap_elem_t));

            map_struct->now_in_map++;

            pthread_rwlock_unlock(&map_struct->rwlock);
            return 1;
        } else {
            pthread_rwlock_unlock(&map_struct->rwlock);
            return -1;
        }
    } else {
        int32_t on_hash_elem_hash = map_struct->add_hash(&on_hash_elem->data) % map_struct->map_size;

        if (on_hash_elem_hash == add_elem_hash) {
            int32_t collision_chain_hash = add_elem_hash;
            array_hashmap_elem_t* collision_chain_elem = (array_hashmap_elem_t*)&map_struct->map[collision_chain_hash * map_struct->type_size];

            while (collision_chain_elem->next != -1) {
                if (map_struct->add_cmp(add_elem, &collision_chain_elem->data)) {
                    if (on_collision) {
                        if (on_collision(add_elem, &collision_chain_elem->data)) {
                            memcpy(&collision_chain_elem->data, add_elem, map_struct->type_size - sizeof(array_hashmap_elem_t));
                        }
                    }
                    if (res_elem) {
                        memcpy(res_elem, &collision_chain_elem->data, map_struct->type_size - sizeof(array_hashmap_elem_t));
                    }
                    pthread_rwlock_unlock(&map_struct->rwlock);
                    return 0;
                }

                collision_chain_hash = collision_chain_elem->next;
                collision_chain_elem = (array_hashmap_elem_t*)&map_struct->map[collision_chain_hash * map_struct->type_size];
            }

            if (map_struct->add_cmp(add_elem, &collision_chain_elem->data)) {
                if (on_collision) {
                    if (on_collision(add_elem, &collision_chain_elem->data)) {
                        memcpy(&collision_chain_elem->data, add_elem, map_struct->type_size - sizeof(array_hashmap_elem_t));
                    }
                }
                if (res_elem) {
                    memcpy(res_elem, &collision_chain_elem->data, map_struct->type_size - sizeof(array_hashmap_elem_t));
                }
                pthread_rwlock_unlock(&map_struct->rwlock);
                return 0;
            }

            if (map_struct->now_in_map < map_struct->map_size * map_struct->max_load) {
                int32_t new_elem_hash = collision_chain_hash;
                array_hashmap_elem_t* new_elem = (array_hashmap_elem_t*)&map_struct->map[new_elem_hash * map_struct->type_size];
                while (new_elem->next != -2) {
                    new_elem_hash = (new_elem_hash + 1) % map_struct->map_size;
                    new_elem = (array_hashmap_elem_t*)&map_struct->map[new_elem_hash * map_struct->type_size];
                }

                new_elem->next = -1;
                memcpy(&new_elem->data, add_elem, map_struct->type_size - sizeof(array_hashmap_elem_t));

                collision_chain_elem->next = new_elem_hash;

                map_struct->now_in_map++;

                pthread_rwlock_unlock(&map_struct->rwlock);
                return 1;
            } else {
                pthread_rwlock_unlock(&map_struct->rwlock);
                return -1;
            }
        } else {
            if (map_struct->now_in_map < map_struct->map_size * map_struct->max_load) {
                int32_t collision_chain_hash = on_hash_elem_hash;
                array_hashmap_elem_t* collision_chain_elem = (array_hashmap_elem_t*)&map_struct->map[collision_chain_hash * map_struct->type_size];

                while (collision_chain_elem->next != add_elem_hash) {
                    collision_chain_hash = collision_chain_elem->next;
                    collision_chain_elem = (array_hashmap_elem_t*)&map_struct->map[collision_chain_hash * map_struct->type_size];
                }

                int32_t new_elem_hash = add_elem_hash;
                array_hashmap_elem_t* new_elem = (array_hashmap_elem_t*)&map_struct->map[new_elem_hash * map_struct->type_size];
                while (new_elem->next != -2) {
                    new_elem_hash = (new_elem_hash + 1) % map_struct->map_size;
                    new_elem = (array_hashmap_elem_t*)&map_struct->map[new_elem_hash * map_struct->type_size];
                }

                memcpy(new_elem, on_hash_elem, map_struct->type_size);
                collision_chain_elem->next = new_elem_hash;

                on_hash_elem->next = -1;
                memcpy(&on_hash_elem->data, add_elem, map_struct->type_size - sizeof(array_hashmap_elem_t));

                map_struct->now_in_map++;

                pthread_rwlock_unlock(&map_struct->rwlock);
                return 1;
            } else {
                pthread_rwlock_unlock(&map_struct->rwlock);
                return -1;
            }
        }
    }
}

int32_t array_hashmap_find_elem(const array_hashmap_t* map_struct_c, const void* find_elem, void* res_elem)
{
    array_hashmap_t* map_struct = (array_hashmap_t*)map_struct_c;

    if (!map_struct || !find_elem) {
        return -1;
    }

    if (!map_struct->find_hash || !map_struct->find_cmp) {
        return -1;
    }

    pthread_rwlock_rdlock(&map_struct->rwlock);

    int32_t find_elem_hash = map_struct->find_hash(find_elem) % map_struct->map_size;
    array_hashmap_elem_t* on_hash_elem = (array_hashmap_elem_t*)&map_struct->map[find_elem_hash * map_struct->type_size];

    if (on_hash_elem->next == -2) {
        pthread_rwlock_unlock(&map_struct->rwlock);
        return 0;
    }

    int32_t collision_chain_hash = find_elem_hash;
    while (collision_chain_hash != -1) {
        array_hashmap_elem_t* collision_chain_elem = (array_hashmap_elem_t*)&map_struct->map[collision_chain_hash * map_struct->type_size];
        if (map_struct->find_cmp(find_elem, &collision_chain_elem->data)) {
            if (res_elem) {
                memcpy(res_elem, &collision_chain_elem->data, map_struct->type_size - sizeof(array_hashmap_elem_t));
            }
            pthread_rwlock_unlock(&map_struct->rwlock);
            return 1;
        }

        collision_chain_hash = collision_chain_elem->next;
    }

    pthread_rwlock_unlock(&map_struct->rwlock);
    return 0;
}

int32_t array_hashmap_del_elem(const array_hashmap_t* map_struct_c, const void* del_elem, void* res_elem)
{
    array_hashmap_t* map_struct = (array_hashmap_t*)map_struct_c;

    if (!map_struct || !del_elem) {
        return -1;
    }

    if (!map_struct->find_hash || !map_struct->find_cmp || map_struct->iter_flag) {
        return -1;
    }

    pthread_rwlock_wrlock(&map_struct->rwlock);

    int32_t del_elem_hash = map_struct->find_hash(del_elem) % map_struct->map_size;
    array_hashmap_elem_t* on_hash_elem = (array_hashmap_elem_t*)&map_struct->map[del_elem_hash * map_struct->type_size];

    if (on_hash_elem->next == -2) {
        pthread_rwlock_unlock(&map_struct->rwlock);
        return 0;
    }

    int32_t prev_elem_hash = -1;
    int32_t collision_chain_hash = del_elem_hash;
    while (collision_chain_hash != -1) {
        array_hashmap_elem_t* collision_chain_elem = (array_hashmap_elem_t*)&map_struct->map[collision_chain_hash * map_struct->type_size];
        if (map_struct->find_cmp(del_elem, &collision_chain_elem->data)) {
            if (res_elem) {
                memcpy(res_elem, &collision_chain_elem->data, map_struct->type_size - sizeof(array_hashmap_elem_t));
            }

            if (collision_chain_elem->next == -1) {
                if (prev_elem_hash != -1) {
                    array_hashmap_elem_t* prev_elem = (array_hashmap_elem_t*)&map_struct->map[prev_elem_hash * map_struct->type_size];

                    prev_elem->next = -1;
                }

                collision_chain_elem->next = -2;
            } else {
                int32_t next_elem_hash = collision_chain_elem->next;
                array_hashmap_elem_t* next_elem = (array_hashmap_elem_t*)&map_struct->map[next_elem_hash * map_struct->type_size];

                memcpy(collision_chain_elem, next_elem, map_struct->type_size);

                next_elem->next = -2;
            }

            map_struct->now_in_map--;

            pthread_rwlock_unlock(&map_struct->rwlock);
            return 1;
        }

        prev_elem_hash = collision_chain_hash;
        collision_chain_hash = collision_chain_elem->next;
    }

    pthread_rwlock_unlock(&map_struct->rwlock);
    return 0;
}

int32_t array_hashmap_del_elem_by_func(const array_hashmap_t* map_struct_c, int32_t (*decide)(const void*))
{
    array_hashmap_t* map_struct = (array_hashmap_t*)map_struct_c;

    if (!map_struct || !decide || map_struct->iter_flag) {
        return 0;
    }

    pthread_rwlock_wrlock(&map_struct->rwlock);

    int32_t del_count = 0;
    for (int32_t i = 0; i < map_struct->map_size; i++) {
        array_hashmap_elem_t* elem = (array_hashmap_elem_t*)&map_struct->map[i * map_struct->type_size];
        if (elem->next == -2) {
            continue;
        }

        int32_t elem_hash = map_struct->add_hash(&elem->data) % map_struct->map_size;
        if (elem_hash != i) {
            continue;
        }

        int32_t prev_elem_hash = -1;
        int32_t collision_chain_hash = elem_hash;
        while (collision_chain_hash != -1) {
            array_hashmap_elem_t* collision_chain_elem = (array_hashmap_elem_t*)&map_struct->map[collision_chain_hash * map_struct->type_size];
            if (decide(&collision_chain_elem->data)) {
                if (collision_chain_elem->next == -1) {
                    if (prev_elem_hash != -1) {
                        array_hashmap_elem_t* prev_elem = (array_hashmap_elem_t*)&map_struct->map[prev_elem_hash * map_struct->type_size];
                        prev_elem->next = -1;
                    }

                    collision_chain_elem->next = -2;
                    collision_chain_hash = -1;
                } else {
                    int32_t next_elem_hash = collision_chain_elem->next;
                    array_hashmap_elem_t* next_elem = (array_hashmap_elem_t*)&map_struct->map[next_elem_hash * map_struct->type_size];

                    memcpy(collision_chain_elem, next_elem, map_struct->type_size);

                    next_elem->next = -2;
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
}

const array_hashmap_iter_t* array_hashmap_get_iter(const array_hashmap_t* map_struct_c)
{
    array_hashmap_t* map_struct = (array_hashmap_t*)map_struct_c;

    if (!map_struct) {
        return NULL;
    }

    array_hashmap_iter_t* iter = malloc(sizeof(array_hashmap_iter_t));

    if (!iter) {
        return NULL;
    }

    iter->next = -1;
    iter->map_struct = map_struct;

    pthread_rwlock_wrlock(&map_struct->rwlock);

    map_struct->iter_flag = 1;

    pthread_rwlock_unlock(&map_struct->rwlock);

    return iter;
}

int32_t array_hashmap_next(const array_hashmap_iter_t* iter_c, void* next_elem)
{
    array_hashmap_iter_t* iter = (array_hashmap_iter_t*)iter_c;

    if (!iter || !next_elem) {
        return 0;
    }

    pthread_rwlock_rdlock(&iter->map_struct->rwlock);

    for (int32_t i = iter->next + 1; i < iter->map_struct->map_size; i++) {
        array_hashmap_elem_t* elem = (array_hashmap_elem_t*)&iter->map_struct->map[i * iter->map_struct->type_size];
        if (elem->next != -2) {
            iter->next = i;
            memcpy(next_elem, &elem->data, iter->map_struct->type_size - sizeof(array_hashmap_elem_t));
            pthread_rwlock_unlock(&iter->map_struct->rwlock);
            return 1;
        }
    }

    pthread_rwlock_unlock(&iter->map_struct->rwlock);
    return 0;
}

void array_hashmap_del_iter(const array_hashmap_iter_t* iter_c)
{
    array_hashmap_iter_t* iter = (array_hashmap_iter_t*)iter_c;

    if (!iter) {
        return;
    }

    pthread_rwlock_wrlock(&iter->map_struct->rwlock);

    iter->map_struct->iter_flag = 0;

    pthread_rwlock_unlock(&iter->map_struct->rwlock);

    free(iter);
}

void del_array_hashmap(const array_hashmap_t* map_struct_c)
{
    array_hashmap_t* map_struct = (array_hashmap_t*)map_struct_c;

    if (!map_struct) {
        return;
    }

    pthread_rwlock_wrlock(&map_struct->rwlock);

    free(map_struct->map);
    free(map_struct);

    pthread_rwlock_unlock(&map_struct->rwlock);
}
