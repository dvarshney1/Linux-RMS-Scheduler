#ifndef __MP2_GIVEN_INCLUDE__
#define __MP2_GIVEN_INCLUDE__

#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/timer.h>

struct mp2_task_struct {
	struct task_struct *linux_task;
	struct list_head task_node;
	struct timer_list tasl_timer;
	pid_t pid;
	unsigned long period_ms;
	unsigned long compute_time_ms;
	unsigned long deadline_jiff;
	int task_state;
};

struct task_struct* find_task_by_pid(unsigned int nr)
{
    struct task_struct* task;
    rcu_read_lock();
    task=pid_task(find_vpid(nr), PIDTYPE_PID);
    rcu_read_unlock();
    return task;
}

#endif
