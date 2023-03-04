#include <linux/slab.h>
#include "debug.h"
#include "wacom.h"

void printkbyte(int b, int size)
{
    int spaces = (size - 1) / 4;
    char *buf = kmalloc(size + spaces + 1, GFP_ATOMIC);

    int ii = size;
    int i = size + spaces;
    while (i > 0) {
        buf[--i] = '0' + (b & 1);
        ii--;
        if ((size - ii) % 4 == 0 && ii != 0)
            buf[--i] = ' ';
        b = b >> 1;
    }

    printk("%s: %s\n", DRIVER_NAME, buf);
    kfree(buf);
}
