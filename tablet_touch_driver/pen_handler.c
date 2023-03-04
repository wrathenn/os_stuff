#include <linux/input.h>
#include "pen_handler.h"

int irq_pen(struct tablet *tablet, struct urb* urb) {
    int x = 0, y = 0;
    int pressure = 0, distance = 0;
    unsigned char pen = 0, btn1 = 0, btn2 = 0;
    unsigned char range, proximity, ready;
    unsigned char *data = tablet->data;

    struct wacom_features *wacom_features = &tablet->features;
    struct input_dev *input = tablet->input_dev;

    range = (data[1] & 0x80) == 0x80;
    proximity = (data[1] & 0x40) == 0x40;
    ready = (data[1] & 0x20) == 0x20;

    if (ready) {
        pressure = le16_to_cpup((__le16 *)&data[6]);
        pen = data[1] & 0x01;
        btn1 = data[1] & 0x02;
        btn2 = data[1] & 0x04;
    }

    if (proximity) {
        x = le16_to_cpup((__le16 *)&data[2]);
        y = le16_to_cpup((__le16 *)&data[4]);

        if (data[1] & 0x08) {
            tablet->tool[0] = BTN_TOOL_RUBBER;
            tablet->id[0] = ERASER_DEVICE_ID;
        } else {
            tablet->tool[0] = BTN_TOOL_PEN;
            tablet->id[0] = STYLUS_DEVICE_ID;
        }
        tablet->reporting_data = true;
    }
    if (range) {
        if (data[8] <= wacom_features->distance_max)
            distance = wacom_features->distance_max - data[8];
    } else {
        tablet->id[0] = 0;
    }

    if (tablet->reporting_data) {
        input_report_key(input, BTN_TOUCH, pen);
        input_report_key(input, BTN_STYLUS, btn1);
        input_report_key(input, BTN_STYLUS2, btn2);

        if (proximity || !range) {
            input_report_abs(input, ABS_X, x);
            input_report_abs(input, ABS_Y, y);
        }
        input_report_abs(input, ABS_PRESSURE, pressure);
        input_report_abs(input, ABS_DISTANCE, distance);

//        input_report_key(input, tablet->tool[0], range); /* PEN or RUBBER */
        input_report_abs(input, ABS_MISC, tablet->id[0]); /* TOOL ID */
    }

    if (!range) {
        tablet->reporting_data = false;
    }
    return 1;
}
