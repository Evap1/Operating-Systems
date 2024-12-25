#include <linux/kernel.h>
#include <linux/errno.h>	// for EINVAL
#include <linux/cred.h>		// for EPERM
#include <linux/sched.h>
#include <linux/pid.h>    	// for finding pid
#include <linux/init_task.h>    // for init_task
#include <linux/sched/task.h>	//for get_task_struct

#define SWORD_CLEARANCE    0x4
#define MIDNIGHT_CLEARANCE 0x2
#define CLAMP_CLEARANCE    0x1

asmlinkage long sys_hello(void) {
	printk("Hello, World!\n");
	return 0;
}

asmlinkage long sys_set_sec(int sword, int midnight, int clamp) {
	struct task_struct *task = current;
	
	if (!capable(CAP_SYS_ADMIN)) {
		return -EPERM;
	}
	if (sword < 0 || midnight < 0 || clamp < 0) {
		return -EINVAL;
	}

	task->clearance_mode = 0;						//clear before applying the clearance

	if (sword >= 1) {		// "msb"
		task->clearance_mode |= SWORD_CLEARANCE;
	}
	if (midnight >= 1) {
		task->clearance_mode |= MIDNIGHT_CLEARANCE;
	}
	if (clamp >= 1) {		// lsb
		task->clearance_mode |= CLAMP_CLEARANCE;
	}

	return 0;
}

asmlinkage long sys_get_sec(char clr) {
	struct task_struct *task = current;
	int clearance;

	switch (clr) {
    case 's':
        clearance = SWORD_CLEARANCE;
        break;
    case 'm':
        clearance = MIDNIGHT_CLEARANCE;
        break;
    case 'c':
        clearance = CLAMP_CLEARANCE;
        break;
    default:
        return -EINVAL;
	}

	return (task->clearance_mode & clearance) != 0;
}

asmlinkage long sys_check_sec(pid_t pid, char clr) {
	struct task_struct *curr_task = current;
	struct task_struct *task;
	int clearance;

	switch (clr) {
    case 's':
        clearance = SWORD_CLEARANCE;
        break;
    case 'm':
        clearance = MIDNIGHT_CLEARANCE;
        break;
    case 'c':
        clearance = CLAMP_CLEARANCE;
        break;
    default:
        return -EINVAL;
	}

	rcu_read_lock();			// start critical section
	
	task = find_task_by_vpid(pid);
	if (!task) {
		rcu_read_unlock();		// stop critical section
		return -ESRCH;
	}

	if ((curr_task->clearance_mode & clearance) == 0) {	// check if calling process has correct clearance
		rcu_read_unlock();		// stop critical section
		return -EPERM;
	}

	rcu_read_unlock();			// stop critical section
	return (task->clearance_mode & clearance) != 0;
}

asmlinkage long sys_set_sec_branch(int height, char clr) {
	struct task_struct *curr_task = current;
	struct task_struct *parent_task;
	struct task_struct *next_parent;
	int clearance, num_of_changed_parents = 0;

	// check if height is valid - greater than 0
	if (height <= 0) {
		return -EINVAL;
	}

	// check if clr is valid and current process has it
	switch (clr) {
    case 's':
        clearance = SWORD_CLEARANCE;
        break;
    case 'm':
        clearance = MIDNIGHT_CLEARANCE;
        break;
    case 'c':
        clearance = CLAMP_CLEARANCE;
        break;
    default:
        return -EINVAL;
	}

	if ((curr_task->clearance_mode & clearance) == 0) {
		return -EPERM;
	}

	rcu_read_lock();			// start critical section

	parent_task = curr_task->parent;							 // start from parent

	while ((parent_task != &init_task) && (height > 0)) { //check if the process is not init
		/* safe increment reference count so the pointer
		   won't be invalid incase someone freed it */
		get_task_struct(parent_task);

		if ((parent_task->clearance_mode & clearance) == 0) {
			parent_task->clearance_mode |= clearance;
			num_of_changed_parents++;
		}

		height--;
	    next_parent = parent_task->parent;
        put_task_struct(parent_task);
		parent_task = next_parent;
	}

	if (height > 0 && parent_task == &init_task) {				//if reached init mode and theres still height, update
        	get_task_struct(parent_task);
	if ((parent_task->clearance_mode & clearance) == 0) {
            parent_task->clearance_mode |= clearance;
            num_of_changed_parents++;
        }
		put_task_struct(parent_task);
    }

	rcu_read_unlock();			// stop critical section
	return num_of_changed_parents;
}
