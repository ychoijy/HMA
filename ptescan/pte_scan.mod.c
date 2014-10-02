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
	{ 0x9b83dafd, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x4a9152f1, __VMLINUX_SYMBOL_STR(current_task) },
	{ 0x23d5a8f, __VMLINUX_SYMBOL_STR(__put_task_struct) },
	{ 0x1bbe925c, __VMLINUX_SYMBOL_STR(get_rq_task) },
	{ 0xf9a482f9, __VMLINUX_SYMBOL_STR(msleep) },
	{ 0xfe7c4287, __VMLINUX_SYMBOL_STR(nr_cpu_ids) },
	{ 0xc0a3d105, __VMLINUX_SYMBOL_STR(find_next_bit) },
	{ 0xb9249d16, __VMLINUX_SYMBOL_STR(cpu_possible_mask) },
	{ 0x6ae6279d, __VMLINUX_SYMBOL_STR(mmput) },
	{ 0xf394153a, __VMLINUX_SYMBOL_STR(inc_zone_page_state) },
	{ 0x68a75634, __VMLINUX_SYMBOL_STR(put_page) },
	{ 0xdd08201c, __VMLINUX_SYMBOL_STR(isolate_lru_page) },
	{ 0xbd6f03f7, __VMLINUX_SYMBOL_STR(putback_movable_pages) },
	{ 0xf5893abf, __VMLINUX_SYMBOL_STR(up_read) },
	{ 0xe9c56ecf, __VMLINUX_SYMBOL_STR(migrate_pages) },
	{ 0xb189da9f, __VMLINUX_SYMBOL_STR(node_data) },
	{ 0x57a6ccd0, __VMLINUX_SYMBOL_STR(down_read) },
	{ 0x5f29afe1, __VMLINUX_SYMBOL_STR(get_task_mm) },
	{ 0xda3e43d1, __VMLINUX_SYMBOL_STR(_raw_spin_unlock) },
	{ 0xd52bf1ce, __VMLINUX_SYMBOL_STR(_raw_spin_lock) },
	{ 0xb4511f39, __VMLINUX_SYMBOL_STR(pv_mmu_ops) },
	{ 0x5ce01aeb, __VMLINUX_SYMBOL_STR(alloc_pages_current) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "819DBE1285CA7DB6C7D1EB3");
