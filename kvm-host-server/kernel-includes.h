#ifndef KERNELINCLUDES_H
#define KERNELINCLUDES_H
/* stuff copied from linux kernel (4.7.3) - /usr/src/linux-headers-4.7.3/include/ */


struct module_layout {
        /* The actual code + data. */
        void *base;
        /* Total size. */
        unsigned int size;
        /* The size of the executable code.  */
        unsigned int text_size;
        /* Size of RO section of the module (text+rodata) */
        unsigned int ro_size;


        /*Removing struct mod_tree_node mtn; although it in use in the kernel I am compiling since
         * copying it here requires a lot of other code to be copie from kernel, which seems impossible at the moment
         * Instead of it a char buffer[] is created so that sizeof() returns the correct value
*/
//#ifdef CONFIG_MODULES_TREE_LOOKUP - commented out since it was set in kernel's config (linux-4.7.3 make olddefconfig)
//        struct mod_tree_node mtn;
//#endif
        //new addition by me - not in original code - to maintain sizeof() value
        char buff[56]; //to maintain sizeof() after removing  struct mod_tree_node mtn;
};

#endif // KERNELINCLUDES_H

