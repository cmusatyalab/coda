
/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2018 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/
/* system */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* venus */
#include <venus/venusfid.h>
#include <venus/vproc.h>

/* This file holds the symbols from venus.cc needed for enable 
   the build of the tests. */

#define UNSET_PRIMARYUSER 0 /* primary user of this machine */

/* Some helpers to add fd/callbacks to the inner select loop */
struct mux_cb_entry {
    struct mux_cb_entry *next;
    int fd;
    void (*cb)(int fd, void *udata);
    void *udata;
};
static struct mux_cb_entry *_MUX_CBEs;

uid_t PrimaryUser = UNSET_PRIMARYUSER;

/* Logging/Console */
const char *consoleFile = "/usr/coda/etc/console";
const char *VenusLogFile = "/tmp/test";

/* Process */
int parent_fd = -1;
vproc *Main =  NULL;
int nofork = 1;

/* Cache */
const char *CachePrefix = "";
unsigned int CacheBlocks = 100000;

/* Mariner */
const char *MarinerSocketPath = "/usr/coda/spool/mariner";
int masquerade_port = 0;
#if defined(HAVE_SYS_UN_H) && !defined(__CYGWIN32__)
int mariner_tcp_enable = 0;
#else
int mariner_tcp_enable = 1;
#endif

/* ASR */
pid_t ASRpid;
VenusFid ASRfid;
uid_t ASRuid;
const char *ASRLauncherFile = NULL;
const char *ASRPolicyFile = NULL;

/* Other */
int detect_reintegration_retry = 0;
int CleanShutDown = 0;
const char *SpoolDir = "/usr/coda/spool";
const char *venusRoot = "/coda";
int PiggyValidations = 15;
int plan9server_enabled = 0;
int option_isr = 0;
const char *kernDevice = "/dev/cfs0";
int SearchForNOreFind = 0; 
int redzone_limit = -1;
int yellowzone_limit = -1;
VenusFid rootfid;

/* Helper to add a file descriptor with callback to main select loop.
 *
 * Call with cb == NULL to remove existing callback.
 * cb is called with fd == -1 when an existing callback is removed or updated.
 */
void MUX_add_callback(int fd, void (*cb)(int fd, void *udata), void *udata)
{
    struct mux_cb_entry *cbe = _MUX_CBEs, *prev = NULL;

    for (; cbe; cbe = cbe->next)
    {
        if (cbe->fd == fd) {
            /* remove old callback entry */
            if (prev)
                prev->next = cbe->next;
            else
                _MUX_CBEs = cbe->next;

            /* allow cb to free udata resources */
            cbe->cb(-1, cbe->udata);

            free(cbe);
            break;
        }
        prev = cbe;
    }
    /* if we are not adding a new callback, we're done */
    if (cb == NULL)
        return;

    cbe = (struct mux_cb_entry *)malloc(sizeof(*cbe));
    assert(cbe != NULL);

    cbe->fd = fd;
    cbe->cb = cb;
    cbe->udata = udata;

    cbe->next = _MUX_CBEs;
    _MUX_CBEs = cbe;
}