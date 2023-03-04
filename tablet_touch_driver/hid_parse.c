#include "hid_parse.h"

#include <linux/slab.h>
#include <linux/input.h>
#include <asm-generic/unaligned.h>

#ifndef fallthrough
#  if defined __has_attribute
#    if __has_attribute(__fallthrough__)
#      define fallthrough                    __attribute__((__fallthrough__))
#    endif
#  endif
#endif
#ifndef fallthrough
#  define fallthrough                    do {} while (0)  /* fallthrough */
#endif

static int parse_hid(
    struct usb_interface *intf,
    struct hid_descriptor *hid_desc,
    struct wacom_features *features
) {
    struct usb_device *usb_dev = interface_to_usbdev(intf);
    unsigned char *report;
    int result = 0;
    int i = 0;
    int page = 0;
    int finger = 0;
    int pen = 0;

    report = kzalloc(hid_desc->w_descriptor_length, GFP_KERNEL);
    if (!report) {
        return -ENOMEM;
    }

    result = usb_control_msg(
        usb_dev,
        usb_rcvctrlpipe(usb_dev, 0),
        USB_REQ_GET_DESCRIPTOR,
        USB_RECIP_INTERFACE | USB_DIR_IN,
        HID_DEVICET_REPORT << 8,
        intf->altsetting[0].desc.bInterfaceNumber, /* interface */
        report,
        hid_desc->w_descriptor_length,
        5000 // 5 secs
    );
    if (result < 0) {
        printk("%s: No need to parse hid descriptor", DRIVER_NAME);
        kfree(report);
        return 0;
    }

    for (i = 0; i < hid_desc->w_descriptor_length; i++) {
        int item = report[i] & 0xFC;
        int len = report[i] & 0x03;
        int data = 0;

        switch (len) {
            case 3:
                len = 4;
                data |= (report[i+4] << 24);
                data |= (report[i+3] << 16);
                fallthrough;
            case 2:
                data |= (report[i+2] << 8);
                fallthrough;
            case 1:
                data |= (report[i+1]);
                break;
            default:
                break;
        }

        switch (item) {
            case HID_USAGE_PAGE:
//                printk("IN CASE HID_USAGE_PAGE");
                page = data;
                break;
            case HID_USAGE:
//                printk("IN CASE HID_USAGE");
                if (len < 4) {
                    data |= (page << 16);
                }

                switch (data) {
                    case HID_USAGE_WT_X:
//                        printk("IM IN CASE HID_USAGE_WT_X");
                        break;
                    case HID_USAGE_WT_Y:
//                        printk("IM IN CASE HID_USAGE_WT_Y");
                        break;
                    case HID_USAGE_X:
//                        printk("IM IN CASE HID_USAGE_X");
                        if (finger) {
                            features->device_type = BTN_TOOL_FINGER;
                            features->pktlen = 20;
                            features->x_max =
                                    get_unaligned_le16(&report[i + 8]);
                        } else if (pen) {
                            features->device_type = BTN_TOOL_PEN;
                        }
                        break;
                    case HID_USAGE_Y:
//                        printk("IN HID_USAGE_Y");
                        if (finger) {
                            features->y_max =
                                    get_unaligned_le16(&report[i + 6]);
                        } else if (pen) {
                            features->y_max =
                                    get_unaligned_le16(&report[i + 3]);
                        }
                        break;
                    case HID_USAGE_WT_FINGER:
//                        printk("IN HID_USAGE_WT_FINGER");
                    case HID_USAGE_FINGER:
//                        printk("IN HID_USAGE_FINGER");
                        finger = 1;
                        break;
                    case HID_USAGE_WT_STYLUS:
//                        printk("IN HID_USAGE_WT_STYLUS");
                    case HID_USAGE_STYLUS:
//                        printk("IN HID_USAGE_STYLUS");
                        pen = 1;
                        break;

                    case HID_USAGE_CONTACTMAX:
//                        printk("IN HID_USAGE_CONTACTMAX");
                        break;
                    case HID_USAGE_PRESSURE:
//                        printk("IN HID_USAGE_PRESSURE");
                        break;
                }
                break;
            case HID_COLLECTION_END:
//                printk("IN HID_COLLECTION_END");
                finger = page = 0;
                break;

            case HID_COLLECTION:
//                printk("IN HID_COLLECTION");
                break;
            case HID_LONGITEM:
//                printk("IN HID_LONGITEM");
                break;
        }
        i += len;
    }

    result = 0;
    kfree(report);
    return result;
}

int parse_hid_descriptor(
        struct usb_interface *intf,
        struct wacom_features *features
) {
    int error = 0;
    struct usb_host_interface *host_interface = intf->cur_altsetting;
    struct hid_descriptor *hid_desc;

    features->device_type = BTN_TOOL_PEN;

    error = usb_get_extra_descriptor(host_interface, HID_DEVICET_HID, &hid_desc);
    if (error) {
        error = usb_get_extra_descriptor(&host_interface->endpoint[0], HID_DEVICET_REPORT, &hid_desc);
        if (error) {
            printk(KERN_ERR"%s: Error while retrieving extra class descriptor in %s (line %d)", DRIVER_NAME, __func__, __LINE__);
            return error;
        }
    }

    error = parse_hid(intf, hid_desc, features);
    return error;
}

