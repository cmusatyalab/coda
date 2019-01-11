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

/******************************************/
/* Print out vldb, copied from vol/vldb.c */
/******************************************/

/* NOTE: since the vldb is a hash table that contains two entries for each
 * volume (namely hashed by name and hashed by volid in ascii), each volume
 * is printed out twice!
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>

#include <lwp.h>
#include <lock.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <vice.h>
#include <vnode.h>
#include <volume.h>
#include <vldb.h>

#define LEFT(i) 2 * (i);
#define RIGHT(i) 2 * (i) + 1;
#define VID(lp) ntohl((lp)->volumeId[(lp)->volumeType])
#define UNIQUE(vid) ((vid)&0xffffff) /* strip hostid bits */

void heapify(struct vldb a[], int i, int size)
{
    int l, r, largest;
    struct vldb tmp;

    l = LEFT(i);
    r = RIGHT(i);

    largest = ((l <= size) && VID(&a[l]) > VID(&a[i])) ? l : i;

    if ((r <= size) && VID(&a[r]) > VID(&a[largest]))
        largest = r;

    if (largest != i) {
        memcpy(&tmp, &a[i], sizeof(struct vldb));
        memcpy(&a[i], &a[largest], sizeof(struct vldb));
        memcpy(&a[largest], &tmp, sizeof(struct vldb));
        heapify(a, largest, size);
    }
}

void heapsort(struct vldb a[], int length)
{
    int i, size = length;
    struct vldb tmp;

    for (i = length / 2; i >= 1; i--)
        heapify(a, i, size);

    for (i = length; i >= 2; i--) {
        memcpy(&tmp, &a[i], sizeof(struct vldb));
        memcpy(&a[i], &a[1], sizeof(struct vldb));
        memcpy(&a[1], &tmp, sizeof(struct vldb));
        heapify(a, 1, --size);
    }
}

void main(int argc, char **argv)
{
    if (argc > 1)
        printf("Usage: %s\n", argv[0]); /* code to supress warnings */

    struct vldb buffer[8];

    int VLDB_fd = open(VLDB_PATH, O_RDONLY, 0);
    if (VLDB_fd == -1)
        exit(EXIT_FAILURE);

    int size          = 8; /* Current size of VLDB array */
    int nentries      = 0; /* Number of valid records in VLDB */
    struct vldb *VLDB = (struct vldb *)malloc(size * sizeof(struct vldb));
    int i;

    for (;;) {
        int nRecords = 0;
        int n        = read(VLDB_fd, (char *)buffer, sizeof(buffer));
        if (n < 0) {
            printf("VLDBPrint: read failed for VLDB\n");
            exit(EXIT_FAILURE);
        }
        if (n == 0)
            break;

        nRecords = (n >> LOG_VLDBSIZE);

        for (i = 0; i < nRecords; i++) {
            struct vldb *vldp = &buffer[i];

            /* There are two entries in the VLDB for each volume, one is keyed on the
   volume name, and the other is keyed on the volume id in alphanumeric form.
   I feel we should only print out the entry with the volume name. */

            if ((VID(vldp) != 0) && (VID(vldp) != atoi(vldp->key))) {
                memcpy(&VLDB[nentries++], vldp, sizeof(struct vldb));
                if (nentries == size) {
                    struct vldb *tmp;

                    size *= 2;
                    tmp = (struct vldb *)malloc(size * sizeof(struct vldb));
                    memcpy(tmp, VLDB, nentries * sizeof(struct vldb));
                    free(VLDB);
                    VLDB = tmp;
                }
            }
        }
    }

    heapsort(VLDB, nentries);
    for (i = 0; i < nentries; i++) {
        struct vldb *vldp = &VLDB[i];
        printf("VID =%x, key = %s, type = %x, servers = (%x", VID(vldp),
               vldp->key, vldp->volumeType, vldp->serverNumber[0]);

        for (byte j = 1; j < vldp->nServers; j++)
            printf(",%x", vldp->serverNumber[j]);
        printf(")\n");
    }
}
