/*
 *
 * code related to handling character device exported in root mount name space
 */
#include "chardev.h" //has IOCTL definitions

dev_t cdev_no;
const int cdev_count=1; //must remeber to set it to 1 at least, else I was getting "No such device or address" error while trying to access it
struct class *cdev_class=NULL;
struct device *cdev_device=NULL;
struct cdev *cdev_cdev=NULL;
DEFINE_MUTEX(cdev_mutex);

struct module_table_struct;
int cdev_open(struct inode *inode, struct file *file);
int cdev_release(struct inode *inode, struct file *file);
ssize_t cdev_read(struct file *file, char __user *buff, size_t count,loff_t *offp);
ssize_t cdev_write(struct file *file, const char __user *buff, size_t count, loff_t *offp);
long cdev_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param);
/*cdev ops*/
struct file_operations cdev_fops={
        .owner = THIS_MODULE,
        .open=cdev_open,
        .release=cdev_release,
        //.read=cdev_read, //read not implemented - for now
        .write=cdev_write,
        .unlocked_ioctl=cdev_ioctl,
};



/*Create chardev in host namespace, using which user space program can interact with lxc-uio-proxy module*/
int __init create_chardev(int *maj_num){
    int ret=0;
    ret=alloc_chrdev_region(&cdev_no,0,cdev_count,cdev_name);
    if(ret<0){
        printk(KERN_ALERT"alloc_chrdev_region() - failed to register a range of char device numbers\n");
        return ret;
    }
    cdev_class=class_create(THIS_MODULE,cdev_class_name);
    if (IS_ERR(cdev_class)){
        printk(KERN_ALERT"class_create() error\n");
        unregister_chrdev_region(cdev_no,cdev_count);
        return PTR_ERR(cdev_class);
    }
    cdev_device=device_create(cdev_class,NULL,cdev_no,NULL,cdev_name);

    if(IS_ERR(cdev_device)){
        printk(KERN_ALERT"device_create() error \n");
        class_destroy(cdev_class);
        unregister_chrdev_region(cdev_no,cdev_count);
        return PTR_ERR(cdev_device);
    }

    cdev_cdev=cdev_alloc();
    if (cdev_cdev==NULL){
        printk(KERN_ALERT"cdev_alloc() error \n");
        device_destroy(cdev_class,cdev_no);
        class_destroy(cdev_class);
        unregister_chrdev_region(cdev_no,cdev_count);
        return -1;
    }

    cdev_init(cdev_cdev,&cdev_fops); /*TODO: is there no error check for this?*/
    ret=cdev_add(cdev_cdev,cdev_no,cdev_count);
    if (ret<0){
        printk(KERN_ALERT"cdev_add() error \n");
        device_destroy(cdev_class,cdev_no);
        class_destroy(cdev_class);
        unregister_chrdev_region(cdev_no,cdev_count);
        return -1;
    }
*maj_num=MAJOR(cdev_no);//similarly we can get minor number using MINOR() macro

#ifdef debug_on
    printk(KERN_DEBUG"lxc-uio-proxy device created \n");
#endif
    return 0;
}

int __exit remove_chardev(void){
    device_destroy(cdev_class,cdev_no);
    class_destroy(cdev_class);
    unregister_chrdev_region(cdev_no,cdev_count);
#ifdef debug_on
    printk(KERN_DEBUG"lxc-uio-proxy device removed \n");
#endif
return 0;
}

int cdev_open(struct inode *inode, struct file *file){
    mutex_lock(&cdev_mutex);/*TODO: check what happens if a program crashes in the middle*/

    mutex_unlock(&cdev_mutex);
    return 0;
}
int cdev_release(struct inode *inode, struct file *file){
    mutex_lock(&cdev_mutex);/*TODO: check what happens if a program crashes in the middle*/

    mutex_unlock(&cdev_mutex);
    return 0;
}
ssize_t cdev_read(struct file *file, char __user *buff, size_t count,loff_t *offp){
    mutex_lock(&cdev_mutex);/*TODO: check what happens if a program crashes in the middle*/

    mutex_unlock(&cdev_mutex);
    printk(KERN_ALERT"[lxc-uio] - cdev_read() Not implemented\n");
    return 0;
}
ssize_t cdev_write(struct file *file, const char __user *buff, size_t count, loff_t *offp){
    mutex_lock(&cdev_mutex);/*TODO: check what happens if a program crashes in the middle*/
    packet packet1;
    int ret_val=0;
    int bytes_to_copy=sizeof(packet1);
    int bytes_not_copied=copy_from_user(&packet1,buff,bytes_to_copy);
    mutex_unlock(&cdev_mutex);

    if(bytes_not_copied==0){
        ret_val=process_packet(packet1);
        if(ret_val==0){
            //success
#ifdef debug_on
            printk(KERN_DEBUG"[lxc-uio] cdev_write() bytes_copied[%ld] process_packet() success\n",bytes_to_copy);
#endif
            return bytes_to_copy;
        }else{
            //error
            printk(KERN_ALERT"[lxc-uio] Error. process_packet() error. ret-val[%d]\n",ret_val);
            return -1;
        }
    }
    else {
        printk(KERN_ALERT"[lxc-uio] Error. Whole struct packet should be sent in single write operation.\n");
        return -1;
    }
    return 0;
}

long cdev_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param){
    int intval=0,retval=0;
    struct file *fl=NULL;

    if(ioctl_num==kvm_host_server_register){


#ifdef debug_on
        printk(KERN_DEBUG"[lxc-uio] New registration from kvm-host-server \n");
#endif
        mutex_lock(&ks_mutex);
        if(ks!=NULL){
            printk(KERN_WARNING"[lxc-uio] ks isn't null, still trying to register a new server. Previous server will be forgotten\n");
            kfree(ks);
            ks=NULL;
        }

        ks=(struct kvm_host_server *)kmalloc(sizeof(struct kvm_host_server),GFP_KERNEL);
        if(ks==NULL){ //out of memory
            retval=-ENOMEM;
            goto out_unlock;
        }
        ks->pid=current->pid;
        intval=(int)ioctl_param;//fd of eventfd
        if(intval<0){
            printk(KERN_ALERT"[lxc-uio] fd of eventfd cannot be zero\n");
            retval=-1;
            goto out_unlock;
        }

        rcu_read_lock();
        fl=fcheck(intval);
        rcu_read_unlock();

        if(fl==NULL){
            //error
            printk(KERN_ALERT"[lxc-uio] Error getting file object corresponding to the fd\n");
            retval=-1;
            goto out_unlock;
        }

        ks->fp=fl;
out_unlock:
        mutex_unlock(&ks_mutex);
        return retval;



    }else if(ioctl_num==kvm_host_server_de_register){


#ifdef debug_on
        printk(KERN_DEBUG"[lxc-uio] De-registration from kvm-host-server \n");
#endif
        mutex_lock(&ks_mutex);
        kfree(ks);
        ks=NULL;
        mutex_unlock(&ks_mutex);
        return 0;


    }else if(ioctl_num==kvm_host_server_get_pending_module){ /*only one at a time - user space will have to issue multiple ioctls to get all*/

        //send a single entry from all the entries of module_table[] which were put before the server got registered
#ifdef debug_on
        printk(KERN_DEBUG"[lxc-uio] ioctl() - kvm_host_server_get_pending_module \n");
#endif

        void *to=(void *)ioctl_param;
        module_table_struct* mt=table_pop();
        int num_mod_to_return=1; // no of entries we are giving to user space - 0 or 1
        if(mt==NULL){
            //no new entry
#ifdef debug_on
            printk(KERN_DEBUG"[lxc-uio] No new entries, table_pop() returned null\n");
#endif
            num_mod_to_return=0;
            retval=0;
            goto skip_copy;
        }
        __uint128_t uuid=0;
        uuid=mt->uuid;
        retval=copy_to_user(to,&uuid,sizeof(uuid));
        if(retval==0){
            //success
        }else{
            //error
            printk(KERN_ALERT"copy_to_user() error.\n");
            retval=-retval; //number of bytes that cannot be copied
            goto end_pop;
        }
        skip_copy:
        retval=(num_mod_to_return==1)?1:0; // we will either return 1 element or 0 element, retval sould indicate that
        return retval;// all ok
        end_pop:
        table_pop_reverse();
        return retval;//error

    }else if(ioctl_num==kvm_host_server_get_uuid_size){

        void *ptr=(void*)ioctl_param; /*this pointer points to memory area of 128 bit- get uuid and send size */
        __uint128_t uuid=0;
        module_table_struct *entry=NULL;

        retval=copy_from_user(&uuid,ptr,128/8); //128 bits to bytes
        if(retval==0){
            //success
            entry=table_get_copy(uuid);
            if(entry==NULL){
                printk(KERN_ALERT"[lxc-uio] ERROR: table_get_copy(uuid) returnd null. invalid uuid?\n");
                retval=-1;
                goto end2;
            }
            unsigned int size=entry->module->core_layout.size+entry->module->init_layout.size;
            retval=copy_to_user(ptr,&size,sizeof(size));
            if(retval!=0){
                //error
                printk(KERN_ALERT"[lxc-uio] copy_to_user() error\n");
                retval=-retval;
                goto end2;
            }

        }else{
            //error
            printk(KERN_ALERT"[lxc-uio] copy_from_user() error\n");
            retval=-retval;
            goto end2;
        }

        end2:
        if(entry!=NULL){kfree(entry);}
        return retval;
    }else if(ioctl_num==kvm_host_server_get_init_layout||ioctl_num==kvm_host_server_get_core_layout){
        void *ptr=(void*)ioctl_param; //guranteed to be of size sizeof(struct module_layout)
        __uint128_t uuid=0;
        struct module_layout layout;
        module_table_struct *entry=NULL;

        retval=copy_from_user(&uuid,ptr,128/8);//128 bits to bytes
        if(retval==0){
            //success
            entry=table_get_copy(uuid);
            if(entry==NULL){
                printk(KERN_ALERT"[lxc-uio] ERROR: table_get_copy(uuid) returnd null. invalid uuid?\n");
                retval=-1;
                goto end3;
            }
            if(ioctl_num==kvm_host_server_get_init_layout){
                layout=entry->module->init_layout;
            }
            else if(ioctl_num==kvm_host_server_get_core_layout){
            layout=entry->module->core_layout;
            }else {
                //will never happen, since these conditions are already checked above
                retval=-1;
                goto end3;
            }

            retval=copy_to_user(ptr,&layout,sizeof(layout));
            if(retval!=0){
                //error
                printk(KERN_ALERT"[lxc-uio] copy_to_user() error\n");
                retval=-retval;
                goto end3;
            }

        }else{
            //error
            printk(KERN_ALERT"[lxc-uio] copy_to_user() error\n");
            retval=-retval;
            goto end3;
        }

    end3:
        if(entry!=NULL){kfree(entry);}
        return retval;
    }
    else{
        //default of switch statement
        printk(KERN_ALERT"[lxc-uio] Error invalid ioctl number\n");
        return -1;

    }

        /*  - switch statement converted to if-else ladder since if can compare variables, switch cannot, and out Major number is a variable.
    switch(ioctl_num){
    case kvm_host_server_register:
#ifdef debug_on
        printk(KERN_DEBUG"[lxc-uio] New registration from kvm-host-server \n");
#endif
        mutex_lock(&ks_mutex);
        if(ks!=NULL){
            printk(KERN_WARNING"[lxc-uio] ks isn't null, still trying to register a new server. Previous server will be forgottenn\n");
            kfree(ks);
            ks=NULL;
        }

        ks=(struct kvm_host_server *)kmalloc(sizeof(struct kvm_host_server),GFP_KERNEL);
        if(ks==NULL){ //out of memory
            retval=-ENOMEM;
            goto out_unlock;
        }
        ks->pid=current->pid;
        intval=(int)ioctl_param;//fd of eventfd
        if(intval<0){
            printk(KERN_ALERT"[lxc-uio] fd of eventfd cannot be zero\n");
            retval=-1;
            goto out_unlock;
        }

        rcu_read_lock();
        fl=fcheck(intval);
        rcu_read_unlock();

        if(fl==NULL){
            //error
            printk(KERN_ALERT"[lxc-uio] Error getting file object corresponding to the fd\n");
            retval=-1;
            goto out_unlock;
        }

        ks->fp=fl;
out_unlock:
        mutex_unlock(&ks_mutex);
        return retval;
        break;

    case kvm_host_server_de_register:
#ifdef debug_on
        printk(KERN_DEBUG"[lxc-uio] De-registration from kvm-host-server \n");
#endif
        kfree(ks);
        ks=NULL;
        return 0;
        break;
    case kvm_host_server_get_pending_modules:
        //send all entries from module_table[] which were put before the server got registered
        //TODO: implement this
        return -1;//not implemented
    default:
        printk(KERN_ALERT"[lxc-uio] Error invalid ioctl number\n");
        return -1;
    }
    */
}
