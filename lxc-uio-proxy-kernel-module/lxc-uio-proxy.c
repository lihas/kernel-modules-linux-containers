/*
This gets module struct from load_module() kernel function, stores it in an array. This also gets memory pointer(s) from user space program and populates the memory region by the module.
*/

#define lxc_uio_proxy_config //since global-config.h file is shared, we do conditional ompilation in it

#pragma GCC diagnostic push /*do not want to see warnings from these files*/
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Waddress"
#pragma GCC diagnostic ignored "-Wlogical-op"
#include <linux/kernel.h>
#include<linux/uuid.h>
#include<linux/sched.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/fdtable.h>
#include <linux/eventfd.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include<linux/kvm.h>
#pragma GCC diagnostic pop


//local includes
#include "global-config.h"
#include "vm-memory.h"
#include "protocol.h"

/*Global config - in global-config.h*/

//data structures
typedef struct {
__uint128_t uuid;
struct nsproxy *nsproxy;
struct module *module;
} module_table_struct;

//prototypes
int lxc_uio_proxy_put_module_1(__uint128_t uuid,struct nsproxy *nsproxy,struct module *module);
int process_packet(packet packet);
int send_event(void);
module_table_struct* table_get_copy(__uint128_t uuid);
//#ifdef debug_on -- since now major number is being sent by /proc entry to kvm-host-server, thus it is mandatory till the two are decopled
extern struct file_operations lxc_uio_proc_ops;
//#endif

module_table_struct* table_pop(void);
int table_pop_reverse(void);

//global variables
extern int (*lxc_uio_proxy_put_module)(__uint128_t uuid,struct nsproxy *nsproxy,struct module *module);
int lxc_uio_cdev_major_number=-1; //setting initially to an invalid number
struct kvm_host_server{
    int pid;
    struct file *fp;
} *ks=NULL;
DEFINE_MUTEX(ks_mutex);

#include "cdev.c"
/*module table BEGIN*/
module_table_struct *module_table=NULL;
DEFINE_MUTEX(table_mutex);

int __init table_init(void){
    int ret_val=0;
    mutex_lock(&table_mutex);

    if(module_table!=NULL){
        printk(KERN_ALERT"[lxc-uio] module_table already initialized\n");
        ret_val=-1;
        goto out_unlock;
    }
    module_table = kmalloc(table_size*sizeof(*module_table),GFP_KERNEL);
    if(module_table==NULL){
        printk(KERN_ALERT"[lxc-uio] kmalloc() failed.");
        ret_val=-1;
        goto out_unlock;
    }
out_unlock:
    mutex_unlock(&table_mutex);
    return ret_val;
}

int table_put(module_table_struct mt){
    mutex_lock(&table_mutex);
    int ret_val=0;
    if (module_table==NULL){
        printk(KERN_ALERT"[lxc-uio] ERROR - module_table pointer is NULL\n");
        ret_val=-1;
        goto out_unlock;
    }

    if(table_num_entries>table_size){//TODO: Limit the increase upto a maximum value
        /*should never happen*/
        printk(KERN_ALERT"[lxc-uio] lxc-uio-proxy table_num_entries greater than table_size\n");
        ret_val=-ENOMEM;
        goto out_unlock;
    }
    if(table_num_entries==table_size){/*table full*/
        module_table=krealloc(module_table,2*table_size*sizeof(*module_table),GFP_KERNEL);
        if(!module_table){
            /*krealloc failed*/
            //TODO: when krealloc() fails the original block is unchanged, thus if quiting program, it may need to be freed first
            ret_val=-ENOMEM;
            goto out_unlock;
        }
        table_size=2*table_size;
    }
    module_table[table_num_entries]=mt;
    table_num_entries++;

    out_unlock:
    mutex_unlock(&table_mutex);
    return ret_val;
}

/*
 * get copy since we dont want access to table without lock
 * the caller should free the memory allocated
*/
module_table_struct* table_get_copy(__uint128_t uuid){
    /*return table entry pointer on sucess, NULL on error*/
    module_table_struct *ret_val=kmalloc(sizeof(module_table_struct),GFP_KERNEL);
    if(ret_val==NULL){
        return NULL;
    }
    mutex_lock(&table_mutex);
    for(long long i=0;i<table_num_entries;i++){
        if (module_table[i].uuid==uuid){
            *ret_val=*&module_table[i];
            break;
        }
    }
    mutex_unlock(&table_mutex);
    return ret_val;
}
module_table_struct* table_pop(void){
    module_table_struct *p=NULL;
    mutex_lock(&table_mutex);
    if(table_index+1<=table_num_entries-1){//last table element is at [table_num_entries-1] index
    table_index++;
    p=&module_table[table_index];
    }
    mutex_unlock(&table_mutex);
    return p;
}

int table_pop_reverse(void){//un-pop last popped
    int retval=0;
    mutex_lock(&table_mutex);
    if(table_index<=0){
        //error
        printk(KERN_ALERT"[lxc-uio] table_pop_reverse() error\n");
        retval=-1;
        goto out_unlocked;
    }
    table_index--;
    out_unlocked:
    mutex_unlock(&table_mutex);
    return retval;
}

const module_table_struct* table_peek(void){

    const module_table_struct *p=NULL;
    mutex_lock(&table_mutex);
    if(table_index+1<=table_num_entries-1){//last table element is at [table_num_entries-1] index
    p=&module_table[table_index+1];
    }
    mutex_unlock(&table_mutex);
    return p;

}
/*module table END*/

int __init start(void){

    struct module *owner=NULL;
    const struct kernel_symbol *ks=NULL;
    int ret_val=0;

    int ret=table_init();
    if(ret<0){
        printk(KERN_ALERT"module_table init error\n");
        ret_val=-1;
        goto out;
    }

    //module.c exports (*lxc_uio_proxy_put_module)() function pointer, which has to be set with the implementation of it
    //created in this file. so that it can be used there.
    mutex_lock(&module_mutex);
    ks=find_symbol("lxc_uio_proxy_put_module",&owner,NULL,false,false);
    mutex_unlock(&module_mutex);

    if(ks==NULL){
        //function pointer symbol not present;
        printk(KERN_ALERT"[lxc-uio] (*lxc_uio_proxy_put_module)() function pointer not exported from kernel/module.c. Exiting. Is this the correct version of kernel that you are running?\n");
        ret_val=-1;
        goto out_free;
    }
    if(lxc_uio_proxy_put_module==NULL){
        //everything okay, set the pointer
        lxc_uio_proxy_put_module=lxc_uio_proxy_put_module_1; //WARN: no lock taken here. Assuming only one insmod of this module is done at a time.
    }
    else {
        printk(KERN_ALERT"[lxc-uio] (*lxc_uio_proxy_put_module)() already set to a non null value. Wasn't expecting this. Exiting. Improper cleanup during exit of this module?\n");
        ret_val=-1;
        goto out_free;
    }

    ret=create_chardev(&lxc_uio_cdev_major_number);
    if(ret<0){
        printk(KERN_ALERT"[lxc-uio] Error creating chardev. Exiting. \n");
        ret_val=-1;
        goto out_put_module;
    }

//#ifdef debug_on -- since now major number is being sent by /proc entry to kvm-host-server, thus it is mandatory till the two are decopled
    if(!proc_create(lxc_uio_proc_name,lxc_uio_proc_permission,NULL,&lxc_uio_proc_ops)){
        printk(KERN_ALERT"[lxc-uio] Error creating proc entry.\n");
        ret_val=-ENOMEM;
        goto out_chardev;
    }
//#endif
    return ret_val; //everything ok

out_chardev:
    remove_chardev();
out_put_module:
    lxc_uio_proxy_put_module=NULL;
out_free:
    kfree(module_table);
out:
    return ret_val;
}

void __exit end(void){
    remove_chardev();
    lxc_uio_proxy_put_module=NULL;
    kfree(module_table);
#ifdef debug_on
    remove_proc_entry(lxc_uio_proc_name,NULL);
#endif
    return;
}

/*lxc-uio-proxy API BEGIN*/
/*put information in database about a module which is being inserted from a container - will be called from load_module() kernel function during insmod*/
int lxc_uio_proxy_put_module_1(__uint128_t uuid,struct nsproxy *nsproxy,struct module *module){
    int ret_val=0;
    unsigned long long *p=NULL; //pointer to 64bits data type
    module_table_struct module_table;
    module_table.uuid=uuid;
    module_table.nsproxy=nsproxy;
    module_table.module=module;
    ret_val=table_put(module_table);

#ifdef debug_on
    p=(unsigned long long *)&module_table.uuid;
    printk(KERN_DEBUG"[lxc-uio] lxc_uio_proxy_put_module() called. module-name [%s] module-uuid[%llx-%llx] module-init-base[%p] module-init-size[%u] module-core-base[%p] module-core-size[%u] module-starup-function-address[%p]\n",module->name,*p,*(p+1),module->init_layout.base,module->init_layout.size,module->core_layout.base,module->core_layout.size,module->init);
    printk(KERN_DEBUG"[lxc-uio] exported symbols from modules (total=%u): \n",module->num_syms);
    for(unsigned int i=0;i < module->num_syms;i++){
        printk(KERN_DEBUG"[lxc-uio] name[%s] value[%lu]\n",module->syms[i].name,module->syms[i].value);
    }
#endif

    if(ret_val==0){
        //no error
        //send eventfd notification to user space kvm-host-server

        send_event(); //send eventfd notification to user space
    }

    return ret_val;
}

/*lxc-uio-proxy API END*/

/*Auxilliary functions*/
/*
 * process packet received from user space of host.
 * IMPORTANT: Getting called from a mutex lock contex from cdev_write()
 * The packet received is for an uninitialized module.
 * Three things need to be done -
 *                              handle IDT area (not implemented)
 *                              handle init area (not implemented)
 *                              handle core area (implemented)
 * IMPORTANT - Dont Use module_layout coming in the packet- use that from your own data-base
 * RETURN - 0 success
 *          -ve error
 * */
int process_packet(packet packet1){
    __uint128_t uuid=packet1.uuid;
    module_table_struct* mts=table_get_copy(uuid);
    int ret_val=0;

    /*IDT*/
    //(not implemented)

    /*init area*/
    //(not implemented)
    printk(KERN_WARNING"[lxc-uio] WARNING: process_packet() only handles core area as of now (IDT and init area not handled)\n");

    /*core area*/

    ret_val=copy_to_user((void *)packet1.memory.region_core.userspace_addr,mts->module->core_layout.base,mts->module->core_layout.size);

    if(ret_val==0){
        //success
        return 0;
    }else{
        //error
        ret_val*=-1; //send number of characters not copied in negative form
        return ret_val;
    }
    //
    /*
    void *from=NULL;
    unsigned int num_bytes=0;
    unsigned long bytes_not_copied=0;
    __uint128_t uuid=packet.uuid;
    void *pointer=packet.pointer;
    unsigned int mem_size=packet.mem_size;
    module_table_struct *entry=table_get_copy(uuid);
    if(entry==NULL){
        printk(KERN_ALERT"[lxc-uio] ERROR: error in table_get_copy(uuid). invalid uuid?\n");
        return -1;
    }
    //const void *from=entry->module->module_init_rw;
    //    unsigned long num_bytes=entry->module->init_size;

    //TDOD: Temporarity just copying module's init areas - later in correct this
    from=entry->module->init_layout.base;
    num_bytes=entry->module->init_layout.text_size;

    if(mem_size<num_bytes){
        printk(KERN_ALERT"[lxc-uio] Memory size created by user space server is less than that required for the module\n");
        return -1;
    }

    bytes_not_copied=copy_to_user(pointer,from,num_bytes);
if(entry!=NULL){kfree(entry);} //since we are getting a copy using malloc, hence we must also free
return 0;
*/
}

int send_event(void){
    const module_table_struct* ptr=NULL;
    struct eventfd_ctx * efd_ctx = NULL;
    int retval=0;
    mutex_lock(&ks_mutex);
    if(ks!=NULL){
        //server registered
        ptr=table_peek(); //is there an element in table array?
        if(ptr==NULL){
        //nothing in table
            printk(KERN_ALERT"[lxc-uio] module_table empty. wasn't expecting this. \n");
            retval=-1;
            goto out_unlock;
        }
        //everythig okay - send eventfd notification() to user space - logic from "http://stackoverflow.com/questions/13607730/writing-to-eventfd-from-kernel-module"
        efd_ctx=eventfd_ctx_fileget(ks->fp);
        if(!efd_ctx){
            //error
            printk(KERN_ALERT"[lxc-uio] Error in eventfd_ctx_fileget()\n");
            retval=-1;
            goto out_unlock;
        }
        eventfd_signal(efd_ctx,1);
        printk(KERN_DEBUG"[lxc-uio] eventfd() sent\n");
        eventfd_ctx_put(efd_ctx);
    }else{
        //server not registered
        //do nothing - when a server will register itself, it will ask for pending entries via ioctl()
        goto out_unlock;
    }

 out_unlock:
    mutex_unlock(&ks_mutex);
    return retval;
}
module_init(start);
module_exit(end);
MODULE_LICENSE("GPL");


//#ifdef debug_on -- since now major number is being sent by /proc entry to kvm-host-server, thus it is mandatory till the two are decopled

/*using single_open() seq_file as explained here - https://www.linux.com/learn/kernel-newbie-corner-kernel-debugging-using-proc-sequence-files-part-1*/
static int lxc_uio_proc_show(struct seq_file *m, void *v){

    seq_printf(m,"%s \nmajor number [%d] \nmodule_table: {address [%p],table_size [%lld], num_entries [%lld], table_index [%lld] }\n",cdev_name,lxc_uio_cdev_major_number,module_table,table_size,table_num_entries,table_index);

    return 0;
}

int lxc_uio_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file,lxc_uio_proc_show,NULL);
}

struct file_operations lxc_uio_proc_ops = {
    .owner = THIS_MODULE,
    .open = lxc_uio_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};
//#endif
