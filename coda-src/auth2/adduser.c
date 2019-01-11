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
 -- Routine to add a new user to the auth data base from a list built during user int

*/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>

#include "auth2.h"

#ifdef __cplusplus
}
#endif __cplusplus

int main(int argc, char **argv);
PRIVATE int AddNewUser(char *uid, char *pw);
PRIVATE void MakeString(char *input);
PRIVATE char *NextField(char *input);
PRIVATE char *NextRecord(char *input);

PRIVATE RPC2_Handle AuthID;

int main(int argc, char **argv)
{
    register int i;
    char *current;
    char *next;
    char *uid;
    char *pw;
    int rc;
    int fd;
    struct stat buff;
    char *auid   = 0;
    char *apw    = 0;
    char *filenm = 0;
    char *area   = 0;
    RPC2_EncryptionKey key;

    /* parse arguments    */
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-f") == 0) {
                filenm = argv[++i];
                continue;
            }
            break;
        }
        if (auid == 0) {
            auid = argv[i];
            continue;
        }
        if (apw == 0) {
            apw = argv[i];
            continue;
        }
        break;
    }

    if (!auid || !apw || !filenm) {
        printf("usage: newuser -f filename authuserid authpasswd\n");
        fflush(stdout);
        exit(EXIT_FAILURE);
    }
    /* Bind to auth server using auid and apw */

    U_InitRPC();
    memset(key, 0, sizeof(RPC2_EncryptionKey));
    strncpy(key, apw, sizeof(RPC2_EncryptionKey));
    rc = U_BindToServer(1, auid, key, &AuthID);
    if (rc != AUTH_SUCCESS) {
        printf("Bind to Auth Server failed %s\n", U_Error(rc));
        fflush(stdout);
        exit(EXIT_FAILURE);
    }

    /* open input file and read it into area malloc */
    if (stat(filenm, &buff)) {
        printf("Could not stat %s because %d\n", filenm, errno);
        fflush(stdout);
        exit(EXIT_FAILURE);
    }
    area = (char *)malloc(buff.st_size + 1);
    fd   = open(filenm, O_RDONLY, 0);
    if (fd <= 0) {
        printf("Could not open %s because %d\n", filenm, errno);
        fflush(stdout);
        exit(EXIT_FAILURE);
    }
    rc = read(fd, area, buff.st_size);
    if (rc != buff.st_size) {
        printf("Could nor read %s got %d bytes instead of %d, error = %d\n",
               filenm, rc, buff.st_size, errno);
        fflush(stdout);
        exit(EXIT_FAILURE);
    }
    close(fd);
    *(area + buff.st_size + 1) = '\0';

    /* parse data in area and pass it to AddNewUser. The first field in each line is the
    uid, the second field in each line is the password. Fields are blank separated */

    for (current = area; current < area + buff.st_size; current = next) {
        next = NextRecord(current); /* next line */
        uid  = current; /* use first field  */
        pw   = NextField(uid); /* use second field */
        MakeString(uid); /* make uid a string */
        MakeString(pw); /* make pw a string */
        AddNewUser(uid, pw);
    }

    /* clean up and Unbind the connection */
    if (area)
        free(area);
    RPC2_Unbind(AuthID);
    return (0);
}

PRIVATE int AddNewUser(char *uid, char *pw)
{
    RPC2_Integer vid;
    int rc;
    RPC2_EncryptionKey ek;
    char userid[256];

    /* get vid from input uid */
    memset(ek, 0, sizeof(RPC2_EncryptionKey));
    strncpy(ek, pw, sizeof(RPC2_EncryptionKey));
    rc = AuthNameToId(AuthID, uid, &vid);
    if (rc != AUTH_SUCCESS) {
        printf("Name translation for %s failed %s\n", uid, U_Error(rc));
        fflush(stdout);
        return (-1);
    }

    /* pass vid and pw to auth server */
    strcpy(userid, uid);
    strcat(userid, " ");
    rc = AuthNewUser(AuthID, vid, ek, userid);
    if (rc != AUTH_SUCCESS) {
        printf("Add User for %s failed with %s\n", uid, U_Error(rc));
        fflush(stdout);
        return (-1);
    }
    return (0);
}

PRIVATE void MakeString(char *input)
{ /* scans for the first field delimiter and makes it an end of string */
    register char srch;
    register int i;

    for (i = 0; i < 256; i++) {
        if ((srch = input[i]) == '\0' || srch == '\t' || srch == ' ' ||
            srch == '\n') {
            input[i] = '\0';
            break;
        }
    }
}

PRIVATE char *NextField(char *input)
{ /* scans the input string for up to 255 characters and returns the address
      of the next field.  If no field is found in 255 characters or a null is found,
      it returns a zero */
    register char srch;
    register int i;

    for (i = 0; i < 256; i++) {
        if ((srch = input[i]) == '\0') { /* null - end of string */
            return (0);
        }
        if (srch == '\t' || srch == ' ') {
            return &input[i + 1];
        }
    }
    return (0);
}

PRIVATE char *NextRecord(char *input)
{ /* scans the input string to find the next record which is either a null or one
      beyond a new line character */
    register char srch;
    register int i;

    for (i = 0; i < 256; i++) {
        if ((srch = input[i]) == '\0' || srch == '\n') {
            return &input[i + 1];
        }
    }
}
