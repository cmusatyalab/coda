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



/* This is a revamp of the Coda error handling. Aims and constraints:
   - errors returned by RPC's are platform independent. Ports to other
     platforms have made it mandatory to instate this without delay. 
   - there is room to accomodate Coda specific errors (like VSALVAGE)
     as well as system errors.
   - we want quick and easy translation of errors:
     a) from Coda to system errors (typically for clients)
     b) from system to Coda errors (typically for servers)
     c) we want "perror" like functions to save time in coding
   - Coda servers should only return errors which all of the client
     platforms can handle. 
   - If errors arrive on certain clients/servers and are not recognized
     a serious log message is printed and a default error is code
     is returned. 
*/

#ifndef ERRORS_H
#define ERRORS_H

#include <errno.h>
#include <rpc2/errorsdefs.h>

/* exported definitions */
const char *cerror(int err); /* like perror */

#endif
