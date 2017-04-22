#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/unistd.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
MODULE_LICENSE("Dual BSD/GPL");
#define __NR_ptree 233
#define MAXBUFFERSIZE 2048

struct prinfo {
    pid_t parent_pid; // process id of parent
    pid_t pid; // process id
    pid_t first_child_pid; // pid of youngest child
    pid_t next_sibling_pid; // pid of older sibling
    long state; // current state of process
    long uid; // user id of process owner
    char comm[16]; // name of program executed
};

void convert(struct task_struct* from, struct prinfo* to) // convert from task_struct to prinfo
{
    to->parent_pid = (from->parent) ? from->parent->pid : 0;

    to->pid = from->pid;

    to->first_child_pid = list_empty(&(from->children)) ? 0 : list_entry(from->children.next, struct task_struct, sibling)->pid;

    // the next_sibling_pid might points to the root parent of the current process, which will be fixed while printing.
    to->next_sibling_pid = (list_empty(&(from->sibling))) ? 0 : list_entry(from->sibling.next, struct task_struct, sibling)->pid;

    to->state = from->state;

    to->uid = from->cred->uid;

    get_task_comm(to->comm, from);
}

void dfs(struct task_struct* task, struct prinfo* buf, int* nr)
{
    struct list_head *p; // iterator
    struct task_struct *q;
    convert(task, &buf[*nr]); // copy
    ++(*nr);
    list_for_each(p, &(task->children)) // iterate over children processes
    {
    	q = list_entry(p, struct task_struct, sibling);
    	dfs(q, buf, nr);
    }
}

static int (*oldcall)(void);

int ptree(struct prinfo* buf, int* nr)
{
	// The memory space is allocated in kernel space rather than user space
    struct prinfo * _buf = (struct prinfo *)kmalloc(sizeof(struct prinfo) * MAXBUFFERSIZE, GFP_KERNEL);
    int _nr = 0;
    
    read_lock(&tasklist_lock);
    dfs(&init_task, _buf, &_nr);
    read_unlock(&tasklist_lock);

    // copy from kernel space to user space
    if (copy_to_user(buf, _buf, sizeof(struct prinfo) * MAXBUFFERSIZE)) // exception handling
    {
        printk(KERN_ERR "Copy buffer failed!\n");
        return 1;
    }
    if (copy_to_user(nr, &_nr, sizeof(int))) // exception handling
    {
        printk(KERN_ERR "Copy buffer failed!\n");
        return 1;
    }

    // release the memory
    kfree(_buf);
    return 0;
}

static int ptree_init(void)
{
    long *syscall = (long*)0xc000d8c4;
    oldcall = (int(*)(void))(syscall[__NR_ptree]); // preserve the former system call
    syscall[__NR_ptree] = (unsigned long)ptree; // assign the new system call
    printk(KERN_INFO "Module ptree loaded successfully!\n");
    return 0;
}

static void ptree_exit(void)
{
    long *syscall = (long*)0xc000d8c4;
    syscall[__NR_ptree] = (unsigned long)oldcall; // restore the old system call
    printk(KERN_INFO "Module ptree removed successfully!\n");
}

module_init(ptree_init);
module_exit(ptree_exit);