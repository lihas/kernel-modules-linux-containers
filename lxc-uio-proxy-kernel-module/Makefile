ccflags-y := -std=gnu99 -Wno-declaration-after-statement -Wtype-limits -Wsign-compare -Waddress -Wlogical-op
#-H -g -v
obj-m += lxc-uio-proxy.o
.PHONY:all,clean
all: lxc-uio-proxy.c cdev.c protocol.h udev-lxc-uio-proxy.rules
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	sudo cp udev-lxc-uio-proxy.rules /etc/udev/
	-sudo rm /etc/udev/rules.d/010-udev-lxc-uio-proxy.rules
	sudo ln -s /etc/udev/udev-lxc-uio-proxy.rules /etc/udev/rules.d/010-udev-lxc-uio-proxy.rules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	sudo rm /etc/udev/udev-lxc-uio-proxy.rules
	sudo rm /etc/udev/rules.d/010-udev-lxc-uio-proxy.rules
