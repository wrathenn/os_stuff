#ifndef OSC_HID_PARSE_H
#define OSC_HID_PARSE_H

#define HID_DEVICET_HID		(USB_TYPE_CLASS | 0x01)
#define HID_DEVICET_REPORT	(USB_TYPE_CLASS | 0x02)
#define HID_USAGE_UNDEFINED		0x00
#define HID_USAGE_PAGE			0x04
#define HID_USAGE_PAGE_DIGITIZER	0x0d
#define HID_USAGE_PAGE_DESKTOP		0x01
#define HID_USAGE_PAGE_WACOMTOUCH	0xff00
#define HID_USAGE			0x08
#define HID_USAGE_X			((HID_USAGE_PAGE_DESKTOP << 16) | 0x30)
#define HID_USAGE_Y			((HID_USAGE_PAGE_DESKTOP << 16) | 0x31)
#define HID_USAGE_PRESSURE		((HID_USAGE_PAGE_DIGITIZER << 16) | 0x30)
#define HID_USAGE_X_TILT		((HID_USAGE_PAGE_DIGITIZER << 16) | 0x3d)
#define HID_USAGE_Y_TILT		((HID_USAGE_PAGE_DIGITIZER << 16) | 0x3e)
#define HID_USAGE_FINGER		((HID_USAGE_PAGE_DIGITIZER << 16) | 0x22)
#define HID_USAGE_STYLUS		((HID_USAGE_PAGE_DIGITIZER << 16) | 0x20)
#define HID_USAGE_WT_X			((HID_USAGE_PAGE_WACOMTOUCH << 16) | 0x130)
#define HID_USAGE_WT_Y			((HID_USAGE_PAGE_WACOMTOUCH << 16) | 0x131)
#define HID_USAGE_WT_FINGER		((HID_USAGE_PAGE_WACOMTOUCH << 16) | 0x22)
#define HID_USAGE_WT_STYLUS		((HID_USAGE_PAGE_WACOMTOUCH << 16) | 0x20)
#define HID_USAGE_CONTACTMAX		((HID_USAGE_PAGE_DIGITIZER << 16) | 0x55)
#define HID_COLLECTION			0xa0
#define HID_COLLECTION_LOGICAL		0x02
#define HID_COLLECTION_END		0xc0
#define HID_LONGITEM			0xfc

#include <linux/usb.h>
#include "wacom.h"

struct hid_descriptor {
    struct usb_descriptor_header header;
    __le16   bcd_hid;
    u8       b_countryCode;
    u8       b_num_descriptors;
    u8       b_descriptor_type;
    __le16   w_descriptor_length;
} __attribute__ ((packed));

int parse_hid_descriptor(
    struct usb_interface *intf,
    struct wacom_features *features
);

#endif //OSC_HID_PARSE_H
