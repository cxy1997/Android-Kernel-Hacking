#include <stdlib.h>
#include <string.h>
#include "stdio.h"
#define MAXBUFFERSIZE 2048
#define MAXPROCESSNUMBER 256
struct prinfo
{
    pid_t parent_pid; // process id of parent
    pid_t pid; // process id
    pid_t first_child_pid; // pid of youngest child
    pid_t next_sibling_pid; // pid of older sibling
    long state; // current state of process
    long uid; // user id of process owner
    char comm[16]; // name of program executed
};
void display(struct prinfo* buf, int* nr)
{
	int stack[MAXPROCESSNUMBER];
	int tabcnt[MAXPROCESSNUMBER];
	memset(tabcnt, 0, MAXPROCESSNUMBER * sizeof(int));
    int i, j, top = 0;

    /* Preprocessing, to prevent some "next_sibling_pid" from refering to the root parent.
    Stack is used to determine the number of tabs, which decreases time complexity to O(n).*/
    for (i = 0; i < *nr; ++i)
    {
    	if (i == *nr-1 || (buf[i].next_sibling_pid != buf[i+1].pid && buf[i].first_child_pid != buf[i+1].pid))
    		buf[i].next_sibling_pid = 0;
    	if (top == 0 || buf[i].pid == buf[stack[top-1]].first_child_pid)
    		stack[top++] = i;
    	else
    	{
    		while (buf[i].pid != buf[stack[top-1]].next_sibling_pid)
    		{
    			--top;
    	        buf[stack[top]].next_sibling_pid = 0;
    	    }
            stack[top-1] = i;
        }
        tabcnt[i] = top-1;
    }

    // print
    for (i = 0; i < *nr; ++i)
    {
    	for (j = 0; j < tabcnt[i]; ++j) printf("\t");
    	printf("%s,%d,%ld,%d,%d,%d,%ld\n", buf[i].comm, buf[i].pid, buf[i].state, buf[i].parent_pid, buf[i].first_child_pid, buf[i].next_sibling_pid, buf[i].uid);
    }
}
int main(int argc, char *argv[])
{
    struct prinfo *buf = (struct prinfo*)malloc(sizeof(struct prinfo) * MAXBUFFERSIZE);
    int nr;
    syscall(233, buf, &nr); // call ptree
    display(buf, &nr);
    free(buf);
    return 0;
}
