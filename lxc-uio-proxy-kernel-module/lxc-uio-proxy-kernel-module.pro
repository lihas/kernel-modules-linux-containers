######################################################################
# Automatically generated by qmake (3.0) Thu Sep 15 15:19:46 2016
######################################################################

TEMPLATE = app
TARGET = lxc-uio-proxy-kernel-module
INCLUDEPATH += .

# Input
HEADERS += protocol.h \
    vm-memory.h \
    global-config.h
SOURCES += cdev.c lxc-uio-proxy.c chardev.h
LINUX_HEADERS_PATH = /usr/src/linux-headers-$$system(uname -r)
