#ifndef _BLURB_
#define _BLURB_
/*
 * This code was originally part of the CMU SCS library "libcs".
 * A branch of that code was taken in May 1996, to produce this
 * standalone version of the library for use in Coda and Odyssey
 * distribution.  The copyright and terms of distribution are
 * unchanged, as reproduced below.
 *
 * Copyright (c) 1990-96 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND CARNEGIE MELLON UNIVERSITY
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT
 * SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Users of this software agree to return to Carnegie Mellon any
 * improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 *
 * Export of this software is permitted only after complying with the
 * regulations of the U.S. Deptartment of Commerce relating to the
 * Export of Technical Data.
 */

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./lib-src/libcs/run.c,v 1.1 1996/11/22 19:19:38 braam Exp $";
#endif /*_BLURB_*/

/*  run[c], run[c]v, run[c]p, run[c]vp -- exec process and wait for it to exit
 *
 *  Usage:
 *	i = run (file, arg1, arg2, ..., argn, 0);
 *	i = runv (file, arglist);
 *	i = runp (file, arg1, arg2, ..., argn, 0);
 *	i = runvp (file, arglist);
 *	i = runc (func, file, arg1, arg2, ..., argn, 0);
 *	i = runcv (func, file, arglist);
 *	i = runcp (func, file, arg1, arg2, ..., argn, 0);
 *	i = runcvp (func, file, arglist);
 *
 *  Run, runv, runp, runvp and runc, runcv, runcp, runcvp have argument lists
 *  exactly like the corresponding routines, execl, execv, execlp, execvp.  The
 *  run routines perform a fork, then:
 *  IN THE NEW PROCESS, an execl[p] or execv[p] is performed with the specified
 *  arguments (after first invoking the supplied function in the runc* cases).
 *  The process returns with a -1 code if the exec was not successful.
 *  IN THE PARENT PROCESS, the signals SIGQUIT and SIGINT are disabled,
 *  the process waits until the newly forked process exits, the
 *  signals are restored to their original status, and the return
 *  status of the process is analyzed.
 *  All run routines return:  -1 if the exec failed or if the child was
 *  terminated abnormally; otherwise, the exit code of the child is
 *  returned.
 *
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#if	__STDC__
#include <stdarg.h>
#define	VA_START(a, b) va_start(a, b)
#else
#include <varargs.h>
#define	VA_START(a, b) va_start(a)
#endif

#include "libcs.h"

#define RUN_MAXARGS 1024

#if	defined(__NetBSD__) || defined(__WIN32__) || defined(LINUX)
#define	sigif	sigaction
#define	sif_handler	sa_handler
#define	sif_mask	sa_mask
#define	sif_flags	sa_flags
#else
#define	sigif	sigvec
#define	sif_handler	sv_handler
#define	sif_mask	sv_mask
#define	sif_flags	sv_flags
#endif

static
int dorun (func,name,argv,usepath)
int (*func)();
const char *name;
char **argv;
int usepath;
{
	int wpid;
	register int pid;
#ifdef	__linux__
	struct sigaction ignoresig,intsig,quitsig;
#else
	struct sigif ignoresig,intsig,quitsig;
#endif
#if	defined(__NetBSD__) || defined(__WIN32__) || defined(__LINUX__)
	int status;
#else
	union wait status;
#endif
	int execvp(), execv();

#if defined(__sgi) || defined(_AIX)
#define vfork() fork()
#endif
	if ((pid = vfork()) == -1)
		return(-1);	/* no more process's, so exit with error */

	if (pid == 0) {			/* child process */
		if (func)
		    (*func)();
		(void) setgid (getgid());
		(void) setuid (getuid());
		(*(usepath ? execvp : execv)) (name,argv);
		fprintf (stderr,"run: can't exec %s\n",name);
		_exit (0377);
	}
#ifdef	__linux__
	ignoresig.sa_handler = SIG_IGN;	/* ignore INT and QUIT signals */
#else
	ignoresig.sif_handler = SIG_IGN;	/* ignore INT and QUIT signals */
#endif
#if	defined(__NetBSD__) || defined(__WIN32__)
	sigemptyset(&ignoresig.sif_mask);
	ignoresig.sif_flags = 0;
#elif LINUX
	ignoresig.sa_mask = 0;
	ignoresig.sa_flags = 0;
#else
	ignoresig.sif_mask = 0;
	ignoresig.sif_flags = 0;
#endif
	(void) sigif (SIGINT,&ignoresig,&intsig);
	(void) sigif (SIGQUIT,&ignoresig,&quitsig);

	do {
#if	defined(__NetBSD__) || defined(__WIN32__)
		wpid = waitpid (-1, &status, WUNTRACED);
#else
		wpid = wait3 (&status, WUNTRACED, (struct rusage *)0);
#endif
		if (WIFSTOPPED (status)) {
		    (void) kill (0,SIGTSTP);
		    wpid = 0;
		}
	} while (wpid != pid && wpid != -1);

	/* restore signals */
	(void) sigif (SIGINT,&intsig,(struct sigif *)0);
	(void) sigif (SIGQUIT,&quitsig,(struct sigif *)0);
#if	defined(__NetBSD__) || defined(__WIN32__)
	if (WIFSIGNALED (status) ||
	    (WIFEXITED(status) && WEXITSTATUS(status) == 0xff))
		return (-1);

	return (WEXITSTATUS(status));
#else
	if (WIFSIGNALED (status) || status.w_retcode == 0377)
		return (-1);

	return (status.w_retcode);
#endif
}

int runv (name,argv)
const char *name;
char **argv;
{
	return (dorun ((int (*)())0, name, argv, 0));
}

int runvp (name,argv)
const char *name;
char **argv;
{
	return (dorun ((int (*)())0, name, argv, 1));
}

int 
#if	__STDC__
runp (const char *name, ...)
#else
runp (name,va_alist)
const char *name;
va_dcl
#endif
{
	va_list ap;
	int val;
	int i;
  	char *args[RUN_MAXARGS];

	VA_START(ap, name);
	for (i=0; i<RUN_MAXARGS; i++)
	  {
	    args[i] = va_arg(ap,char *);
	    if (args[i] == (char *) 0)
	      break;
	  }

	if (i == RUN_MAXARGS)
	    return -1;

	val = runvp (name, args);
	va_end(ap);

	return (val);
}

int runcv (func,name,argv)
int (*func)();
const char *name;
char **argv;
{
	return (dorun (func, name, argv, 0));
}

int
#if	__STDC__
runc (int (*func)(), const char *name, ...)
#else
runc (func,name,va_alist)
int (*func)();
const char *name;
va_dcl
#endif
{
	va_list ap;
	int val;
	int i;
  	char *args[RUN_MAXARGS];

	VA_START(ap, name);
	for (i=0; i<RUN_MAXARGS; i++)
	  {
	    args[i] = va_arg(ap,char *);
	    if (args[i] == (char *) 0)
	      break;
	  }

	if (i == RUN_MAXARGS)
	    return -1;

	va_end(ap);
	val = runcv (func, name, args);
	return(val);
}

int runcvp (func,name,argv)
int (*func)();
const char *name;
char **argv;
{
	return (dorun (func, name, argv, 1));
}

int
#if	__STDC__
runcp (int (*func)(), const char *name, ...)
#else
runcp (func,name,va_alist)
int (*func)();
const char *name;
va_dcl
#endif
{
	va_list ap;
	int val;
	int i;
  	char *args[RUN_MAXARGS];

	VA_START(ap, name);
	for (i=0; i<RUN_MAXARGS; i++)
	  {
	    args[i] = va_arg(ap,char *);
	    if (args[i] == (char *) 0)
	      break;
	  }

	if (i == RUN_MAXARGS)
	    return -1;
	val = runcvp (func, name, args);
	va_end(ap);
	return (val);
}

int
#if	__STDC__
run (const char *name, ...)
#else
run (name,va_alist)
const char *name;
va_dcl
#endif
{
	va_list ap;
	int val;
	int i;
  	char *args[RUN_MAXARGS];

	VA_START(ap, name);
	for (i=0; i<RUN_MAXARGS; i++)
	  {
	    args[i] = va_arg(ap,char *);
	    if (args[i] == (char *) 0)
	      break;
	  }

	if (i == RUN_MAXARGS)
	    return -1;

	val = runv (name, args);
	va_end(ap);
	return(val);
}
