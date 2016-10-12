#ifndef DEBUG_H
#define DEBUG_H
int enable_debug(int vcpufd,bool enable){
    /*from https://github.com/virtualopensystems/kvm-unit-tests/blob/6c015068f85c37ce4adce6a23ed8110e9ae91904/api/kvmxx.cc*/

    int ret_val=0;

    struct kvm_guest_debug gd;
    gd.control=0;

    if(enable){
        gd.control|=KVM_GUESTDBG_ENABLE;
        gd.control|=KVM_GUESTDBG_SINGLESTEP;
    }

    ret_val=ioctl(vcpufd,KVM_SET_GUEST_DEBUG,&gd);

    if(ret_val<0){
        print_error("KVM_SET_GUEST_DEBUG");
        return -1;
    }

    return 0;
}


#endif // DEBUG_H

