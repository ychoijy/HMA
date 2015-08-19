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
	{ 0x701183f3, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0xacd5dc4d, __VMLINUX_SYMBOL_STR(kthread_stop) },
	{ 0x7c8fb954, __VMLINUX_SYMBOL_STR(__put_task_struct) },
	{ 0xf9a482f9, __VMLINUX_SYMBOL_STR(msleep) },
	{ 0xfe7c4287, __VMLINUX_SYMBOL_STR(nr_cpu_ids) },
	{ 0xc0a3d105, __VMLINUX_SYMBOL_STR(find_next_bit) },
	{ 0x61f918ab, __VMLINUX_SYMBOL_STR(get_rq_task) },
	{ 0xb9249d16, __VMLINUX_SYMBOL_STR(cpu_possible_mask) },
	{ 0xbf8a8ca, __VMLINUX_SYMBOL_STR(mqvec_init) },
	{ 0xb6cb89c2, __VMLINUX_SYMBOL_STR(wake_up_process) },
	{ 0x3eba395a, __VMLINUX_SYMBOL_STR(kthread_create_on_node) },
	{ 0x12a38747, __VMLINUX_SYMBOL_STR(usleep_range) },
	{ 0xb3f7646e, __VMLINUX_SYMBOL_STR(kthread_should_stop) },
	{ 0xa6cdfacf, __VMLINUX_SYMBOL_STR(next_zone) },
	{ 0x98255265, __VMLINUX_SYMBOL_STR(first_online_pgdat) },
	{ 0x3aa55807, __VMLINUX_SYMBOL_STR(mmput) },
	{ 0xf5893abf, __VMLINUX_SYMBOL_STR(up_read) },
	{ 0x57a6ccd0, __VMLINUX_SYMBOL_STR(down_read) },
	{ 0x806da7a0, __VMLINUX_SYMBOL_STR(get_task_mm) },
	{ 0xda3e43d1, __VMLINUX_SYMBOL_STR(_raw_spin_unlock) },
	{ 0x4f68e5c9, __VMLINUX_SYMBOL_STR(do_gettimeofday) },
	{ 0xd52bf1ce, __VMLINUX_SYMBOL_STR(_raw_spin_lock) },
	{ 0xae1f73c0, __VMLINUX_SYMBOL_STR(node_data) },
	{ 0xb7745cff, __VMLINUX_SYMBOL_STR(inc_zone_page_state) },
	{ 0x15fcc7e4, __VMLINUX_SYMBOL_STR(put_page) },
	{ 0xd9e85285, __VMLINUX_SYMBOL_STR(isolate_lru_page) },
	{ 0x25c16b7, __VMLINUX_SYMBOL_STR(pv_mmu_ops) },
	{ 0x137d3798, __VMLINUX_SYMBOL_STR(alloc_pages_current) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "9A7B2017C758A2D2A70CEA8");
