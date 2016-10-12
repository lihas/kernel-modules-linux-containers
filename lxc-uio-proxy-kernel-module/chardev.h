/*contains IOCTL related definitions
*/


extern int lxc_uio_cdev_major_number; /*has to be defined somewhere, this file makes use of that*/


#define kvm_host_server_register _IOWR(lxc_uio_cdev_major_number,1,int) /* int for receiving eventfd */
/*when the host server starts up, it registers itself using this IOCTL.
During registration, the kernel module takes
*/


#define kvm_host_server_de_register _IO(lxc_uio_cdev_major_number,2)/* server tells that it is going down. */


/*
 * kvm_host_server_get_pending_module
return value : 0 no new elements
               1 one new element being returned
               -ve error
*/
#define kvm_host_server_get_pending_module _IOWR(lxc_uio_cdev_major_number,3,void *) /* for sending uuid from array of elements from module_table[] using pointer to a memory area given by the user space program */

/*kvm_host_server_get_uuid_size
 * void * points to 128 bit user space address which brings in the uuid, and takes away size
 * RETURN : 0 success
 *          -ve fail
*/
/*total size of the module corresponding to the given uuid*/
#define kvm_host_server_get_uuid_size _IOWR(lxc_uio_cdev_major_number,4,void *)

/*
 * kvm_host_server_get_init_layout
 * void * points to sizeof(struct module_layout), this brings in uuid (128 bits), and sends init_layout
 *RETURN : 0 success
 *         -ve fail
*/
#define kvm_host_server_get_init_layout _IOWR(lxc_uio_cdev_major_number,5,void *)



/*
 * kvm_host_server_get_init_layout
 * void * points to sizeof(struct module_layout), this brings in uuid (128 bits), and sends core_layout
 *RETURN : 0 success
 *         -ve fail
*/
#define kvm_host_server_get_core_layout _IOWR(lxc_uio_cdev_major_number,6,void *)
