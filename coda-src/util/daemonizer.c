/* BLURB gpl

                           Coda File System
                              Release 6

             Copyright (c) 2004 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include "coda_flock.h"

static int check_child_completion(int fd)
{
    fd_set rfds, efds;
    char check = '\0';
    int n;

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    FD_ZERO(&efds);
    FD_SET(fd, &efds);

    n = select(fd+1, &rfds, NULL, &efds, NULL);
    if (n > 0 && FD_ISSET(fd, &rfds))
	n = read(fd, &check, 1);

    return (n && check) ? 0 : 1;
}

#if !defined (__CYGWIN32__)
static int pidfd = -1;
#else
static int lockfd = -1;
#endif

void update_pidfile(char *pidfile)
{
    char str[11]; /* can we assume that pid_t is limited to 32-bit? */
    int rc, n;

#if !defined(__CYGWIN32__)

    pidfd = open(pidfile, O_WRONLY | O_CREAT, 0640);
    if (pidfd < 0) {
	fprintf(stderr, "Can't open pidfile \"%s\"\n", pidfile);
	exit(1);
    }

    rc = myflock(pidfd, MYFLOCK_EX, MYFLOCK_NB);
    if (rc < 0) {
	fprintf(stderr, "Can't lock pidfile \"%s\", am I already running?\n",
		pidfile);
	exit(1);
    }

#else

    /* On Cygwin, locking the pid file makes it impossible for
       vutil to open the file to the venus' pid.  Use a different
       lock file. */

    char lockname[MAXPATHLEN];
    int  pidfd;
    int  namelen = strlen(pidfile);

    if ((namelen+4) > MAXPATHLEN) {
      fprintf(stderr, "pid file name too long.\n");
      exit(1);
    }

    n = snprintf (lockname, MAXPATHLEN, "%s.lk", pidfile);
    assert(n > namelen);
    
    lockfd = open(lockname, O_WRONLY | O_CREAT, 0640);
    if (lockfd < 0) {
	fprintf(stderr, "Can't open lock file \"%s\"\n", lockname);
	exit(1);
    }

    rc = myflock(lockfd, MYFLOCK_EX, MYFLOCK_NB);
    if (rc < 0) {
	fprintf(stderr, "Can't lock lock file \"%s\", am I already running?\n",
		lockname);
	exit(1);
    }

    pidfd = open(pidfile, O_WRONLY | O_CREAT, 0640);
    if (pidfd < 0) {
	fprintf(stderr, "Can't open pidfile \"%s\"\n", pidfile);
	exit(1);
    }

#endif

    n = snprintf(str, sizeof(str), "%d\n", getpid());
    assert(n >= 0 && n < sizeof(str));

    /* write pid to lockfile */
    ftruncate(pidfd, 0);
    rc = write(pidfd, str, n);
    if (rc != n) {
	fprintf(stderr, "Can't update pidfile \"%s\"\n", pidfile);
	exit(1);
    }

#if !defined(__CYGWIN32__)
    /* leave pidfd open otherwise we lose the lock */
#else
    /* leave lockfd open, close the pidfd.  */
    close(pidfd);
#endif
}


int daemonize(void)
{
    int fds[2];
    int fd, rc;
    pid_t pid;

    /* grab a pipe so that the child can inform the parent when it is ready */
    rc = pipe(fds);
    if (rc) {
	fprintf(stderr, "daemonize: failed to create pipe\n");
	exit(1);
    }

    /* fork into background */
    pid = fork();
    if (pid > 0) {
	close(fds[1]);
	rc = check_child_completion(fds[0]);
	exit(rc);
    }

    if (pid < 0) {
	close(fds[0]);
	close(fds[1]);
	fprintf(stderr, "daemonize: failed to fork\n");
	/* should we continue in the foreground? */
	return -1;
    }

    /* obtain a new process group, change cwd, clear umask */
    setsid();
    chdir("/");

    /* second part of the double fork */
    pid = fork();
    if (pid != 0)
	exit(0);

    /* close almost all filedescriptors (except for the pipe to the parent). */
    for (fd = 3; fd < FD_SETSIZE; fd++)
	if (fd != fds[1])
	    close(fd);

    /* detach stdin by redirecting from /dev/null. */
    freopen("/dev/null", "r", stdin);

    /* and return the fd that connects to our parent. */
    return fds[1];
}

/* let the parent process know that we've succesfully started. */
void gogogo(int parent_fd)
{
    if (parent_fd < 0)
	return;

    /* write anything as long as it isn't \0 */
    if (write(parent_fd, "1", 1) != 1) {
	/* something must have gone wrong with the parent */
	exit(1);
    }

    close(parent_fd);
}

#ifdef TESTING
int main(int argc, char **argv)
{
    int parent;

    parent = daemonize("/tmp/pid");

    sleep(5);

    if (argc > 1) {
	gogogo(parent);
	sleep(30);
    }

    exit(0);
}
#endif
