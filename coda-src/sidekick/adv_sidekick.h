#ifndef _ADV_SIDEKICK_H_
#define _ADV_SIDEKICK_H_

#ifdef __cplusplus
extern "C" {
#endif __cplusplus
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
#endif __cplusplus

#include "adv_skk.h"
#include "coda_assert.h" 
#include "coda_string.h" 
#include "coda_wait.h"
#include "copyfile.h"
#include "venus_adv.h"
#include "venusioctl.h"

#define HOMEDIR_PREFIX "/coda/usr/"
#define DEF_LOGFILE "/usr/coda/etc/skk.log"
#define INTEREST_FILE SYSCONFDIR "/skk.interests"
#define DSTACK 65536
#define DEF_ROCK 1
#define ASRARGS 7
#define NOT_IN_SESSION	0
#define LOCAL_GLOBAL	1
#define SERVER_SERVER	2

extern int table(int, int, char *, int, int);
extern int ffilecopy(FILE*, FILE*);

extern FILE *logfile;
extern int err, reqcnt;
extern int vmajor, vminor, session;
extern RPC2_PortIdent rpc2_LocalPort;
extern RPC2_Handle VenusCID;
extern char *InterestNames[MAXEVENTS];

RPC2_Handle contact_venus(const char *);
int executor(char *, int, int);
int get_homedir(int, char *);
void init_RPC(void);
int interests(int);
void knock(const char *, int, int);
int parse_cmd_line(int, char **);
int parse_path(const char *, char *, char *);
int parse_resolvefile(const char *, const char *, char *);
int worker(void *);

#define freeif(pointer)				\
do {						\
 if (pointer != NULL) {				\
   free(pointer);				\
   pointer = NULL;				\
 } 						\
} while (0)

#define lprintf(msg...)				\
do {						\
  if (err) fprintf(stderr, ##msg);		\
  fprintf(logfile, ##msg);			\
  fflush(logfile);				\
} while (0)

#define quit(msg...)				\
do {						\
  if (logfile != NULL) {			\
    fprintf(logfile, ##msg);			\
    fprintf(logfile, "\n");			\
  }						\
  if (err || (logfile == NULL)) {		\
    fprintf(stderr, ##msg);			\
    fprintf(stderr, "\n");			\
  }						\
  if (lwp_ready) LWP_TerminateProcessSupport();	\
  exit(-1);					\
} while (0)

struct pnode {
  PROCESS cpid;
  RPC2_PacketBuffer *pbuf;
  pnode *next;
  char name[32];
  int kid, tmp, req;
};

#endif /* _ADV_SIDEKICK_H_ */
