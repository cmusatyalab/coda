/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#include <unistd.h>
#include <stdlib.h>
#include <rpc2/errors.h>
#include <rpc2/rpc2.h>

/* translate RPC2 error to System Error */
int RPC2_R2SError(int rpc2_err)
{
    int sys_err;
    if (rpc2_err <= 0) return rpc2_err;

    switch (rpc2_err) {
#include <switchc2s.h>
    default:
	fprintf(stderr, "Unknown translation for rpc2 error %d\n", rpc2_err);
	sys_err = 4711;
    }
    return sys_err;
}

/* translate System error to RPC2 error */
int RPC2_S2RError(int sys_err)
{
    int rpc2_err;
    if ( sys_err <= 0 ) return sys_err;

    switch (sys_err) {
#include <switchs2c.h>
    default:
	fprintf(stderr, "Unknown translation for system errno %d\n", sys_err);
	rpc2_err = 4711;
    }
    return rpc2_err;
}

const char *cerror(int err)
{
    const char *txt;

    switch (err) {
#include <switchs2e.h>
    case 0: txt = "Success"; break;
    default: txt = "Unknown error!";
    }
    return txt;
}
