/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _ADV_SIDEKICK_H_
#define _ADV_SIDEKICK_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <errno.h>
#include <lwp/lock.h>
#include <lwp/lwp.h>
#include <ports.h>
#include <pwd.h>
#include <rpc2/rpc2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <struct.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __cplusplus
}
#endif

#include "adv_skk.h"
#include "coda_assert.h" 
#include "coda_string.h" 
#include "coda_wait.h"
#include "copyfile.h"
#include "repcmds.h"
#include "venus_adv.h"
#include "venusioctl.h"

#define ASRARGS 7 /* XXX - maximum number of arguments to an ASR */
#define ASRLOGDIR "/tmp/asrlogs"
#define DEF_LOGFILE "/usr/coda/etc/sidekick.log"
#define DEF_ROCK 1
#define DSTACK 131072
#define FIXEDDIR "/tmp"
#define HOMEDIR_PREFIX "/coda/usr/"
#define INTEREST_FILE SYSCONFDIR "/sidekick.intr"

int table(int, int, char *, int, int);
int ffilecopy(FILE*, FILE*);

extern FILE *logfile;
extern int err, reqcnt;
extern int vmajor, vminor, session;
extern RPC2_PortIdent rpc2_LocalPort;
extern RPC2_Handle VenusCID;
extern char *InterestNames[MAXEVENTS];

struct pnode {
    PROCESS cpid;
    RPC2_PacketBuffer *pbuf;
    pnode *next;
    char name[32];
    int realm, volume;
    int kid, req;
};

RPC2_Handle contact_venus(const char *);
int executor(char *, int, int);
int get_homedir(int, char *);
void init_RPC(void);
int interests(int);
void knock(const char *, int, int);
int parse_cmd_line(int, char **);
int parse_path(const char *, char *, char *);
int parse_resolvefile(const char *, const char *, char *);
int point(char *linkpath, char *target);
int worker(void *);

#define lprintf(msg...)				\
do {						\
  if (err) fprintf(stderr, ##msg);		\
  fprintf(logfile, ##msg);			\
  fflush(logfile);				\
} while (0)

#define quit(msg...)				\
do {						\
  struct pnode *tmp;				\
  if (logfile != NULL) {			\
    fprintf(logfile, ##msg);			\
    fprintf(logfile, "\n");			\
  }						\
  if (err || (logfile == NULL)) {		\
    fprintf(stderr, ##msg);			\
    fprintf(stderr, "\n");			\
  }						\
  while ((tmp = phead) != NULL) {		\
    phead = phead->next;			\
    free(tmp);					\
  }						\
  exit(-1);					\
} while (0)

#endif /* _ADV_SIDEKICK_H_ */
