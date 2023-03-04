#include <linux/input.h>
#include <linux/usb/input.h>
#include <linux/slab.h>
#include <linux/input/mt.h>
#include "wacom.h"

static int setup_input_dev_capabilities(struct input_dev *input_dev, struct wacom_features *features) {
    set_bit(EV_KEY, input_dev->evbit);

    set_bit(BTN_TOUCH, input_dev->keybit);

    if (features->device_type == BTN_TOOL_PEN) {
        set_bit(ABS_MISC, input_dev->absbit);
        set_bit(EV_ABS, input_dev->evbit);
        input_set_abs_params(input_dev, ABS_X, 0,
                             features->x_max,
                             4, 0);
        input_set_abs_params(input_dev, ABS_Y, 0,
                             features->y_max,
                             4, 0);
        input_set_abs_params(input_dev, ABS_PRESSURE, 0,
                             features->pressure_max, 0, 0);

        /* penabled devices have fixed resolution for each model */
        input_abs_set_res(input_dev, ABS_X, features->x_resolution);
        input_abs_set_res(input_dev, ABS_Y, features->y_resolution);

        set_bit(INPUT_PROP_POINTER, input_dev->propbit);
        set_bit(BTN_TOOL_RUBBER, input_dev->keybit);
        set_bit(BTN_TOOL_PEN, input_dev->keybit);
        set_bit(BTN_STYLUS, input_dev->keybit);
        set_bit(BTN_STYLUS2, input_dev->keybit);
        input_set_abs_params(input_dev, ABS_DISTANCE, 0,
                             features->distance_max,
                             1, 0);
    } else if (features->device_type == BTN_TOOL_FINGER) {
        set_bit(EV_REL, input_dev->evbit);
        set_bit(REL_WHEEL, input_dev->relbit);
        set_bit(REL_HWHEEL, input_dev->relbit);
        set_bit(REL_X, input_dev->relbit);
        set_bit(REL_Y, input_dev->relbit);

        set_bit(BTN_LEFT, input_dev->keybit);
        set_bit(BTN_RIGHT, input_dev->keybit);
        set_bit(BTN_MIDDLE, input_dev->keybit);
        set_bit(BTN_FORWARD, input_dev->keybit);
        set_bit(BTN_BACK, input_dev->keybit);
        set_bit(KEY_ZOOMIN, input_dev->keybit);
        set_bit(KEY_ZOOMOUT, input_dev->keybit);
    }

    set_bit(BTN_MOUSE, input_dev->keybit);
    return 0;
}

int setup_input_dev(
    struct tablet *tablet,
    struct wacom_features *features,
    int (*open_func)(struct input_dev *dev),
    void (*close_func)(struct input_dev *dev)
) {
    struct input_dev *input_dev;
    struct usb_interface *intf = tablet->intf;
    struct usb_device *usbdev = interface_to_usbdev(intf);
    int error = -ENOMEM;

    input_dev = input_allocate_device();
    if (!input_dev) {
        printk(KERN_ERR"%s: Error allocating input device in %s (line %d)", DRIVER_NAME, __func__, __LINE__);
        return error;
    }

    input_dev->name = "Virtual Mouse Device";
//    input_dev->dev.parent = &intf->dev;
    // https://elixir.bootlin.com/linux/latest/source/include/linux/input.h#L137
    // Функция, которой драйвер подготавливает устройство для генерации событий
    input_dev->open = open_func;
    input_dev->close = close_func;
    // https://elixir.bootlin.com/linux/latest/source/include/linux/usb/input.h#L14
    // Заполнить struct input_id на основе struct usb_device
    usb_to_input_id(usbdev, &input_dev->id);
    // Установить указатель на выделенную память под информацию от устройства
    input_set_drvdata(input_dev, tablet);

    tablet->input_dev = input_dev;

    error = setup_input_dev_capabilities(input_dev, features);
    if (error) {
        printk(KERN_ERR"%s: Error while setting input device capabilities ib %s (line %d)", DRIVER_NAME, __func__, __LINE__);
        return error;
    }

    error = input_register_device(input_dev);
    if (error) {
        input_free_device(input_dev);
        printk(KERN_ERR"%s: Error while registering device in %s (line %d)", DRIVER_NAME, __func__, __LINE__);
        return error;
    }

    return 0;
}

int wacom_set_report(
    struct usb_device *usb_dev,
    struct usb_interface *usb_intf
) {
    unsigned char *rep_data;
    int len = 2;
    int report_id = 2;
    int mode = 2;
    int error = 0;

    rep_data = kzalloc(len, GFP_KERNEL);
    if (!rep_data) {
        printk(KERN_ERR"%s: Error in allocating rep_data memory in function %s (%d)", DRIVER_NAME, __func__, __LINE__);
        return -ENOMEM;
    }

    rep_data[0] = report_id;
    rep_data[1] = mode;
    error = usb_control_msg(
        usb_dev,
        usb_sndctrlpipe(usb_dev, 0),
        0x09, // SET_REPORT
        USB_TYPE_CLASS | USB_RECIP_INTERFACE,
        (0x03 << 8) + report_id,
        usb_intf->altsetting[0].desc.bInterfaceNumber,
        rep_data,
        len,
        1000
    );

    if (error < 0) {
        printk(KERN_ERR"%s: Error in usb_control_msg in function %s (%d), error code is %d", DRIVER_NAME, __func__, __LINE__, error);
        kfree(rep_data);
        return error;
    }

    kfree(rep_data);
    return 0;
}
