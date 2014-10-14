#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <asm/pgtable.h>
#include <linux/delay.h>
#include <linux/migrate_mode.h>
#include <linux/migrate.h>
#include <linux/mm_inline.h>
#include <linux/mmzone.h>

#define MAX_EXE		50
#define IS_DIRTY	1
#define IS_NOT_DIRTY	0

#define COUNT_SHIFT	34
#define COUNT_MASK	((1UL << COUNT_SHIFT) - 1)

#define MAKE_PFN_COUNT(ref, count)	\
	(unsigned long)(((count) << COUNT_SHIFT) | (COUNT_MASK & (ref)))

#define GET_COUNT(ref)	\
	((unsigned long)(ref) >> COUNT_SHIFT)

struct task_struct *get_rq_task(int cpu);
extern int isolate_lru_page(struct page *page);
void get_random_bytes(void *buf, int nbytes);


struct page *alloc_migrate_to_dram(struct page *page, unsigned long private,
				  int **resultp)
{
	gfp_t gfp_mask = GFP_USER | __GFP_MOVABLE;

	printk("%s:%d\n", __func__, __LINE__);

	return alloc_page(gfp_mask);
}
struct page *alloc_migrate_to_pcm(struct page *page, unsigned long private,
				  int **resultp)
{
	gfp_t gfp_mask = GFP_USER | __GFP_MOVABLE | __GFP_PCM;

	printk("%s:%d\n", __func__, __LINE__);

	return alloc_page(gfp_mask);
}
struct page *pte_scan(struct mm_struct *mm, unsigned long address)
{
	struct page *page;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	unsigned long pfn;
	spinlock_t *ptl;
	unsigned long count;

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
/*
 *  I will add page's information here
 */
	pfn = pte_pfn(*pte);
	page = pte_page(*pte);

	printk("before pfn : %0#10lx\n", pfn);
	count = GET_COUNT(pfn);
	count++;
	pfn = MAKE_PFN_COUNT(pfn, count);
	printk("after pfn : %0#10lx, count : %lu\n", pfn, count);
/*
	if (pte_young(*pte)) {
		if (pte_dirty(*pte)){
			if(page->dirty_history == 0) {
				page->dirty_history = 1;
				printk("%s:%d freq_count : %d\n", __func__, __LINE__,
				       page->freq_count);
			} else {
				printk("%s:%d dirty_history is 1\n", __func__, __LINE__);
			}
		} else {
			page->dirty_history = 0;
			printk("%s:%d dirty_history reset\n",__func__, __LINE__);
		}
	}
*/

	pte_unmap_unlock(pte, ptl);

	return page;
out:
	return NULL;
}


int vm_scan(struct task_struct *task)
{
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	unsigned long i;
	int ret = 0;
	struct page *page;
	struct zone *zone;

	LIST_HEAD(source);

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

	down_read(&mm->mmap_sem);

	vma = mm->mmap;

	while(vma){
		for(i = vma->vm_start ; i < vma->vm_end; i += PAGE_SIZE) {
			page = pte_scan(mm, i);
			if (page == NULL) continue;

			zone = page_zone(page);

			if (!get_page_unless_zero(page)){
				continue;
			}
#if 0
			if (!strcmp(zone->name, "PCM")){
				printk("PCM\n");
				ret = isolate_lru_page(page);
				if (!ret) { /* Success */
					printk("Succes isolate_lru_page\n");
					put_page(page);
					list_add_tail(&page->lru, &source);
					inc_zone_page_state(page,
						NR_ISOLATED_ANON + page_is_file_cache(page));
				} else {
					printk(KERN_ALERT "removing page from LRU failed\n");
					put_page(page);
					continue;
				}
			} else if (!strcmp(zone->name, "Normal")){
				printk("Normal\n");
			} else {
				printk("Another\n");
#endif

#if 1
			if (!strcmp(zone->name, "Normal")){
				printk("Normal\n");
				ret = isolate_lru_page(page);
				if (!ret) { /* Success */
					printk("Succes isolate_lru_page\n");
					put_page(page);
					list_add_tail(&page->lru, &source);
					inc_zone_page_state(page,
						NR_ISOLATED_ANON + page_is_file_cache(page));
				} else {
					printk(KERN_ALERT "removing page from LRU failed\n");
					put_page(page);
					continue;
				}

			} else if (!strcmp(zone->name, "PCM")){
				printk("PCM\n");
			} else {
				printk("Another\n");
			}
#endif
		}

		vma = vma->vm_next;
	}

	if (!list_empty(&source)) {
#if 0
		ret = migrate_pages(&source, alloc_migrate_to_dram,
				    0, MIGRATE_SYNC, MR_MEMORY_HOTPLUG);
#endif
#if 1
		ret = migrate_pages(&source, alloc_migrate_to_pcm, 0,
				    MIGRATE_SYNC, MR_MEMORY_HOTPLUG);
#endif
		if (ret)
			putback_movable_pages(&source);
	}

	up_read(&mm->mmap_sem);
	return ret;
}


int main_process_scan(void)
{
	struct task_struct *curr = NULL;
	int cpu;
	int count = MAX_EXE;
	LIST_HEAD(pcm_write_list);

	while(count--) {
		for_each_possible_cpu(cpu){
			printk("############   cpu : %d   ##########\n", cpu);
			curr = get_rq_task(cpu);
			if (curr == NULL) {
				printk("curr is NULL\n");
				return 0;
			}

		//	if (!strcmp(curr->comm, "main")){
				get_task_struct(curr);
				printk("%dth vm_scan start %s\n", count, curr->comm);
				vm_scan(curr);
				put_task_struct(curr);
		//	}
		}
		msleep(5000);
	}

	return 0;
}

int __init init_pte_scan(void)
{
	main_process_scan();

	return 0;
}

void __exit exit_pte_scan(void)
{
}

module_init(init_pte_scan);
module_exit(exit_pte_scan);

MODULE_LICENSE("GPL");
