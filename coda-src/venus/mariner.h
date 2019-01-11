/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2018 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/*
 *
 * Specification of the Venus Mariner facility.
 *
 */

#ifndef _VENUS_MARINER_H_
#define _VENUS_MARINER_H_ 1

class mariner;
class mariner_iterator;

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#ifdef __cplusplus
}
#endif

#include "vproc.h"

class plan9server;

const int MWBUFSIZE = 80;

void MarinerInit(void);
void MarinerMux(int fd, void *udata);
void MarinerLog(const char *, ...);
void MarinerReport(VenusFid *, uid_t);
void MarinerReportVolState(const char *volname, const char *realm,
                           const char *state, int cml_entries,
                           const struct VolFlags *vflags);
void PrintMariners(void);
void PrintMariners(FILE *);
void PrintMariners(int);

class mariner : public vproc {
    friend void MarinerInit();
    friend void MarinerMux(int fd, void *udata);
    friend void MarinerReport(VenusFid *, uid_t);
    friend void PrintMariners(int);
    friend int k_Replace(VenusFid *fid_1, VenusFid *fid_2);
    friend class plan9server;

    static int nmariners;
    struct Lock write_lock;

    unsigned dying : 1;
    unsigned logging : 1; /* for MarinerLog() */
    unsigned reporting : 1; /* for MarinerReport() */
    unsigned want_volstate : 1; /* for MarinerReportVolState() */
    uid_t uid; /* valid iff reporting = 1 */
    int fd;
    char commbuf[MWBUFSIZE];

    plan9server *p9srv;

    mariner(int);
    int operator=(mariner &); /* not supported! */
    virtual ~mariner();

    /* LWP-aware non-blocking read/write functions */
    ssize_t read_until_done(void *buf, size_t len);
    ssize_t write_until_done(const void *buf, size_t len);

    int Read();
    int Write(const char *buf, ...);
    int AwaitRequest();
    void Resign(int);
    void PathStat(char *);
    void FidStat(VenusFid *);
    void Rpc2Stat();

protected:
    virtual void main(void);

public:
    int IsLogging(void) { return logging; }
    int WantVolState(void) { return want_volstate; }
    int write(char *buf, int len) { return write_until_done(buf, len); }
};

class mariner_iterator : public vproc_iterator {
public:
    mariner_iterator();
    mariner *operator()();
};

#endif /* _VENUS_MARINER_H_ */
