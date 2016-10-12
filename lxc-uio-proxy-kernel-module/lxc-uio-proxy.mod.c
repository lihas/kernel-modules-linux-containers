#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xeb9dcc92, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x2e5255f9, __VMLINUX_SYMBOL_STR(single_release) },
	{ 0x9a8735a8, __VMLINUX_SYMBOL_STR(seq_read) },
	{ 0x8c75b67, __VMLINUX_SYMBOL_STR(seq_lseek) },
	{ 0x941f2aaa, __VMLINUX_SYMBOL_STR(eventfd_ctx_put) },
	{ 0xdf0f75c6, __VMLINUX_SYMBOL_STR(eventfd_signal) },
	{ 0x862bcd13, __VMLINUX_SYMBOL_STR(eventfd_ctx_fileget) },
	{ 0x7195c878, __VMLINUX_SYMBOL_STR(remove_proc_entry) },
	{ 0x5319c70d, __VMLINUX_SYMBOL_STR(proc_create_data) },
	{ 0x9e62ee8, __VMLINUX_SYMBOL_STR(lxc_uio_proxy_put_module) },
	{ 0x6430637, __VMLINUX_SYMBOL_STR(find_symbol) },
	{ 0x4c156e94, __VMLINUX_SYMBOL_STR(module_mutex) },
	{ 0xdb7305a1, __VMLINUX_SYMBOL_STR(__stack_chk_fail) },
	{ 0x2ef0e183, __VMLINUX_SYMBOL_STR(current_task) },
	{ 0x4f8b5ddb, __VMLINUX_SYMBOL_STR(_copy_to_user) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0x4f6b400b, __VMLINUX_SYMBOL_STR(_copy_from_user) },
	{ 0x4db54248, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0x237d03ad, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0x7c2d098f, __VMLINUX_SYMBOL_STR(krealloc) },
	{ 0xd2b09ce5, __VMLINUX_SYMBOL_STR(__kmalloc) },
	{ 0x5f122761, __VMLINUX_SYMBOL_STR(device_destroy) },
	{ 0x29c28ad8, __VMLINUX_SYMBOL_STR(cdev_add) },
	{ 0x9d1bde47, __VMLINUX_SYMBOL_STR(cdev_init) },
	{ 0x49b450a1, __VMLINUX_SYMBOL_STR(cdev_alloc) },
	{ 0x5c2c45e9, __VMLINUX_SYMBOL_STR(class_destroy) },
	{ 0x45d942a0, __VMLINUX_SYMBOL_STR(device_create) },
	{ 0x7485e15e, __VMLINUX_SYMBOL_STR(unregister_chrdev_region) },
	{ 0xc70469d7, __VMLINUX_SYMBOL_STR(__class_create) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x29537c9e, __VMLINUX_SYMBOL_STR(alloc_chrdev_region) },
	{ 0xe96b5235, __VMLINUX_SYMBOL_STR(seq_printf) },
	{ 0xa7d5b7c8, __VMLINUX_SYMBOL_STR(single_open) },
	{ 0x5131f665, __VMLINUX_SYMBOL_STR(mutex_unlock) },
	{ 0x9970a00, __VMLINUX_SYMBOL_STR(mutex_lock) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "6116661F5C266355F4C8BF3");
