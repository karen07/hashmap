#include "array_hashmap.h"
#include <stdio.h>
#include <unistd.h>

uint32_t djb33_hash(const char* s)
{
    uint32_t h = 5381;
    while (*s) {
        h += (h << 5);
        h ^= *s++;
    }
    return h;
}

char* urls;

typedef struct url_data {
    uint32_t url_pos;
    int32_t time;
} url_data_t;

uint32_t add_url_hash(const void* void_elem)
{
    const url_data_t* elem = void_elem;
    return djb33_hash(&urls[elem->url_pos]);
}

int32_t add_url_cmp(const void* void_elem1, const void* void_elem2)
{
    const url_data_t* elem1 = void_elem1;
    const url_data_t* elem2 = void_elem2;

    return !strcmp(&urls[elem1->url_pos], &urls[elem2->url_pos]);
}

uint32_t find_url_hash(const void* void_elem)
{
    const char* elem = void_elem;
    return djb33_hash(elem);
}

int32_t find_url_cmp(const void* void_elem1, const void* void_elem2)
{
    const char* elem1 = void_elem1;
    const url_data_t* elem2 = void_elem2;

    return !strcmp(elem1, &urls[elem2->url_pos]);
}

int32_t url_decide(const void* void_elem)
{
    if (!void_elem) {
        return 0;
    }

    return 1;
}

int32_t url_on_collision(const void* void_elem1, const void* void_elem2)
{
    const url_data_t* elem1 = void_elem1;
    const url_data_t* elem2 = void_elem2;

    if (elem1->time > elem2->time) {
        return 1;
    } else {
        return 0;
    }
}

int main(void)
{
    FILE* urls_fd = fopen("block_urls_lines", "r");
    if (urls_fd == NULL) {
        system("curl https://antifilter.download/list/domains.lst 2>/dev/null > block_urls_lines");
        urls_fd = fopen("block_urls_lines", "r");
        if (urls_fd == NULL) {
            printf("Can't open url file\n");
            exit(EXIT_FAILURE);
        }
    }

    fseek(urls_fd, 0, SEEK_END);
    int64_t urls_file_size = ftell(urls_fd);
    fseek(urls_fd, 0, SEEK_SET);

    if (urls_file_size > 0) {
        if (urls) {
            free(urls);
        }

        urls = malloc(urls_file_size);
        if (urls == 0) {
            printf("No free memory for urls\n");
            exit(EXIT_FAILURE);
        }
        if (fread(urls, urls_file_size, 1, urls_fd) != 1) {
            printf("Can't read url file\n");
            exit(EXIT_FAILURE);
        }

        int32_t urls_map_size = 0;
        for (int32_t i = 0; i < urls_file_size; i++) {
            if (urls[i] == '\n') {
                urls[i] = 0;
                urls_map_size++;
            }
        }

        printf("URLs count %d\n", urls_map_size);

        const array_hashmap_t* urls_map_struct = init_array_hashmap(urls_map_size / 0.8, 0.7, sizeof(url_data_t));
        array_hashmap_set_func(urls_map_struct, add_url_hash, add_url_cmp, find_url_hash, find_url_cmp);

        int32_t url_offset = 0;
        for (int32_t i = 0; i < urls_map_size; i++) {
            url_data_t add_elem;
            add_elem.url_pos = url_offset;
            add_elem.time = i;

            array_hashmap_add_elem(urls_map_struct, &add_elem, NULL, NULL);

            url_offset = strchr(&urls[url_offset + 1], 0) - urls + 1;
        }

        printf("\nMap size %d\n", array_hashmap_get_size(urls_map_struct));

        url_data_t find_elem;
        int32_t find_res;

        printf("\nFind instagram.com\n");
        find_res = array_hashmap_find_elem(urls_map_struct, "instagram.com", &find_elem);
        if (find_res) {
            printf("pos %d, str %s, time %d\n", find_elem.url_pos, &urls[find_elem.url_pos], find_elem.time);
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
            printf("pos %d, str %s, time %d\n", find_elem.url_pos, &urls[find_elem.url_pos], find_elem.time);
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
            printf("pos %d, str %s, time %d\n", find_elem.url_pos, &urls[find_elem.url_pos], find_elem.time);
        } else {
            printf("instagram.com not in map\n");
        }

        const array_hashmap_iter_t* iter;
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
        array_hashmap_del_iter(iter);

        del_array_hashmap(urls_map_struct);
        fflush(stdout);
    }

    free(urls);
    fclose(urls_fd);

    return 0;
}
