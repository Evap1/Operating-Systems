#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/sched.h>

asmlinkage long sys_hello(void) {
	printk("Hello, World!\n");
	return 0;
}

asmlinkage long sys_set_sec(int sword, int midnight, int clamp) {
	struct task_struct *task = current;

	if (sword < 0 || midnight < 0 || clamp < 0) {
		return -EINVAL;
	}

	if (sword >= 1) {		// lsb
		task->clearance_mode |= 1;
	}
	if (midnight >= 1) {
		task->clearance_mode |= 2;
	}
	if (clamp >= 1) {		//"msb"
		task->clearance_mode |= 4;
	}
	printk("Clearance: %d\n", task->clearance_mode);
	return 0;
}
