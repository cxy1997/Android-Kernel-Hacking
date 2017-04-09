#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
int main()
{
	pid_t parentpid = getpid();
	pid_t childpid = fork();
	if (childpid > 0)
	{
	    printf("515030910339 Parent PID = %d\n", parentpid); // print parent process ID
	    wait(NULL); // wait for the child process to finish
	}
	else if (childpid == 0) // if in child process
	{
		printf("515030910339 Child PID = %d\n", getpid());
		printf("Execuating ptree in child process:\n");
		execl("./ptree", "ptree", NULL); // execuate ptree
		_exit(0);
	}
	return 0;
}