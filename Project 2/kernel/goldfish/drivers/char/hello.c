#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/sched.h>
/*在这个头文件里面包含了所有的系统调用*/
#include<linux/unistd.h>
MODULE_LICENSE("Dual BSD/GPL");
#define __NR_mysetuid 356

static int (*oldgetuid)(void);
static int sys_getuid(int n, char* str)
{
    printk("this is my system second call!\n the uid = %ld\n str: %s\n",n,str);
    return n;
}
static int addsyscall_init(void)
{
     long *syscall = (long*)0xc000d8c4;
        /*保存原来的系统调用表中此位置的系统调用*/
        oldgetuid = (int(*)(void))(syscall[__NR_mysetuid]);
        /*把自己的系统调用放入系统调用表项中，注意类型转换*/
     syscall[__NR_mysetuid] = (unsigned long )sys_getuid;
     printk(KERN_INFO "module load!\n");
        return 0;

}
/*这里是退出函数。__exit标志表明如果我们不是以模块方式编译这段程序，则这个 标志后的函数可以丢弃。也就是说，模块被编译进内核，只要内核还在运行，就不会被卸载*/
static void addsyscall_exit(void)
{
    long *syscall = (long*)0xc000d8c4;
     syscall[__NR_mysetuid] = (unsigned long )oldgetuid;
     printk(KERN_INFO "module exit!\n");
}
module_init(addsyscall_init);
module_exit(addsyscall_exit);
MODULE_DESCRIPTION("my sys_setuid");
MODULE_AUTHOR("gyeve");