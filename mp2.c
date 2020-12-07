#define LINUX

#include <linux/module.h>
#include <linux/kernel.h>
#include "mp2_given.h"
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
// #include <sys/types.h>
// #include <sys/kmem.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/kthread.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daksh Varshney");
MODULE_DESCRIPTION("CS-423 MP2");

#define DEBUG 1
#define FILENAME "status"   //name of the filename 
#define DIRECTORY "mp2"     //name of the directory 
#define COMMA ","
#define SLAB_CACHE_NAME "mp2_cache_name"
#define MAX_BUFFER_LENGTH 200
#define SHIFT_AMOUNT 100000
#define SHIFT_MASK ((1 << SHIFT_AMOUNT) - 1)
#define ADMISSION_CONTROL_REGION 69300

/**********************************
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
**********************************/

/* Global Variables */
struct proc_dir_entry *proc_dir;
struct proc_dir_entry *proc_entry;
spinlock_t my_lock;
spinlock_t thread_lock;
struct mp2_task_struct *mp2_global_struct;
struct kmem_cache *mp2_slab_cache; 
struct task_struct *my_kernel_thread;
static long LARGE_VALUE = 10000000000; 
struct mp2_task_struct *task_currently_running = NULL;


//Function Declarations
static ssize_t mp2_read (struct file *file, char __user *buffer, size_t count, loff_t *data);
static ssize_t mp2_write (struct file *file, const char __user *buffer, size_t count, loff_t *data);
int __init mp2_init(void);
void __exit mp2_exit(void);
void callback_registration(char *buf, size_t count);
void callback_yield(char *buf);
void callback_deregistration(char *buf, size_t count);
void get_PID_PERIOD_COMPUTATION(char *buf, int *PID_ptr, int *PERIOD_ptr, int *COMPUTATION_ptr, size_t count);
void callback_timer(unsigned long PID);
void get_PID(char *buf, int *PID_ptr, size_t count);
int thread_function(void *data);

LIST_HEAD(list_head);

enum task_state {READY, RUNNING, SLEEPING};

static const struct file_operations mp2_file = {
	.owner = THIS_MODULE,
	.read = mp2_read,
	.write = mp2_write	
};

/**********************************************************
static kmem_cache_t mp2_cache = {
	slabs_full:	LIST_HEAD_INIT(mp2_cache.slabs_full),
	slabs_partial:	LIST_HEAD_INIT(mp2_cache.slabs_partial),
	slabs_free:	LIST_HEAD_INIT(mp2_cache.slabs_free),
	objsize:	sizeof(struct mp2_task_struct),
	flags:		SLAB_NO_REAP,
	spinlock:	SPIN_LOCK_UNLOCKED,
	colour_off:	L1_CACHE_BYTES,
	name:		"mp2_kmem_cache",
}; 
***********************************************************/

void callback_timer(unsigned long PID){
	//DO STUFF HERE (top half)
	struct mp2_task_struct *temp_task;
	struct mp2_task_struct *my_task;
	struct list_head *pos;
	
	my_task = NULL;

	//iterate over the list to find a task with a matching PID
	spin_lock_irq(&my_lock);
	list_for_each(pos, &list_head) {
		temp_task = list_entry(pos, struct mp2_task_struct, task_node);
		if(temp_task->pid == PID){
			my_task = temp_task;
			break;
		}
	}
	
	//sanity check to prevent seg faults
	if(my_task) {my_task->task_state = READY;}

	//wake up the dispatcher thread
	wake_up_process(my_kernel_thread);
	spin_unlock_irq(&my_lock);
}

int thread_function(void *data){
	//Thread callbacll function (kernel)
	// printk(KERN_ALERT "Callback Function Started");
	struct mp2_task_struct *next_task;
	struct mp2_task_struct *temp_task;
	struct list_head *pos;
	int min_period;
	struct sched_param sparam_old;
	struct sched_param sparam_new;

	while(1) {
		//sleep the dispathching thread
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
		
		//critical section start
		spin_lock_irq(&my_lock);
		min_period = LARGE_VALUE;
		next_task = NULL;

		//itearte over the linked list to find a READY task
		list_for_each(pos, &list_head) {
			temp_task = list_entry(pos, struct mp2_task_struct, task_node);
			if(temp_task->task_state == READY && temp_task->period_ms < min_period){
				next_task = temp_task;
				min_period = temp_task->period_ms;
			}
		}

		//2 cases, if found and if not found
		if(!next_task && task_currently_running != NULL){
			//next_task does not exist
			//preempt current task
			task_currently_running->task_state = READY;
			sparam_old.sched_priority = 0;
			sched_setscheduler(task_currently_running->linux_task, SCHED_NORMAL, &sparam_old);
			task_currently_running->task_state = RUNNING;
			wake_up_process(task_currently_running->linux_task);
			sparam_new.sched_priority = 99;
			sched_setscheduler(task_currently_running->linux_task, SCHED_FIFO, &sparam_new);
		}
		else if(task_currently_running){
			//preempt current task, change the currently running task
			task_currently_running->task_state = READY;
			next_task->task_state = RUNNING;
			sparam_old.sched_priority = 0;
			sched_setscheduler(task_currently_running->linux_task, SCHED_NORMAL, &sparam_old);
			wake_up_process(next_task->linux_task);
			sparam_new.sched_priority = 99;
			sched_setscheduler(next_task->linux_task, SCHED_FIFO, &sparam_new);
			task_currently_running = next_task;
		}
		//end of critical section
		spin_unlock_irq(&my_lock);
	}
	return 0;
}

void get_PID_PERIOD_COMPUTATION(char *buf, int *PID_ptr, int *PERIOD_ptr, int *COMPUTATION_ptr, size_t count){
	int relative_index = 0, answer_index = 0, which = 0;
	char *start = buf+3;
	char *answer = (char *)kmalloc(count+1, GFP_KERNEL);
	memset(answer, 0, count+1);

	//get all the 3 values from registration callnack handler
	while(start[relative_index] != '\0'){
		answer[answer_index] = start[relative_index];
		if(start[relative_index] == ','){
			//switch to assign to separate variables 
			switch(which){
				case 0: 
					kstrtol(answer, 10, (long *)PID_ptr);
					which++;
					break;
				case 1:
					kstrtol(answer, 10, (long *)PERIOD_ptr);
					which++;
					break;
			}	
			memset(answer, 0, count+1);
			answer_index = 0;
			relative_index += 2;
			continue;
		}
		answer_index++;
		relative_index++;	
	}
	//convert from string to int base 10
	kstrtol(answer, 10, (long *)COMPUTATION_ptr);
	printk("PID: %d", *PID_ptr);
	printk("PERIOD: %d", *PERIOD_ptr);
	printk("COMPUTATION: %d", *COMPUTATION_ptr);
	kfree(answer);
}

void get_PID(char *buf, int *PID_ptr, size_t count){
	int answer_index = 0;
	char *start = buf+3;
	char *answer = (char*)kmalloc(count+1, GFP_KERNEL);
	memset(answer, 0, count+1);
	//find the PID value from buf
	while(start[answer_index] != '\0'){
		answer[answer_index] = start[answer_index];
		answer_index++;
	}
	//convert from string to int base 10
	kstrtol(answer, 10, (long *)PID_ptr);
	kfree(answer);
}

void callback_registration(char *buf, size_t count){
	//printk(KERN_ALERT "Inside Registration Function\n");
	
	//registration function implementation
	int PID;
	char m;
	unsigned long PERIOD;
	unsigned long COMPUTATION;
	unsigned long C;
	unsigned long P;
	unsigned long admission_control_value;
	struct list_head *pos;
	struct mp2_task_struct *task_pointer;
	
	//retrieve, PID, PERIOD, COMPUTATION
	sscanf(buf, "%c%*[,] %d%*[,] %lu%*[,] %lu", &m, &PID, &PERIOD, &COMPUTATION);
	/************************
	CHECK FOR ADMISSION CONTROL HERE
	************************/
	admission_control_value = 0;
	C=0;
	P=0;
	list_for_each(pos, &list_head){
		task_pointer = list_entry(pos, struct mp2_task_struct, task_node);
		C = task_pointer->compute_time_ms;
		P = task_pointer->period_ms;
		admission_control_value += (C * SHIFT_AMOUNT)/P;
	}
	C = COMPUTATION;
	P = PERIOD;
	
	//right shift (multiple so that 10^5 first places are decimals
	admission_control_value += (C * SHIFT_AMOUNT)/P;
	if(admission_control_value >= ADMISSION_CONTROL_REGION){
		printk("No Entry for non_admission control\n");
		return;
	}
	/****************************/
	mp2_global_struct = kmem_cache_alloc(mp2_slab_cache, GFP_KERNEL);
	// get_PID_PERIOD_COMPUTATION(buf, &PID, &PERIOD, &COMPUTATION, count);

	printk(KERN_ALERT "Reached Here\n");
	printk(KERN_ALERT "PID: %d, PERIOD: %lu, COMPUTATION: %lu\n", PID, PERIOD, COMPUTATION);

	//assign all teh parameters to this struct
	mp2_global_struct->task_state = SLEEPING;
	mp2_global_struct->pid = PID;
	mp2_global_struct->period_ms = PERIOD;
	mp2_global_struct->compute_time_ms = COMPUTATION;
	mp2_global_struct->deadline_jiff = 0;
	mp2_global_struct->linux_task = find_task_by_pid(PID);
	
	//initialise timer, add to list 
	init_timer(&(mp2_global_struct->tasl_timer));
	mp2_global_struct->tasl_timer.expires = jiffies;
	mp2_global_struct->tasl_timer.function = callback_timer;
	mp2_global_struct->tasl_timer.data = PID;

	INIT_LIST_HEAD(&(mp2_global_struct->task_node));
	list_add(&(mp2_global_struct->task_node), &list_head);
	
	mp2_global_struct = NULL;
	//for debugging
	printk(KERN_ALERT "Registration Function SucessFully Ended\n");
}

void callback_yield(char *buf){
	//printk(KERN_ALERT "Inside Yield Function\n");
	
	//yield function implementation
	int PID;
	int previous_task_deadline;
	int new_deadline;
	char m;
	struct list_head *pos;
	struct mp2_task_struct *task_pointer;	

	sscanf(buf, "%c%*[,] %d", &m, &PID);
	printk(KERN_ALERT "PID; %d\n", PID);
	
	//iterate over the list to see process to yield which matches the PID
	list_for_each(pos, &list_head){
		task_pointer = list_entry(pos, struct mp2_task_struct, task_node);
		if(task_pointer->pid == PID){
			mp2_global_struct = task_pointer;
			break;
		}
	}
	
	//update the deadline (jiffs)
	previous_task_deadline = mp2_global_struct->deadline_jiff;
	if(previous_task_deadline != 0){
		new_deadline = previous_task_deadline + jiffies + msecs_to_jiffies(mp2_global_struct->period_ms);
	}
	else {
		new_deadline = previous_task_deadline + msecs_to_jiffies(mp2_global_struct->period_ms);
	}
	
	//only change if next period not started yet
	if(new_deadline < jiffies){
		mp2_global_struct->deadline_jiff = new_deadline;
		return;
	}

	mp2_global_struct->task_state = SLEEPING;
	mp2_global_struct->deadline_jiff = new_deadline;	

	//reset the timer and wake up teh dispatcher to schedule the nest process
	mod_timer(&(mp2_global_struct->tasl_timer), mp2_global_struct->deadline_jiff);
	set_task_state(mp2_global_struct->linux_task, TASK_UNINTERRUPTIBLE);
	wake_up_process(my_kernel_thread);
	schedule();
}

void callback_deregistration(char *buf, size_t count){
	//printk(KERN_ALERT "Inside De-Registration Function\n");
	
	//de-registration function implementation
	int PID;
	int currently_running;
	char m;
	struct list_head *pos = NULL;
	struct mp2_task_struct *task_pointer = NULL;
	sscanf
(buf, "%c%*[,] %d", &m, &PID);
	printk(KERN_ALERT "PID: %d\n", PID);
	// get_PID(buf, &PID, count);
	// mp2_slab_cache = kmem_cache_lookup(SLAB_CACHE_NAME);
	if(!mp2_slab_cache) return; //sanity check
	
	//iterate over the struct to find the relevant one
	list_for_each(pos, &list_head){
		task_pointer = list_entry(pos, struct mp2_task_struct, task_node);
		if(task_pointer->pid == PID){
			mp2_global_struct = task_pointer;
			break;
		}
	}
	if(mp2_global_struct == task_currently_running) {currently_running = 1;}
	else {currently_running = 0;}
	
	//delete from task_list
	list_del(&(mp2_global_struct->task_node));
	mp2_global_struct->task_state = SLEEPING;
	mp2_global_struct->pid = -1;
	mp2_global_struct->linux_task = NULL;
	del_timer(&(mp2_global_struct->tasl_timer));
	kmem_cache_free(mp2_slab_cache, mp2_global_struct);	
	
	if(currently_running){
		task_currently_running = NULL;
		wake_up_process(my_kernel_thread);
	}
	printk(KERN_ALERT "De-registration Function Successfully Ended\n");
}

static ssize_t mp2_read (struct file *file, char __user *buffer, size_t count, loff_t *data){
	// implementation goes here...
	//printk(KERN_ALERT "Start of MP2 Read CallBac Function");
	char *copy_buffer;
	
	struct mp2_task_struct *task_pointer;
	struct list_head *pos;

	int length_to_copy;
	int PID;
	unsigned long PERIOD;
	unsigned long COMPUTATION;

	//int current_copy = 0;
	int i=0, buffer_length = 0;	
	int num_to_copy= 0;
	//int PID;
	unsigned long cpu;
	int return_val_copy;
	char each_line[500];

	if(count > MAX_BUFFER_LENGTH){ copy_buffer = (char *)kmalloc(sizeof(char) * (count + 1), GFP_KERNEL);}
	else {copy_buffer = (char *)kmalloc(sizeof(char) * MAX_BUFFER_LENGTH, GFP_KERNEL);}

	//sanity check
	if(!copy_buffer) {return 0;}
	if(*data != 0) {return 0;}
	
	//critical section start
	spin_lock_irq(&my_lock);

	/* while(buffer[i] != '\0'){
		buffer_length++;
		i++;
	} */
	
	num_to_copy = 0;
	PERIOD = 0;
	COMPUTATION = 0;
	PID = 0;	
	return_val_copy = 0;
	cpu = 0;

	if(count + *data < buffer_length){length_to_copy = count;}
	else {length_to_copy = buffer_length - *data;}
	
	if(count <= 0) { spin_unlock_irq(&my_lock); return 0;}
	
	// Figure Out
	memset(each_line, 0, 500);
	buffer_length = 0;
	list_for_each(pos, &list_head){
		task_pointer = list_entry(pos, struct mp2_task_struct, task_node);
		sprintf(each_line, "PID: %d, PERIOD: %lu, COMPUTATION: %lu\n", task_pointer->pid, task_pointer->period_ms, task_pointer->compute_time_ms);
		 i = 0;
		while(each_line[i]){
			copy_buffer[buffer_length] = each_line[i];
			i++;
			buffer_length++;
		}
		memset(each_line, 0, 500);
	}	
	copy_buffer[buffer_length+1] = '\0';
	*data += buffer_length + 1;
	copy_to_user(buffer, copy_buffer, buffer_length + 1);	
	kfree(copy_buffer);
	spin_unlock_irq(&my_lock);

	printk(KERN_ALERT "Read Function Ended");
	return buffer_length + 1;
}

static ssize_t mp2_write (struct file *file, const char __user *buffer, size_t count, loff_t *data){
	// implementation goes here...
	// int copied;
	// sanity check
// 	printk(KERN_ALERT "Count: %d, Data_Offset: %d\n", count, *data);

//	if(*data != 0){return 0;}
	//printk(KERN_ALERT "START OF WRITE FUNCTION\n");
	char *buf;
	char type_of_message;
	buf = (char *) kmalloc(count+1, GFP_KERNEL);
	
	//above implementation and some from below taken/copied directly from te slides
	//start of critical section
	spin_lock_irq(&my_lock);
	copy_from_user(buf, buffer, count);
	buf[count] = '\0';
	printk(KERN_ALERT "String: %s\n", buf);
	type_of_message = buf[0];
	printk(KERN_ALERT "Message: %c\n", type_of_message);
	//*data += count;
	//spin_unlock_irq(&my_lock);
	//return 0;
	switch(type_of_message){
		case 'Y':
			printk(KERN_ALERT "Reached Here");
			//*data += count;
			//spin_unlock_irq(&my_lock);
			//return 0;
			callback_yield(buf);
			break;				
		case 'R':
			callback_registration(buf, count);
			break;
		case 'D':
			callback_deregistration(buf, count);
			break;
		default:
			*data += count;
			kfree(buf);
			spin_unlock_irq(&my_lock);
			printk(KERN_ALERT "Should Not Reach Here");
			return 0;
			//return -EINVAL;
	}
//	*data += count;
	kfree(buf);
	spin_unlock_irq(&my_lock); //end of critical section
	return count;
}

// mp2_init - Called when module is loaded
int __init mp2_init(void){
	#ifdef DEBUG
	printk(KERN_ALERT "MP2 MODULE LOADING\n");
	#endif
	
	//create proc filesystem directory entry
	proc_dir = proc_mkdir(DIRECTORY, NULL);
	
	//sanity check for errors in creating the directory
	if(!proc_dir){
		printk(KERN_ALERT "Proc_Dir Directory Error Creating\n");
		return -ENOENT;
	}	
	
	// create the entry for the filename insode the directroy and give it read and write permission
	proc_entry = proc_create(FILENAME, 0666, proc_dir, &mp2_file);

	//sanity check for errors in creating the file
	if(!proc_entry){
		//if so, remove the DIRECTORY
		remove_proc_entry(DIRECTORY, proc_dir);
		printk(KERN_ALERT "Proc_Entry Filename Error Creating\n");
		return -ENOENT; 
	}

	//initialize our spin_lock to be used later
	spin_lock_init(&my_lock);
	spin_lock_init(&thread_lock);

	//slab allocation
	mp2_slab_cache = KMEM_CACHE(mp2_task_struct, SLAB_PANIC); //figure out if need constructor
	//create and run the kernel thread;
	my_kernel_thread = kthread_create(thread_function, NULL, "kthread_dispatcher");

	return 0;
}

void __exit mp2_exit(void){
	struct mp2_task_struct *pos;
        struct mp2_task_struct *n;  
	
	 #ifdef DEBUG	
        printk(KERN_ALERT "MP2 MODULE UNLOADING\n");
        #endif
	
	//critical section
	spin_lock_irq(&my_lock);
	list_for_each_entry_safe(pos, n, &list_head, task_node){
		del_timer(&(pos->tasl_timer));
		list_del(&(pos->task_node));
		kmem_cache_free(mp2_slab_cache, pos);
	}
	spin_unlock_irq(&my_lock);
	
	//destroy the cache
	kmem_cache_destroy(mp2_slab_cache);
	
	//stop the thread
	kthread_stop(my_kernel_thread);

	//remove the file and the directory
	remove_proc_entry(FILENAME, proc_dir);
	remove_proc_entry(DIRECTORY, NULL);
}

//Register init and exit functions
module_init(mp2_init);
module_exit(mp2_exit);

