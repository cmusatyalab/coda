#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#ifdef	MACH
/* just in case we are built with gcc 1.36 */
#define	__MACH__ 1
#endif

char *env[]  = {
		"space for BLD from old env",
#if	defined(__MACH__)
		"PATH=/afs/cs/project/coda/@@sys/bin:/usr/cs/bin:/usr/ucb:/bin:/usr/bin",
		"LPATH=/afs/cs/project/coda/@@sys/lib:/usr/cs/lib:/lib:/usr/lib",
		"CPATH=/afs/cs/project/coda/@@sys/include:/usr/cs/include:/usr/include",
		"GCC_EXEC_PREFIX=/afs/cs/misc/gnu-comp/@sys/alpha/lib/gcc-lib/",
#elif	defined(__linux__)
		"PATH=/bin:/usr/bin",
#elif	defined(__NetBSD__)
		"PATH=/bin:/usr/bin",
#endif
		0 };

extern int errno;

struct rlimit rlimit;

main(int cnt, char *argv[], char *oldenv[])
{
	int skip = 1;
	extern char *getenv();
	char *bld = getenv("BLD");

	if ((int)bld) {
		env[0] = bld-4;	/* XXX */
		skip--;
	}

#ifdef	__MACH__
	getrlimit(RLIMIT_DATA, &rlimit);
	rlimit.rlim_cur = rlimit.rlim_max;
	setrlimit(RLIMIT_DATA, &rlimit);
#endif

#ifdef	__MACH__
	argv[0] = "/usr/bin/gmake";
	execve(argv[0], argv, &env[skip]);

	argv[0] = "/afs/cs/project/coda/@@sys/bin/gmake";
	execve(argv[0], argv, &env[skip]);

#elif	defined(__linux__)

	argv[0] = "/usr/bin/make";
	execve(argv[0], argv, &env[skip]);

#elif	defined(__NetBSD__)
	argv[0] = "/usr/bin/gmake";
	execve(argv[0], argv, &env[skip]);

	argv[0] = "/afs/cs/project/coda/@@sys/bin/gmake";
	execve(argv[0], argv, &env[skip]);
#endif

	printf("cmake: execve %s failed. errno = %d\n", argv[0], errno);
}

