#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <asm/pgtable.h>
#include <linux/delay.h>

#define IS_DIRTY	1
#define IS_NOT_DIRTY	0

struct task_struct *get_rq_task(int cpu);


int pte_scan(struct mm_struct *mm, unsigned long address)
{
	struct page *page;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	spinlock_t *ptl;

	pgd = pgd_offset(mm, address);
	if(!pgd_present(*pgd)) {
		goto out;
	}
	pud = pud_offset(pgd, address);
	if(!pud_present(*pud)) {
		goto out;
	}
	pmd = pmd_offset(pud, address);
	if(!pmd_present(*pmd)) {
		goto out;
	}

	if (pmd_trans_huge(*pmd)){
		goto out;
	}

	pte = pte_offset_map(pmd, address);

	if(!pte_present(*pte)){
		pte_unmap(pte);
		goto out;
	}

	ptl = pte_lockptr(mm, pmd);
	spin_lock(ptl);

	page = pte_page(*pte);

	(page->overlooked_count)++;

	if (pte_dirty(*pte) && page->dirty_history == IS_NOT_DIRTY) {
		page->dirty_history = IS_DIRTY;
	} else if (!pte_dirty(*pte) && page->dirty_history == IS_DIRTY) {
		printk("::::::::::::    DIRTY -> RESET    ::::::::::\n");
		(page->freq_count)++;
		page->overlooked_count = 0;
		page->dirty_history = IS_NOT_DIRTY;
	}

	printk("page_history = %d, freq_count = %d, overlook_count = %d \n", page->dirty_history, page->freq_count, page->overlooked_count);

	pte_unmap_unlock(pte, ptl);

	return 0;
out:
	//printk("pte access error!!!\n");
	return -1;
}

int vm_scan(struct task_struct *task)
{
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	unsigned long i;
	int ret;
	unsigned long count=0;

	mm = get_task_mm(task);

	if (mm == NULL) {
		printk("No mm_stuct!!\n");
		return 0;
	}

	if (mm->mmap == NULL) {
		mmput(mm);
		printk("No vm_area\n");
		return 0;
	}

	vma = mm->mmap;

	printk("total vm_page number : %lu\n", mm->total_vm);
	while(vma){
		count = 0;
		for(i = vma->vm_start ; i < vma->vm_end ; i += PAGE_SIZE) {
			count++;
			ret = pte_scan(mm, i);
		}
		printk("vm_area_struct_count : %lu\n", count);

		vma = vma->vm_next;
	}

	mmput(mm);
	return 0;
}

int __init init_pte_scan(void)
{
	struct task_struct *curr = NULL;
	int cpu;

	int count = 100;

	while(count--) {
		for_each_possible_cpu(cpu){
			printk("########################   cpu : %d   #########################\n", cpu);
			curr = get_rq_task(cpu);
			if (curr == NULL) {
				printk("curr is NULL\n");
				return 0;
			}
			get_task_struct(curr);
			printk("vm_scan start %s\n", curr->comm);
			vm_scan(curr);
			put_task_struct(curr);
		}
		msleep(1000);
	}

	return 0;
}

void __exit exit_pte_scan(void)
{
}

module_init(init_pte_scan);
module_exit(exit_pte_scan);

MODULE_LICENSE("GPL");
