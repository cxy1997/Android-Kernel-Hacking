#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#define __NR_expose_page_table 378

#define PGD_SIZE ((1 << 11) * sizeof(unsigned long))
#define PTE_SIZE ((1 << 20) * sizeof(unsigned long))

#define L_PTE_PRESENT (1 << 0)
#define L_PTE_YOUNG (1 << 1)
#define L_PTE_FILE (1 << 2)
#define L_PTE_DIRTY (1 << 6)
#define L_PTE_RDONLY (1 << 7)
#define L_PTE_USER (1 << 8)
#define L_PTE_XN (1 << 9)
#define L_PTE_SHARED (1 << 10)

#define pte_present(pte) (pte & L_PTE_PRESENT)
#define pte_write(pte) (!(pte & L_PTE_RDONLY))
#define pte_dirty(pte) (pte & L_PTE_DIRTY)
#define pte_young(pte) (pte & L_PTE_YOUNG)
#define pte_exec(pte) (!(pte & L_PTE_XN))

// for a hex byte c, return hex byte (c-8)
char lower(char c)
{
    if (c >= 'a')
        return c - 'a' + '2';
    if (c >= 'A')
        return c - 'A' + '2';
    return c - 8;
}

// convert a hex string up to 8 bytes to unsigned long
unsigned long strtolu(char *str)
{
    unsigned long res;

    // check if the value of the string exceeds (2^31)-1
    int flag = (strlen(str) == 8) && (str[0] >= '8');
    if (flag)
        str[0] = lower(str[0]);
    
    // convert hex to unsigned long
    res = strtol(str, NULL, 16);
    res |= (flag << 31);

    return res;
}

void expose_page_table(pid_t pid, unsigned long fake_pgd, unsigned long page_table_addr, unsigned long begin_vaddr, unsigned long end_vaddr)
{
    unsigned long pageFrame, i;
    unsigned long *p;

    // invoke system call expose_page_table()
    if (syscall(__NR_expose_page_table, pid, fake_pgd, page_table_addr, begin_vaddr, end_vaddr))
    {
        printf("Failed to expose pagetable layout.\n");
        return;
    }

    // calculate virtual page number
    begin_vaddr = (begin_vaddr >> PAGE_SHIFT) & 0xFFFFF;
    end_vaddr = (end_vaddr >> PAGE_SHIFT) & 0xFFFFF;

    // print pagetable entries
    printf("  Virtual Page Number\tPhysical Page Number\n");
    for (i = begin_vaddr; i <= end_vaddr; ++i)
    {
        // calculate physical frame number
        p = (unsigned long*)((unsigned long*)fake_pgd)[i >> 9];
        pageFrame = p[i & 0x1FF];

        // skip empty pagetable entries
        if (pte_present(pageFrame))
        {
            printf("        0x%05lx                0x%05lx\n", i, pageFrame >> PAGE_SHIFT);
        }
    }
}

int main(int argc, char *argv[])
{
    pid_t pid;
    unsigned long fake_pgd, page_table_addr, begin_vaddr, end_vaddr;

    // check paramaters
    if (argc != 4)
    {
        printf("Unmatched parameters for vm_inspector!\n");
        printf("The correct format should be:\n");
        printf("./vm_inspector #Pid #Va_Begin #Va_End\n");
        return -EAGAIN;
    }
    pid = atoi(argv[1]);
    begin_vaddr =strtolu(argv[2]);
    end_vaddr =strtolu(argv[3]);

    // allocate user-space memory
    fake_pgd = (unsigned long)malloc(PGD_SIZE);
    if (!fake_pgd)
        return -EFAULT;
    page_table_addr = (unsigned long)mmap(NULL, PTE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (!page_table_addr)
        return -EFAULT;
    // invoke system call
    expose_page_table(pid, fake_pgd, page_table_addr, begin_vaddr, end_vaddr);
    
    // free user-space memory
    free((void*)fake_pgd);
    munmap((void*)page_table_addr, PTE_SIZE);
    return 0;
}
