#ifndef OSC_TOUCH_HANDLER_H
#define OSC_TOUCH_HANDLER_H

#include "wacom.h"

#define TOUCH_SENSITIVITY 1
#define DATA_TOUCH_OFFSET 9
#define MAX_FINGERS_CNT 2

int irq_touch(struct tablet *tablet);

struct mt_info {
    int x;
    int y;
};

#endif //OSC_TOUCH_HANDLER_H
