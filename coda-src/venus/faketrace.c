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
