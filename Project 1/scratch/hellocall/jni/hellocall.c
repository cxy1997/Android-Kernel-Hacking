#include "stdio.h"
int main(int argc, char *argv[])
{
    printf("This is a test!\n");
    int i = syscall(356, 123, "test string");
    printf("Answer is %d!\n", i);
    printf("Test End!:\n\n");
    return 0;
}
