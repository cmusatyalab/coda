#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/venus/Attic/faketrace.c,v 4.2 1997/02/26 16:03:12 rvb Exp $";
#endif /*_BLURB_*/

#include <stdio.h>

/* Dummy routines to build a fake tracelib for BSD44 port */
Trace_Open () {
    fprintf(stderr, "Trace_Open() in fake trace library should never be called\n");
    exit(-1);
}

Trace_Close() {
    fprintf(stderr, "Trace_Close() in fake trace library should never be called\n");
    exit(-1);
}

Trace_GetRecord() {
    fprintf(stderr, "Trace_GetRecord() in fake trace library should never be called\n");
    exit(-1);
}

Trace_FreeRecord() {
    fprintf(stderr, "Trace_FreeRecord() in fake trace library should never be called\n");
    exit(-1);
}

Trace_SetFilter() {
    fprintf(stderr, "Trace_SetFilter() in fake trace library should never be called\n");
    exit(-1);
}

Trace_Stats() {
    fprintf(stderr, "Trace_Stats() in fake trace library should never be called\n");
    exit(-1);
}

Trace_PrintPreamble() {
    fprintf(stderr, "Trace_PrintPreamble() in fake trace library should never be called\n");
    exit(-1);
}

Trace_GetVersion() {
    fprintf(stderr, "Trace_GetVersion() in fake trace library should never be called\n");
    exit(-1);
}


Trace_NodeIdToStr() {
    fprintf(stderr, "Trace_NodeIdToStr() in fake trace library should never be called\n");
    exit(-1);
}

Trace_OpcodeToStr() {
    fprintf(stderr, "Trace_OpcodeToStr() in fake trace library should never be called\n");
    exit(-1);
}

Trace_FlagsToStr() {
    fprintf(stderr, "Trace_FlagsToStr() in fake trace library should never be called\n");
    exit(-1);
}

Trace_InodeTypeToStr() {
    fprintf(stderr, "Trace_InodeTypeToStr() in fake trace library should never be called\n");
    exit(-1);
}

Trace_FidPtrToStr() {
    fprintf(stderr, "Trace_FidPtrToStr() in fake trace library should never be called\n");
    exit(-1);
}

Trace_OpenFlagsToStr() {
    fprintf(stderr, "Trace_OpenFlagsToStr() in fake trace library should never be called\n");
    exit(-1);
}

Trace_RecTimeToStr() {
    fprintf(stderr, "Trace_RecTimeToStr() in fake trace library should never be called\n");
    exit(-1);
}

Trace_FileTypeToStr() {
    fprintf(stderr, "Trace_FileTypeToStr() in fake trace library should never be called\n");
    exit(-1);
}

Trace_PrintRecord() {
    fprintf(stderr, "Trace_PrintRecord() in fake trace library should never be called\n");
    exit(-1);
}

Trace_DumpRecord() {
    fprintf(stderr, "Trace_DumpRecord() in fake trace library should never be called\n");
    exit(-1);
}


Trace_FidsEqual() {
    fprintf(stderr, "Trace_FidsEqual() in fake trace library should never be called\n");
    exit(-1);
}

Trace_CopyRecord() {
    fprintf(stderr, "Trace_CopyRecord() in fake trace library should never be called\n");
    exit(-1);
}


Trace_GetUser() {
    fprintf(stderr, "Trace_GetUser() in fake trace library should never be called\n");
    exit(-1);
}

Trace_GetFileType() {
    fprintf(stderr, "Trace_GetFileType() in fake trace library should never be called\n");
    exit(-1);
}

Trace_GetFileIndex() {
    fprintf(stderr, "Trace_GetFileIndex() in fake trace library should never be called\n");
    exit(-1);
}

Trace_GetRefCount() {
    fprintf(stderr, "Trace_GetRefCount() in fake trace library should never be called\n");
    exit(-1);
}

Trace_GetFid() {
    fprintf(stderr, "Trace_GetFid() in fake trace library should never be called\n");
    exit(-1);
}

Trace_GetPath() {
    fprintf(stderr, "Trace_GetPath() in fake trace library should never be called\n");
    exit(-1);
}

atot() {
    fprintf(stderr, "atot() in fake trace library should never be called\n");
    exit(-1);
}
