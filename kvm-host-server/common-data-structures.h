#ifndef COMMONDATASTRUCTURES_H
#define COMMONDATASTRUCTURES_H

typedef struct {
    /*region[0]->IDT region[1]->init_area region[2]->core_area - 3 regions already used*/
//struct kvm_userspace_memory_region regions[max_num_memory_regions]; //array - at max 5 memory regions //TODO: to make it generic either malloc the array or use linked list


    /*
     * SLOTS :  0 - boot_ram
     *      x0 - IDTx  - Removed since boot_ram already overlaps first page, hence no need to create memory separately. IDT, etc. can be written to first page, once A20 check code finishes
     *          1 - init_area
     *          2 - core_area

*/
//REMOVED - see above given reason// struct kvm_userspace_memory_region region_IDT; //will hold Interrupt descriptor tables
struct kvm_userspace_memory_region region_init;//will hold init area (corresponding to init_layout of struct module)
struct kvm_userspace_memory_region region_core;//will hold core area (corresponding to core_layout of struct module)

struct kvm_userspace_memory_region boot_ram;//2 MB of initial memory - starting at 0x0000 - overlapping with IDT memory - fix it [sahil]

int num_regions; //total number of regions, may NOT be adjacent
void *base_IDT; //base of IDT - typically will be zero
uint size_IDT; //size in bytes taken by IDT area

struct module_layout core_layout; //get a copy from struct module present in the kernel
struct module_layout init_layout; //get a copy from struct module present in the kernel

} vm_memory;

#ifdef kvm_host_server_config
/*UUID table*/
/*since each installed module gets a uuid, we are tracking it using UUID. this table will store memory regions given to a module, and other data*/

struct UUID_table {
    __uint128_t uuid;
    int vmfd;
    int vcpufd; //only one vcpu can be given
    vm_memory memory;//stores all memory regions, typically there would be only one, but this DS is made generic
    bool processed; //this entry has been processed
    bool could_not_process; //could not process because of some error
    bool a20_check_done;
    bool a20_enabled;
} *uuid_table=NULL;

long long table_num_entries=0;
pthread_mutex_t uuid_table_mutex;

//macros
#define table_set_xxx(index,parameter,value)\
{\
pthread_mutex_lock(&uuid_table_mutex);        \
    errno=0;\
    if(index<=table_num_entries-1){\
    uuid_table[(index)].parameter=(value);\
    }else{print_error("index out of range");errno=-1;}\
pthread_mutex_unlock(&uuid_table_mutex);   \
}

//prototypes
int table_init(void);
int table_put(struct UUID_table entry);
struct UUID_table * table_entry_at_index_copy(long long index);
long long get_table_num_entries();
int table_delete(void);
struct module_layout *  get_uuid_layout(__uint128_t uuid,unsigned layout,int ioctl_fd);
int load_A20_enabled_check_code(long long index);
struct UUID_table * table_entry_at_index_copy_unlocked(long long index);

#endif
#endif // COMMONDATASTRUCTURES_H

