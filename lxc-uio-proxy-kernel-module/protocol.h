//
// data transfer protocol related stuff
//  protocol - how the data is transferred between user space and KS
//

#ifndef KVM_WORK_PROTOCOL_H
#define KVM_WORK_PROTOCOL_H


/*previous definition of struct packet*/
//struct packet{
//    __uint128_t uuid;
//    void *pointer; //pointer to memory area in EPT memory space
//    long long mem_size; //size of memory area pointed to by pointer
//};

/*new definition - allows more than one memory region*/
typedef struct {
__uint128_t uuid;
vm_memory memory; //will have all memory regions - max memory regions can be upto some specified level;
} packet;

#endif //KVM_WORK_PROTOCOL_H
