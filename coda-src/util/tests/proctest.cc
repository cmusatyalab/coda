#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>

extern char *getcommandname(int);

FILE *LogFile = stdout;
int LogLevel = 1000;

int main(int argc, char *argv[]) {
  int pid = getpid();
    printf("CommandName associated with PID=%d is %s\n", 
	   pid, getcommandname(pid));
    fflush(stdout);
}
