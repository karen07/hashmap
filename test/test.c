#include "array_hashmap.h"
#include "uthash.h"
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>

#define FIRST_TEST_TIME 10
#define SECOND_TEST_TIME 100

#define MIN_DOMAIN_LEN 50
#define MAX_DOMAIN_LEN 250
#define DOMAINS_FILE_SIZE_MB 50

typedef struct domain_data {
    uint32_t domain_pos;
    int32_t time;
} domain_data_t;

char *domains = NULL;
char *domains_random = NULL;
int32_t *domain_offsets = NULL;
int32_t domains_map_size = 0;
array_hashmap_t domains_map_struct = NULL;

volatile int32_t thread_count = 0;

pthread_barrier_t threads_barrier_start;
pthread_barrier_t threads_barrier_end;

typedef struct uth_node {
    const char *key;
    size_t keylen;
    domain_data_t val;
    UT_hash_handle hh;
} uth_node_t;

uth_node_t *uth_tab = NULL;
pthread_rwlock_t uth_rwlock;

size_t rss_bytes(void)
{
    long page = sysconf(_SC_PAGESIZE);
    long total_pages = 0, rss_pages = 0;
    FILE *f = fopen("/proc/self/statm", "r");
    if (!f) {
        return 0;
    }
    if (fscanf(f, "%ld %ld", &total_pages, &rss_pages) != 2) {
        fclose(f);
        return 0;
    }
    fclose(f);
    return (size_t)rss_pages * (size_t)page;
}

void heap_trim(void)
{
#if defined(__GLIBC__)
    extern int malloc_trim(size_t);
    malloc_trim(0);
#endif
}

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

void errmsg(const char *format, ...)
{
    va_list args;

    printf("Error: ");

    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    exit(EXIT_FAILURE);
}

void random_permutation(int32_t *array, int32_t size)
{
    int32_t i = 0;

    for (i = 0; i < size - 1; i++) {
        int32_t fir = rand() % size;
        int32_t sec = rand() % size;

        int32_t swap = array[fir];
        array[fir] = array[sec];
        array[sec] = swap;
    }
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

#define RUN_THREAD(func)                                                                        \
    {                                                                                           \
        pthread_barrier_init(&threads_barrier_start, NULL, thread_count + 1);                   \
        pthread_barrier_init(&threads_barrier_end, NULL, thread_count + 1);                     \
        for (i = 0; i < thread_count; i++) {                                                    \
            set_arg = (void *)((int64_t)i);                                                     \
            if (pthread_create(&thread, NULL, func##_thread_func, set_arg)) {                   \
                errmsg("Can't create " #func "_thread %d\n", i);                                \
            }                                                                                   \
            if (pthread_detach(thread)) {                                                       \
                errmsg("Can't detach " #func "_thread %d\n", i);                                \
            }                                                                                   \
        }                                                                                       \
        random_permutation(domain_offsets, domains_map_size);                                   \
        clean_cache();                                                                          \
        pthread_barrier_wait(&threads_barrier_start);                                           \
        gettimeofday(&now_timeval_start, NULL);                                                 \
        pthread_barrier_wait(&threads_barrier_end);                                             \
        gettimeofday(&now_timeval_end, NULL);                                                   \
        now_us_start = now_timeval_start.tv_sec * 1000000 + now_timeval_start.tv_usec;          \
        now_us_end = now_timeval_end.tv_sec * 1000000 + now_timeval_end.tv_usec;                \
        one_op_time_ns[time_index++] = ((now_us_end - now_us_start) * 1000) / domains_map_size; \
    }

void *add_thread_func(void *arg)
{
    int32_t i = 0;
    domain_data_t add_elem;
    int32_t add_res;
    int32_t thread_num;

    thread_num = (int64_t)arg;

    pthread_barrier_wait(&threads_barrier_start);
    for (i = (domains_map_size / thread_count) * thread_num;
         i < (domains_map_size / thread_count) * (thread_num + 1); i++) {
        add_elem.domain_pos = domain_offsets[i];
        add_elem.time = FIRST_TEST_TIME;

        add_res = array_hashmap_add_elem(domains_map_struct, &add_elem, NULL,
                                         array_hashmap_save_old_func);
        if (add_res != array_hashmap_elem_added) {
            errmsg("array_hashmap: Add values error\n");
        }
    }
    pthread_barrier_wait(&threads_barrier_end);

    return NULL;
}

void *find_thread_func(void *arg)
{
    int32_t i = 0;
    domain_data_t find_elem;
    int32_t find_res;
    int32_t thread_num;
    char *domain;

    thread_num = (int64_t)arg;

    pthread_barrier_wait(&threads_barrier_start);
    for (i = (domains_map_size / thread_count) * thread_num;
         i < (domains_map_size / thread_count) * (thread_num + 1); i++) {
        domain = &domains[domain_offsets[i]];
        find_elem.domain_pos = 0;
        find_elem.time = 0;
        find_res = array_hashmap_find_elem(domains_map_struct, domain, &find_elem);
        if (find_res != array_hashmap_elem_finded || find_elem.time != FIRST_TEST_TIME) {
            errmsg("array_hashmap: Check that all values are inserted error\n");
        }
    }
    pthread_barrier_wait(&threads_barrier_end);

    return NULL;
}

void *no_find_thread_func(void *arg)
{
    int32_t i = 0;
    domain_data_t find_elem;
    int32_t find_res;
    int32_t thread_num;
    char *domain;

    thread_num = (int64_t)arg;

    pthread_barrier_wait(&threads_barrier_start);
    for (i = (domains_map_size / thread_count) * thread_num;
         i < (domains_map_size / thread_count) * (thread_num + 1); i++) {
        domain = &domains_random[domain_offsets[i]];
        find_res = array_hashmap_find_elem(domains_map_struct, domain, &find_elem);
        if (find_res != array_hashmap_elem_not_finded) {
            errmsg("array_hashmap: Check that there are no non-inserted elements error\n");
        }
    }
    pthread_barrier_wait(&threads_barrier_end);

    return NULL;
}

void *update_thread_func(void *arg)
{
    int32_t i = 0;
    domain_data_t add_elem;
    int32_t add_res;
    int32_t thread_num;

    thread_num = (int64_t)arg;

    pthread_barrier_wait(&threads_barrier_start);
    for (i = (domains_map_size / thread_count) * thread_num;
         i < (domains_map_size / thread_count) * (thread_num + 1); i++) {
        add_elem.domain_pos = domain_offsets[i];
        add_elem.time = SECOND_TEST_TIME;

        add_res = array_hashmap_add_elem(domains_map_struct, &add_elem, NULL, domain_on_already_in);
        if (add_res != array_hashmap_elem_already_in) {
            errmsg("array_hashmap: Update values error\n");
        }
    }
    pthread_barrier_wait(&threads_barrier_end);

    return NULL;
}

void *check_update_thread_func(void *arg)
{
    int32_t i = 0;
    domain_data_t find_elem;
    int32_t find_res;
    int32_t thread_num;
    char *domain;

    thread_num = (int64_t)arg;

    pthread_barrier_wait(&threads_barrier_start);
    for (i = (domains_map_size / thread_count) * thread_num;
         i < (domains_map_size / thread_count) * (thread_num + 1); i++) {
        domain = &domains[domain_offsets[i]];
        find_elem.domain_pos = 0;
        find_elem.time = 0;
        find_res = array_hashmap_find_elem(domains_map_struct, domain, &find_elem);
        if (find_res != array_hashmap_elem_finded || find_elem.time != SECOND_TEST_TIME) {
            errmsg("array_hashmap: Check the updated values error\n");
        }
    }
    pthread_barrier_wait(&threads_barrier_end);

    return NULL;
}

void *del_thread_func(void *arg)
{
    int32_t i = 0;
    domain_data_t del_elem;
    int32_t del_res;
    int32_t thread_num;
    char *domain;

    thread_num = (int64_t)arg;

    pthread_barrier_wait(&threads_barrier_start);
    for (i = (domains_map_size / thread_count) * thread_num;
         i < (domains_map_size / thread_count) * (thread_num + 1); i++) {
        domain = &domains[domain_offsets[i]];
        del_elem.domain_pos = 0;
        del_elem.time = 0;
        del_res = array_hashmap_del_elem(domains_map_struct, domain, &del_elem);
        if (del_res != array_hashmap_elem_deled || del_elem.time != SECOND_TEST_TIME) {
            errmsg("array_hashmap: Delete everything individually error\n");
        }
    }
    pthread_barrier_wait(&threads_barrier_end);

    return NULL;
}

void uth_init(void)
{
    uth_tab = NULL;
    if (pthread_rwlock_init(&uth_rwlock, NULL)) {
        errmsg("uthash: rwlock init failed\n");
    }
}

void uth_destroy(void)
{
    uth_node_t *cur, *tmp;
    pthread_rwlock_wrlock(&uth_rwlock);
    HASH_ITER(hh, uth_tab, cur, tmp)
    {
        HASH_DEL(uth_tab, cur);
        free(cur);
    }
    pthread_rwlock_unlock(&uth_rwlock);
    pthread_rwlock_destroy(&uth_rwlock);
}

int32_t uth_now_in_map(void)
{
    return (int32_t)HASH_COUNT(uth_tab);
}

array_hashmap_ret_t uth_add_elem(const domain_data_t *add_elem, on_already_in_t on_in)
{
    const char *key = &domains[add_elem->domain_pos];
    size_t keylen = strlen(key);
    uth_node_t *n = NULL;

    pthread_rwlock_wrlock(&uth_rwlock);
    HASH_FIND(hh, uth_tab, key, keylen, n);
    if (!n) {
        n = (uth_node_t *)malloc(sizeof(*n));
        if (!n) {
            pthread_rwlock_unlock(&uth_rwlock);
            errmsg("uthash: oom\n");
        }
        n->key = key;
        n->keylen = keylen;
        n->val = *add_elem;
        HASH_ADD_KEYPTR(hh, uth_tab, n->key, n->keylen, n);
        pthread_rwlock_unlock(&uth_rwlock);
        return array_hashmap_elem_added;
    } else {
        if (on_in) {
            if (on_in == array_hashmap_save_new_func) {
                n->val = *add_elem;
            } else {
                if (on_in(add_elem, &n->val)) {
                    n->val = *add_elem;
                }
            }
        }
        pthread_rwlock_unlock(&uth_rwlock);
        return array_hashmap_elem_already_in;
    }
}

array_hashmap_ret_t uth_find_elem(const char *key, domain_data_t *out)
{
    uth_node_t *n = NULL;
    pthread_rwlock_rdlock(&uth_rwlock);
    HASH_FIND(hh, uth_tab, key, strlen(key), n);
    if (!n) {
        pthread_rwlock_unlock(&uth_rwlock);
        return array_hashmap_elem_not_finded;
    }
    if (out) {
        *out = n->val;
    }
    pthread_rwlock_unlock(&uth_rwlock);
    return array_hashmap_elem_finded;
}

array_hashmap_ret_t uth_del_elem(const char *key, domain_data_t *out)
{
    uth_node_t *n = NULL;
    pthread_rwlock_wrlock(&uth_rwlock);
    HASH_FIND(hh, uth_tab, key, strlen(key), n);
    if (!n) {
        pthread_rwlock_unlock(&uth_rwlock);
        return array_hashmap_elem_not_deled;
    }
    if (out) {
        *out = n->val;
    }
    HASH_DEL(uth_tab, n);
    free(n);
    pthread_rwlock_unlock(&uth_rwlock);
    return array_hashmap_elem_deled;
}

array_hashmap_deled_count uth_del_by_func(del_func_t fn)
{
    uth_node_t *cur, *tmp;
    int32_t cnt = 0;
    pthread_rwlock_wrlock(&uth_rwlock);
    HASH_ITER(hh, uth_tab, cur, tmp)
    {
        if (fn(&cur->val)) {
            HASH_DEL(uth_tab, cur);
            free(cur);
            cnt++;
        }
    }
    pthread_rwlock_unlock(&uth_rwlock);
    return cnt;
}

void *uth_add_thread_func(void *arg)
{
    int32_t i = 0, add_res, thread_num = (int32_t)(int64_t)arg;
    domain_data_t add_elem;
    pthread_barrier_wait(&threads_barrier_start);
    for (i = (domains_map_size / thread_count) * thread_num;
         i < (domains_map_size / thread_count) * (thread_num + 1); i++) {
        add_elem.domain_pos = (uint32_t)domain_offsets[i];
        add_elem.time = FIRST_TEST_TIME;
        add_res = uth_add_elem(&add_elem, array_hashmap_save_old_func);
        if (add_res != array_hashmap_elem_added) {
            errmsg("uthash: Add values error\n");
        }
    }
    pthread_barrier_wait(&threads_barrier_end);
    return NULL;
}

void *uth_find_thread_func(void *arg)
{
    int32_t i = 0, find_res, thread_num = (int32_t)(int64_t)arg;
    domain_data_t out;
    char *domain;
    pthread_barrier_wait(&threads_barrier_start);
    for (i = (domains_map_size / thread_count) * thread_num;
         i < (domains_map_size / thread_count) * (thread_num + 1); i++) {
        domain = &domains[domain_offsets[i]];
        find_res = uth_find_elem(domain, &out);
        if (find_res != array_hashmap_elem_finded || out.time != FIRST_TEST_TIME) {
            errmsg("uthash: Check that all values are inserted error\n");
        }
    }
    pthread_barrier_wait(&threads_barrier_end);
    return NULL;
}

void *uth_no_find_thread_func(void *arg)
{
    int32_t i = 0, find_res, thread_num = (int32_t)(int64_t)arg;
    domain_data_t out;
    char *domain;
    pthread_barrier_wait(&threads_barrier_start);
    for (i = (domains_map_size / thread_count) * thread_num;
         i < (domains_map_size / thread_count) * (thread_num + 1); i++) {
        domain = &domains_random[domain_offsets[i]];
        find_res = uth_find_elem(domain, &out);
        if (find_res != array_hashmap_elem_not_finded) {
            errmsg("uthash: Check that there are no non-inserted elements error\n");
        }
    }
    pthread_barrier_wait(&threads_barrier_end);
    return NULL;
}

void *uth_update_thread_func(void *arg)
{
    int32_t i = 0, add_res, thread_num = (int32_t)(int64_t)arg;
    domain_data_t add_elem;
    pthread_barrier_wait(&threads_barrier_start);
    for (i = (domains_map_size / thread_count) * thread_num;
         i < (domains_map_size / thread_count) * (thread_num + 1); i++) {
        add_elem.domain_pos = (uint32_t)domain_offsets[i];
        add_elem.time = SECOND_TEST_TIME;
        add_res = uth_add_elem(&add_elem, domain_on_already_in);
        if (add_res != array_hashmap_elem_already_in) {
            errmsg("uthash: Update values error\n");
        }
    }
    pthread_barrier_wait(&threads_barrier_end);
    return NULL;
}

void *uth_check_update_thread_func(void *arg)
{
    int32_t i = 0, find_res, thread_num = (int32_t)(int64_t)arg;
    domain_data_t out;
    char *domain;
    pthread_barrier_wait(&threads_barrier_start);
    for (i = (domains_map_size / thread_count) * thread_num;
         i < (domains_map_size / thread_count) * (thread_num + 1); i++) {
        domain = &domains[domain_offsets[i]];
        find_res = uth_find_elem(domain, &out);
        if (find_res != array_hashmap_elem_finded || out.time != SECOND_TEST_TIME) {
            errmsg("uthash: Check the updated values error\n");
        }
    }
    pthread_barrier_wait(&threads_barrier_end);
    return NULL;
}

void *uth_del_thread_func(void *arg)
{
    int32_t i = 0, del_res, thread_num = (int32_t)(int64_t)arg;
    domain_data_t out;
    char *domain;
    pthread_barrier_wait(&threads_barrier_start);
    for (i = (domains_map_size / thread_count) * thread_num;
         i < (domains_map_size / thread_count) * (thread_num + 1); i++) {
        domain = &domains[domain_offsets[i]];
        del_res = uth_del_elem(domain, &out);
        if (del_res != array_hashmap_elem_deled || out.time != SECOND_TEST_TIME) {
            errmsg("uthash: Delete everything individually error\n");
        }
    }
    pthread_barrier_wait(&threads_barrier_end);
    return NULL;
}

int32_t main(void)
{
    char *domain;

    int32_t is_thread_safety;

    int64_t domains_file_size = 0;
    int64_t processed = 0;

    int32_t i = 0;
    double step = 0;

    domain_data_t add_elem;
    int32_t add_res;

    domain_data_t find_elem;
    int32_t find_res;

    int32_t del_elem_by_func_res;

    struct timeval now_timeval_start;
    struct timeval now_timeval_end;

    uint64_t now_us_start;
    uint64_t now_us_end;

    int32_t time_index = 0;
    int32_t one_op_time_ns[100];

    pthread_t thread;
    void *set_arg;

    int32_t domain_len;
    char sybmol;

    int32_t print_format = 0;
    char *print_data[100];

    int32_t domains_map_size_all = 0;

    size_t mem_base = 0;
    size_t mem_array = 0;

    print_data[0] = "Load %;";
    print_data[1] = "Mem MB;";
    print_data[2] = "Insert;";
    print_data[3] = "Lookup hit;";
    print_data[4] = "Lookup miss;";
    print_data[5] = "Update;";
    print_data[6] = "Verify update;";
    print_data[7] = "Delete each;";
    print_data[8] = "Delete all;";

    srand(time(NULL));

    /* Random domain list generator */
    {
        domains_file_size = DOMAINS_FILE_SIZE_MB * 1024 * 1024;

        domains = malloc(domains_file_size);
        if (domains == NULL) {
            errmsg("No free memory for domains\n");
        }

        domains_random = malloc(domains_file_size);
        if (domains_random == NULL) {
            errmsg("No free memory for domains_random\n");
        }

        processed = 0;
        while (processed < domains_file_size - MAX_DOMAIN_LEN) {
            domain_len = rand() % (MAX_DOMAIN_LEN - MIN_DOMAIN_LEN) + MIN_DOMAIN_LEN;
            for (i = 0; i < domain_len - 1; i++) {
                sybmol = rand() % ('z' - 'a' + 2);
                if (sybmol == ('z' - 'a' + 1)) {
                    domains[processed + i] = '.';
                } else {
                    domains[processed + i] = sybmol + 'a';
                }
            }
            domains[processed + domain_len - 1] = '\n';
            processed += domain_len;
        }

        domains_file_size = processed;
    }
    /* Random domain list generator */

    /* Gen domain_offsets */
    {
        for (i = 0; i < domains_file_size; i++) {
            if (domains[i] == '\n') {
                domains[i] = 0;
                domains_map_size++;
            }
        }

        domains_map_size_all = domains_map_size;

        domain_offsets = (int32_t *)malloc(domains_map_size * sizeof(int32_t));
        domain_offsets[0] = 0;

        for (i = 0; i < domains_map_size - 1; i++) {
            domain_offsets[i + 1] =
                (int32_t)(strchr(&domains[domain_offsets[i] + 1], 0) - domains + 1);
        }

        memcpy(domains_random, domains, (int32_t)domains_file_size);
        for (i = 0; i < domains_map_size; i++) {
            domains_random[domain_offsets[i]] = '&';
        }
    }
    /* Gen domain_offsets */

    /* Check is_thread_safet */
    {
        domains_map_struct = array_hashmap_init(domains_map_size, 1.0, sizeof(domain_data_t));
        if (domains_map_struct == NULL) {
            errmsg("Init error\n");
        }

        is_thread_safety = array_hashmap_is_thread_safety(domains_map_struct);

        array_hashmap_del(&domains_map_struct);

        if (!is_thread_safety) {
            errmsg("Need thread safety version\n");
        }
    }
    /* Check is_thread_safet */

    for (thread_count = 1; thread_count <= 8; thread_count++) {
        domains_map_size = domains_map_size_all - domains_map_size_all % thread_count;
        printf("Domains count: %d\n", domains_map_size);
        printf("Threads count: %d\n", thread_count);
        printf("\n");

        printf("array_hashmap\n");
        for (i = 0; i < 9; i++) {
            printf("%s", print_data[i]);
        }
        printf("\n");

        for (step = 1.00; step > 0.95; step -= 0.01) {
            time_index = 0;

            /* Get memory usage */
            heap_trim();
            mem_base = rss_bytes();
            /* Get memory usage */

            /* Init */
            domains_map_struct =
                array_hashmap_init(domains_map_size / step, 1.0, sizeof(domain_data_t));
            if (domains_map_struct == NULL) {
                errmsg("array_hashmap: Init error\n");
            }

            array_hashmap_set_func(domains_map_struct, domain_add_hash, domain_add_cmp,
                                   domain_find_hash, domain_find_cmp, domain_find_hash,
                                   domain_find_cmp);
            /* Init */

            /* Add values */
            RUN_THREAD(add);
            /* Add values */

            /* Get memory usage */
            mem_array = rss_bytes() - mem_base;
            /* Get memory usage */

            /* Check that all values are inserted */
            RUN_THREAD(find);
            /* Check that all values are inserted */

            /* Check that there are no non-inserted elements */
            RUN_THREAD(no_find);
            /* Check that there are no non-inserted elements */

            /* Update values */
            RUN_THREAD(update);
            /* Update values */

            /* Check the updated values */
            RUN_THREAD(check_update);
            /* Check the updated values */

            /* Delete everything individually */
            RUN_THREAD(del);
            /* Delete everything individually */

            /* Check that everything is deleted */
            for (i = 0; i < domains_map_size; i++) {
                domain = &domains[domain_offsets[i]];
                find_res = array_hashmap_find_elem(domains_map_struct, domain, &find_elem);
                if (find_res != array_hashmap_elem_not_finded) {
                    errmsg("array_hashmap: Check that everything is deleted error\n");
                }
            }
            for (i = 0; i < domains_map_size; i++) {
                domain = &domains_random[domain_offsets[i]];
                find_res = array_hashmap_find_elem(domains_map_struct, domain, &find_elem);
                if (find_res != array_hashmap_elem_not_finded) {
                    errmsg("array_hashmap: Check that everything is deleted error\n");
                }
            }
            if (array_hashmap_now_in_map(domains_map_struct) != 0) {
                errmsg("array_hashmap: Check that everything is deleted error\n");
            }
            /* Check that everything is deleted */

            /* Add values */
            for (i = 0; i < domains_map_size; i++) {
                add_elem.domain_pos = domain_offsets[i];
                add_elem.time = SECOND_TEST_TIME;

                add_res = array_hashmap_add_elem(domains_map_struct, &add_elem, NULL,
                                                 array_hashmap_save_old_func);
                if (add_res != array_hashmap_elem_added) {
                    errmsg("array_hashmap: Add values error\n");
                }
            }
            /* Add values */

            /* Delete everything at once */
            TIMER_START();
            del_elem_by_func_res =
                array_hashmap_del_elem_by_func(domains_map_struct, domain_del_func);
            if (del_elem_by_func_res != domains_map_size) {
                errmsg("array_hashmap: Delete everything at once error\n");
            }
            TIMER_END();
            /* Delete everything at once */

            /* Check that everything is deleted */
            for (i = 0; i < domains_map_size; i++) {
                domain = &domains[domain_offsets[i]];
                find_res = array_hashmap_find_elem(domains_map_struct, domain, &find_elem);
                if (find_res != array_hashmap_elem_not_finded) {
                    errmsg("array_hashmap: Check that everything is deleted error\n");
                }
            }
            for (i = 0; i < domains_map_size; i++) {
                domain = &domains_random[domain_offsets[i]];
                find_res = array_hashmap_find_elem(domains_map_struct, domain, &find_elem);
                if (find_res != array_hashmap_elem_not_finded) {
                    errmsg("array_hashmap: Check that everything is deleted error\n");
                }
            }
            if (array_hashmap_now_in_map(domains_map_struct) != 0) {
                errmsg("array_hashmap: Check that everything is deleted error\n");
            }
            /* Check that everything is deleted */

            /* Destroy */
            array_hashmap_del(&domains_map_struct);
            /* Destroy */

            /* Time statistics*/
            print_format = (int32_t)(strlen(print_data[0]) - 1);
            printf("%*d;", print_format, (int32_t)(step * 100));
            print_format = (int32_t)(strlen(print_data[1]) - 1);
            printf("%*.*f;", print_format, 2, (double)mem_array / (1024.0 * 1024.0));
            for (i = 0; i < time_index; i++) {
                print_format = (int32_t)(strlen(print_data[i + 2]) - 1);
                printf("%*d;", print_format, one_op_time_ns[i]);
            }
            printf("\n");
            fflush(stdout);
            /* Time statistics*/
        }

        printf("uthash\n");
        for (i = 0; i < 9; i++) {
            printf("%s", print_data[i]);
        }
        printf("\n");

        for (step = 1.00; step > 0.95; step -= 0.01) {
            time_index = 0;

            /* Get memory usage */
            heap_trim();
            mem_base = rss_bytes();
            /* Get memory usage */

            /* Init */
            uth_init();
            /* Init */

            /* Add values */
            RUN_THREAD(uth_add);
            /* Add values */

            /* Get memory usage */
            mem_array = rss_bytes() - mem_base;
            /* Get memory usage */

            /* Check that all values are inserted */
            RUN_THREAD(uth_find);
            /* Check that all values are inserted */

            /* Check that there are no non-inserted elements */
            RUN_THREAD(uth_no_find);
            /* Check that there are no non-inserted elements */

            /* Update values */
            RUN_THREAD(uth_update);
            /* Update values */

            /* Check the updated values */
            RUN_THREAD(uth_check_update);
            /* Check the updated values */

            /* Delete everything individually */
            RUN_THREAD(uth_del);
            /* Delete everything individually */

            /* Check that everything is deleted */
            for (i = 0; i < domains_map_size; i++) {
                domain = &domains[domain_offsets[i]];
                if (uth_find_elem(domain, &find_elem) != array_hashmap_elem_not_finded) {
                    errmsg("uthash: Check that everything is deleted error\n");
                }
            }
            for (i = 0; i < domains_map_size; i++) {
                domain = &domains_random[domain_offsets[i]];
                if (uth_find_elem(domain, &find_elem) != array_hashmap_elem_not_finded) {
                    errmsg("uthash: Check that everything is deleted error\n");
                }
            }
            if (uth_now_in_map() != 0) {
                errmsg("uthash: Check that everything is deleted error\n");
            }
            /* Check that everything is deleted */

            /* Add values */
            for (i = 0; i < domains_map_size; i++) {
                add_elem.domain_pos = (uint32_t)domain_offsets[i];
                add_elem.time = SECOND_TEST_TIME;

                if (uth_add_elem(&add_elem, array_hashmap_save_old_func) !=
                    array_hashmap_elem_added) {
                    errmsg("uthash: Add values error\n");
                }
            }
            /* Add values */

            /* Delete everything at once */
            TIMER_START();
            del_elem_by_func_res = uth_del_by_func(domain_del_func);
            if (del_elem_by_func_res != domains_map_size) {
                errmsg("uthash: Delete everything at once error\n");
            }
            TIMER_END();
            /* Delete everything at once */

            /* Check that everything is deleted */
            for (i = 0; i < domains_map_size; i++) {
                domain = &domains[domain_offsets[i]];
                if (uth_find_elem(domain, &find_elem) != array_hashmap_elem_not_finded) {
                    errmsg("uthash: Check that everything is deleted error\n");
                }
            }
            for (i = 0; i < domains_map_size; i++) {
                domain = &domains_random[domain_offsets[i]];
                if (uth_find_elem(domain, &find_elem) != array_hashmap_elem_not_finded) {
                    errmsg("uthash: Check that everything is deleted error\n");
                }
            }
            if (uth_now_in_map() != 0) {
                errmsg("uthash: Check that everything is deleted error\n");
            }
            /* Check that everything is deleted */

            /* Destroy */
            uth_destroy();
            /* Destroy */

            /* Time statistics*/
            print_format = (int32_t)(strlen(print_data[0]) - 1);
            printf("%*d;", print_format, (int32_t)(step * 100));
            print_format = (int32_t)(strlen(print_data[1]) - 1);
            printf("%*.*f;", print_format, 2, (double)mem_array / (1024.0 * 1024.0));
            for (i = 0; i < time_index; i++) {
                print_format = (int32_t)(strlen(print_data[i + 2]) - 1);
                printf("%*d;", print_format, one_op_time_ns[i]);
            }
            printf("\n");
            fflush(stdout);
            /* Time statistics*/
        }

        printf("\n");
    }

    free(domains);
    free(domains_random);

    printf("Success\n");
    return EXIT_SUCCESS;
}
