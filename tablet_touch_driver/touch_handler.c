#include <asm-generic/unaligned.h>
#include <linux/input.h>
#include <linux/slab.h>
#include "touch_handler.h"

// How much fingers are active based on coordinates parsed
static int mt_info_finger_cnt(struct mt_info *mt_info_arr, int len) {
    int res = 0;
    int i;
    struct mt_info *cur;
    for (i = 0; i < len; i++) {
        cur = mt_info_arr + i;
        if (cur->x != -1 && cur->y != -1)
            res++;
    }
    return res;
}

static int mt_info_rel_x(struct mt_info *prev, struct mt_info *cur) {
    if (prev->x == -1 || cur->x == -1)
        return -1;
    return cur->x - prev->x;
}

static int mt_info_rel_y(struct mt_info *prev, struct mt_info *cur) {
    if (prev->y == -1 || cur->y == -1)
        return -1;
    return cur->y - prev->y;
}

static void mt_info_assign(struct mt_info *dst, struct mt_info *src, int cnt) {
    int i;
    for (i = 0; i < cnt; i++) {
        dst[i] = src[i];
    }
}

static void mt_info_init(struct mt_info *mt_info, int cnt) {
    int i;
    for (i = 0; i < cnt; ++i) {
        mt_info[i].x = -1;
        mt_info[i].y = -1;
    }
}

static bool mt_info_any_not_init(const struct mt_info *mt_info, int cnt) {
    int i;
    const struct mt_info *cur;
    for (i = 0; i < cnt; i++) {
        cur = mt_info + i;
        if (cur->x == -1 && cur->y == -1) {
            return true;
        }
    }
    return false;
}

static void parse_touch(const unsigned char *data, struct mt_info *mt_info) {
    bool is_touch = data[3] & 0b10000000; // 4th byte is 1000_000X if touch
    if (is_touch) {
        mt_info->x = get_unaligned_be16(&data[3]) & 0x7ff; // & 0b0000_0111_1111_1111
        mt_info->y = get_unaligned_be16(&data[5]) & 0x7ff; // & 0b0000_0111_1111_1111
    } else {
        mt_info->x = -1;
        mt_info->y = -1;
    }
}

static void button_handler(
    struct input_dev *input,
    const unsigned char *data
) {
    unsigned char button_data = data[1];
    static bool lmb /* left */ = 0, rmb /* right */ = 0, cmb /* wheel click */, bmb /* back */ = 0, fmb /* forward */ = 0;

    if (button_data == 0b00000000) {
        if (lmb) {
            printk("%s: Left unclick", DRIVER_NAME);
            input_report_key(input, BTN_LEFT, 0);
            lmb = 0;
        }
        if (rmb) {
            printk("%s: Right unclick", DRIVER_NAME);
            input_report_key(input, BTN_RIGHT, 0);
            rmb = 0;
        }
        if (cmb) {
            printk("%s: Middle unclick", DRIVER_NAME);
            input_report_key(input, BTN_MIDDLE, 0);
            cmb = 0;
        }
        if (bmb) {
            printk("%s: Back unclick", DRIVER_NAME);
            input_report_key(input, BTN_BACK, 0);
            bmb = 0;
        }
        if (fmb) {
            printk("%s: Forward unclick", DRIVER_NAME);
            input_report_key(input, BTN_FORWARD, 0);
            fmb = 0;
        }
    }
    if (button_data == 0b00000011 && !cmb) {
        cmb = 1;
        input_report_key(input, BTN_MIDDLE, 1);
        printk("%s: Middle click", DRIVER_NAME);
    } else if (button_data == 0b00000001 && !lmb) {
        lmb = 1;
        input_report_key(input, BTN_LEFT, 1);
        printk("%s: Left click", DRIVER_NAME);
    } else if (button_data == 0b00000010 && !rmb) {
        rmb = 1;
        input_report_key(input, BTN_RIGHT, 1);
        printk("%s: Right click", DRIVER_NAME);
    }

    if (button_data == 0b00000100 && !fmb) {
        fmb = 1;
        input_report_key(input, BTN_FORWARD, 1);
        printk("%s: Forward click", DRIVER_NAME);
    }
    if (button_data == 0b00001000 && !bmb) {
        bmb = 1;
        input_report_key(input, BTN_BACK, 1);
        printk("%s: Back click", DRIVER_NAME);
    }
}

static void one_finger_handler(
    struct input_dev *input,
    const unsigned char *data,
    struct mt_info* cur_mt_info,
    struct mt_info* prev_mt_info,
    int max_fingers_cnt
) {
//    printk("%s: Current touch is %d - %d", cur_mt_info->x, cur_mt_info->y);
    int mov_x = mt_info_rel_x(prev_mt_info, cur_mt_info);
    int mov_y = mt_info_rel_y(prev_mt_info, cur_mt_info);
    input_report_rel(input, REL_X, mov_x * TOUCH_SENSITIVITY);
    input_report_rel(input, REL_Y, mov_y * TOUCH_SENSITIVITY);

    if ((data[2] & 0b11110000) == 0b11110000) {
        printk(KERN_INFO"%s: Pressed hard enough for wheel click", DRIVER_NAME);
        input_report_key(input, BTN_MIDDLE, 1);
    } else {
        input_report_key(input, BTN_MIDDLE, 0);
    }
    mt_info_assign(prev_mt_info, cur_mt_info, max_fingers_cnt);
}

static void two_fingers_handler(
    struct input_dev *input,
    const unsigned char *data,
    struct mt_info* cur_mt_info,
    struct mt_info* prev_mt_info,
    int max_fingers_cnt
) {
    int dx1, dy1, dx2, dy2, abs_x, abs_y;
    int dxm, dym;
    struct mt_info *cur_f1 = cur_mt_info, *cur_f2 = cur_mt_info + 1,
                   *prev_f1 = prev_mt_info, *prev_f2 = prev_mt_info + 1;

    // If previous touch was not double click
    if (mt_info_any_not_init(prev_mt_info, max_fingers_cnt)) {
        mt_info_assign(prev_mt_info, cur_mt_info, max_fingers_cnt);
        return;
    }

    dx1 = prev_f1->x - cur_f1->x;
    dy1 = prev_f1->y - cur_f1->y;
    dx2 = prev_f2->x - cur_f2->x;
    dy2 = prev_f2->y - cur_f2->y;

    abs_x = abs(dx1 - dx2);
    abs_y = abs(dy1 - dy2);
    if (abs_x < 2 && abs_y < 2) {
        dxm = (dx1 + dx2) / 2;
        dym = (dy1 + dy2) / 2;
        // Scroll
        if (abs(dxm) < 2 && abs(dym) > 2) {
            printk(KERN_INFO"%s: Vertical Scroll for %d", DRIVER_NAME, -dym);
            input_report_rel(input, REL_WHEEL, -dym);
        } else if (abs(dxm) > 2 && abs(dym) < 2) {
            printk(KERN_INFO"%s: Horizontal Scroll for %d", DRIVER_NAME, dxm);
            input_report_rel(input, REL_HWHEEL, dxm);
        }
    }
    mt_info_assign(prev_mt_info, cur_mt_info, max_fingers_cnt);
}

int irq_touch(struct tablet *tablet) {
    int i;
    int finger_cnt;
    int max_finger_cnt = tablet->features.touch_max;
    static struct mt_info mt_info[MAX_FINGERS_CNT] = {{-1, -1}, {-1, -1}};
    static struct mt_info new_mt_info[MAX_FINGERS_CNT];

    struct input_dev *input = tablet->input_dev;
    unsigned char *data = tablet->data;

    if (data[0] != 0b00000010) {
        printk("%s: Not touch event in %s (line %d)", DRIVER_NAME, __func__, __LINE__);
        return 0;
    }

    for (i = 0; i < max_finger_cnt; i++) {
        parse_touch(data + i * DATA_TOUCH_OFFSET, new_mt_info + i);
    }
    finger_cnt = mt_info_finger_cnt(new_mt_info, 2);

    switch (finger_cnt) {
        case 0:
            mt_info_init(mt_info, max_finger_cnt);
            break;
        case 1:
            one_finger_handler(input, data, new_mt_info, mt_info, max_finger_cnt);
            break;
        case 2:
            two_fingers_handler(input, data, new_mt_info, mt_info, max_finger_cnt);
            break;
        default:
            return 0;
    }
    button_handler(input, data);

    return 1;
}
