/*This server runs in the host (outside any container). It's task is to recieve UUID(s) of the module(s) being insmod from the tap created in load_module().
It then creates a VM environment using KVM-API, and does the following.
1. allocate memory to the VM environment.
1. send address of this memory to lxc-uio-proxy-module.ko running in the kernel,
 so that it can write module binary in this address space.
1. configure interrupt requirements (APIC-v? how?)
*/
#define kvm_host_server_config //since we have a shared config file, we do conditional compilation using this
/*includes from standard directory*/
#pragma GCC diagnostic push /*do not want to see warnings from these files*/
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Waddress"
#pragma GCC diagnostic ignored "-Wlogical-op"
#include<unistd.h>
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<ctype.h>
#include<errno.h>
#include<sys/ioctl.h>
#include<sys/eventfd.h>
#include<fcntl.h>
#include<pthread.h>
#include <stdbool.h>
#include<sys/mman.h>
#include<linux/kvm.h>
#include<unistd.h>
#include<execinfo.h>
#include<signal.h>
#pragma GCC diagnostic pop


/*Global config - in global-config.h header file*/

/*to print backtrace - from http://www.helicontech.co.il/?id=linuxbt */
void bt(void) {
    int c, i;

    void *addresses[10];
    char **strings;

    c = backtrace(addresses, 10);
    strings = backtrace_symbols(addresses,c);
    printf("\nbacktrace: %d frames \n", c-1);
    for(i = 1; i < c; i++) { //loop starting from 1 since we want to skip the bt() function from getting printed
        printf("%d: %p ", i, addresses[i]);
        printf("%s\n", strings[i]); }
    }


/*Global variables*/
int lxc_uio_cdev_fd=-1;
int lxc_uio_eventfd=-1; //to store eventfd fd
#define ERROR   "\x1B[31m" //RED
#define INFO   "\x1B[36m" //CYAN
#define WARN   "\x1B[32m" //GREEN
#define RESET "\x1B[0m" //RESET console color
#define print_error(...)  printf(ERROR); printf("[lxc-uio] ERROR: "); printf( __VA_ARGS__ ); printf(" file-name[%s] line-number[%d]",__FILE__,__LINE__); printf(RESET); bt(); printf("\n"); //always surround the macro with braces {}
#define print_info(...)  printf(INFO); printf("[lxc-uio] INFO: "); printf( __VA_ARGS__ ); printf(RESET); printf("\n"); //always surround the macro with braces {}
#define print_warning(...)  printf(WARN); printf("[lxc-uio] WARN: "); printf( __VA_ARGS__ ); printf(RESET); printf("\n"); //always surround the macro with braces {}


//local includes
#include "global-config.h"
#include "kernel-includes.h"
#include "chardev.h"
#include "vm-memory.h"
#include "uuid-table.h"
#include "protocol.h"


/*for compatibility*/
int lxc_uio_cdev_major_number=-1; //for compaitbility with chardev of lxc-uio-proxy-kernel-module - since chardev.h is directly hard linked from that directory

/*prototypes*/
int process_entry(int kvmfd,int chardevfd);
int get_new_entry(int ioctl);
int get_major_number(void);
int register_server(int ioctl_fd,int eventfd);
int deregister_server(int ioctl_fd);
unsigned int get_uuid_size(__uint128_t uuid,int ioctl_fd);
int send_packet(__uint128_t uuid,vm_memory memory,int chardevfd);
int initialize_memory_uuid(__uint128_t uuid, int vmfd,int cdev_fd);
struct module_layout *  get_uuid_layout(__uint128_t uuid,unsigned layout,int ioctl_fd);
void signal_callback_handler(int signum);

int main(int argc, char *argv[]){
    int major_number=-1;
    int ret_val=-1;
    int kvmfd=-1;

    signal(SIGINT, signal_callback_handler);//register signal handler

    major_number=get_major_number();

    if(major_number<0){
        print_error("Got a negative major number: %d", major_number);
        ret_val=-1;
        goto out;
    }

    /*kvm config*/
    kvmfd=open("/dev/kvm",O_RDWR|O_CLOEXEC);
    if(kvmfd<0){
        print_error("kvm open() error");
        goto out;
    }

    ret_val=ioctl(kvmfd,KVM_CHECK_EXTENSION,KVM_CAP_USER_MEMORY);
    if(ret_val<0){
        print_error("error in KVM_CHECK_EXTENSION");
        goto out;
    }else if(ret_val==0){
        print_error("Required KVM extension KVM_CAP_USER_MEM not available");
        goto out;
    }else {
        //TODO: what does a positive ret_val mean here?
    }
#ifdef debug_on
    print_info("KVM extension version %d\n",ret_val);
#endif

     /*Eventfd config*/
    int eventfd1=eventfd(0,0);
    if(eventfd1<0){
        print_error("error creating eventfd : %s",strerror(errno));
        ret_val=-1;
        goto out;
    }
    lxc_uio_eventfd=eventfd1;

    /*ioctl config*/
    lxc_uio_cdev_fd=open(lxc_uio_cdev_path,O_RDWR); //prefer open() over fopen() in low level code since we don't require buffering, etc. that fopen() provides. Otherwise fopen()0 is better and more portable. To get corresponding fd from FILE* use fileno(FILE*)
    if(lxc_uio_cdev_fd<0){
        print_error("open(%s) error : %s",lxc_uio_cdev_path,strerror(errno));
        ret_val=-1;
        goto out_efd;
    }
    ret_val=register_server(lxc_uio_cdev_fd,lxc_uio_eventfd);
    if(ret_val<0){
     goto out_efd;
    }

    /*server registered*/
#ifdef debug_on
print_info("server registered to proxy module");
#endif
/*initalise internal database which will hold information about modules*/
ret_val=table_init();
if(ret_val<0){
    goto out_deregister;
}

/*set up event fd*/
fd_set readset;
int max_fd=lxc_uio_eventfd;

/*get new entries from proxy module*/
ret_val=1;
while(ret_val>0){
    ret_val=get_new_entry(lxc_uio_cdev_fd);
}
if(ret_val==0){
    //no new entry
}else if(ret_val<0){
    //error
    goto out_table;
}

ret_val=1;
while(ret_val>0){
    ret_val=process_entry(kvmfd,lxc_uio_cdev_fd);
}
if(ret_val==0){
    //no new entry to process
    //okay
}else if(ret_val<0){
    //error
    print_error("process_entry() error : ret_val : %d",ret_val);
    goto out_table;
}else{
    print_error("process_entry() : unexpected error : ret_val %d",ret_val);
    goto out_table;
}

/*no more events to process - wait for notification from proxy module via eventfd*/
while(true){
    FD_ZERO(&readset);
    max_fd=lxc_uio_eventfd;
    FD_SET(lxc_uio_eventfd,&readset);
    print_info("waiting for new events ...");
    ret_val=select(max_fd+1,&readset,NULL,NULL,NULL);
    if(ret_val<0){
        //error
        print_error("error in select() call. errno : %d strerror: %s",errno,strerror(errno));
        goto out_table;
    }
    if(FD_ISSET(lxc_uio_eventfd,&readset)){
        //new event received from proxy module in the kernel
#ifdef debug_on
        print_info("eventfd event received");
#endif
        ret_val=get_new_entry(lxc_uio_cdev_fd);
        if(ret_val==0){
            //no new entry, but then why was select triggered?
            print_error("select() triggered but proxy module has no new entry");
            ret_val=-1;
            goto out_table;
        }else if(ret_val==1) {
            //one event received
            uint64_t temp_efd_counter_value=0; //temporary - not much use - so that eventfd doesn't keep firing
            int ret_val2=read(lxc_uio_eventfd,&temp_efd_counter_value,sizeof(temp_efd_counter_value));

            if(ret_val2!=sizeof(temp_efd_counter_value)){
                //error
                print_error("error reading eventfd counter value");
                goto out_table;
            }

            ret_val2=process_entry(kvmfd,lxc_uio_cdev_fd);

            if(ret_val2==0){
                //we just received a new entry, how can there be no new entry to process?
                print_error("new entry received but process_entry() processed zero events");
                goto out_table;
            }else if(ret_val2==1){
                //okay - one event processed - as expected
                print_info("one entry processed");
            }else if(ret_val2<0){
                //error
                print_error("process_entry() error. ret_val[%d]",ret_val2);
                goto out_table;
            }else{
                //somehing I had not anticipated
                print_error("process_entry() unexpected error. ret_val[%d]",ret_val2);
                goto out_table;
            } //process_entry() if-else end
        }else if(ret_val<0){
            //error
            print_error("get_new_entry() error. return-value[%d]",ret_val);
            goto out_table;
        }else{
            //no other return value possible
            print_error("unexpected value returned by get_new_entry() : %d",ret_val);
            goto out_table;
        }//get new entry if-else end

    } //FD_ISSET() end

}//while(true) end


/*program end - cleanup*/
out_table:
ret_val=table_delete();
out_deregister:
ret_val=deregister_server(lxc_uio_cdev_fd);
if(ret_val<0)
    {print_error("server deregister error");}
#ifdef debug_on
else
    {print_info("server successfully deregistered");}
#endif
close(lxc_uio_cdev_fd);
out_efd:
    lxc_uio_cdev_fd=-1;
    close(lxc_uio_eventfd);
out:
    return ret_val;
}

/*Auxilliary functions*/

void signal_callback_handler(int signum){
    int ret_val=0;
    switch(signum){
    case SIGINT:
        print_warning("SIG_INT signal received. quitting.");
        ret_val=deregister_server(lxc_uio_cdev_fd);
        if(ret_val<0)
            {print_error("server deregister error");}
        #ifdef debug_on
        else
            {print_info("server successfully deregistered");}
        #endif
        close(lxc_uio_cdev_fd);
        lxc_uio_cdev_fd=-1;
        close(lxc_uio_eventfd);
        break;
    default:
        print_warning("signal received, but it isn't SIG_INT. May have undefined behaviour.");
    }
    exit(ret_val);
}

/*process_entry() - single threaded
 * Take one unprocessed entry(in any order) from table and process it
 * RETURN: 0 - no new entry to process
 *         1 - one entry processed
 *         -ve - error
*/
int process_entry(int kvmfd,int chardevfd){ //process the UUID(s) received from kernel
int ret_val=0;
struct UUID_table * ptr=NULL;
long long num_entries=get_table_num_entries(),i=0;

if(num_entries==0)
{
#ifdef debug_on
print_info("process_entry() - table empty");
#endif
ret_val=0;
goto out;
}else if(num_entries<0){
    //error - should never happen
    print_error("negative number of entries in table : table_num_entries[%lld]",num_entries);
    ret_val=-1;
    goto out;
}

for(i=0;i<num_entries;i++){

    ptr=table_entry_at_index_copy(i);

if(ptr==NULL){
//although there are entries in table, as checked by code, still we got null. shouldn't happen
    print_error("table_num_entries > 0 but table_entry_at_index(%lld) returned NULL",i);
    ret_val=-1;
    goto out;
}
if(ptr->processed==true && ptr->could_not_process==true){
    //contradictory conditions - should never occur - BUG_CHECK
    print_error("for table entry at index %lld both processed, and could_not_process are true. contradiction",i);
    ret_val=-1;
    goto out;
}
if(ptr->processed==false && ptr->could_not_process==false){
//this entry needs processing
ret_val=1;

int vmfd=ioctl(kvmfd,KVM_CREATE_VM,0ul);
if(vmfd<0){
    //error
    table_set_xxx(i,could_not_process,true);
    if(errno<0){
        //error, but don't worry since we are already going out
    }
    print_error("KVM_CREATE_VM vmfd returned is %d",vmfd);
    ret_val=-1;
    goto out;
}

table_set_xxx(i,vmfd,vmfd);
if(errno<0){
    //error setting vmfd
    table_set_xxx(i,could_not_process,true);
    if(errno<0){
        //error, but don't worry since we are already going out
    }
    print_error("error setting vmfd for index %lld",i);
    ret_val=-1;
    goto out;
}
/*initialize memory - OLD*/
//unsigned int size=get_uuid_size(ptr->uuid,lxc_uio_cdev_fd);
//if(errno<0){
//    table_set_xxx(i,could_not_process,true);
//    if(errno<0){
//        //error, but don't worry since we are already going out
//    }
//    print_error("error getting size of module");
//    ret_val=errno;
//    goto out;
//}

//uint64_t *mem=mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0);//TODO: PROT_EXEC MAP_LOCKED needed? freeing of this memory?

//if(mem==MAP_FAILED){
//    //error
//    table_set_xxx(i,could_not_process,true);
//    if(errno<0){
//        //error, but don't worry since we are already going out
//    }
//    print_error("mmap() error");
//    ret_val=-1;
//    goto out;
//}
///*setup memory*/
//ret_val=table_add_memory_uuid(ptr->uuid,mem,size,vmfd);
//if(ret_val<0){
//    //error
//    table_set_xxx(i,could_not_process,true);
//    if(errno<0){
//        //error, but don't worry since we are already going out
//    }
//    print_error("table_add_memory_uuid() error. value returned is %d",ret_val);
//    ret_val=-1;
//    goto out;
//}
/*initialize memory - OLD END*/

/*initialize memory*/
ret_val=initialize_memory_uuid(ptr->uuid,vmfd,lxc_uio_cdev_fd);
        if(ret_val<0){
        //error
            print_error("initialize_memory_uuid()");
            table_set_xxx(i,could_not_process,true);
            if(errno<0){
                //error, but don't worry since we are already going out
            }
            goto out;
}else{
            print_info("memory initialized");
        }


/*re-load ptr since entry would have changed by now*/
ptr=table_entry_at_index_copy(i);
if(ptr==NULL){ print_error("file[%s] line[%d]",__FILE__,__LINE__); ret_val=-1; goto out;}

/*initialize memory END*/

/*send packet to proxy module*/
ret_val=send_packet(ptr->uuid,ptr->memory,chardevfd);
if(ret_val<0){
    //error
    //retval is already negative - don't set it to -1
    table_set_xxx(i,could_not_process,true);
    if(errno<0){
        //error, but don't worry since we are already going out
    }
    print_error("send_packet() failed");
    goto out;
}

/*re-load ptr since entry would have changed by now*/
if(ptr!=NULL){free(ptr);}
ptr=table_entry_at_index_copy(i);
if(ptr==NULL){ print_error("file[%s] line[%d]",__FILE__,__LINE__); ret_val=-1; goto out;}



print_info("module binary copied to new user space");

/*initialize CPU*/

int vcpufd = ioctl(vmfd,KVM_CREATE_VCPU,0ul); //last parameter is vcpu ID
if (vcpufd<0){
    //error
    table_set_xxx(i,could_not_process,true);
    if(errno<0){
        //error, but don't worry since we are already going out
    }
    print_error("KVM_CREATE_VCPU vcpufd returned is %d",vcpufd);
    ret_val=-1;
    goto out;
}


table_set_xxx(i,vcpufd,vcpufd);
if(errno<0){
    //error setting vmfd
    table_set_xxx(i,could_not_process,true);
    if(errno<0){
        //error, but don't worry since we are already going out
    }
    print_error("error setting vcpufd for index %lld",i);
    ret_val=-1;
    goto out;
}

ret_val=ioctl(kvmfd,KVM_GET_VCPU_MMAP_SIZE,0);

if (ret_val<0){
    //error
    table_set_xxx(i,could_not_process,true);
    if(errno<0){
        //error, but don't worry since we are already going out
    }
    print_error("KVM_GET_VCPU_MMAP_SIZE ret_val is %d",ret_val);
    ret_val=-1;
    goto out;
}

size_t mmap_size=ret_val;
struct kvm_run * run=NULL;
if(mmap_size<sizeof(*run)){
    print_error("KVM_GET_VCPU_MMAP_SIZE unexpectedly small");
    ret_val=-1;
    goto out;
}
run=mmap(NULL,mmap_size,PROT_READ|PROT_WRITE,MAP_SHARED,vcpufd,0);

if(run==NULL){
    print_error("mmap() vcpufd error");
    ret_val=-1;
    goto out;
}

/*May have to rewrite the following code*/
/*notice how registers are being set - READ/MODIFY/WRITE */
struct kvm_sregs sregs;
ret_val = ioctl(vcpufd,KVM_GET_SREGS,&sregs); /*READ*/
if(ret_val<0){
    print_error("ioctl() error - KVM_GET_SREGS. returned value is %d",ret_val);
    goto out;
}

sregs.cs.base=0;/*MODIFY*/
sregs.cs.selector = 0;

ret_val = ioctl(vcpufd,KVM_SET_SREGS,&sregs); /*WRITE*/

if(ret_val<0){
    print_error("ioctl() error - KVM_SET_SREGS. returned value is %d",ret_val);
    goto out;
}

/*Initialize registers: instruction pointer for out code, addends, and initial flags required by x86 architecture*/

if(ptr!=NULL){free(ptr);}
ptr=table_entry_at_index_copy(i);
if(ptr==NULL){ print_error("file[%s] line[%d]",__FILE__,__LINE__); ret_val=-1; goto out;}

struct kvm_regs regs = {
    //.rip	=	0x1000,
    .rip	=	0x1000, //ptr->memory.core_layout.base, //[sahil]
    .rax	=	2,
    .rbx	=	2,
    .rflags	=	0x2,
};
ret_val = ioctl(vcpufd,KVM_SET_REGS,&regs);
if(ret_val<0){print_error("KVM_SET_REGS"); goto out;}


while(1){
    ret_val=ioctl(vcpufd,KVM_RUN,NULL);

    if(ret_val<0){
        print_error("ioctl() error KVM_RUN");
        goto out;
    }
    switch(run->exit_reason){
case KVM_EXIT_HLT:
        print_info("KVM_EXIT_HLT");
        ret_val=1;
        table_set_xxx(i,processed,true);
        if(errno<0){
            //error, but don't worry since we are already going out
            print_error("error setting table entry's state to processed. entry's index in table [%lld]",i);
            ret_val=-1;
        }

        goto out;
        break;
case KVM_EXIT_IO:
        if(run->io.direction == KVM_EXIT_IO_OUT && run->io.size == 1 && run->io.port == 0x3f8 && run->io.count == 1)
        {
            print_info("KVM_EXIT_IO");
            putchar(*(((char *)run) + run->io.data_offset));
            ret_val=1;
        }else{
            print_error("unhandled KVM_EXIT_IO");
            ret_val=0;
        }
        break;
case KVM_EXIT_FAIL_ENTRY:
        print_error("KVM_EXIT_FAIL_ENTRY : hardware_entry_failure_reason=0x%llx",(unsigned long long)run->fail_entry.hardware_entry_failure_reason);
        ret_val=-1;
        goto out;
        break;
case KVM_EXIT_INTERNAL_ERROR:
        print_error("KVM_EXIT_INTERNAL_ERROR: suberror = 0x%x",run->exit_reason);

        ret_val=-1;
        goto out;
        break;
    }
}

/*run the init function*/


/*set ret_val=1 if 1 module was processed successfully*/
goto out; //all processing related to the module that needed partitioning has ended, finish processing
//this entry needs processing END
}

else if(ptr->could_not_process==true){
//cannot be processed
    print_warning("table entry at index %lld cannot be processed",i);
    continue;
}else if(ptr->processed==true){
    continue;
}

}//for()

/*if code reached here => no element to process*/
ret_val=0;
out:
if(ptr!=NULL){free(ptr);}
return ret_val;
}//process_entry() END


unsigned int get_uuid_size(__uint128_t uuid,int ioctl_fd){
    errno=0;
    unsigned int *size=NULL;

    int ret_val=0;

    ret_val=ioctl(ioctl_fd,kvm_host_server_get_uuid_size,&uuid);//on return uuid will have size
    if(ret_val<0){
        //error
        print_error("kvm_host_server_get_uuid_size ioctl() error, ret_val[%d]",ret_val);
        errno=ret_val;
        return 0;
    }else{
        //success
        errno=0;
        size=(unsigned int *)&uuid;
        return *size;
    }

    //should not reach here
    errno =-1;
    return 0;
}

/*from proxy module*/
/*will be called both on eventfd, and when the server starts*/
/*
 * return value: 1 - one new entry received, 0 - no new entry received, -ve - error
*/
int get_new_entry(int ioctl_fd){
int ret_val=0;
__uint128_t uuid=0;
void *ptr=&uuid;
ret_val=ioctl(ioctl_fd,kvm_host_server_get_pending_module,ptr); //proxy module will write uuid at the given address
if(ret_val<0){
    print_error("error getting new entry from proxy module. ioctl returned with %d",ret_val);
    goto out;
}else if(ret_val==0){
    //no new entry
#ifdef debug_on
    print_info("no new entry present in proxy module");
#endif
    goto out;
} else if(ret_val==1){
    //one new entry received

struct UUID_table entry;
entry.uuid=uuid;
entry.vmfd=-1;
entry.vcpufd=-1;
vm_memory *mptr=new_memory();
if(mptr==NULL){
    //error
    print_error("new_memory() returned NULL pointer");
    ret_val=-1;
    goto out;
}else{
entry.memory=*mptr;
free(mptr);
}
entry.processed=false;
entry.could_not_process=false;
table_put(entry);

#ifdef debug_on
unsigned long long *p=NULL;
p=(unsigned long long *)&uuid;
print_info("new entry : uuid received [%llx-%llx]",*p,*(p+1));
#endif

}else {
    //kernel module can only send 3 values - 0,1 or -ve, if we receive any other => Bug in code
    print_error("unexpected value received from proxy module : %d",ret_val);
}
out:
return ret_val;
}


int get_major_number(void){
    int major_number=-1;

    char path[sizeof("/proc/")+sizeof(lxc_uio_proc_name)]="/proc/";

    strcat(path,lxc_uio_proc_name);

    if(access(path,F_OK)< 0){
        /*File does not exist*/
        print_error("\'%s\' file does not exists. Has the proxy module been loaded?",path);
        major_number=-1;
        goto out;
    }

    if(access(path,R_OK)< 0){
        /*File exist, but we have no read permission on file*/
        print_error("%s file exists, but no read permission.",path);
        major_number=-1;
        goto out;
    }

    FILE *fp=fopen(path,"r");
    if(fp==NULL){
        print_error("not able to open file \'%s\' %s",path,strerror(errno));
        major_number=-1;
        goto out;
    }

    long unsigned int size=500;
    char *line=(char *)malloc(size);
    int len=0;

    if(line==NULL){
        /*malloc() failed*/
        print_error("malloc() failed %s",strerror(errno));
        major_number=-1;
        goto out_fclose;
    }

    /*Major number is on the second line*/

    if(getdelim(&line,&size,'\n',fp) < 0){
        print_error("error getting line from file");
        major_number=-1;
        goto out_clean;
    }

    len=getdelim(&line,&size,'\n',fp);

    if( len < 0){
        print_error("error getting line from file");
        major_number=-1;
        goto out_clean;
    }

    char *p=line;

    while(!isdigit(*p)){
        ++p;
        if(p-line>=len){
            print_error("Major number not found in %s",path);
            major_number=-1;
            goto out_clean;
        }

    }

    long major_number_long = strtol(p,NULL,10);

    /*check for strtol() conversion errors*/

    switch(errno){

    case EINVAL:
        print_error("strtol()  invalid base");
        major_number=-1;
        goto out_clean;
        break;
    case ERANGE:
        print_error("strtol() out of range error.");
        major_number=-1;
        goto out_clean;
        break;
    case 0: //success
        break;
    default:
        /*should never happen since strtol() only produces above 2 errors*/
        /* future proofing? */
        print_error("strtol() unhandled error : %s",strerror(errno));
        major_number=-1;
        goto out_clean;
    }
    int largest_int=~(1u<<(sizeof(int)*8-1));
    if(major_number_long > largest_int){
        /*major number is represented by integer, thus cannot be larger than the largest integer*/
        print_error("major number invalid");
        major_number=-1;
        goto out_clean;
    }

    /*all is well*/
    major_number=major_number_long;
    lxc_uio_cdev_major_number=major_number;

out_clean:
    free(line);

out_fclose:
    /*clean up*/
    if(fclose(fp)!=0){

    switch(errno){
    case 0:
        /*success*/
        break;
    case EBADF:
        /*fail*/
        print_error("fclose() error : %s",strerror(errno));
        break;
    default:
        /*this should never occur*/
        print_error("fclose() error : unhandled condition. wasn't expecting this");
        break;

            }
    }


out:
    return major_number;
}

int register_server(int ioctl_fd,int eventfd){

        int ret_val=0;

        ret_val=ioctl(ioctl_fd,kvm_host_server_register,eventfd); /*fd,device dependent request code,untyped pointer to memory*/
        if(ret_val<0)
        {
            print_error("server register error");
            goto out;
        }
out:
     return ret_val;
}

int deregister_server(int ioctl_fd){

        int ret_val=-1;

        ret_val=ioctl(ioctl_fd,kvm_host_server_de_register,0); /*fd,device dependent request code,untyped pointer to memory*/
        if(ret_val<0)
        {
            print_error("server deregister error");
            goto out;
        }
out:
     return ret_val;
}

/*instead of creating packet and then sending, send all required parameters, so that if defn of packet changes, the corresponding function will also change, and error will occur, thus good for catching bugs*/
/*
 * send packet to proxy module, by
 * writing it to chardev.
 * Return Value: 0 success
                -ve failure
*/
int send_packet(__uint128_t uuid,vm_memory memory,int chardevfd){
       int ret_val=0;
       ssize_t bytes_written=0;
       packet packet1;

       packet1.uuid=uuid;
       packet1.memory=memory;

       bytes_written=write(chardevfd,&packet1,sizeof(packet1));

       if(bytes_written<0){
           //error
           print_error("send_packet() write() error. errno[%d] error-string[%s]",errno,strerror(errno));
           ret_val=-1;
           goto out;
       }else if(bytes_written==sizeof(packet1)){
           //success
           ret_val=0;
           goto out;
       }else{
           print_error("send_packet() write() error. wanted-to-write[%lu] wrote[%ld]",sizeof(packet1),bytes_written);
           ret_val=-1;
           goto out;
       }


       out:
       return ret_val;

}
