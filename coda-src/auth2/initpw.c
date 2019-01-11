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

#*/

/*
                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.   This  code is provided "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to  modify,  distribute and sublicense this code,  which is
based on Version 2  of  AFS  and  does  not  contain  the features and
enhancements that are part of  Version 3 of  AFS.  Version 3 of AFS is
commercially   available   and  supported  by   Transarc  Corporation,
Pittsburgh, PA.

*/

/*
initpw.c -- hack routine to initially generate the pw file used by auth2

*/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/file.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>

#include <lwp/lwp.h>
#include <rpc2/rpc2.h>
#include <util.h>

#ifdef __cplusplus
}
#endif

int main(int argc, char **argv);
static void parse(char *line, RPC2_EncryptionKey outpw, char **last);

static int DebugLevel = 0;
static int KeyIsValid = FALSE;
static RPC2_EncryptionKey EKey;

int main(int argc, char **argv)
{
    int i;
    char thisline[1000], *lastpart;
    RPC2_EncryptionKey thispw;
    PROCESS mypid;

    /* Obtain invocation options */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-x") == 0 && i < argc - 1) {
            DebugLevel = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "-k") == 0 && i < argc - 1) {
            KeyIsValid = TRUE;
            strncpy((char *)EKey, argv[++i], sizeof(RPC2_EncryptionKey));
            continue;
        }
        printf("Usage: initpw [-x debuglevel] [-k key]\n");
        exit(EXIT_FAILURE);
    }

    /* Reads lines from stdin of the form:
     *
     *   <ViceId> <Clear Password> <other junk>\n
     *
     * Produces lines on stdout of the form:
     *
     *   <ViceId> <Hex representation of encrypted password> <other junk>\n
     */
    if (!KeyIsValid)
        fprintf(stderr, "WARNING: no key specified\n");

    CODA_ASSERT(LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, &mypid) ==
                LWP_SUCCESS);
    while (TRUE) {
        if (fgets(thisline, sizeof(thisline), stdin) == NULL)
            break;
        if (thisline[strlen(thisline) - 1] == '\n')
            thisline[strlen(thisline) - 1] = '\0';
        parse(thisline, thispw, &lastpart);
        if (KeyIsValid)
            rpc2_Encrypt((char *)thispw, (char *)thispw,
                         sizeof(RPC2_EncryptionKey), EKey, RPC2_XOR);
        printf("%s\t", thisline); /* only viceid part */
        for (i = 0; i < sizeof(RPC2_EncryptionKey); i++)
            printf("%02x", thispw[i]);
        printf("\t%s\n", lastpart);
    }
    return (0);
}

static void parse(char *line, RPC2_EncryptionKey outpw, char **last)
/* line:    input: first tab is replaced by null */
/* outpw:   output: filled with password */
/* last:    output: points to first character of uninterpreted part */
{
    char *pp;
    int i;
    pp = strchr(line, '\t');
    if (pp == NULL) {
        fprintf(stderr, "Bogus line in input file: \"%s\"\n", line);
        abort();
    }
    *pp++ = 0;
    memset(outpw, 0, sizeof(RPC2_EncryptionKey));
    i = 0;
    while (pp && *pp != 0 && *pp != '\t' && i < sizeof(RPC2_EncryptionKey))
        outpw[i++] = *pp++;
    while (pp && *pp != 0 && *pp != '\t')
        pp++;
    if (*pp == 0)
        *last = pp;
    else
        *last = pp + 1; /* skip over tab */
}
