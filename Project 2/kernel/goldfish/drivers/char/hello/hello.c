#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/sched.h>
#include<linux/unistd.h>
MODULE_LICENSE("Dual BSD/GPL");
#define __NR_hellocall 356

static int (*oldcall)(void);
static int sys_hellocall(int n, char* str)
{
    printk("this is my system second call!\n the uid = %ld\n str: %s\n",n,str);
    return n;
}
static int addsyscall_init(void)
{
    long *syscall = (long*)0xc000d8c4;
    oldcall = (int(*)(void))(syscall[__NR_hellocall]);
    syscall[__NR_hellocall] = (unsigned long )sys_hellocall;
    printk(KERN_INFO "module load!\n");
    return 0;

}

static void addsyscall_exit(void)
{
    long *syscall = (long*)0xc000d8c4;
    syscall[__NR_hellocall] = (unsigned long )oldcall;
    printk(KERN_INFO "module exit!\n");
}

module_init(addsyscall_init);
module_exit(addsyscall_exit);
