#kvm-host-server
This server will reside in the host (root namespace, outside of any container).

This server runs in the host (outside any container). It's task is to recieve UUID of the module being insmod from the tap created in load_module().
It then creates a VM environment using KVM-API, and does the following.
1. allocate memory to the VM environment.
1. send address of this memory to lxc-uio-proxy-module.ko running in the kernel,
 so that it can write module binary in this address space.
1. configure interrupt requirements (APIC-v? how?)

