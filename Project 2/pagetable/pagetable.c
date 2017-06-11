#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/unistd.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/slab.h>
#include <asm/errno.h>
#include <asm/pgtable.h>
#define __NR_get_pagetable_layout 233
#define __NR_expose_page_table 378
MODULE_LICENSE("Dual BSD/GPL");

#define PGD_SIZE ((1 << 11) * sizeof(unsigned long))
#define EACH_PTE_SIZE ((1 << 9) * sizeof(unsigned long))

struct pagetable_layout_info
{
    uint32_t pgdir_shift;
    uint32_t page_shift;
};

struct walk_record
{
    unsigned long *pgd_base;
    unsigned long pte_base;
};

static int (*oldcall_NR_get_pagetable_layout)(void), (*oldcall_NR_expose_page_table)(void);

int for_pmd_entry(pmd_t *pmd, unsigned long addr, unsigned long next, struct mm_walk *walk)
{
    unsigned long pmdIndex = (addr >> 21) & 0x7FF;
    struct walk_record *record = (struct walk_record*)walk->private;
    int pfn, err;
    struct vm_area_struct *vma = find_vma(current->mm, record->pte_base);

    // sanity check
    if (!vma)
    {
        printk(KERN_INFO "vma empty!\n");
        return -EFAULT;
    }
    if (!pmd)
    {
        printk(KERN_INFO "pmd empty!\n");
        return 0;
    }
    if (pmd_bad(*pmd))
    {
        printk(KERN_INFO "Bad pmd!\n");
        return -EINVAL;
    }
    printk(KERN_INFO "At pmd: 0x%lx\n", (unsigned long)*pmd);

    // get page frame number
    pfn = page_to_pfn(pmd_page((unsigned long)*pmd));

    if (!pfn_valid(pfn))
    {
        printk(KERN_INFO "pfn invalid!\n");
        return -EFAULT;
    }

    // remap target pagetable entries to user space
    err = remap_pfn_range(vma, record->pte_base, pfn, PAGE_SIZE, vma->vm_page_prot);
    if (err)
    {
        printk(KERN_INFO "remap_pfn_range failed!\n");
        return err;
    }

    // generate fake pgd and update pte information
    record->pgd_base[pmdIndex] = record->pte_base;
    printk(KERN_INFO "pgd_base[%lu] = 0x%08lx\n", pmdIndex, record->pte_base);
    record->pte_base += EACH_PTE_SIZE;
    
    return 0;
}

int get_pagetable_layout(struct pagetable_layout_info __user * pgtbl_info, int size)
{
    struct pagetable_layout_info layout;
    printk(KERN_INFO "Syscall get_pagetable_layout() invoked!\n");

    // exception handling
    if (size < sizeof(struct pagetable_layout_info))
        return -EINVAL;

    // get the pagetable layout information
    layout.pgdir_shift = PGDIR_SHIFT;
    layout.page_shift = PAGE_SHIFT;

    // copy the acquired information to user space
    if (copy_to_user(pgtbl_info, &layout, sizeof(struct pagetable_layout_info)))
        return -EFAULT;
    return 0;
}

int expose_page_table(pid_t pid, unsigned long fake_pgd, unsigned long page_table_addr, unsigned long begin_vaddr, unsigned long end_vaddr)
{
    struct pid *pid_struct = NULL;
    struct task_struct *target_process = NULL;
    struct vm_area_struct *tmp;
    struct walk_record record;
    struct mm_walk walk;

    printk(KERN_INFO "Syscall expose_page_table() invoked!\n");

    // sanity check
    if (begin_vaddr >= end_vaddr) 
        return -EINVAL;

    // get the target process's task_struct
    pid_struct = find_get_pid(pid);
    if (!pid_struct) // if pid is illegal
        return -EINVAL;
    target_process = get_pid_task(pid_struct, PIDTYPE_PID);
    printk(KERN_INFO "The target process is %s.\n", target_process->comm); // print target process name

    // traverse the virtual memory areas
    down_write(&target_process->mm->mmap_sem);
    printk(KERN_INFO "Virtual memory of the target process:\n");
    for (tmp = target_process->mm->mmap; tmp; tmp = tmp->vm_next)
        printk(KERN_INFO "    0x%08lx - 0x%08lx\n", tmp->vm_start, tmp->vm_end);
    up_write(&target_process->mm->mmap_sem);

    // set up walk_record
    record.pgd_base = (unsigned long*)kmalloc(PGD_SIZE, GFP_KERNEL);
    if (!record.pgd_base)
        return -EFAULT;
    memset(record.pgd_base, 0, PGD_SIZE); // clear trash values
    record.pte_base = page_table_addr;
    if (!record.pte_base)
        return -EFAULT;

    // set up mm_walk
    walk.pgd_entry = NULL;//for_pgd_entry;
    walk.pud_entry = NULL;
    walk.pmd_entry = for_pmd_entry;//NULL;
    walk.pte_entry = NULL;
    walk.pte_hole = NULL;
    walk.hugetlb_entry = NULL;
    walk.mm  = target_process->mm;
    walk.private = (void*)(&record);

    // start to dump
    down_write(&target_process->mm->mmap_sem);
    walk_page_range(begin_vaddr, end_vaddr, &walk);
    up_write(&target_process->mm->mmap_sem);

    printk("End page walk!\n");

    // copy the page directory back to user space
    if (copy_to_user((void*)fake_pgd, record.pgd_base, PGD_SIZE))
        return -EFAULT;

    kfree(record.pgd_base);
    return 0;
}

static int pagetable_layout_init(void)
{
    long *syscall = (long*)0xc000d8c4;

    // install get_pagetable_layout()
    oldcall_NR_get_pagetable_layout = (int(*)(void))(syscall[__NR_get_pagetable_layout]); // preserve former system call
    syscall[__NR_get_pagetable_layout] = (unsigned long)get_pagetable_layout; // assign new system call
    printk(KERN_INFO "Module Get_pagetable_layout loaded successfully!\n");

    // install expose_page_table()
    oldcall_NR_expose_page_table = (int(*)(void))(syscall[__NR_expose_page_table]); // preserve former system call
    syscall[__NR_expose_page_table] = (unsigned long)expose_page_table; // assign new system call
    printk(KERN_INFO "Module Expose_page_table loaded successfully!\n");

    return 0;
}

static void pagetable_layout_exit(void)
{
    long *syscall = (long*)0xc000d8c4;

    // uninstall get_pagetable_layout()
    syscall[__NR_get_pagetable_layout] = (unsigned long)oldcall_NR_get_pagetable_layout; // restore old system call
    printk(KERN_INFO "Module Get_pagetable_layout removed successfully!\n");

    // uninstall expose_page_table()
    syscall[__NR_expose_page_table] = (unsigned long)oldcall_NR_expose_page_table; // restore old system call
    printk(KERN_INFO "Module Expose_page_table removed successfully!\n");
}

module_init(pagetable_layout_init);
module_exit(pagetable_layout_exit);