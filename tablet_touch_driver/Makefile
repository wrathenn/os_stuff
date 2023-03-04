ifneq ($(KERNELRELEASE),)
	obj-m += touch_tablet.o
	touch_tablet-y += touch_handler.o pen_handler.o \
					  touch_tablet_main.o debug.o wacom.o hid_parse.o
else
	CURRENT = $(shell uname -r)
	KDIR = /lib/modules/$(CURRENT)/build
	PWD = $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	make small_clean

small_clean:
	@rm -f *.o .*.o.d
	@rm -f .*.cmd
	@rm -f modules.order
	@rm -f *.symvers
	@rm -f *.mod *.mod.c

clean: small_clean
	@rm -f *.ko


endif
