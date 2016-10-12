#ifndef UUIDTABLE_H
#define UUIDTABLE_H

//prototypes
struct module_layout *  get_uuid_layout(__uint128_t uuid,unsigned layout,int ioctl_fd);

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


/*UUID table*/
/*since each installed module gets a uuid, we are tracking it using UUID. this table will store memory regions given to a module, and other data*/
struct UUID_table {
    __uint128_t uuid;
    int vmfd;
    int vcpufd; //only one vcpu can be given
    vm_memory memory;//stores all memory regions, typically there would be only one, but this DS is made generic
    bool processed; //this entry has been processed
    bool could_not_process; //could not process because of some error
} *uuid_table=NULL;
long long table_num_entries=0;
pthread_mutex_t uuid_table_mutex;



//prototypes
int table_init(void);
int table_put(struct UUID_table entry);
struct UUID_table * table_entry_at_index(long long index);
long long get_table_num_entries();
int table_delete(void);



int table_init(void){
    int ret_val=0;
    pthread_mutex_lock(&uuid_table_mutex);
    if(uuid_table!=NULL){
        print_error("calling table_init when uuid_table table already initialized.");
        ret_val=-1;
        goto out_unlock;
    }
    uuid_table=malloc(uuid_table_size*sizeof(*uuid_table));
    if(uuid_table==NULL){
        ret_val=-errno;
        print_error("malloc() failed : %s",strerror(errno));
        goto out_unlock;
    }
    table_num_entries=0;

out_unlock:
    pthread_mutex_unlock(&uuid_table_mutex);\
    return ret_val;
}

int table_delete(void){
    int retval=0;

pthread_mutex_lock(&uuid_table_mutex);
if(uuid_table==NULL){
    print_error("trying to delete an uninitialized table");
    retval=-1;
    goto out_unlock;
}
free(uuid_table);
out_unlock:
    pthread_mutex_unlock(&uuid_table_mutex);
    return retval;
}

int table_put(struct UUID_table entry){
int ret_val=0;
pthread_mutex_lock(&uuid_table_mutex);
    if(uuid_table==NULL){
        print_error("uuid_table pointer is null. Table not initialized?");
        ret_val=-1;
        goto out_unlock;
    }
    if(table_num_entries==uuid_table_size){
        /*table full - increase size*/
        struct UUID_table *p=NULL;
        int newsize=2*uuid_table_size;
        int new_size_bytes=newsize*sizeof(*uuid_table);
        p=realloc(uuid_table,new_size_bytes);
        if(p==0){
            /*realloc failed*/
            /*either keep working with existing table without adding new entry, or quit program, latter is simpler, and is used here*/
            //TODO: when realloc() fails the original block is unchanged, thus if quiting program, it may need to be freed first
            print_error("realloc() failed");
            ret_val=-errno;
            goto out_unlock;
        }
        uuid_table_size=newsize;
    } else
    if(table_num_entries>uuid_table_size){
        /*should never happen*/
        print_error("table_num_entries > uuid_table_size. wasn't expecting this");
        goto out_unlock;
    }

    uuid_table[table_num_entries]=entry;
    table_num_entries++;

out_unlock:
pthread_mutex_unlock(&uuid_table_mutex);
return ret_val;
}

long long get_table_num_entries(){
    int ret_val=0;
    pthread_mutex_lock(&uuid_table_mutex);
        ret_val=table_num_entries;
    pthread_mutex_unlock(&uuid_table_mutex);
    return ret_val;
}

/*
 * caller's responsibility to free the pointer returned since we are sending a copy using malloc()
*/
struct UUID_table * table_entry_at_index_copy(long long index){ //copy since we don't want to access table without lock
    struct UUID_table *p=NULL;
    pthread_mutex_lock(&uuid_table_mutex);

        if(index>=table_num_entries){
            p=NULL;
            goto out_unlock;
        }else{
            p=malloc(sizeof(struct UUID_table));
            if(p==NULL){
                //malloc error
                print_error("table_entry_at_index_copy() malloc() error: %s",strerror(errno));
                goto out_unlock;
            }
            *p=uuid_table[index];
        }

out_unlock:
    pthread_mutex_unlock(&uuid_table_mutex);
    return p;
}
/*
 * initialize memory for the entry corresponding to the given uuid
 * Three regions are created - IDT, init area, core area
 * RETURN : 0 success
 *          -ve error
*/
int initialize_memory_uuid(__uint128_t uuid, int vmfd,int cdev_fd){
    bool found=false;
    int ret_val=0;
    uint64_t *mem=NULL;
    pthread_mutex_lock(&uuid_table_mutex);

    for(long long i=0;i<table_num_entries;i++){
        if(uuid_table[i].uuid==uuid){
#ifdef debug_on
            if(found==false){found=true;}else{print_error("multiple entries for same module or UUID collision!");ret_val=-1;goto out_unlock;}
#else
            found=true;
#endif
            int num_regions=uuid_table[i].memory.num_regions;
            if(num_regions<0){
                //BUG_CHECK
                print_error("num_regions<0");
                ret_val=-1;
                goto out_unlock;
            }
            if(num_regions!=0){
                //memory already initialized?
                print_error("num_regions not zero. trying to initialize already initalized memory? num_regions[%d]",num_regions);
                ret_val=-1;
                goto out_unlock;
            }
            /*num_regions is 0, begin initialization
             *Three regions have to be initialized -
             *      - fisrt page (oth address)  for IDT
             *      - init area (init_layout)
             *      - core area (core_layout)
             */

            /*memory for IDT*/
            unsigned int IDT_size=sysconf(_SC_PAGESIZE)*config_IDT_pages;/*not catching sysconf() error - will only affect poratbility since the flag we are checking for may not exists on a particular system, however this is the most portable version of the flag*/
            mem=mmap(NULL,IDT_size,PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0);//TODO: PROT_EXEC MAP_LOCKED needed? freeing of this memory?
            if(mem==MAP_FAILED){ret_val=-1;  print_error("mmap() failed. file[%s] line[%d]",__FILE__,__LINE__); goto out_unlock; }


            uuid_table[i].memory.size_IDT=IDT_size;
            uuid_table[i].memory.base_IDT=config_IDT_base;

            uuid_table[i].memory.region_IDT.slot=0;
            uuid_table[i].memory.region_IDT.guest_phys_addr=(__u64)uuid_table[i].memory.base_IDT;
            uuid_table[i].memory.region_IDT.memory_size=uuid_table[i].memory.size_IDT;
            uuid_table[i].memory.region_IDT.userspace_addr=(uint64_t)mem;


            ret_val=ioctl(vmfd,KVM_SET_USER_MEMORY_REGION,&uuid_table[i].memory.region_IDT);
            if(ret_val<0){print_error("ioctl() KVM_SET_USER_MEMORY_REGION failed. file[%s] line[%d]",__FILE__,__LINE__); goto out_unlock;}
            uuid_table[i].memory.num_regions++;

            /*memory for init area*/
            struct module_layout *init_layout=NULL;
            init_layout=get_uuid_layout(uuid,kvm_host_server_get_init_layout,cdev_fd);
            if(init_layout==NULL){print_error("ioctl() kvm_host_server_get_init_layout failed. file[%s] line[%d]",__FILE__,__LINE__); goto out_unlock;}

            mem=mmap(NULL,init_layout->size,PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0);//TODO: PROT_EXEC MAP_LOCKED needed? freeing of this memory?
            if(mem==MAP_FAILED){ret_val=-1;  print_error("mmap() failed file[%s] line[%d]",__FILE__,__LINE__); goto out_unlock; }

            uuid_table[i].memory.region_init.slot=1;
            uuid_table[i].memory.region_init.guest_phys_addr=(__u64)init_layout->base;
            uuid_table[i].memory.region_init.memory_size=init_layout->size;
            uuid_table[i].memory.region_init.userspace_addr=(uint64_t)mem;


            ret_val=ioctl(vmfd,KVM_SET_USER_MEMORY_REGION,&uuid_table[i].memory.region_init);
            if(ret_val<0){print_error("ioctl() KVM_SET_USER_MEMORY_REGION failed. file[%s] line[%d]",__FILE__,__LINE__); goto out_unlock;}
            uuid_table[i].memory.num_regions++;


            /*memory for core area*/

            struct module_layout *core_layout=NULL;
            core_layout=get_uuid_layout(uuid,kvm_host_server_get_core_layout,cdev_fd);
            if(core_layout==NULL){print_error("ioctl() kvm_host_server_get_core_layout failed. file[%s] line[%d]",__FILE__,__LINE__); goto out_unlock;}

            mem=mmap(NULL,core_layout->size,PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0);//TODO: PROT_EXEC MAP_LOCKED needed? freeing of this memory?
            if(mem==MAP_FAILED){ret_val=-1;  print_error("mmap() failed. file[%s] line[%d]",__FILE__,__LINE__); goto out_unlock; }

            uuid_table[i].memory.region_core.slot=2;
            uuid_table[i].memory.region_core.guest_phys_addr=0x1000,//(__u64)core_layout->base;//[sahil]
            uuid_table[i].memory.region_core.memory_size=core_layout->size;
            uuid_table[i].memory.region_core.userspace_addr=(uint64_t)mem;


            ret_val=ioctl(vmfd,KVM_SET_USER_MEMORY_REGION,&uuid_table[i].memory.region_core);
            if(ret_val<0){print_error("ioctl() KVM_SET_USER_MEMORY_REGION failed file[%s] line[%d]",__FILE__,__LINE__); goto out_unlock;}
            uuid_table[i].memory.num_regions++;

            //success
            ret_val=0;

/*Dont break the loop here(in debug mode), let it run to catch any bugs or UUID collisions - early fail*/
/*in non-debug mode - optimize*/
#ifndef debug_on
            break;
#endif
        }//uuid match END
    }//for() END

if(found==false)    {
    print_error("entry with the given uuid not found");
    ret_val=-1;
    goto out_unlock;
}
out_unlock:
    pthread_mutex_unlock(&uuid_table_mutex);
    return ret_val;

}//initialize_memory_uuid END


//int table_add_memory_uuid(__uint128_t uuid,uint64_t *mem,unsigned int size,int vmfd){//add memory given by pointer and size of uuid entry
//    if(mem==MAP_FAILED||size==0){
//        //error
//        return -1;
//    }
//bool found=false;
//int ret_val=0;
//pthread_mutex_lock(&uuid_table_mutex);
//for(long long i=0;i<table_num_entries;i++){
//    if(uuid_table[i].uuid==uuid){
//        found=true;

//        int num_regions=uuid_table[i].memory.num_regions;

//        if(num_regions<0){
//            //BUG_CHECK
//            print_error("num_regions<0");
//            ret_val=-1;
//            goto out_unlock;
//        }

//        if(num_regions>=max_num_memory_regions){
//            //cannot add more
//            ret_val=-1;
//            goto out_unlock;
//        }else if(num_regions==0){//first memory region being added - add page for IDT

//            uint64_t *mem_idt=mmap(NULL,sysconf(_SC_PAGESIZE),PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0);//TODO: PROT_EXEC MAP_LOCKED needed? freeing of this memory?
//            if(mem_idt==MAP_FAILED){ret_val=-1;  goto out_unlock; }

//            uuid_table[i].memory.base=memory_region_base;
//            uuid_table[i].memory.base_code=memory_regions_code_base;

//            //IDT
//            uuid_table[i].memory.regions[0].slot=0;
//            uuid_table[i].memory.regions[0].guest_phys_addr=memory_region_base;
//            uuid_table[i].memory.regions[0].memory_size=sysconf(_SC_PAGESIZE); //whole page dedicated to IDT
//            uuid_table[i].memory.regions[0].userspace_addr=(uint64_t)mem_idt;

//            ret_val=ioctl(vmfd,KVM_SET_USER_MEMORY_REGION,&uuid_table[i].memory.regions[0]);
//            if(ret_val<0){
//                print_error("ioctl KVM_SET_USER_MEMORY_REGION failed file[%s] line[%d]",__FILE__,__LINE__);
//                goto out_unlock;
//            }

//            //first code+data region
//            uuid_table[i].memory.regions[1].slot=1;
//            uuid_table[i].memory.regions[1].guest_phys_addr=memory_regions_code_base;
//            uuid_table[i].memory.regions[1].memory_size=size;
//            uuid_table[i].memory.regions[1].userspace_addr=(uint64_t)mem;


//            ret_val=ioctl(vmfd,KVM_SET_USER_MEMORY_REGION,&uuid_table[i].memory.regions[1]);
//            if(ret_val<0){
//                print_error("ioctl KVM_SET_USER_MEMORY_REGION failed file[%s] line[%d]",__FILE__,__LINE__);
//                goto out_unlock;
//            }

//            uuid_table[i].memory.end=memory_regions_code_base+size;
//            uuid_table[i].memory.num_regions=2;//including IDT
//            ret_val=0;
//        }else{
//            uuid_table[i].memory.regions[num_regions].slot=num_regions-1;
//            uuid_table[i].memory.regions[num_regions].guest_phys_addr=uuid_table[i].memory.end;
//            uuid_table[i].memory.regions[num_regions].memory_size=size;
//            uuid_table[i].memory.regions[num_regions].userspace_addr=(uint64_t)mem;


//            ret_val=ioctl(vmfd,KVM_SET_USER_MEMORY_REGION,&uuid_table[i].memory.regions[num_regions]);
//            if(ret_val<0){
//                print_error("ioctl KVM_SET_USER_MEMORY_REGION failed file[%s] line[%d]",__FILE__,__LINE__);
//                goto out_unlock;
//            }

//            uuid_table[i].memory.end+=size;
//            uuid_table[i].memory.num_regions++;
//            ret_val=0;
//        }

//    break;
//    }
//}
//if(!found){
//    ret_val=-1;
//}
//out_unlock:
//pthread_mutex_unlock(&uuid_table_mutex);
//return ret_val;
//}


/*get core_layout or init_layout of the module
 * RETURN : NULL - error
 *          ptr to struct module_layout
 * on ERRRO errno may be set, if not already set by one the function called by this function
*/
struct module_layout *  get_uuid_layout(__uint128_t uuid,unsigned layout,int ioctl_fd){
errno=0;
int ret_val=0;
void *buff=NULL;//we will send uuid in this buffer and get back module_layout
if(layout==kvm_host_server_get_init_layout||layout==kvm_host_server_get_core_layout){
//okay
}
else{
    //error if requesting anything else
        errno=-1;
        return NULL;
}

buff=malloc(sizeof( struct module_layout));
if(buff==NULL){
    //error
    //errno will be set by malloc()
    return NULL;
}
memcpy(buff,&uuid,sizeof(uuid)); //TODO: how to check failure? man page says nothing

ret_val=ioctl(ioctl_fd,layout,buff);
if(ret_val==0){
    //success
    return (struct module_layout *)buff;
}else if(ret_val<0){
    //error
    errno=ret_val;
    return NULL;
}else{
    //unexpected value returned
    print_error("unexpected value returned by ioctl(). file[%s] line[%d]",__FILE__,__LINE__);
    errno =-1;
    return NULL;
}
return NULL; //should never execute
}

#endif // UUIDTABLE_H

