#ifndef VMMEMORYREGION_H
#define VMMEMORYREGION_H
/*VM Memory region - data structure to store memory*/
//typedef struct {
//struct kvm_userspace_memory_region regions[max_num_memory_regions]; //array - at max 5 memory regions //TODO: to make it generic either malloc the array or use linked list
//int num_regions; //total number of regions, may NOT be adjacent
//__u64 base;//starting address of all the regions combined(including IDT) - first page only for IDT - Note that this is different from "void * base" present in struct module_laytou
//__u64 base_code; //modul code+data should not reside in first page
//__u64 end;//ending address of all the regions combined
//} vm_memory;
//#endif // VMMEMORYREGION_H

//vm_memory new_memory(void){
//    vm_memory m;

//    m.num_regions=0;
//    m.base=0;
//    m.end=0;
//    return m; //wrong!! - returning memory from stack!!
//}


typedef struct {
    /*region[0]->IDT region[1]->init_area region[2]->core_area - 3 regions already used*/
//struct kvm_userspace_memory_region regions[max_num_memory_regions]; //array - at max 5 memory regions //TODO: to make it generic either malloc the array or use linked list

struct kvm_userspace_memory_region region_IDT; //will hold Interrupt descriptor tables
struct kvm_userspace_memory_region region_init;//will hold init area (corresponding to init_layout of struct module)
struct kvm_userspace_memory_region region_core;//will hold core area (corresponding to core_layout of struct module)

int num_regions; //total number of regions, may NOT be adjacent
void *base_IDT; //base of IDT - typically will be zero
uint size_IDT; //size in bytes taken by IDT area

struct module_layout core_layout; //get a copy from struct module present in the kernel
struct module_layout init_layout; //get a copy from struct module present in the kernel

} vm_memory;

#ifdef kvm_host_server_config //since this file is shared in both the proxy module and the host server(which runs in user space), and following function is only required in host server
vm_memory* new_memory(void){
    vm_memory *m=NULL;
    m=malloc(sizeof(*m));
    if(m==NULL){
        //error
        return m;
    }else{
        m->num_regions=0;
        m->base_IDT=NULL;
        m->size_IDT=0;
        return m;
    }
    return NULL;//should never reach here
}
#endif


#endif // VMMEMORYREGION_H
