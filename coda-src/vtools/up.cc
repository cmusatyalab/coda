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

/* missing type from C language */
#define Boolean short
#define true 1
#define false 0

/* ************************************************************* */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/viceioctl.h>
#include <stdlib.h>
#include <sysent.h>
#include <a.out.h>
#if defined(__linux__) && defined(sparc)
#define getpagesize() PAGE_SIZE
#endif

#ifdef __cplusplus
}
#endif __cplusplus

/* ************************************************************* */

void main(int argc, char **argv);
static void ScanArgs(int argc, char **argv);
static Boolean MakeParent(char *file, long owner);
static void Copy(char *file1, char *file2, Boolean recursive, int level,
                 Boolean strip);

#define MAXACL 400

static Boolean debug         = false;
static Boolean verbose       = false;
static Boolean TraceOnly     = false;
static Boolean renameTargets = false;
static Boolean oneLevel      = false;
static Boolean strip         = false;
static Boolean preserveDate  = true;

static int pageSize;
static Boolean oldAcl = false;
static char file1[MAXPATHLEN];
static char file2[MAXPATHLEN];

static struct OldAcl {
    int nplus;
    int nminus;
    int offset;
    char data[1];
};

/* ************************************************************ */
/* 								 */
/* main program							 */
/* 								 */
/* ************************************************************ */

void main(int argc, char **argv)
{
    pageSize = getpagesize();
    ScanArgs(argc, argv);

    /* now read each line of the CopyList */
    Copy(file1, file2, !oneLevel, 0, strip);
    exit(EXIT_SUCCESS);
}

/* ************************************************************ */
/* 								 */
/* 								 */
/* ************************************************************ */

static void ScanArgs(int argc, char **argv)
{
    /* skip program name */
    argc--, argv++;

    /* check for -flag options */
    while (argc > 0 && *argv[0] == '-') {
        char *cp = *argv;
        switch (*++cp) {
        case 'd':
            debug = true;
            break;

        case 'v':
            verbose = true;
            break;

        case 's':
            strip = true;
            break;

        case '1':
            oneLevel = true;
            break;

        case 'n':
            verbose   = true;
            TraceOnly = true;
            break;

        case 'o':
            oldAcl = true;
            break;

        case 'r':
            renameTargets = true;
            break;

        case 'x':
            preserveDate = false;
            break;

        default:
            fprintf(stderr, "Unknown option %c\n", *cp);
            fprintf(stderr, "usage: updfiles [foo.upd]\n");
            exit(EXIT_FAILURE);
        }
        argc--, argv++;
    }

    if (argc != 2) {
        fprintf(stderr, "usage: mvs <flags> from to\n");
        exit(EXIT_FAILURE);
    }

    strcpy(file1, argv[0]);
    strcpy(file2, argv[1]);
}

/* ************************************************************ */
/* 								 */
/* MakeParent							 */
/* 	make sure the parent directory of this file exists	 */
/* 	true if exists, false if does not exist			 */
/* 	Owner argument is a hack:all directories made will have this owner. */
/* 
************************************************************ */

static Boolean MakeParent(char *file, long owner)
{
    char parent[MAXPATHLEN];
    char *p;
    struct stat s;

    if (debug)
        fprintf(stderr, "MakeParent of %s\n", file);
    strcpy(parent, file);

    if ((p = strrchr(parent, '/')) == NULL)
        strcpy(parent, ".");
    else if (p > parent)
        *p = '\0';
    else
        p[1] = '\0';

    if (stat(parent, &s) < 0) {
        if (!MakeParent(parent, owner))
            return (false);
        if (verbose) {
            fprintf(stdout, "Creating directory %s.\n", parent);
            fflush(stdout);
        }
        mkdir(parent, 0777);
        chown(parent, owner, -1);
    }
    return (true);
}

/* ************************************************************ */
/* 								 */
/* Copy -- does the work of the program				 */
/* 	Handle one file, possibly copying subfiles 		 */
/* 	if this is a directory					 */
/* 								 */
/* ************************************************************ */

static void Copy(char *file1, char *file2, Boolean recursive, int level,
                 Boolean strip)
/*
char	*file1	    input file name
char	*file2	    output file name
Boolean	recursive   true if directory should be copied
int	level	    level of recursion: 0, 1, ...
Boolean	strip	    true if objects should be stripped
*/
{
    struct stat s1, s2;

    struct ViceIoctl blob;
    char aclspace[MAXACL];

    if (debug)
        fprintf(stderr, "Copy %c%c %s to %s at level %d\n", (strip ? 's' : ' '),
                (recursive ? 'r' : ' '), file1, file2, level);
    if (lstat(file1, &s1) < 0) {
        fprintf(stdout, "Can't find %s\n", file1);
        fflush(stdout);
        return;
    }

    if (lstat(file2, &s2) < 0) {
        if (!MakeParent(file2, s1.st_uid))
            return;
        s2.st_mtime = 0;
    } else {
        if ((s2.st_mode & 0200) == 0) {
            fprintf(
                stdout,
                "File %s is write protected against its owner; not overwritten\n",
                file2);
            fflush(stdout);
            return;
        }
        if (s2.st_uid != s1.st_uid)
            chown(file2, s1.st_uid, -1);
    }
    if ((s1.st_mode & S_IFMT) == S_IFREG) {
        /* **************************************************** */
        /* 							 */
        /* 	Copy Regular File				 */
        /* 							 */
        /* **************************************************** */

        int f1, f2, n;
        long bytesLeft;
        struct exec *head;
        char buf[4096] /*   must be bigger than sizeof (*head) */;
        struct timeval tv[2];

        if (debug)
            fprintf(stderr, "file in = %d, %d; file out = %d, %d\n", s1.st_size,
                    s1.st_mtime, s2.st_size, s2.st_mtime);
        if (s1.st_mtime == s2.st_mtime && s1.st_size == s2.st_size)
            return;

        /* open file f1 for input */
        f1 = open(file1, O_RDONLY, 0);
        if (f1 < 0) {
            fprintf(stdout, "Unable to open input file %s, ", file1);
            if (errno >= sys_nerr)
                fprintf(stdout, "error code = %d\n", errno);
            else
                fprintf(stdout, "%s\n", sys_errlist[errno]);
            fflush(stdout);
            return;
        }

        /* We do the header examination with a pointer instead of
            just reading into a struct exec to keep the later reads
            aligned on block boundaries. */
        n    = read(f1, buf, sizeof(buf));
        head = (struct exec *)buf;
        if (strip && n >= sizeof(*head) && (s1.st_mode & 0111) &&
            !N_BADMAG(*head)) {
            /* This code lifted from strip.c. */
            bytesLeft    = (long)head->a_text + head->a_data;
            head->a_syms = head->a_trsize = head->a_drsize = 0;
            if (head->a_magic == ZMAGIC)
                bytesLeft += pageSize - sizeof(*head);
            /* also include size of header */
            bytesLeft += sizeof(*head);
        } else
            bytesLeft = 0x7fffffff;

        /* check if size of stripped file is same as existing file */
        if (s1.st_mtime == s2.st_mtime && bytesLeft == s2.st_size) {
            close(f1);
            return;
        }

        if (verbose)
            if (bytesLeft == 0x7fffffff)
                fprintf(stdout, "  %s(%d) -> %s(%d)\n", file1, s1.st_size,
                        file2, s1.st_size);
            else
                fprintf(stdout, "  %s(%d) -%s> %s(%d)\n", file1, s1.st_size,
                        strip ? "-s" : "", file2, bytesLeft);
        fflush(stdout);

        if (TraceOnly) {
            if (verbose)
                fprintf(stdout, "  %d (%d) != %d (%d)\n", s1.st_mtime,
                        s1.st_size, s2.st_mtime, s2.st_size);
            close(f1);
            return;
        }

        /*  Rename old file if asked to. */
        if (renameTargets && s2.st_mtime != 0) {
            char newName[MAXPATHLEN];
            strcpy(newName, file2);
            strcat(newName, ".old");
            if (verbose) {
                printf("  Renaming %s to %s.\n", file2, newName);
                fflush(stdout);
            }
            if (rename(file2, newName) < 0) {
                fprintf(stdout, "  Rename of %s to %s failed.\n", file2,
                        newName);
                fflush(stdout);
            }
        }
        /*  open output file */
        f2 = open(file2, O_WRONLY | O_CREAT | O_TRUNC, s1.st_mode | 0200);
        if (f2 < 0) {
            fprintf(stdout, "Unable to open output file %s, ", file2);
            if (errno >= sys_nerr)
                fprintf(stdout, "error code = %d\n", errno);
            else
                fprintf(stdout, "%s\n", sys_errlist[errno]);
            fflush(stdout);
            close(f1);
            return;
        }

        write(f2, buf, n);
        /* Account for bytes past the header that we've read */
        bytesLeft -= n;
        while (bytesLeft > 0) {
            n = bytesLeft > sizeof(buf) ? sizeof(buf) : bytesLeft;
            n = read(f1, buf, n);
            if (n <= 0)
                break;
            write(f2, buf, n);
            bytesLeft -= n;
        }
        if (s1.st_mode & S_ISUID)
            fchmod(f2, s1.st_mode | 0200);
        /* chmod (file2, s1.st_mode); */
        if (preserveDate) {
            tv[0].tv_sec  = s1.st_atime;
            tv[0].tv_usec = 0;
            tv[1].tv_sec  = s1.st_mtime;
            tv[1].tv_usec = 0;
            utimes(file2, tv);
        }
        if (fstat(f2, &s2) < 0)
            printf("WARNING: Unable to stat new file %s\n", file2);
        else if (s1.st_size != s2.st_size)
            printf("WARNING: New file %s is %u bytes long; should be  %u\n",
                   file2, s2.st_size, s1.st_size);
        close(f1);
        close(f2);
        /* Bug in vice2:  have to chown the file here, because the file server currently (1/4/86) rejects the chown after a a create and before the first store. */
        if (s2.st_uid != s1.st_uid)
            chown(file2, s1.st_uid, -1);
    } else if ((s1.st_mode & S_IFMT) == S_IFDIR && (recursive || level == 0)) {
        /* **************************************************** */
        /* 							 */
        /* 	Copy Directory					 */
        /* 							 */
        /* **************************************************** */

        DIR *dir;
        int tfd, code, i;
        struct OldAcl *oacl;
        char tacl[MAXACL];
        struct direct *d;
        char f1[MAXPATHLEN], f2[MAXPATHLEN];
        char *p1, *p2;

        if (verbose) {
            fprintf(stdout, "d:%s -> %s\n", file1, file2);
            fflush(stdout);
        }

        strcpy(f1, file1);
        strcpy(f2, file2);
        p1 = f1 + strlen(f1);
        p2 = f2 + strlen(f2);
        if (p1 == f1 || p1[-1] != '/')
            *p1++ = '/';
        if (p2 == f2 || p2[-1] != '/')
            *p2++ = '/';
        if ((dir = opendir(file1)) == NULL) {
            fprintf(stdout, "Couldn't open %s\n", file1);
            fflush(stdout);
            return;
        }
        while ((d = readdir(dir)) != NULL) {
            if (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)
                continue;
            strcpy(p1, d->d_name);
            strcpy(p2, d->d_name);
            Copy(f1, f2, recursive, level + 1, strip);
        }
        closedir(dir);
        mkdir(file2, 0777); /* Handle case where MakeParent not invoked. */
        chown(file2, s1.st_uid, -1);
        /* If we're supposed to copy access control lists, too. */
        if (1) {
            if (oldAcl) { /* Get an old style acl and convert to new style */
                blob.in       = aclspace;
                blob.out      = aclspace;
                blob.out_size = MAXACL;
                for (i = 1; i < strlen(file1); i++)
                    if (file1[i] == '/')
                        break;
                strcpy(aclspace, &file1[i]);
                blob.in_size = 1 + strlen(aclspace);
                tfd          = open(file1, O_RDONLY, 0);
                if (tfd < 0) {
                    perror("old-acl open");
                    return;
                }
                code = ioctl(tfd, _VICEIOCTL(4), (char *)&blob);
                close(tfd);
                if (code < 0)
                    return;
                /* Now convert the thing. */
                oacl = (struct OldAcl *)(aclspace + 4);
                sprintf(tacl, "%d\n%d\n", oacl->nplus, oacl->nminus);
                strcat(tacl, oacl->data);
                strcpy(aclspace, tacl);
            } else { /* Get a new style acl */
                blob.in       = aclspace;
                blob.out      = aclspace;
                blob.in_size  = 0;
                blob.out_size = MAXACL;
                code          = pioctl(file1, _VICEIOCTL(2), &blob, 0);
                if (code < 0) {
                    perror("getacl");
                    return;
                }
            }
            /* Now set the new style acl. */
            blob.out      = aclspace;
            blob.in       = aclspace;
            blob.out_size = 0;
            blob.in_size  = 1 + strlen(aclspace);
            code          = pioctl(file2, _VICEIOCTL(1), &blob, 0);
            if (code < 0) {
                perror("setacl");
                return;
            }
        }

    }

    else if ((s1.st_mode & S_IFMT) == S_IFLNK) {
        /* Copy symbolic link */
        char linkvalue[MAXPATHLEN + 1];
        int n;
        if ((n = readlink(file1, linkvalue, sizeof(linkvalue))) == -1) {
            printf("Could not read symbolic link %s\n", file1);
            perror("read link");
            return;
        }
        unlink(file2); /* Always make the new link (it was easier) */
        linkvalue[n] = 0;
        if (verbose)
            printf("Symbolic link %s (to %s) -> %s\n", file1, linkvalue, file2);
        if (symlink(linkvalue, file2) == -1) {
            printf("Could not create symbolic link %s\n", file2);
            perror("create link");
            return;
        }
    }
}
