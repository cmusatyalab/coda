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







/*
 *    Generic associative data structure.
 */

#ifndef _BINDING_H_
#define _BINDING_H_ 1


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* from util */
#include <dlist.h>


class binding {
  public:
#ifdef	VENUSDEBUG
    static int allocs;
    static int deallocs;
#endif	VENUSDEBUG

    dlink binder_handle;
    void *binder;
    dlink bindee_handle;
    void *bindee;
    int referenceCount;

    binding();
    binding(binding& b) { abort(); }	/* not supported! */
    int operator=(binding&) { abort(); }	/* not supported! */
    ~binding();

    void IncrRefCount() { referenceCount++; }
    void DecrRefCount() { referenceCount--; }
    int GetRefCount() { return(referenceCount); }

    void print() { print(stdout); }
    void print(FILE *fp) { fflush(fp); print(fileno(fp)); }
    void print(int);
};

#endif	not _BINDING_H_
