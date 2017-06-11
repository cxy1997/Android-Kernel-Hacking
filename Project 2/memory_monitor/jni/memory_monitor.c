#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 
 * This program investigates the file /proc/meminfo 
 * and prints out the 5th and 6th line in it.
 * The two specific lines contains the current size of 
 * active and inactive memory. The examine() function 
 * will be called every 1 second to continuously 
 * monitor the memory status.
 */

void examine()
{
    char s[256];
    freopen("/proc/meminfo", "r", stdin);
    fgets(s, 256, stdin);
    fgets(s, 256, stdin);
    fgets(s, 256, stdin);
    fgets(s, 256, stdin);
    fgets(s, 256, stdin);
    fgets(s, 256, stdin);
    printf("%s", s);
    fgets(s, 256, stdin);
    printf("%s", s);
}

int main(int argc, char *argv[])
{
    int i;
    for (i = 0; i <= 20; ++i)
    {
        printf("After %d second(s):\n", i);
        examine();
        sleep(1);
    }
    return 0;
}
