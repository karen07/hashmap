#include "array_hashmap.h"
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#ifdef THREAD_SAFETY
#include <pthread.h>
#endif

#define FIRST_EXAMPLE_TIME 10
#define SECOND_EXAMPLE_TIME 100

typedef struct url_data {
    uint32_t url_pos;
    int32_t time;
} url_data_t;

char *urls;
int32_t *url_offsets = NULL;
int32_t urls_map_size = 0;
array_hashmap_t urls_map_struct = NULL;

hash djb33_hash(const char *s)
{
    uint32_t h = 5381;
    while (*s) {
        h += (h << 5);
        h ^= *s++;
    }
    return h;
}

hash url_add_hash(const void *add_elem_data)
{
    const url_data_t *elem = add_elem_data;
    return djb33_hash(&urls[elem->url_pos]);
}

bool url_add_cmp(const void *add_elem_data, const void *hashmap_elem_data)
{
    const url_data_t *elem1 = add_elem_data;
    const url_data_t *elem2 = hashmap_elem_data;

    return !strcmp(&urls[elem1->url_pos], &urls[elem2->url_pos]);
}

hash url_find_hash(const void *find_elem_data)
{
    const char *elem = find_elem_data;
    return djb33_hash(elem);
}

bool url_find_cmp(const void *find_elem_data, const void *hashmap_elem_data)
{
    const char *elem1 = find_elem_data;
    const url_data_t *elem2 = hashmap_elem_data;

    return !strcmp(elem1, &urls[elem2->url_pos]);
}

bool url_on_already_in(const void *add_elem_data, const void *hashmap_elem_data)
{
    const url_data_t *elem1 = add_elem_data;
    const url_data_t *elem2 = hashmap_elem_data;

    if (elem1->time > elem2->time) {
        return array_hashmap_save_new;
    } else {
        return array_hashmap_save_old;
    }
}

bool url_del_func(const void *del_elem_data)
{
    const url_data_t *elem = del_elem_data;

    if (elem->time > FIRST_EXAMPLE_TIME) {
        return array_hashmap_save_new; /* FUCK */
    } else {
        return array_hashmap_save_old; /* FUCK */
    }
}

void clean_cache(void)
{
    int32_t i = 0;
    int32_t j = 0;
    int32_t size = 100 * 1024 * 1024;
    char *c = NULL;

    c = (char *)malloc(size);
    for (i = 0; i < 0xffff; i++) {
        for (j = 0; j < size; j++) {
            c[j] = i * j;
        }
    }
    free(c);
}

void random_permutation(int32_t *array, int32_t size)
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

void *add_thread_func(__attribute__((unused)) void *arg)
{
    int32_t i = 0;
    url_data_t add_elem;

    while (1) {
        i = rand() % urls_map_size;

        add_elem.url_pos = url_offsets[i];
        add_elem.time = FIRST_EXAMPLE_TIME;

        array_hashmap_add_elem(urls_map_struct, &add_elem, NULL, NULL);
    }

    return NULL;
}

void *del_thread_func(__attribute__((unused)) void *arg)
{
    int32_t i = 0;
    char *url;

    while (1) {
        i = rand() % urls_map_size;

        url = &urls[url_offsets[i]];

        array_hashmap_del_elem(urls_map_struct, url, NULL);
    }

    return NULL;
}

void *find_thread_func(__attribute__((unused)) void *arg)
{
    int32_t i = 0;
    char *url;

    while (1) {
        i = rand() % urls_map_size;

        url = &urls[url_offsets[i]];

        array_hashmap_find_elem(urls_map_struct, url, NULL);
    }

    return NULL;
}

#define TIMER_START()                                   \
    {                                                   \
        random_permutation(url_offsets, urls_map_size); \
        clean_cache();                                  \
        gettimeofday(&now_timeval_start, NULL);         \
    }

#define TIMER_END()                                                                          \
    {                                                                                        \
        gettimeofday(&now_timeval_end, NULL);                                                \
        now_us_start = now_timeval_start.tv_sec * 1000000 + now_timeval_start.tv_usec;       \
        now_us_end = now_timeval_end.tv_sec * 1000000 + now_timeval_end.tv_usec;             \
        one_op_time_ns[time_index++] = ((now_us_end - now_us_start) * 1000) / urls_map_size; \
    }

int32_t main(void)
{
    FILE *urls_fd = NULL;
    char *urls_random = NULL;
    char *url;

    int64_t urls_file_size = 0;

    int32_t i = 0;

    double step = 0;

    url_data_t add_elem;
    int32_t add_res;

    url_data_t find_elem;
    int32_t find_res;

    url_data_t del_elem;
    int32_t del_res;

    int32_t del_elem_by_func_res;

    struct timeval now_timeval_start;
    struct timeval now_timeval_end;

    uint64_t now_us_start;
    uint64_t now_us_end;

    int32_t time_index = 0;
    int32_t one_op_time_ns[100];

    pthread_t add_thread;
    pthread_t del_thread;
    pthread_t find_thread;

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
    if (urls_random == NULL) {
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

    url_offsets = (int32_t *)malloc(urls_map_size * sizeof(int32_t));
    url_offsets[0] = 0;

    for (i = 0; i < urls_map_size - 1; i++) {
        url_offsets[i + 1] = strchr(&urls[url_offsets[i] + 1], 0) - urls + 1;
    }

    memcpy(urls_random, urls, (int32_t)urls_file_size);
    for (i = 0; i < urls_map_size; i++) {
        urls_random[url_offsets[i]] = '&';
    }

    printf("URLs count: %d\n", urls_map_size);

    /* printf("Fullness;"
           "Add values;"
           "Check that all values are inserted;"
           "Check that there are no non-inserted elements;"
           "Update values;"
           "Check the updated values;"
           "Delete everything individually;"
           "Delete everything at once;\n"); */

    for (step = 0.49; step > 0.49; step -= 0.01) {
        time_index = 0;

        /* Init */
        urls_map_struct = array_hashmap_init(urls_map_size / step, 1, sizeof(url_data_t));
        if (urls_map_struct == NULL) {
            printf("Init error\n");
            return EXIT_FAILURE;
        }

        array_hashmap_set_func(urls_map_struct, url_add_hash, url_add_cmp, url_find_hash,
                               url_find_cmp, url_find_hash, url_find_cmp);
        /* Init */

        /* Add values */
        TIMER_START();
        for (i = 0; i < urls_map_size; i++) {
            add_elem.url_pos = url_offsets[i];
            add_elem.time = FIRST_EXAMPLE_TIME;

            add_res = array_hashmap_add_elem(urls_map_struct, &add_elem, NULL, NULL);
            if (add_res != array_hashmap_elem_added) {
                printf("Add values error\n");
                return EXIT_FAILURE;
            }
        }
        TIMER_END();
        /* Add values */

        /* Check that all values are inserted */
        TIMER_START();
        for (i = 0; i < urls_map_size; i++) {
            url = &urls[url_offsets[i]];
            find_elem.url_pos = 0;
            find_elem.time = 0;
            find_res = array_hashmap_find_elem(urls_map_struct, url, &find_elem);
            if (find_res != array_hashmap_elem_finded || find_elem.time != FIRST_EXAMPLE_TIME) {
                printf("Check that all values are inserted error\n");
                return EXIT_FAILURE;
            }
        }
        TIMER_END();
        /* Check that all values are inserted */

        /* Check that there are no non-inserted elements */
        TIMER_START();
        for (i = 0; i < urls_map_size; i++) {
            url = &urls_random[url_offsets[i]];
            find_res = array_hashmap_find_elem(urls_map_struct, url, &find_elem);
            if (find_res != array_hashmap_elem_not_finded) {
                printf("Check that there are no non-inserted elements error\n");
                return EXIT_FAILURE;
            }
        }
        TIMER_END();
        /* Check that there are no non-inserted elements */

        /* Update values */
        TIMER_START();
        for (i = 0; i < urls_map_size; i++) {
            add_elem.url_pos = url_offsets[i];
            add_elem.time = SECOND_EXAMPLE_TIME;

            add_res = array_hashmap_add_elem(urls_map_struct, &add_elem, NULL, url_on_already_in);
            if (add_res != array_hashmap_elem_already_in) {
                printf("Update values error\n");
                return EXIT_FAILURE;
            }
        }
        TIMER_END();
        /* Update values */

        /* Check the updated values */
        TIMER_START();
        for (i = 0; i < urls_map_size; i++) {
            url = &urls[url_offsets[i]];
            find_elem.url_pos = 0;
            find_elem.time = 0;
            find_res = array_hashmap_find_elem(urls_map_struct, url, &find_elem);
            if (find_res != array_hashmap_elem_finded || find_elem.time != SECOND_EXAMPLE_TIME) {
                printf("Check the updated values error\n");
                return EXIT_FAILURE;
            }
        }
        TIMER_END();
        /* Check the updated values */

        /* Delete everything individually */
        TIMER_START();
        for (i = 0; i < urls_map_size; i++) {
            url = &urls[url_offsets[i]];
            del_elem.url_pos = 0;
            del_elem.time = 0;
            del_res = array_hashmap_del_elem(urls_map_struct, url, &del_elem);
            if (del_res != array_hashmap_elem_deled || del_elem.time != SECOND_EXAMPLE_TIME) {
                printf("Delete everything individually error\n");
                return EXIT_FAILURE;
            }
        }
        TIMER_END();
        /* Delete everything individually */

        /* Check that everything is deleted */
        for (i = 0; i < urls_map_size; i++) {
            url = &urls[url_offsets[i]];
            find_res = array_hashmap_find_elem(urls_map_struct, url, &find_elem);
            if (find_res != array_hashmap_elem_not_finded) {
                printf("Check that everything is deleted error\n");
                return EXIT_FAILURE;
            }
        }
        for (i = 0; i < urls_map_size; i++) {
            url = &urls_random[url_offsets[i]];
            find_res = array_hashmap_find_elem(urls_map_struct, url, &find_elem);
            if (find_res != array_hashmap_elem_not_finded) {
                printf("Check that everything is deleted error\n");
                return EXIT_FAILURE;
            }
        }
        if (array_hashmap_get_size(urls_map_struct) != 0) {
            printf("Check that everything is deleted error\n");
            return EXIT_FAILURE;
        }
        /* Check that everything is deleted */

        /* Add values */
        for (i = 0; i < urls_map_size; i++) {
            add_elem.url_pos = url_offsets[i];
            add_elem.time = SECOND_EXAMPLE_TIME;

            add_res = array_hashmap_add_elem(urls_map_struct, &add_elem, NULL, NULL);
            if (add_res != array_hashmap_elem_added) {
                printf("Add values error\n");
                return EXIT_FAILURE;
            }
        }
        /* Add values */

        /* Delete everything at once */
        TIMER_START();
        del_elem_by_func_res = array_hashmap_del_elem_by_func(urls_map_struct, url_del_func);
        if (del_elem_by_func_res != urls_map_size) {
            printf("Delete everything at once error\n");
            return EXIT_FAILURE;
        }
        TIMER_END();
        /* Delete everything at once */

        /* Check that everything is deleted */
        for (i = 0; i < urls_map_size; i++) {
            url = &urls[url_offsets[i]];
            find_res = array_hashmap_find_elem(urls_map_struct, url, &find_elem);
            if (find_res != array_hashmap_elem_not_finded) {
                printf("Check that everything is deleted error\n");
                return EXIT_FAILURE;
            }
        }
        for (i = 0; i < urls_map_size; i++) {
            url = &urls_random[url_offsets[i]];
            find_res = array_hashmap_find_elem(urls_map_struct, url, &find_elem);
            if (find_res != array_hashmap_elem_not_finded) {
                printf("Check that everything is deleted error\n");
                return EXIT_FAILURE;
            }
        }
        if (array_hashmap_get_size(urls_map_struct) != 0) {
            printf("Check that everything is deleted error\n");
            return EXIT_FAILURE;
        }
        /* Check that everything is deleted */

        printf("%d;", (int32_t)(step * 100));
        for (i = 0; i < time_index; i++) {
            printf("%d;", one_op_time_ns[i]);
        }
        printf("\n");
        fflush(stdout);

        array_hashmap_del(&urls_map_struct);
    }

    if (pthread_create(&add_thread, NULL, add_thread_func, NULL)) {
        printf("Can't create add_thread\n");
        exit(EXIT_FAILURE);
    }

    if (pthread_detach(add_thread)) {
        printf("Can't detach add_thread\n");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&del_thread, NULL, del_thread_func, NULL)) {
        printf("Can't create del_thread\n");
        exit(EXIT_FAILURE);
    }

    if (pthread_detach(del_thread)) {
        printf("Can't detach del_thread\n");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&find_thread, NULL, find_thread_func, NULL)) {
        printf("Can't create find_thread\n");
        exit(EXIT_FAILURE);
    }

    if (pthread_detach(find_thread)) {
        printf("Can't detach find_thread\n");
        exit(EXIT_FAILURE);
    }

    while (1) {
        /* Init */
        urls_map_struct = array_hashmap_init(urls_map_size / step, 1, sizeof(url_data_t));
        if (urls_map_struct == NULL) {
            printf("Init error\n");
            return EXIT_FAILURE;
        }

        array_hashmap_set_func(urls_map_struct, url_add_hash, url_add_cmp, url_find_hash,
                               url_find_cmp, url_find_hash, url_find_cmp);
        /* Init */

        sleep(5);

        printf("URLs in hashmap: %d\n", array_hashmap_get_size(urls_map_struct));

        array_hashmap_del(&urls_map_struct);
    }

    printf("Success\n");
    return EXIT_SUCCESS;
}
