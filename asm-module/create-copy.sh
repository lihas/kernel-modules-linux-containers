#!/bin/bash
cntr=`ls -l|wc -l`
cp -r "kernel_module_hello_world2" "kernel_module_hello_world$cntr"
cd "kernel_module_hello_world$cntr"
mv "hello_world2.c" "hello_world$cntr.c"
rm hello_world2.*
cat <<EOF >Makefile
obj-m += hello_world$cntr.o

EOF

cat<<'EOF' >>Makefile
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
EOF
make
cd ..
echo "$cntr"
