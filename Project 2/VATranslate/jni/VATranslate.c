#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#define __NR_get_pagetable_layout 233
#define __NR_expose_page_table 378

#define PGD_SIZE ((1 << 11) * sizeof(unsigned long))
#define PTE_SIZE ((1 << 20) * sizeof(unsigned long))

// The structure "pagetable_layout_info" stores the information of pagetable layout
struct pagetable_layout_info
{
    uint32_t pgdir_shift;
    uint32_t page_shift;
};

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

void display_pagetable_layout(struct pagetable_layout_info* layout)
{
    printf("============================================================\n");
    printf("Android pagetable layout:\n");
    printf("  pgdir_shift = %u\n", layout->pgdir_shift);
    printf("  page_shift = %u\n", layout->page_shift);
    printf("============================================================\n");
}

void get_pagetable_layout()
{
    struct pagetable_layout_info pgtbl_info;

    // invoke system call get_pagetable_layout()
    if (syscall(__NR_get_pagetable_layout, &pgtbl_info, sizeof(struct pagetable_layout_info))) 
    {
        printf("Failed to get pagetable layout.\n\n");
        return;
    }

    display_pagetable_layout(&pgtbl_info);
}

void expose_page_table(pid_t pid, unsigned long fake_pgd, unsigned long page_table_addr, unsigned long begin_vaddr, unsigned long end_vaddr)
{
    unsigned long physical_addr, v_pgd, v_pte, v_offset, pageFrame;
    unsigned long *p;

    // invoke system call expose_page_table()
	if (syscall(__NR_expose_page_table, pid, fake_pgd, page_table_addr, begin_vaddr, end_vaddr))
    {
    	printf("Failed to expose pagetable layout.\n");
    	return;
    }

    // segment the virtual address
    v_pgd = (begin_vaddr >> 21) & 0x7FF;
    v_pte = (begin_vaddr >> PAGE_SHIFT) & 0x1FF;
    v_offset = begin_vaddr & 0xFFF;

    // calculate physical address
    p = (unsigned long*)((unsigned long*)fake_pgd)[v_pgd];
    pageFrame = p[v_pte] & 0xFFFFF000;
    if (!pageFrame)
    {
        printf("Invalid physical address!");
    }
    physical_addr = pageFrame + v_offset;
    
    // print calculation details
    printf("Virtual Address Translation:\n");
    printf("  virtual address = 0x%08lx\n", begin_vaddr);
    printf("  pgd: 0x%03lx\tpte: 0x%03lx\toffset: 0x%03lx\n", v_pgd, v_pte, v_offset);
    printf("  pgd_base[0x%lx] = 0x%08lx\tpte[0x%lx] = 0x%08lx\n", v_pgd, (unsigned long)p, v_pte, pageFrame);
    printf("  physical address = 0x%08lx\n", physical_addr);
}

int main(int argc, char *argv[])
{
    pid_t pid;
    unsigned long fake_pgd, page_table_addr, begin_vaddr, end_vaddr;

    // invoke system call
    get_pagetable_layout();

    // check paramaters
    if (argc != 3)
    {
        printf("Unmatched parameters for VATranslate!\n");
        printf("The correct format should be:\n");
        printf("./VATranslate #PID #VA\n");
        return -EAGAIN;
    }
    pid = atoi(argv[1]);
    begin_vaddr =strtolu(argv[2]);
    end_vaddr = begin_vaddr + 1;

    // allocate user-space memory
    fake_pgd = (unsigned long)malloc(2 * PAGE_SIZE);
    if (!fake_pgd)
    	return -EFAULT;
    page_table_addr = (unsigned long)mmap(NULL, 1 << 22, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (!page_table_addr)
    	return -EFAULT;
    // invoke system call
    expose_page_table(pid, fake_pgd, page_table_addr, begin_vaddr, end_vaddr);
    
    // free user-space memory
    free((void*)fake_pgd);
    munmap((void*)page_table_addr, 1 << 22);
    return 0;
}
