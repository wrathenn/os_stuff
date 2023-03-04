#!/bin/sh

sudo rmmod touch_tablet wacom hid_logitech_dj usbhid
sudo make
sudo insmod ./touch_tablet.ko
