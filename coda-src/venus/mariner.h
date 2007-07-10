/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
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


const int MWBUFSIZE = 80;

extern fd_set MarinerMask;
extern int MarinerMaxFD;

void MarinerInit(void);
void MarinerMux(fd_set *mask);
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
  friend void MarinerMux(fd_set *mask);
  friend void MarinerReport(VenusFid *, uid_t);
  friend void PrintMariners(int);

    static int tcp_muxfd;
    static int unix_muxfd;
    static int nmariners;

    unsigned DataReady : 1;
    unsigned dying : 1;
    unsigned logging : 1;	    /* for MarinerLog() */
    unsigned reporting : 1;	    /* for MarinerReport() */
    unsigned want_volstate : 1;	    /* for MarinerReportVolState() */
    uid_t uid;			    /* valid iff reporting = 1 */
    int fd;
    char commbuf[MWBUFSIZE];

    mariner(int);
    int operator=(mariner&);    /* not supported! */
    virtual ~mariner();

    int Read();
    int Write(const char *buf, ...);
    void AwaitRequest();
    void Resign(int);
    void PathStat(char *);
    void FidStat(VenusFid *);
    void Rpc2Stat();

  protected:
    virtual void main(void);

  public:
    int IsLogging(void)		  { return logging; }
    int WantVolState(void)	  { return want_volstate; }
    int write(char *buf, int len) { return ::write(fd, buf, len); }
};


class mariner_iterator : public vproc_iterator {

  public:
    mariner_iterator();
    mariner *operator()();
};

#endif /* _VENUS_MARINER_H_ */
