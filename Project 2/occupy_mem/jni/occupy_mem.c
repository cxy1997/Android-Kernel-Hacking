#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/mman.h>

/*
 * This prpgram tries to occupy up to 1GB RAM, 
 * which will always wake up the kawapd process
 * (and gets killed =_=).
 */

int main()
{
    unsigned long **p;
    unsigned long i, j;
	printf("Start to occupy 1GB memory.\n");
    p = (unsigned long**)malloc(1 << 15);
    if (!p)
    	printf("Memory allocation failed.\n");
    else
    {
        for (i = 0; i < (1 << 13); ++i)
        {
        	p[i] = (unsigned long*)malloc(1 << 17);
        	for (j = 0; j < 32; ++j)
        		p[i][j << 10] = 0;
        }
        printf("Releasing after 1s.\n");
        sleep(1);
        for (i = 0; i < (1 << 13); ++i)
        	if(p[i])
        		free(p[i]);
    }
    if (p)
    	free(p);
	return 0;
}