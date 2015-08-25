#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
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
	{ 0x84f5f787, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x745ba834, __VMLINUX_SYMBOL_STR(kthread_stop) },
	{ 0x19bba0a7, __VMLINUX_SYMBOL_STR(__put_task_struct) },
	{ 0xf9a482f9, __VMLINUX_SYMBOL_STR(msleep) },
	{ 0x70f4accd, __VMLINUX_SYMBOL_STR(get_rq_task) },
	{ 0xfe7c4287, __VMLINUX_SYMBOL_STR(nr_cpu_ids) },
	{ 0xc0a3d105, __VMLINUX_SYMBOL_STR(find_next_bit) },
	{ 0xb9249d16, __VMLINUX_SYMBOL_STR(cpu_possible_mask) },
	{ 0xbf8a8ca, __VMLINUX_SYMBOL_STR(mqvec_init) },
	{ 0x7355c596, __VMLINUX_SYMBOL_STR(find_pcm_zone) },
	{ 0xf7c73477, __VMLINUX_SYMBOL_STR(wake_up_process) },
	{ 0xde77d496, __VMLINUX_SYMBOL_STR(kthread_create_on_node) },
	{ 0x12a38747, __VMLINUX_SYMBOL_STR(usleep_range) },
	{ 0xb3f7646e, __VMLINUX_SYMBOL_STR(kthread_should_stop) },
	{ 0x57eccbb3, __VMLINUX_SYMBOL_STR(find_dram_zone) },
	{ 0x28527d9, __VMLINUX_SYMBOL_STR(mmput) },
	{ 0x12412917, __VMLINUX_SYMBOL_STR(up_read) },
	{ 0xac26eea8, __VMLINUX_SYMBOL_STR(down_read) },
	{ 0x9763e96a, __VMLINUX_SYMBOL_STR(get_task_mm) },
	{ 0x4f68e5c9, __VMLINUX_SYMBOL_STR(do_gettimeofday) },
	{ 0x81ed8297, __VMLINUX_SYMBOL_STR(prep_isolate_page) },
	{ 0x26ac900d, __VMLINUX_SYMBOL_STR(_raw_spin_unlock) },
	{ 0xd9921248, __VMLINUX_SYMBOL_STR(_raw_spin_lock) },
	{ 0xd959de17, __VMLINUX_SYMBOL_STR(node_data) },
	{ 0x2adce5a3, __VMLINUX_SYMBOL_STR(pv_mmu_ops) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "9523606AF0D192728762898");
