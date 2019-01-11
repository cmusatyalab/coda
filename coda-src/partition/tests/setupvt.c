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

#include <util.h>
#include <../partition.h>
#include <../vicetab.h>
extern void printnames(struct DiskPartition *dp, int low, int step, int high);

int main(int argc, char **argv)
{
    struct DiskPartition *dp;
    Inode testcreate;
    Device devno;
    int fd, count;
    char *buff = "This is a test string";
    Partent pe, pf;
    FILE *vtab;
    int rc;
    char host[256];

    /* write a vicetab file */
    hostname(host);

    /* set up a simple partition & ftree partition */
    /* they must be on different diskpartions */
    pe = Partent_create(host, "simpled", "simple", "");
    pf = Partent_create(host, "/tmp/f", "ftree", "width=8,depth=5");
    unlink("vicetab");
    rc = creat("vicetab", 00600);
    CODA_ASSERT(rc != -1);
    vtab = Partent_set("vicetab", "r+");
    rc   = Partent_add(vtab, pe);
    CODA_ASSERT(rc == 0);
    rc = Partent_add(vtab, pf);
    CODA_ASSERT(rc == 0);
    Partent_free(&pe);
    Partent_free(&pf);
    Partent_end(vtab);
    printf("Make sure to run makeftree vicetab /tmp/f before continuing!\n");
    return 0;
}
