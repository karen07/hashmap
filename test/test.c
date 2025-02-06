#include "array_hashmap.h"
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define FIRST_TEST_TIME 10
#define SECOND_TEST_TIME 100

typedef struct domain_data {
    uint32_t domain_pos;
    int32_t time;
} domain_data_t;

char *domains;
int32_t *domain_offsets = NULL;
int32_t domains_map_size = 0;
array_hashmap_t domains_map_struct = NULL;

array_hashmap_hash djb33_hash(const char *s)
{
    uint32_t h = 5381;
    while (*s) {
        h += (h << 5);
        h ^= *s++;
    }
    return h;
}

array_hashmap_hash domain_add_hash(const void *add_elem_data)
{
    const domain_data_t *elem = add_elem_data;
    return djb33_hash(&domains[elem->domain_pos]);
}

array_hashmap_bool domain_add_cmp(const void *add_elem_data, const void *hashmap_elem_data)
{
    const domain_data_t *elem1 = add_elem_data;
    const domain_data_t *elem2 = hashmap_elem_data;

    return !strcmp(&domains[elem1->domain_pos], &domains[elem2->domain_pos]);
}

array_hashmap_hash domain_find_hash(const void *find_elem_data)
{
    const char *elem = find_elem_data;
    return djb33_hash(elem);
}

array_hashmap_bool domain_find_cmp(const void *find_elem_data, const void *hashmap_elem_data)
{
    const char *elem1 = find_elem_data;
    const domain_data_t *elem2 = hashmap_elem_data;

    return !strcmp(elem1, &domains[elem2->domain_pos]);
}

array_hashmap_bool domain_on_already_in(const void *add_elem_data, const void *hashmap_elem_data)
{
    const domain_data_t *elem1 = add_elem_data;
    const domain_data_t *elem2 = hashmap_elem_data;

    if (elem1->time > elem2->time) {
        return array_hashmap_save_new;
    } else {
        return array_hashmap_save_old;
    }
}

array_hashmap_bool domain_del_func(const void *del_elem_data)
{
    const domain_data_t *elem = del_elem_data;

    if (elem->time > FIRST_TEST_TIME) {
        return array_hashmap_del_by_func;
    } else {
        return array_hashmap_not_del_by_func;
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
    domain_data_t add_elem;

    while (1) {
        i = rand() % domains_map_size;

        add_elem.domain_pos = domain_offsets[i];
        add_elem.time = FIRST_TEST_TIME;

        array_hashmap_add_elem(domains_map_struct, &add_elem, NULL, array_hashmap_save_old_func);
    }

    return NULL;
}

void *del_thread_func(__attribute__((unused)) void *arg)
{
    int32_t i = 0;
    char *domain;

    while (1) {
        i = rand() % domains_map_size;

        domain = &domains[domain_offsets[i]];

        array_hashmap_del_elem(domains_map_struct, domain, NULL);
    }

    return NULL;
}

void *find_thread_func(__attribute__((unused)) void *arg)
{
    int32_t i = 0;
    char *domain;

    while (1) {
        i = rand() % domains_map_size;

        domain = &domains[domain_offsets[i]];

        array_hashmap_find_elem(domains_map_struct, domain, NULL);
    }

    return NULL;
}

#define TIMER_START()                                         \
    {                                                         \
        random_permutation(domain_offsets, domains_map_size); \
        clean_cache();                                        \
        gettimeofday(&now_timeval_start, NULL);               \
    }

#define TIMER_END()                                                                             \
    {                                                                                           \
        gettimeofday(&now_timeval_end, NULL);                                                   \
        now_us_start = now_timeval_start.tv_sec * 1000000 + now_timeval_start.tv_usec;          \
        now_us_end = now_timeval_end.tv_sec * 1000000 + now_timeval_end.tv_usec;                \
        one_op_time_ns[time_index++] = ((now_us_end - now_us_start) * 1000) / domains_map_size; \
    }

int32_t main(void)
{
    FILE *domains_fd = NULL;
    char *domains_random = NULL;
    char *domain;

    int32_t is_thread_safety;

    int64_t domains_file_size = 0;

    int32_t i = 0;

    double step = 0;

    domain_data_t add_elem;
    int32_t add_res;

    domain_data_t find_elem;
    int32_t find_res;

    domain_data_t del_elem;
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

    domains_fd = fopen("domains", "r");
    if (domains_fd == NULL) {
        printf("Can't open domains file\n");
        exit(EXIT_FAILURE);
    }

    fseek(domains_fd, 0, SEEK_END);
    domains_file_size = ftell(domains_fd);
    fseek(domains_fd, 0, SEEK_SET);

    if (domains_file_size == 0) {
        printf("Empty file\n");
        exit(EXIT_FAILURE);
    }

    domains = malloc(domains_file_size);
    if (domains == NULL) {
        printf("No free memory for domains\n");
        exit(EXIT_FAILURE);
    }

    domains_random = malloc(domains_file_size);
    if (domains_random == NULL) {
        printf("No free memory for domains_random\n");
        exit(EXIT_FAILURE);
    }

    if (fread(domains, domains_file_size, 1, domains_fd) != 1) {
        printf("Can't read domain file\n");
        exit(EXIT_FAILURE);
    }

    fclose(domains_fd);

    for (i = 0; i < domains_file_size; i++) {
        if (domains[i] == '\n') {
            domains[i] = 0;
            domains_map_size++;
        }
    }

    domain_offsets = (int32_t *)malloc(domains_map_size * sizeof(int32_t));
    domain_offsets[0] = 0;

    for (i = 0; i < domains_map_size - 1; i++) {
        domain_offsets[i + 1] = strchr(&domains[domain_offsets[i] + 1], 0) - domains + 1;
    }

    memcpy(domains_random, domains, (int32_t)domains_file_size);
    for (i = 0; i < domains_map_size; i++) {
        domains_random[domain_offsets[i]] = '&';
    }

    printf("Domains count: %d\n", domains_map_size);

    printf("Fullness;"
           "Add values;"
           "Check that all values are inserted;"
           "Check that there are no non-inserted elements;"
           "Update values;"
           "Check the updated values;"
           "Delete everything individually;"
           "Delete everything at once;\n");

    for (step = 1.00; step > 0.01; step -= 0.01) {
        time_index = 0;

        /* Init */
        domains_map_struct =
            array_hashmap_init(domains_map_size / step, 1.0, sizeof(domain_data_t));
        if (domains_map_struct == NULL) {
            printf("Init error\n");
            return EXIT_FAILURE;
        }

        array_hashmap_set_func(domains_map_struct, domain_add_hash, domain_add_cmp,
                               domain_find_hash, domain_find_cmp, domain_find_hash,
                               domain_find_cmp);
        /* Init */

        /* Add values */
        TIMER_START();
        for (i = 0; i < domains_map_size; i++) {
            add_elem.domain_pos = domain_offsets[i];
            add_elem.time = FIRST_TEST_TIME;

            add_res = array_hashmap_add_elem(domains_map_struct, &add_elem, NULL,
                                             array_hashmap_save_old_func);
            if (add_res != array_hashmap_elem_added) {
                printf("Add values error\n");
                return EXIT_FAILURE;
            }
        }
        TIMER_END();
        /* Add values */

        /* Check that all values are inserted */
        TIMER_START();
        for (i = 0; i < domains_map_size; i++) {
            domain = &domains[domain_offsets[i]];
            find_elem.domain_pos = 0;
            find_elem.time = 0;
            find_res = array_hashmap_find_elem(domains_map_struct, domain, &find_elem);
            if (find_res != array_hashmap_elem_finded || find_elem.time != FIRST_TEST_TIME) {
                printf("Check that all values are inserted error\n");
                return EXIT_FAILURE;
            }
        }
        TIMER_END();
        /* Check that all values are inserted */

        /* Check that there are no non-inserted elements */
        TIMER_START();
        for (i = 0; i < domains_map_size; i++) {
            domain = &domains_random[domain_offsets[i]];
            find_res = array_hashmap_find_elem(domains_map_struct, domain, &find_elem);
            if (find_res != array_hashmap_elem_not_finded) {
                printf("Check that there are no non-inserted elements error\n");
                return EXIT_FAILURE;
            }
        }
        TIMER_END();
        /* Check that there are no non-inserted elements */

        /* Update values */
        TIMER_START();
        for (i = 0; i < domains_map_size; i++) {
            add_elem.domain_pos = domain_offsets[i];
            add_elem.time = SECOND_TEST_TIME;

            add_res =
                array_hashmap_add_elem(domains_map_struct, &add_elem, NULL, domain_on_already_in);
            if (add_res != array_hashmap_elem_already_in) {
                printf("Update values error\n");
                return EXIT_FAILURE;
            }
        }
        TIMER_END();
        /* Update values */

        /* Check the updated values */
        TIMER_START();
        for (i = 0; i < domains_map_size; i++) {
            domain = &domains[domain_offsets[i]];
            find_elem.domain_pos = 0;
            find_elem.time = 0;
            find_res = array_hashmap_find_elem(domains_map_struct, domain, &find_elem);
            if (find_res != array_hashmap_elem_finded || find_elem.time != SECOND_TEST_TIME) {
                printf("Check the updated values error\n");
                return EXIT_FAILURE;
            }
        }
        TIMER_END();
        /* Check the updated values */

        /* Delete everything individually */
        TIMER_START();
        for (i = 0; i < domains_map_size; i++) {
            domain = &domains[domain_offsets[i]];
            del_elem.domain_pos = 0;
            del_elem.time = 0;
            del_res = array_hashmap_del_elem(domains_map_struct, domain, &del_elem);
            if (del_res != array_hashmap_elem_deled || del_elem.time != SECOND_TEST_TIME) {
                printf("Delete everything individually error\n");
                return EXIT_FAILURE;
            }
        }
        TIMER_END();
        /* Delete everything individually */

        /* Check that everything is deleted */
        for (i = 0; i < domains_map_size; i++) {
            domain = &domains[domain_offsets[i]];
            find_res = array_hashmap_find_elem(domains_map_struct, domain, &find_elem);
            if (find_res != array_hashmap_elem_not_finded) {
                printf("Check that everything is deleted error\n");
                return EXIT_FAILURE;
            }
        }
        for (i = 0; i < domains_map_size; i++) {
            domain = &domains_random[domain_offsets[i]];
            find_res = array_hashmap_find_elem(domains_map_struct, domain, &find_elem);
            if (find_res != array_hashmap_elem_not_finded) {
                printf("Check that everything is deleted error\n");
                return EXIT_FAILURE;
            }
        }
        if (array_hashmap_now_in_map(domains_map_struct) != 0) {
            printf("Check that everything is deleted error\n");
            return EXIT_FAILURE;
        }
        /* Check that everything is deleted */

        /* Add values */
        for (i = 0; i < domains_map_size; i++) {
            add_elem.domain_pos = domain_offsets[i];
            add_elem.time = SECOND_TEST_TIME;

            add_res = array_hashmap_add_elem(domains_map_struct, &add_elem, NULL,
                                             array_hashmap_save_old_func);
            if (add_res != array_hashmap_elem_added) {
                printf("Add values error\n");
                return EXIT_FAILURE;
            }
        }
        /* Add values */

        /* Delete everything at once */
        TIMER_START();
        del_elem_by_func_res = array_hashmap_del_elem_by_func(domains_map_struct, domain_del_func);
        if (del_elem_by_func_res != domains_map_size) {
            printf("Delete everything at once error\n");
            return EXIT_FAILURE;
        }
        TIMER_END();
        /* Delete everything at once */

        /* Check that everything is deleted */
        for (i = 0; i < domains_map_size; i++) {
            domain = &domains[domain_offsets[i]];
            find_res = array_hashmap_find_elem(domains_map_struct, domain, &find_elem);
            if (find_res != array_hashmap_elem_not_finded) {
                printf("Check that everything is deleted error\n");
                return EXIT_FAILURE;
            }
        }
        for (i = 0; i < domains_map_size; i++) {
            domain = &domains_random[domain_offsets[i]];
            find_res = array_hashmap_find_elem(domains_map_struct, domain, &find_elem);
            if (find_res != array_hashmap_elem_not_finded) {
                printf("Check that everything is deleted error\n");
                return EXIT_FAILURE;
            }
        }
        if (array_hashmap_now_in_map(domains_map_struct) != 0) {
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

        array_hashmap_del(&domains_map_struct);
    }

    domains_map_struct = array_hashmap_init(domains_map_size, 1.0, sizeof(domain_data_t));
    if (domains_map_struct == NULL) {
        printf("Init error\n");
        return EXIT_FAILURE;
    }

    is_thread_safety = array_hashmap_is_thread_safety(domains_map_struct);

    array_hashmap_del(&domains_map_struct);

    if (is_thread_safety) {
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

        for (i = 0; i < 60; i++) {
            domains_map_struct = array_hashmap_init(domains_map_size, 1.0, sizeof(domain_data_t));
            if (domains_map_struct == NULL) {
                printf("Init error\n");
                return EXIT_FAILURE;
            }

            array_hashmap_set_func(domains_map_struct, domain_add_hash, domain_add_cmp,
                                   domain_find_hash, domain_find_cmp, domain_find_hash,
                                   domain_find_cmp);

            sleep(1);

            array_hashmap_del(&domains_map_struct);
        }
    }

    free(domains);
    free(domains_random);

    printf("Success\n");
    return EXIT_SUCCESS;
}
