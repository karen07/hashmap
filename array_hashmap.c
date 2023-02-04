#include "array_hashmap.h"

const array_hashmap_t* init_array_hashmap(int32_t map_size, double max_load, int32_t type_size, uint32_t (*hash_func)(const void*), int32_t (*cmp_func)(const void*, const void*))
{
    if (map_size == 0 || max_load == 0 || type_size == 0 || hash_func == NULL || cmp_func == NULL) {
        return NULL;
    }

    array_hashmap_t* map_struct = malloc(sizeof(array_hashmap_t));
    if (map_struct == NULL) {
        return NULL;
    }

    map_struct->map_size = map_size;
    map_struct->max_load = max_load;
    map_struct->type_size = type_size + sizeof(array_hashmap_elem_t);
    for (operations_t i = hashmap_add; i <= hashmap_del; i++) {
        map_struct->hash_func[i] = hash_func;
        map_struct->cmp_func[i] = cmp_func;
    }
    map_struct->now_in_map = 0;
    map_struct->iter_flag = 0;

    map_struct->map = malloc(map_struct->map_size * map_struct->type_size);
    if (map_struct->map == NULL) {
        free(map_struct);
        return NULL;
    }

    if (pthread_rwlock_init(&map_struct->rwlock, NULL)) {
        free(map_struct->map);
        free(map_struct);
        return NULL;
    }

    for (int32_t i = 0; i < map_struct->map_size; i++) {
        array_hashmap_elem_t* elem = (array_hashmap_elem_t*)&map_struct->map[i * map_struct->type_size];
        elem->next = hashmap_empty;
    }

    return map_struct;
}

void array_hashmap_set_find_funcs(const array_hashmap_t* map_struct_c, uint32_t (*hash_func)(const void*), int32_t (*cmp_func)(const void*, const void*))
{
    array_hashmap_t* map_struct = (array_hashmap_t*)map_struct_c;

    if (map_struct == NULL || hash_func == NULL || cmp_func == NULL) {
        return;
    }

    map_struct->hash_func[hashmap_find] = hash_func;
    map_struct->cmp_func[hashmap_find] = cmp_func;
}

void array_hashmap_set_del_funcs(const array_hashmap_t* map_struct_c, uint32_t (*hash_func)(const void*), int32_t (*cmp_func)(const void*, const void*))
{
    array_hashmap_t* map_struct = (array_hashmap_t*)map_struct_c;

    if (map_struct == NULL || hash_func == NULL || cmp_func == NULL) {
        return;
    }

    map_struct->hash_func[hashmap_del] = hash_func;
    map_struct->cmp_func[hashmap_del] = cmp_func;
}

int32_t array_hashmap_get_size(const array_hashmap_t* map_struct)
{
    if (map_struct == NULL) {
        return hashmap_null_point;
    }

    return map_struct->now_in_map;
}

int32_t array_hashmap_operations(array_hashmap_t* map_struct, const void* in_elem, void* out_elem, operations_t operation)
{
    next_val_t in_elem_hash = map_struct->hash_func[operation](in_elem) % map_struct->map_size;
    array_hashmap_elem_t* pos_elem = (array_hashmap_elem_t*)&map_struct->map[in_elem_hash * map_struct->type_size];

    if (pos_elem->next == hashmap_empty) {
        if (operation == hashmap_find || operation == hashmap_del) {
            return hashmap_no_elem;
        }

        if (map_struct->iter_flag) {
            return hashmap_iter_process;
        }

        if (map_struct->now_in_map < map_struct->map_size * map_struct->max_load) {
            pos_elem->next = hashmap_alone;
            memcpy(&pos_elem->data, in_elem, map_struct->type_size - sizeof(array_hashmap_elem_t));

            map_struct->now_in_map++;

            return hashmap_ok;
        } else {
            return hashmap_no_free_mem;
        }
    } else {
        next_val_t pos_elem_hash = map_struct->hash_func[hashmap_add](&pos_elem->data) % map_struct->map_size;

        if (pos_elem_hash == in_elem_hash) {
            next_val_t prev_elem_hash = hashmap_alone;
            next_val_t collision_elem_hash = in_elem_hash;
            while (collision_elem_hash != hashmap_alone) {
                array_hashmap_elem_t* collision_elem = (array_hashmap_elem_t*)&map_struct->map[collision_elem_hash * map_struct->type_size];
                if (map_struct->cmp_func[operation](in_elem, &collision_elem->data)) {
                    if (out_elem) {
                        memcpy(out_elem, &collision_elem->data, map_struct->type_size - sizeof(array_hashmap_elem_t));
                    }
                    if (operation == hashmap_add) {
                        return hashmap_elem_already_in;
                    }
                    if (operation == hashmap_replace) {
                        memcpy(&collision_elem->data, in_elem, map_struct->type_size - sizeof(array_hashmap_elem_t));
                        return hashmap_elem_already_in;
                    }
                    if (operation == hashmap_find) {
                        return hashmap_ok;
                    }
                    if (operation == hashmap_del) {
                        if (collision_elem->next == hashmap_alone) {
                            if (prev_elem_hash != hashmap_alone) {
                                array_hashmap_elem_t* prev_elem = (array_hashmap_elem_t*)&map_struct->map[prev_elem_hash * map_struct->type_size];
                                prev_elem->next = hashmap_alone;
                            }

                            collision_elem->next = hashmap_empty;
                        } else {
                            int32_t next_elem_hash = collision_elem->next;
                            array_hashmap_elem_t* next_elem = (array_hashmap_elem_t*)&map_struct->map[next_elem_hash * map_struct->type_size];

                            memcpy(collision_elem, next_elem, map_struct->type_size);

                            next_elem->next = hashmap_empty;
                        }

                        map_struct->now_in_map--;

                        return hashmap_ok;
                    }
                }

                prev_elem_hash = collision_elem_hash;
                collision_elem_hash = collision_elem->next;
            }

            if (operation == hashmap_find || operation == hashmap_del) {
                return hashmap_no_elem;
            }

            if (map_struct->iter_flag) {
                return hashmap_iter_process;
            }

            if (map_struct->now_in_map < map_struct->map_size * map_struct->max_load) {
                next_val_t new_elem_hash = prev_elem_hash;
                array_hashmap_elem_t* new_elem = (array_hashmap_elem_t*)&map_struct->map[new_elem_hash * map_struct->type_size];
                while (new_elem->next != hashmap_empty) {
                    new_elem_hash = (new_elem_hash + 1) % map_struct->map_size;
                    new_elem = (array_hashmap_elem_t*)&map_struct->map[new_elem_hash * map_struct->type_size];
                }

                new_elem->next = hashmap_alone;
                memcpy(&new_elem->data, in_elem, map_struct->type_size - sizeof(array_hashmap_elem_t));

                array_hashmap_elem_t* prev_elem = (array_hashmap_elem_t*)&map_struct->map[prev_elem_hash * map_struct->type_size];
                prev_elem->next = new_elem_hash;

                map_struct->now_in_map++;

                return hashmap_ok;
            } else {
                return hashmap_no_free_mem;
            }
        } else {
            if (operation == hashmap_find || operation == hashmap_del) {
                return hashmap_no_elem;
            }

            if (map_struct->iter_flag) {
                return hashmap_iter_process;
            }

            if (map_struct->now_in_map < map_struct->map_size * map_struct->max_load) {
                next_val_t collision_elem_hash = pos_elem_hash;
                array_hashmap_elem_t* collision_elem = (array_hashmap_elem_t*)&map_struct->map[collision_elem_hash * map_struct->type_size];
                while (collision_elem->next != in_elem_hash) {
                    collision_elem_hash = collision_elem->next;
                    collision_elem = (array_hashmap_elem_t*)&map_struct->map[collision_elem_hash * map_struct->type_size];
                }

                next_val_t new_elem_hash = in_elem_hash;
                array_hashmap_elem_t* new_elem = (array_hashmap_elem_t*)&map_struct->map[new_elem_hash * map_struct->type_size];
                while (new_elem->next != hashmap_empty) {
                    new_elem_hash = (new_elem_hash + 1) % map_struct->map_size;
                    new_elem = (array_hashmap_elem_t*)&map_struct->map[new_elem_hash * map_struct->type_size];
                }

                memcpy(new_elem, pos_elem, map_struct->type_size);

                collision_elem->next = new_elem_hash;

                pos_elem->next = hashmap_alone;
                memcpy(&pos_elem->data, in_elem, map_struct->type_size - sizeof(array_hashmap_elem_t));

                map_struct->now_in_map++;

                return hashmap_ok;
            } else {
                return hashmap_no_free_mem;
            }
        }
    }
}

int32_t array_hashmap_add_elem(const array_hashmap_t* map_struct_c, const void* add_elem, void* res_elem)
{
    array_hashmap_t* map_struct = (array_hashmap_t*)map_struct_c;

    if (map_struct == NULL || add_elem == NULL) {
        return hashmap_null_point;
    }

    pthread_rwlock_wrlock(&map_struct->rwlock);
    int32_t res = array_hashmap_operations(map_struct, add_elem, res_elem, hashmap_add);
    pthread_rwlock_unlock(&map_struct->rwlock);
    return res;
}

int32_t array_hashmap_repl_elem(const array_hashmap_t* map_struct_c, const void* repl_elem, void* res_elem)
{
    array_hashmap_t* map_struct = (array_hashmap_t*)map_struct_c;

    if (map_struct == NULL || repl_elem == NULL) {
        return hashmap_null_point;
    }

    pthread_rwlock_wrlock(&map_struct->rwlock);
    int32_t res = array_hashmap_operations(map_struct, repl_elem, res_elem, hashmap_replace);
    pthread_rwlock_unlock(&map_struct->rwlock);
    return res;
}

int32_t array_hashmap_find_elem(const array_hashmap_t* map_struct_c, const void* find_elem, void* res_elem)
{
    array_hashmap_t* map_struct = (array_hashmap_t*)map_struct_c;

    if (map_struct == NULL || find_elem == NULL) {
        return hashmap_null_point;
    }

    pthread_rwlock_rdlock(&map_struct->rwlock);
    int32_t res = array_hashmap_operations(map_struct, find_elem, res_elem, hashmap_find);
    pthread_rwlock_unlock(&map_struct->rwlock);
    return res;
}

int32_t array_hashmap_del_elem(const array_hashmap_t* map_struct_c, const void* del_elem, void* res_elem)
{
    array_hashmap_t* map_struct = (array_hashmap_t*)map_struct_c;

    if (map_struct == NULL || del_elem == NULL) {
        return hashmap_null_point;
    }

    pthread_rwlock_wrlock(&map_struct->rwlock);
    int32_t res = array_hashmap_operations(map_struct, del_elem, res_elem, hashmap_del);
    pthread_rwlock_unlock(&map_struct->rwlock);
    return res;
}

int32_t array_hashmap_add_elem_wait(const array_hashmap_t* map_struct_c, const void* add_elem, void* res_elem)
{
    array_hashmap_t* map_struct = (array_hashmap_t*)map_struct_c;

    if (map_struct == NULL || add_elem == NULL) {
        return hashmap_null_point;
    }

    pthread_rwlock_wrlock(&map_struct->iterlock);
    pthread_rwlock_unlock(&map_struct->iterlock);

    pthread_rwlock_wrlock(&map_struct->rwlock);
    int32_t res = array_hashmap_operations(map_struct, add_elem, res_elem, hashmap_add);
    pthread_rwlock_unlock(&map_struct->rwlock);
    return res;
}

int32_t array_hashmap_repl_elem_wait(const array_hashmap_t* map_struct_c, const void* repl_elem, void* res_elem)
{
    array_hashmap_t* map_struct = (array_hashmap_t*)map_struct_c;

    if (map_struct == NULL || repl_elem == NULL) {
        return hashmap_null_point;
    }

    pthread_rwlock_wrlock(&map_struct->iterlock);
    pthread_rwlock_unlock(&map_struct->iterlock);

    pthread_rwlock_wrlock(&map_struct->rwlock);
    int32_t res = array_hashmap_operations(map_struct, repl_elem, res_elem, hashmap_replace);
    pthread_rwlock_unlock(&map_struct->rwlock);
    return res;
}

int32_t array_hashmap_find_elem_wait(const array_hashmap_t* map_struct_c, const void* find_elem, void* res_elem)
{
    array_hashmap_t* map_struct = (array_hashmap_t*)map_struct_c;

    if (map_struct == NULL || find_elem == NULL) {
        return hashmap_null_point;
    }

    pthread_rwlock_wrlock(&map_struct->iterlock);
    pthread_rwlock_unlock(&map_struct->iterlock);

    pthread_rwlock_rdlock(&map_struct->rwlock);
    int32_t res = array_hashmap_operations(map_struct, find_elem, res_elem, hashmap_find);
    pthread_rwlock_unlock(&map_struct->rwlock);
    return res;
}

int32_t array_hashmap_del_elem_wait(const array_hashmap_t* map_struct_c, const void* del_elem, void* res_elem)
{
    array_hashmap_t* map_struct = (array_hashmap_t*)map_struct_c;

    if (map_struct == NULL || del_elem == NULL) {
        return hashmap_null_point;
    }

    pthread_rwlock_wrlock(&map_struct->iterlock);
    pthread_rwlock_unlock(&map_struct->iterlock);

    pthread_rwlock_wrlock(&map_struct->rwlock);
    int32_t res = array_hashmap_operations(map_struct, del_elem, res_elem, hashmap_del);
    pthread_rwlock_unlock(&map_struct->rwlock);
    return res;
}

int32_t array_hashmap_del_elem_by_func(const array_hashmap_t* map_struct_c, int32_t (*decide)(const void*))
{
    array_hashmap_t* map_struct = (array_hashmap_t*)map_struct_c;

    if (map_struct == NULL || decide == NULL) {
        return hashmap_null_point;
    }

    if (map_struct->iter_flag) {
        return hashmap_iter_process;
    }

    pthread_rwlock_wrlock(&map_struct->rwlock);

    int32_t del_count = 0;
    for (int32_t i = 0; i < map_struct->map_size; i++) {
        array_hashmap_elem_t* elem = (array_hashmap_elem_t*)&map_struct->map[i * map_struct->type_size];
        if (elem->next == hashmap_empty) {
            continue;
        }

        next_val_t elem_hash = map_struct->hash_func[hashmap_add](&elem->data) % map_struct->map_size;
        if (elem_hash != i) {
            continue;
        }

        next_val_t prev_elem_hash = hashmap_alone;
        next_val_t collision_elem_hash = elem_hash;
        while (collision_elem_hash != hashmap_alone) {
            array_hashmap_elem_t* collision_elem = (array_hashmap_elem_t*)&map_struct->map[collision_elem_hash * map_struct->type_size];
            if (decide(&collision_elem->data)) {
                if (collision_elem->next == hashmap_alone) {
                    if (prev_elem_hash != hashmap_alone) {
                        array_hashmap_elem_t* prev_elem = (array_hashmap_elem_t*)&map_struct->map[prev_elem_hash * map_struct->type_size];
                        prev_elem->next = hashmap_alone;
                    }

                    collision_elem->next = hashmap_empty;
                    collision_elem_hash = hashmap_alone;
                } else {
                    next_val_t next_elem_hash = collision_elem->next;
                    array_hashmap_elem_t* next_elem = (array_hashmap_elem_t*)&map_struct->map[next_elem_hash * map_struct->type_size];

                    memcpy(collision_elem, next_elem, map_struct->type_size);

                    next_elem->next = hashmap_empty;
                }

                del_count++;
                map_struct->now_in_map--;
            } else {
                prev_elem_hash = collision_elem_hash;
                collision_elem_hash = collision_elem->next;
            }
        }
    }

    pthread_rwlock_unlock(&map_struct->rwlock);
    return del_count;
}

const array_hashmap_iter_t* array_hashmap_get_iter(const array_hashmap_t* map_struct_c)
{
    array_hashmap_t* map_struct = (array_hashmap_t*)map_struct_c;
    if (map_struct == NULL) {
        return NULL;
    }

    array_hashmap_iter_t* iter = malloc(sizeof(array_hashmap_iter_t));
    if (iter == NULL) {
        return NULL;
    }

    iter->next = hashmap_alone;
    iter->map_struct = map_struct;

    pthread_rwlock_wrlock(&map_struct->rwlock);

    map_struct->iter_flag++;

    pthread_rwlock_unlock(&map_struct->rwlock);

    pthread_rwlock_wrlock(&map_struct->iterlock);

    return iter;
}

int32_t array_hashmap_next(const array_hashmap_iter_t* iter_c, void* next_elem)
{
    array_hashmap_iter_t* iter = (array_hashmap_iter_t*)iter_c;

    if (iter == NULL || next_elem == NULL) {
        return hashmap_null_point;
    }

    pthread_rwlock_rdlock(&iter->map_struct->rwlock);

    for (int32_t i = iter->next + 1; i < iter->map_struct->map_size; i++) {
        array_hashmap_elem_t* elem = (array_hashmap_elem_t*)&iter->map_struct->map[i * iter->map_struct->type_size];
        if (elem->next != hashmap_empty) {
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

    if (iter == NULL) {
        return;
    }

    pthread_rwlock_wrlock(&iter->map_struct->rwlock);

    iter->map_struct->iter_flag--;

    pthread_rwlock_unlock(&iter->map_struct->rwlock);

    pthread_rwlock_unlock(&iter->map_struct->iterlock);

    free(iter);
}

void del_array_hashmap(const array_hashmap_t* map_struct_c)
{
    array_hashmap_t* map_struct = (array_hashmap_t*)map_struct_c;

    if (map_struct == NULL) {
        return;
    }

    if (map_struct->iter_flag) {
        return;
    }

    pthread_rwlock_wrlock(&map_struct->rwlock);

    free(map_struct->map);
    free(map_struct);

    pthread_rwlock_unlock(&map_struct->rwlock);
}
