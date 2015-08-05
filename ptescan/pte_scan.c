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

#define MAX_EXE		40
#define MQ_MIGRATE_TH	2

#define READ_WEIGHT	4

struct task_struct *get_rq_task(int cpu);
extern int isolate_lru_page(struct page *page);
void get_random_bytes(void *buf, int nbytes);

void print_mq(struct zone *dram_zone, struct zone *pcm_zone)
{
	int i;
	unsigned long count;
	struct page *p;

	printk("====== PCM MQ =====\n");
	for_each_mq(i) {
		count = 0;
		printk("level %d : ", i);
		list_for_each_entry(p, &pcm_zone->mqvec.lists[i], mq) {
			if (p!= NULL) {
				count++;
			}
		}
		printk("%ld\n", count);
	}

	count = 0;
	printk("====== DRAM MQ =====\n");
	for_each_mq(i) {
		count = 0;
		printk("level %d : ", i);
		list_for_each_entry(p, &dram_zone->mqvec.lists[i], mq) {
			if (p!= NULL) {
				count++;
			}
		}
		printk("%ld\n", count);
	}
	printk("=======================\n");
}

struct page *alloc_migrate_to_dram(struct page *page, unsigned long private,
				  int **resultp)
{
	gfp_t gfp_mask = GFP_USER | __GFP_MOVABLE | __GFP_DRAM;

//	printk("%s:%d\n", __func__, __LINE__);

	return alloc_page(gfp_mask);
}
struct page *alloc_migrate_to_pcm(struct page *page, unsigned long private,
				  int **resultp)
{
	gfp_t gfp_mask = GFP_USER | __GFP_MOVABLE;

//	printk("%s:%d\n", __func__, __LINE__);

	return alloc_page(gfp_mask);
}

int migrate_to_dram(struct page *page, struct zone *dram_zone, int level)
{
	int ret;
	LIST_HEAD(source);

	if (!get_page_unless_zero(page)){
		return 0;
	}
	ret = isolate_lru_page(page);
	if (!ret) { /* Success */
//		printk("Succes isolate_lru_page\n");
		put_page(page);
		list_add_tail(&page->lru, &source);
		inc_zone_page_state(page,
				    NR_ISOLATED_ANON + page_is_file_cache(page));
	} else {
		printk(KERN_ALERT "removing page from LRU failed\n");
		put_page(page);
	}
	if (!list_empty(&source)) {
		ret = migrate_pages(&source, alloc_migrate_to_dram,
				    0, MIGRATE_SYNC, MR_MEMORY_HOTPLUG);
		if (ret) {
			putback_movable_pages(&source);
		} else {
			list_move_tail(&page->mq, &dram_zone->mqvec.lists[level]);
		}
	}
	return 1;
}

int migrate_to_pcm(struct page *page)
{
	int ret;
	LIST_HEAD(source);

	if (!get_page_unless_zero(page)){
		return 0;
	}
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
		return 0;
	}
	if (!list_empty(&source)) {
		ret = migrate_pages(&source, alloc_migrate_to_pcm,
				    0, MIGRATE_SYNC, MR_MEMORY_HOTPLUG);
		if (ret)
			putback_movable_pages(&source);
	}

	return 1;
}

void check_promote(struct page *p, struct zone *dram_zone, struct zone *pcm_zone)
{
	int i=0, j=0;
	int pre = p->frq - 1;
	int cur = p->frq;
	struct zone *zone = page_zone(p);
	/*
	 *	The pre 0 CASE
	 *  		1. dram's level 0 because of demotion -> pass
	 *  		2. pcm's level 0 because of demotion -> pass
	 *  		3. wait_list
	 */
	if (pre < 0)
		return;

	if (pre == 0) {
		if (!list_empty(&p->wait)) {	// wait_list -> DRAM or PCM
			list_del_init(&p->wait);

			if (p->pre_level >= MQ_MIGRATE_TH) {	//  wait_list -> DRAM
#ifdef DEBUG
				printk("[%s] : Wait_list -> DRAM\n",
				       __func__);
#endif
				printk("p->prelevel = %d\n",
				       p->pre_level);
				migrate_to_dram(p, dram_zone, 0);

			} else {	// wait_list -> PCM
#ifdef DEBUG
				printk("[%s] : Wait_list -> PCM\n",
				       __func__);
#endif
				list_move_tail(&p->mq, &pcm_zone->mqvec.lists[0]);
			}
		}
	} else {
		for (i=0;pre != 0; i++)
			pre = pre >> 1;

		for (j=0;cur != 0; j++)
			cur = cur >> 1;

		if (i != j) {
			if (j-1 < MQ_LEVEL) {
				//printk("[%s] Move level: %d -> %d\n",
				//       __func__, j-2,j-1);
				p->level = j-1;
				if (!strcmp(zone->name, "Normal")){
					list_move_tail(&p->mq,
						       &dram_zone->mqvec.lists[j-1]);
				} else {
					if ((j-1) >= MQ_MIGRATE_TH) {
						migrate_to_dram(p, dram_zone, j-1);
					} else {
						list_move_tail(&p->mq,
							       &pcm_zone->mqvec.lists[j-1]);
					}

				}
			}
		}
	}

}
void ata_rfmq(struct page *page, struct zone *dram_zone, struct zone *pcm_zone)
{
	check_promote(page, dram_zone, pcm_zone);
}

struct page *pte_scan(struct mm_struct *mm, unsigned long address)
{
	struct page *page;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	pte_t reset_pte;
	unsigned long pfn;
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
		//printk("huge page\n");
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

	if (pte_young(*pte)) {
		if (pte_dirty(*pte)){
			page->frq++;
	//		printk("%s:%d frq = %d\n", __func__, __LINE__,
	//		       page->frq);
			reset_pte = pte_mkclean(*pte);
			set_pte(pte, reset_pte);

		} else {
			page->read_count++;
			if (page->read_count >= READ_WEIGHT) {
				page->frq++;
				page->read_count = 0;
			}
	//		printk("%s:%d read_count = %d\n",__func__, __LINE__,
	//		       page->read_count);
		}
		reset_pte = pte_mkold(*pte);
		set_pte(pte, reset_pte);
	}

	pte_unmap_unlock(pte, ptl);

	return page;
out:
	return NULL;
}


int vm_scan(struct task_struct *task, struct zone *dram_zone, struct zone *pcm_zone)
{
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	unsigned long i;
	int ret = 0;
	struct page *page;
	struct zone *zone;
	unsigned long count = 0;

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

			if (!strcmp(zone->name, "DMA")) {
				continue;
			} else {
				if (!strcmp(zone->name, "PCM")
				    || !strcmp(zone->name, "DMA32")) {
				//	printk("PCM or DMA32\n");
				} else if (!strcmp(zone->name, "Normal")){
					count++;
				//	printk("DRAM\n");
				}
				ata_rfmq(page, dram_zone, pcm_zone);
			}
		}
		vma = vma->vm_next;
	}
	printk("count = %ld\n", count);

	up_read(&mm->mmap_sem);

	return ret;
}

struct zone* find_dram_zone(void)
{
	struct zone *zone;

	for_each_zone(zone) {
		if (!strcmp(zone->name, "Normal")) {
			return zone;
		}
	}
	return NULL;
}

struct zone* find_pcm_zone(void)
{
	struct zone *zone;

	for_each_zone(zone) {
		if (!strcmp(zone->name, "PCM")) {
			return zone;
		}
	}
	return NULL;
}

void mqvec_init(struct mqvec *mqvec) {
	int mq;

	for_each_mq(mq) {
		INIT_LIST_HEAD(&mqvec->lists[mq]);
	}
}

int main_process_scan(void)
{
	struct task_struct *curr = NULL;
	int cpu;
	int count = MAX_EXE;
	LIST_HEAD(pcm_write_list);
	struct zone *dram_zone = NULL, *pcm_zone = NULL;

	dram_zone = find_dram_zone();
	pcm_zone = find_pcm_zone();

	if (dram_zone == NULL || pcm_zone == NULL) {
		printk("NULLNULLNULL\n");
		return 0;
	}

	mqvec_init(&dram_zone->mqvec);
	mqvec_init(&pcm_zone->mqvec);

	while(count--) {
		for_each_possible_cpu(cpu){
			curr = get_rq_task(cpu);
			if (curr == NULL) {
				printk("curr is NULL\n");
				return 0;
			}

			if (!strcmp(curr->comm, "main")){
				printk("############   cpu : %d   ##########\n", cpu);
				get_task_struct(curr);
				printk("%dth vm_scan start %s\n", count, curr->comm);
				vm_scan(curr, dram_zone, pcm_zone);
				put_task_struct(curr);
			}
		}
		print_mq(dram_zone, pcm_zone);
		msleep(1000);
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
