#include "buddy_malloc_2.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char const *argv[])
{
    void *alloced[100];
    size_t alloced_count = 0;

    const char *cur_arg = argv[1];
    size_t cur_arg_i = 1;
    while (strcmp(cur_arg, "end") != 0) {
        const char action = cur_arg[0];
        if (action == 'a') {
            size_t alloc_size;
            sscanf(cur_arg, "a_%lu", &alloc_size);
            void *p = malloc_2(alloc_size);
            alloced[alloced_count] = p;
            alloced_count += 1;
        }
        else if (action == 'f') {
            size_t free_i;
            sscanf(cur_arg, "f_%lu", &free_i);
            void *p = alloced[free_i];
            free_2(p);
        }
        cur_arg_i += 1;
        cur_arg = argv[cur_arg_i];
        buddy_debug_print_tree_2();
    }
    // buddy_debug_print_tree_2();
    // void *test1 = malloc_2(100);
    // buddy_debug_print_tree_2();
    // printf("______________\n");
    //
    // void *test = malloc_2(1);
    // buddy_debug_print_tree_2();
    // printf("______________\n");

    // test = malloc_2(1);
    // buddy_debug_print_tree_2();
    // printf("______________\n");

    // test = malloc_2(1);
    // buddy_debug_print_tree_2();
    // printf("______________\n");

    // free_2(test1);
    // free_2(test);
    // buddy_debug_print_tree_2();

    return 0;
}
