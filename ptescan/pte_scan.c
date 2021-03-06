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
#include <linux/kthread.h>

#define MAX_EXE		500
#define MQ_MIGRATE_TH	2
#define DEMOTE_TH	2
#define LIFE_TIME	100000
#define SLEEP_TIME	100000

#define READ_WEIGHT	4

extern struct task_struct *get_rq_task(int cpu);
extern int isolate_lru_page(struct page *page);
extern int prep_isolate_page(struct page *page);
extern struct zone* find_pcm_zone(void);
extern struct zone* find_dram_zone(void);
extern void flush_tlb_page(struct vm_area_struct *vma, unsigned long start);

struct task_struct *task;

void print_wait(struct zone *pcm_zone)
{
	unsigned long count;
	struct page *p;
	struct page *t;

	count  = 0;
	list_for_each_entry_safe(p, t, &pcm_zone->mqvec.wait_list, wait) {
		if (p != NULL) {
			count ++;
		}
	}

	if (count != 0) {
		printk("====== WAIT_LIST  =====\n");
		printk(" wait num : %ld\n", count);
		printk("=======================\n");
	}
}

void print_victim(struct zone *dram_zone)
{
	unsigned long count;
	struct page *p;
	struct page *t;

	count  = 0;
	list_for_each_entry_safe(p, t, &dram_zone->mqvec.victim_list, victim) {
		if (p != NULL) {
			count ++;
		}
	}

	if (count != 0) {
		printk("====== VICTIM  =====\n");
		printk(" victim num : %ld\n", count);
		printk("=======================\n");
	}
}

void print_mq(struct zone *dram_zone, struct zone *pcm_zone)
{
	int i;
	unsigned long count;
	struct page *p;
	struct page *t;

	printk("====== PCM MQ =====\n");
	for_each_mq(i) {
		count = 0;
		printk("level %d : ", i);
		spin_lock(&dram_zone->mq_lock);
		list_for_each_entry_safe(p, t, &pcm_zone->mqvec.lists[i], mq) {
			if (p!= NULL) {
				count++;
			}
		}
		spin_unlock(&dram_zone->mq_lock);
		printk("%ld\n", count);
	}

	count = 0;
	printk("====== DRAM MQ =====\n");
	for_each_mq(i) {
		count = 0;
		printk("level %d : ", i);
		spin_lock(&dram_zone->mq_lock);
		list_for_each_entry_safe(p, t, &dram_zone->mqvec.lists[i], mq) {
			if (p!= NULL) {
				count++;
			}
		}
		spin_unlock(&dram_zone->mq_lock);
		printk("%ld\n", count);
	}
	printk("=======================\n");
}

void prep_migrate_to_dram(pte_t *pte)
{
	pte_t entry;

	entry = *pte;
	entry = pte_mknotpresent(entry);
	entry = pte_mkdrammigration(entry);
	set_pte(pte, entry);
}

void check_promote(pte_t *pte, struct zone *dram_zone, struct zone *pcm_zone)
{
	struct page *p = pte_page(*pte);
	struct zone *zone = page_zone(p);
	int i=0, j=0;
	int pre = p->frq - 1;
	int cur = p->frq;
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
			spin_lock(&dram_zone->mq_lock);
			list_del_init(&p->wait);
			spin_unlock(&dram_zone->mq_lock);
			p->level = 0;
			if (p->pre_level >= MQ_MIGRATE_TH) {	//  wait_list -> DRAM
#ifdef DEBUG
				printk("[%s] : Wait_list -> DRAM\n",
				       __func__);
#endif
//				printk("p->prelevel = %d\n",
//				       p->pre_level);
				if (prep_isolate_page(p)) {
					prep_migrate_to_dram(pte);
				}

			} else {	// wait_list -> PCM
#ifdef DEBUG
				printk("[%s] : Wait_list -> PCM\n",
				       __func__);
#endif

				spin_lock(&dram_zone->mq_lock);
				list_move_tail(&p->mq, &pcm_zone->mqvec.lists[0]);
				spin_unlock(&dram_zone->mq_lock);
			}
		}
	} else {
		for (i=0;pre != 0; i++)
			pre = pre >> 1;

		for (j=0;cur != 0; j++)
			cur = cur >> 1;

		if (i != j) {
			p->level = j-1;
			if (p->level < MQ_LEVEL) {
				if (!strcmp(zone->name, "Normal")){
					spin_lock(&dram_zone->mq_lock);
					list_move_tail(&p->mq,
						       &dram_zone->mqvec.lists[p->level]);

					spin_unlock(&dram_zone->mq_lock);
				} else {
					if ((j-1) >= MQ_MIGRATE_TH) {
						if (prep_isolate_page(p)) {
							prep_migrate_to_dram(pte);
						}
					} else {
						spin_lock(&dram_zone->mq_lock);
						list_move_tail(&p->mq,
							       &pcm_zone->mqvec.lists[p->level]);
						spin_unlock(&dram_zone->mq_lock);
					}

				}
			}
		}
	}

}

void write_op(struct page *page, pte_t *pte){
	pte_t reset_pte;

	page->frq++;
	reset_pte = pte_mkclean(*pte);
	set_pte(pte, reset_pte);
}

void read_op(struct page *page){
	page->read_count++;
	if (page->read_count >= READ_WEIGHT) {
		page->frq++;
		page->read_count = 0;
	}
}

int pte_scan(struct mm_struct *mm, struct vm_area_struct *vma,
	     unsigned long address, struct zone *dram_zone, struct zone *pcm_zone)
{
	struct page *page;
	struct zone *zone;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	pte_t reset_pte;
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

	page = pte_page(*pte);

	if (pte_young(*pte)) {
		if (pte_dirty(*pte)){
			write_op(page, pte);
		} else {
			read_op(page);
		}
		reset_pte = pte_mkold(*pte);
		set_pte(pte, reset_pte);

		do_gettimeofday(&page->expire_time);
		page->demote_count = 0;
	}

	zone = page_zone(page);

	if (!strcmp(zone->name, "DMA")) {
		//
	} else {
		if (!strcmp(zone->name, "PCM")
		    || !strcmp(zone->name, "DMA32")) {
			//	printk("PCM or DMA32\n");
		} else if (!strcmp(zone->name, "Normal")){
			//	printk("DRAM\n");
		}

		check_promote(pte, dram_zone, pcm_zone);
	}

	flush_tlb_page(vma, address);
	pte_unmap_unlock(pte, ptl);

	return 1;
out:
	return 0;
}

int vm_scan(struct task_struct *task, struct zone *dram_zone, struct zone *pcm_zone)
{
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	unsigned long addr;
	int ret = 0;

	mm = get_task_mm(task);

	if (mm == NULL) {
		printk("No mm_stuct!!\n");
		return 0;
	}

	vma = mm->mmap;

	if (vma == NULL) {
		mmput(mm);
		printk("VMA is NULL!!\n");
		return 0;
	}

	down_read(&mm->mmap_sem);

	while(vma){
		for(addr = vma->vm_start ; addr < vma->vm_end; addr += PAGE_SIZE) {
			ret = pte_scan(mm, vma, addr, dram_zone, pcm_zone);
		}
		vma = vma->vm_next;
	}

	up_read(&mm->mmap_sem);

	mmput(mm);
	return 1;
}

void demote_check(struct zone *dram_zone,
		  struct list_head *src, struct list_head *victim_list){
	struct timeval cur_time;
	struct page *page;
	do_gettimeofday(&cur_time);

	list_for_each_entry(page, src, mq){
		if (cur_time.tv_sec != page->expire_time.tv_sec ||
		    cur_time.tv_usec - page->expire_time.tv_usec > LIFE_TIME) {
			page->demote_count++;
			do_gettimeofday(&page->expire_time);
		}

		if (page->demote_count >= DEMOTE_TH) {
			if (list_empty(&page->victim)) {
				spin_lock(&dram_zone->mq_lock);
				list_move_tail(&page->victim,
					       victim_list);
				spin_unlock(&dram_zone->mq_lock);
			}
		}
	}
}

static int aging(void *data)
{
	struct zone *dram_zone;
	struct mqvec *dram_mqvec;
	int level;

	dram_zone = find_dram_zone();
	dram_mqvec = &dram_zone->mqvec;

	while (!kthread_should_stop()) {
		for (level = 0; level < MQ_LEVEL; level++) {
			if (!list_empty(&dram_mqvec->lists[level])) {
				demote_check(dram_zone,
					     &dram_mqvec->lists[level],
					     &dram_mqvec->victim_list);
			}
		}

		usleep_range(SLEEP_TIME,SLEEP_TIME);
	}
	return 0;
}

void create_aging_th(void)
{

	task = kthread_run(aging, NULL, "aging");
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

	create_aging_th();

	while(count--) {
		for_each_possible_cpu(cpu){
			curr = get_rq_task(cpu);

			if (!strcmp(curr->comm, "main")){
				printk("############   cpu : %d   ##########\n", cpu);
				get_task_struct(curr);
				printk("%dth vm_scan start %s\n", count, curr->comm);
				vm_scan(curr, dram_zone, pcm_zone);
				put_task_struct(curr);
				print_wait(pcm_zone);
				print_mq(dram_zone, pcm_zone);
				print_victim(dram_zone);
			}
		}
		msleep(100);
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
	if (task) {
		kthread_stop(task);
		task = NULL;
	}
}

module_init(init_pte_scan);
module_exit(exit_pte_scan);

MODULE_LICENSE("GPL");
