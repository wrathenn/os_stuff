#ifndef OSC_WACOM_H
#define OSC_WACOM_H

#include<linux/types.h>
#include<linux/workqueue.h>

#define DRIVER_NAME "Wacom Touch Tablet Driver"

/* device IDs */
#define STYLUS_DEVICE_ID	0x02
#define TOUCH_DEVICE_ID		0x03
#define CURSOR_DEVICE_ID	0x06
#define ERASER_DEVICE_ID	0x0A
#define PAD_DEVICE_ID		0x0F

#define ID_VENDOR_TABLET  0x056a /* Wacom Co. */
#define ID_PRODUCT_TABLET 0x00D1 /* cth-460 */
#define BAMBOO_PT 41
#define WACOM_INTUOS_RES 100
#define WACOM_PKGLEN_BBFUN 9
#define USB_PACKET_LEN  20

struct wacom_features {
    const char *name;
    int pktlen;
    int x_max;
    int y_max;
    int pressure_max;
    int distance_max;
    int type;
    int x_resolution;
    int y_resolution;
    int touch_max;
    int device_type;
};

struct tablet {
    dma_addr_t         data_dma;
    struct urb        *urb;
    struct usb_device *usb_dev;
    struct usb_interface *intf;
    struct input_dev  *input_dev;
    unsigned char     *data;
    char               phys[32];
    struct wacom_features features;

    bool reporting_data; // Should tablet actions create input signals?
    int id[2];
    int tool[2];
};

struct container_urb {
    struct urb *urb;
    struct work_struct work;
};

// Functions
int setup_input_dev(
    struct tablet *tablet,
    struct wacom_features *features,
    int (*open_func)(struct input_dev *dev),
    void (*close_func)(struct input_dev *dev)
);

int wacom_set_report(
    struct usb_device *usb_dev,
    struct usb_interface *usb_intf
);

#endif //OSC_WACOM_H
