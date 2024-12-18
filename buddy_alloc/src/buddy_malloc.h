#ifndef BUDDY_MALLOC_H
#define BUDDY_MALLOC_H

#include <stdlib.h>

void *malloc_1(size_t request);
void free_1(void *ptr);
void buddy_debug_print_tree(void);

#endif
