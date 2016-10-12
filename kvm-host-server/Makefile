CFLAGS = -std=gnu99 -Wno-declaration-after-statement -Wtype-limits -Wsign-compare -Waddress -Wlogical-op -g -rdynamic

.PHONY: all, clean

all:kvm-host-server.c
	gcc $(CFLAGS) kvm-host-server.c -o kvm-host-server

clean:
	rm kvm-host-server

#-I /usr/src/linux-headers-$(shell uname -r)/include/
