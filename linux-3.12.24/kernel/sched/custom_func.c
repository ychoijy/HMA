#include <linux/kernel.h>
#include "sched.h"

struct task_struct *get_rq_task(int cpu)
{
	return cpu_curr(cpu);
}
EXPORT_SYMBOL(get_rq_task);
