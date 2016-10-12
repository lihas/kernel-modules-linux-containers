#ifndef GLOBALCONFIG_H
#define GLOBALCONFIG_H

/*Global config*/
//shared
#define debug_on
const char lxc_uio_proc_name[]="lxc-uio-proxy-debug";
#define max_num_memory_regions 5 //max number of memory regions


//kvm-host-server
#ifdef kvm_host_server_config
const char lxc_uio_cdev_path[]="/dev/lxc-uio-proxy"; //Parameter shoudl match lxc-uio-proxy.c
long long uuid_table_size=256;//initial size - may change as program runs
//__u64 memory_region_base =0; //starting address of memory space of each module - first page only for IDT
//__u64 memory_regions_code_base=0x1000;//starting address of memory space for code of each module - 0x10000 to avoid IDT in first page
#define config_IDT_pages 1 //number of pages to give to IDT
#define config_IDT_base 0 //the starting address of the memory region dedicated to IDT
#endif

//lxc-uio-proxy
#ifdef lxc_uio_proxy_config
long long table_size=256,table_num_entries=0; //TODO: set a maximum limit on table_size, depending on practical constraints and the max value supported by this data type
long long table_index=-1; //table index - index upto which table elements have been poped()
const char cdev_name[265]="lxc-uio-proxy",cdev_class_name[256]="lxc-uio-proxy";
//#ifdef debug_on -- since now major number is being sent by /proc entry to kvm-host-server, thus it is mandatory till the two are decopled
umode_t lxc_uio_proc_permission = 0444; //setting this to 0 also means a default permission of 0444
//#endif
#endif

#endif // GLOBALCONFIG_H

