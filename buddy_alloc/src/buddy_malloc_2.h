#ifndef BUDDY_MALLOC_H
#define BUDDY_MALLOC_H

#include <stdlib.h>

void *malloc_2(size_t request);
void free_2(void *ptr);
void buddy_debug_print_tree_2(void);

#endif
