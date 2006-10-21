/* BLURB gpl

			    Coda File System
				Release 6

	      Copyright (c) 2006 Carnegie Mellon University
		    Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

			Additional copyrights
			    none currently
#*/

/*
 * Send signals to venus to control debug levels, shutdown, log rotation, etc.
 *
 * Not to be confused with coda-src/vol/vutil.cc which contains volume related
 * helper functions. But we can't rename that file to volutil.cc because that
 * would get confusing because we have a binary called volutil (which is built
 * from the source file coda-src/volutil/volclient.cc).
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <assert.h>

#include "coda_string.h"
#include "codaconf.h"

#ifdef __cplusplus
}
#endif

#include "venus.private.h"

#define fail(msg...) do{ fprintf(stderr, ## msg); exit(EXIT_FAILURE); }while(0);

#define MAX_CTRL_LEN 79

static void usage(const char *msg)
{
    if (msg) fputs(msg, stderr);
    fputs("Usage:\tvutil [--shutdown|--clearstats|--stats|--swaplogs]\n"
	  "\tvutil [--debug <loglevel> [<rpc2_loglevel> [<lwp_loglevel>]]]\n",
	  stderr);
    exit(EXIT_SUCCESS);
}

static void logrotate(const char *log)
{
    char *newpath = (char *)malloc(strlen(log) + 3);
    char *oldpath = (char *)malloc(strlen(log) + 3);
    char *tmp;
    int i;

    assert(newpath && oldpath);

    sprintf(newpath, "%s.9", log);
    for (i = 8; i >= 0; i--) {
	sprintf(oldpath, "%s.%d", log, i);
	if (access(oldpath, F_OK) == 0)
	    rename(oldpath, newpath);
	tmp = newpath; newpath = oldpath; oldpath = tmp;
    }
    if (access(log, F_OK) == 0)
	rename(log, newpath);

    free(newpath);
    free(oldpath);
}

int main(int argc, char **argv)
{
    char *cache_dir, *pid_file, *ctrl_file, *log_file, *err_file;

    unsigned int loglevel, rpc2loglvl, lwploglvl;
    char *arg, ctrl[MAX_CTRL_LEN+1];
    pid_t venus_pid = 0;
    int rc, i, tmp = 0;
    int rotated;
    FILE *fp;

    cache_dir = pid_file = ctrl_file = log_file = err_file = NULL;

    if (argc == 1 ||
	strcmp(argv[1], "-h") == 0 ||
	strcmp(argv[1], "--help") == 0)
	usage(NULL);

    /* Load setting from the venus.conf configuration file */
    codaconf_init("venus.conf");

    CODACONF_STR(cache_dir, "cachedir", DFLT_CD);
    CODACONF_STR(pid_file, "pid_file", DFLT_PIDFILE);
    if (*pid_file != '/') {
	char *tmp = (char *)malloc(strlen(cache_dir) + strlen(pid_file) + 2);
	assert(tmp != NULL);
	sprintf(tmp, "%s/%s", cache_dir, pid_file);
	pid_file = tmp;
    }
    CODACONF_STR(ctrl_file, "run_control_file", DFLT_CTRLFILE);
    if (*ctrl_file != '/') {
	char *tmp = (char *)malloc(strlen(cache_dir) + strlen(ctrl_file) + 2);
	assert(tmp != NULL);
	sprintf(tmp, "%s/%s", cache_dir, ctrl_file);
	ctrl_file = tmp;
    }
    CODACONF_STR(log_file, "logfile", DFLT_LOGFILE);
    CODACONF_STR(err_file, "errorlog", DFLT_ERRLOG);

    /* read venus process id from the pid file */
    fp = fopen(pid_file, "r");
    if (fp) {
	rc = fscanf(fp, "%d", &tmp);
	fclose(fp);
    }
    if (fp && rc == 1)
	venus_pid = (pid_t)tmp;

    argc--; argv++;
    while(argc--)
    {
	rotated = 0;
	arg = *(argv++);

	/* remove one or two leading dashes */
	if (arg[0] == '-') arg++;
	if (arg[0] == '-') arg++;

	/* venus shutdown */
	if (strcmp(arg, "shutdown") == 0) {
	    /* make sure we aren't about to kill the system */
	    if (venus_pid <= 1)
		fail("Got invalid process id from %s\n", pid_file);
	    kill(venus_pid, SIGTERM);
	    break; /* no point in trying to send any other signals */
	}

	/* rotate log, actually venus only closes and reopens the log so
	 * we'll do the log rotation here. */
	if (strcmp(arg, "swaplog") == 0 || strcmp(arg, "swap") == 0)
	{
	    logrotate(log_file);
	    logrotate(err_file);
	    snprintf(ctrl, MAX_CTRL_LEN, "SWAPLOGS\n");
	    rotated = 1;
	}
	/* set debug levels */
	else if (strcmp(arg, "debug") == 0 ||
		 strcmp(arg, "d") == 0)
	{
	    loglevel = rpc2loglvl = lwploglvl = 0;
	    errno = 0;

	    /* we need at least one argument */
	    if (argc) loglevel = strtoul(argv[0], NULL, 10);
	    if (!argc || errno) usage("vutil --debug: missing debug level\n");
	    argc--; argv++;

	    /* and now try to parse the optional arguments */
	    if (argc) {
		rpc2loglvl = strtoul(argv[0], NULL, 10);
		if (errno) rpc2loglvl = 0;
		else { argc--; argv++; }
	    }
	    if (argc && errno == 0) {
		lwploglvl = strtoul(argv[0], NULL, 10);
		if (errno) lwploglvl = 0;
		else { argc--; argv++; }
	    }

	    snprintf(ctrl, MAX_CTRL_LEN, "DEBUG %u %u %u\n",
		     loglevel, rpc2loglvl, lwploglvl);
	}
	/* reset statistic counters */
	else if (strcmp(arg, "clearstats") == 0 ||
		 strcmp(arg, "cs") == 0)
	    snprintf(ctrl, MAX_CTRL_LEN, "STATSINIT\n");

	/* dump venus state */
	else if (strcmp(arg, "stats") == 0 ||
		 strcmp(arg, "stat") == 0 ||
		 strcmp(arg, "s") == 0)
	    snprintf(ctrl, MAX_CTRL_LEN, "STATS\n");

	else usage("vutil: unknown argument\n");

	/* update control file */
	ctrl[MAX_CTRL_LEN] = '\0';
	fp = fopen(ctrl_file, "w");
	if (fp) {
	    fputs(ctrl, fp);
	    fclose(fp);
	}

	if (!fp)
	    fprintf(stderr, "Failed to write to %s\n", ctrl_file);

	/* make sure we aren't about to kill the system */
	else if (venus_pid <= 1) {
	    fprintf(stderr, "Failed to get a valid pid from %s\n", pid_file);
	    unlink(ctrl_file);
	}
	else if (kill(venus_pid, SIGHUP) != 0) {
	    if (!rotated)
		fprintf(stderr, "Failed to signal venus\n");
	    unlink(ctrl_file);
	}

	/* wait until venus has processed the signal, it will unlink the
	 * control file when it has read the command. */
	for (i = 0; i < 60; i++) {
	    rc = access(ctrl_file, F_OK);
	    if (rc != 0) break;
	    sleep(1);
	}
	if (rc == 0) {
	    unlink(ctrl_file);
	    fail("Venus did not respond to %s\n", arg);
	}
    }
    exit(EXIT_SUCCESS);
}

