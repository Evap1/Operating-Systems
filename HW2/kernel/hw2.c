#include <linux/kernel.h>
#include <linux/errno.h>	// for EINVAL
#include <linux/cred.h>		// for EPERM
#include <linux/sched.h>
#include <linux/pid.h>    	// for finding pid

#define SWORD_CLEARANCE    0x1
#define MIDNIGHT_CLEARANCE 0x2
#define CLAMP_CLEARANCE    0x4

asmlinkage long sys_hello(void) {
	printk("Hello, World!\n");
	return 0;
}

asmlinkage long sys_set_sec(int sword, int midnight, int clamp) {
	struct task_struct *task = current;
	
	if (!capable(CAP_SYS_ADMIN)) {
    	return -EPERM;
	}
	else if (sword < 0 || midnight < 0 || clamp < 0) {
		return -EINVAL;
	}

	task->clearance_mode = 0;						//clear before applying the clearance

	if (sword >= 1) {		// lsb
		task->clearance_mode |= SWORD_CLEARANCE;
	}
	if (midnight >= 1) {
		task->clearance_mode |= MIDNIGHT_CLEARANCE;
	}
	if (clamp >= 1) {		//"msb"
		task->clearance_mode |= CLAMP_CLEARANCE;
	}

	return 0;
}

asmlinkage long sys_get_sec(char clr) {
	return 0;
}

asmlinkage long sys_check_sec(pid_t pid, char clr) {
	return 0;
}

asmlinkage long sys_set_sec_branch(int height, char clr) {
	return 0;
}
