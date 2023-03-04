#ifndef OSC_PEN_HANDLER_H
#define OSC_PEN_HANDLER_H

#include "wacom.h"

int irq_pen(struct tablet *tablet, struct urb* urb);

#endif //OSC_PEN_HANDLER_H
