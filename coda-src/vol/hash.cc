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








/* hash.c:  get a hash code for a string in the range 1 to size */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <strings.h>

#ifdef __cplusplus
}
#endif __cplusplus


int HashString(register char *s, unsigned int size)
{
    register unsigned int sum;
    register int n;
    
    /* Sum the string in reverse so that consecutive integers, as strings, do not
       hash to consecutive locations */
    for (sum = 0, n = strlen(s), s += n-1; n--; s--)
        sum = (sum*31) + (*s-31);
    return ((sum % size) + 1);
}
