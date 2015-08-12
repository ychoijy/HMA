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
	{ 0xfacb6cc4, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x6e215015, __VMLINUX_SYMBOL_STR(__put_task_struct) },
	{ 0xf9a482f9, __VMLINUX_SYMBOL_STR(msleep) },
	{ 0xfe7c4287, __VMLINUX_SYMBOL_STR(nr_cpu_ids) },
	{ 0xc0a3d105, __VMLINUX_SYMBOL_STR(find_next_bit) },
	{ 0x9f36b537, __VMLINUX_SYMBOL_STR(get_rq_task) },
	{ 0xb9249d16, __VMLINUX_SYMBOL_STR(cpu_possible_mask) },
	{ 0xbf8a8ca, __VMLINUX_SYMBOL_STR(mqvec_init) },
	{ 0x43a63fc8, __VMLINUX_SYMBOL_STR(wake_up_process) },
	{ 0xb88f522b, __VMLINUX_SYMBOL_STR(kthread_create_on_node) },
	{ 0x12a38747, __VMLINUX_SYMBOL_STR(usleep_range) },
	{ 0xa6cdfacf, __VMLINUX_SYMBOL_STR(next_zone) },
	{ 0x98255265, __VMLINUX_SYMBOL_STR(first_online_pgdat) },
	{ 0xe12dc2ba, __VMLINUX_SYMBOL_STR(mmput) },
	{ 0xf5893abf, __VMLINUX_SYMBOL_STR(up_read) },
	{ 0x57a6ccd0, __VMLINUX_SYMBOL_STR(down_read) },
	{ 0xa318c588, __VMLINUX_SYMBOL_STR(get_task_mm) },
	{ 0xda3e43d1, __VMLINUX_SYMBOL_STR(_raw_spin_unlock) },
	{ 0x4f68e5c9, __VMLINUX_SYMBOL_STR(do_gettimeofday) },
	{ 0xd52bf1ce, __VMLINUX_SYMBOL_STR(_raw_spin_lock) },
	{ 0xa6a98616, __VMLINUX_SYMBOL_STR(pv_mmu_ops) },
	{ 0xdd98d85b, __VMLINUX_SYMBOL_STR(node_data) },
	{ 0xbd6f03f7, __VMLINUX_SYMBOL_STR(putback_movable_pages) },
	{ 0xd41b5e25, __VMLINUX_SYMBOL_STR(inc_zone_page_state) },
	{ 0x45ce4fde, __VMLINUX_SYMBOL_STR(migrate_pages) },
	{ 0x174750f2, __VMLINUX_SYMBOL_STR(put_page) },
	{ 0x1bc301d5, __VMLINUX_SYMBOL_STR(isolate_lru_page) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x9f9d0e0, __VMLINUX_SYMBOL_STR(alloc_pages_current) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "D84E64C955D40D427C28598");
