#include "array_hashmap.h"
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <time.h>

typedef struct url_data {
    uint32_t url_pos;
    int32_t time;
} url_data_t;

char *urls;

uint32_t djb33_hash(const char *s)
{
    uint32_t h = 5381;
    while (*s) {
        h += (h << 5);
        h ^= *s++;
    }
    return h;
}

uint32_t add_url_hash(const void *add_elem_data)
{
    const url_data_t *elem = add_elem_data;
    return djb33_hash(&urls[elem->url_pos]);
}

int32_t add_url_cmp(const void *add_elem_data, const void *hashmap_elem_data)
{
    const url_data_t *elem1 = add_elem_data;
    const url_data_t *elem2 = hashmap_elem_data;

    return !strcmp(&urls[elem1->url_pos], &urls[elem2->url_pos]);
}

uint32_t find_url_hash(const void *find_elem_data)
{
    const char *elem = find_elem_data;
    return djb33_hash(elem);
}

int32_t find_url_cmp(const void *find_elem_data, const void *hashmap_elem_data)
{
    const char *elem1 = find_elem_data;
    const url_data_t *elem2 = hashmap_elem_data;

    return !strcmp(elem1, &urls[elem2->url_pos]);
}

int32_t url_on_already_in(const void *add_elem_data, const void *hashmap_elem_data)
{
    const url_data_t *elem1 = add_elem_data;
    const url_data_t *elem2 = hashmap_elem_data;

    if (elem1->time > elem2->time) {
        return 1;
    } else {
        return 0;
    }
}

void clean_cache(void)
{
    int i = 0;
    int j = 0;
    int size = 100 * 1024 * 1024;
    char *c = NULL;

    c = (char *)malloc(size);
    for (i = 0; i < 0xffff; i++) {
        for (j = 0; j < size; j++) {
            c[j] = i * j;
        }
    }
    free(c);
}

void make_random(int32_t *array, int32_t size)
{
    int32_t i = 0;

    srand(time(NULL));

    for (i = 0; i < size - 1; i++) {
        int32_t fir = rand() % size;
        int32_t sec = rand() % size;

        int32_t swap = array[fir];
        array[fir] = array[sec];
        array[sec] = swap;
    }
}

int main(void)
{
    FILE *urls_fd = NULL;
    char *urls_random = NULL;
    int32_t *url_offsets = NULL;

    int32_t urls_map_size = 0;
    int64_t urls_file_size = 0;

    int32_t i = 0;

    double step = 0;

    url_data_t add_elem;
    int32_t add_res;

    url_data_t find_elem;
    int32_t find_res;

    struct timeval now_timeval_start;
    struct timeval now_timeval_end;

    uint64_t now_us_start;
    uint64_t now_us_end;

    int32_t add_one_op_time_ns;
    int32_t find_suc_one_op_time_ns;
    int32_t find_fail_one_op_time_ns;
    int32_t fullness;

    const array_hashmap_t *urls_map_struct = NULL;

    urls_fd = fopen("urls", "r");
    if (urls_fd == NULL) {
        printf("Can't open urls file\n");
        exit(EXIT_FAILURE);
    }

    fseek(urls_fd, 0, SEEK_END);
    urls_file_size = ftell(urls_fd);
    fseek(urls_fd, 0, SEEK_SET);

    if (urls_file_size == 0) {
        printf("Empty file\n");
        exit(EXIT_FAILURE);
    }

    urls = malloc(urls_file_size);
    if (urls == NULL) {
        printf("No free memory for urls\n");
        exit(EXIT_FAILURE);
    }

    urls_random = malloc(urls_file_size);
    if (urls_random == 0) {
        printf("No free memory for urls_random\n");
        exit(EXIT_FAILURE);
    }

    if (fread(urls, urls_file_size, 1, urls_fd) != 1) {
        printf("Can't read url file\n");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < urls_file_size; i++) {
        if (urls[i] == '\n') {
            urls[i] = 0;
            urls_map_size++;
        }
    }

    memcpy(urls_random, urls, (int32_t)urls_file_size);

    url_offsets = (int32_t *)malloc(urls_map_size * sizeof(int32_t));
    url_offsets[0] = 0;

    for (i = 0; i < urls_map_size - 1; i++) {
        url_offsets[i + 1] = strchr(&urls[url_offsets[i] + 1], 0) - urls + 1;
    }

    make_random(url_offsets, urls_map_size);

    for (step = 1.00; step > 0.49; step -= 0.01) {
        urls_map_struct = array_hashmap_init(urls_map_size / step, 1, sizeof(url_data_t));
        array_hashmap_set_func(urls_map_struct, add_url_hash, add_url_cmp, find_url_hash,
                               find_url_cmp);

        /* Новые */
        clean_cache();

        gettimeofday(&now_timeval_start, NULL);

        for (i = 0; i < urls_map_size; i++) {
            add_elem.url_pos = url_offsets[i];
            add_elem.time = i;

            add_res = array_hashmap_add_elem(urls_map_struct, &add_elem, NULL, NULL);
            if (add_res != array_hashmap_elem_added) {
                printf("\n");
                printf("Fail\n");
                fflush(stdout);
                return EXIT_FAILURE;
            }
        }

        gettimeofday(&now_timeval_end, NULL);

        now_us_start = now_timeval_start.tv_sec * 1000000 + now_timeval_start.tv_usec;
        now_us_end = now_timeval_end.tv_sec * 1000000 + now_timeval_end.tv_usec;

        add_one_op_time_ns = ((now_us_end - now_us_start) * 1000) / urls_map_size;
        /* Новые */

        /* Всталенные */
        clean_cache();

        gettimeofday(&now_timeval_start, NULL);

        for (i = 0; i < urls_map_size; i++) {
            find_res = array_hashmap_find_elem(urls_map_struct, &urls[url_offsets[i]], &find_elem);
            if (find_res != array_hashmap_elem_finded) {
                printf("\n");
                printf("Fail\n");
                fflush(stdout);
                return EXIT_FAILURE;
            }
        }

        gettimeofday(&now_timeval_end, NULL);

        now_us_start = now_timeval_start.tv_sec * 1000000 + now_timeval_start.tv_usec;
        now_us_end = now_timeval_end.tv_sec * 1000000 + now_timeval_end.tv_usec;

        find_suc_one_op_time_ns = ((now_us_end - now_us_start) * 1000) / urls_map_size;
        /* Всталенные */

        /* Не всталенные */
        clean_cache();

        gettimeofday(&now_timeval_start, NULL);

        for (i = 0; i < urls_map_size; i++) {
            urls_random[url_offsets[i]] = '&';
            find_res =
                array_hashmap_find_elem(urls_map_struct, &urls_random[url_offsets[i]], &find_elem);
            if (find_res != array_hashmap_elem_not_finded) {
                printf("\n");
                printf("Fail\n");
                fflush(stdout);
                return EXIT_FAILURE;
            }
        }

        gettimeofday(&now_timeval_end, NULL);

        now_us_start = now_timeval_start.tv_sec * 1000000 + now_timeval_start.tv_usec;
        now_us_end = now_timeval_end.tv_sec * 1000000 + now_timeval_end.tv_usec;

        find_fail_one_op_time_ns = ((now_us_end - now_us_start) * 1000) / urls_map_size;
        /* Не всталенные */

        fullness = (array_hashmap_get_size(urls_map_struct) / (urls_map_size / step)) * 100;

        printf("%d;%d;%d;%d;\n", fullness, add_one_op_time_ns, find_suc_one_op_time_ns,
               find_fail_one_op_time_ns);

        array_hashmap_del(urls_map_struct);
    }

    printf("Success\n");
    fflush(stdout);
    return EXIT_SUCCESS;

    /*int32_t c_count = collision_count_print(urls_map_struct);

    printf("Elem count %d, Max elem count %d, Collision elem count %d, %ld",
           array_hashmap_get_size(urls_map_struct), (int32_t)(urls_map_size / step), c_count);

    url_data_t find_elem;
    int32_t find_res;

    printf("\nFind instagram.com\n");
    find_res = array_hashmap_find_elem(urls_map_struct, "instagram.com", &find_elem);
    if (find_res) {
        printf("pos %d, str %s, time %d\n", find_elem.url_pos, &urls[find_elem.url_pos],
               find_elem.time);
    } else {
        printf("instagram.com not in map\n");
    }

    printf("\nChange instagram.com time\n");
    url_data_t add_elem;
    add_elem.url_pos = find_elem.url_pos;
    add_elem.time = 600000;
    url_data_t res_elem;
    array_hashmap_add_elem(urls_map_struct, &add_elem, &res_elem, url_on_collision);
    printf("pos %d, str %s, time %d\n", res_elem.url_pos, &urls[res_elem.url_pos], res_elem.time);

    printf("\nFind instagram.com\n");
    find_res = array_hashmap_find_elem(urls_map_struct, "instagram.com", &find_elem);
    if (find_res) {
        printf("pos %d, str %s, time %d\n", find_elem.url_pos, &urls[find_elem.url_pos],
               find_elem.time);
    } else {
        printf("instagram.com not in map\n");
    }

    printf("\nDel instagram.com\n");
    array_hashmap_del_elem(urls_map_struct, "instagram.com", &res_elem);
    printf("pos %d, str %s, time %d\n", res_elem.url_pos, &urls[res_elem.url_pos], res_elem.time);

    printf("\nMap size %d\n", array_hashmap_get_size(urls_map_struct));

    printf("\nFind instagram.com\n");
    find_res = array_hashmap_find_elem(urls_map_struct, "instagram.com", &find_elem);
    if (find_res) {
        printf("pos %d, str %s, time %d\n", find_elem.url_pos, &urls[find_elem.url_pos],
               find_elem.time);
    } else {
        printf("instagram.com not in map\n");
    }

    const array_hashmap_iter_t *iter;
    url_data_t walk_elem;
    int32_t count;

    printf("\nWalk in map\n");
    count = 0;
    iter = array_hashmap_get_iter(urls_map_struct);
    while (array_hashmap_next(iter, &walk_elem)) {
        count++;
    }
    printf("in map elem count %d\n", count);
    array_hashmap_del_iter(iter);

    printf("\nDel all elem\n");
    count = array_hashmap_del_elem_by_func(urls_map_struct, url_decide);
    printf("del elem count %d\n", count);

    printf("\nMap size %d\n", array_hashmap_get_size(urls_map_struct));

    printf("\nWalk in map\n");
    count = 0;
    iter = array_hashmap_get_iter(urls_map_struct);
    while (array_hashmap_next(iter, &walk_elem)) {
        count++;
    }
    printf("in map elem count %d\n", count);
    array_hashmap_del_iter(iter);*/

    return 0;
}
