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

static char *rcsid = "$Header: blurb.doc,v 1.1 96/11/22 13:29:31 raiff Exp $";
#endif /*_BLURB_*/



#include <cvnode.h>
#include <volume.h>
#include <index.h>
#include <recov.h>
#include <camprivate.h>

/* norton-setup.c */
extern int norton_debug;
extern void NortonInit(char *log_dev, char *data_dev, int data_len);

/* commands.c */
extern void InitParsing();
extern void notyet(int, char **);
extern void examine(int argc, char *argv[]);
extern void show_debug(int, char**);
extern void set_debug(int, char **);

/* norton-volume.c */
extern void print_volume(VolHead *);
extern void print_volume_details(VolHead *);
extern void PrintVV(vv_t *);
extern VolHead *GetVol(int);
extern VolHead *GetVol(char *);
extern int GetVolIndex(int);
extern void list_vols(int, char **);
extern void list_vols();
extern void show_volume(int, char **);
extern void show_volume(int);
extern void show_volume(char *);
extern void show_volume_details(int, char **);
extern void show_volume_details(int);
extern void show_volume_details(char *);
extern void show_index(int, char **);
extern void show_index(int);
extern void show_index(char *);
extern void delete_volume(int, char **);
extern void undelete_volume(int, char **);

/* norton-vnode.c */
extern void show_vnode(int, char **);
extern void show_vnode(int, int, int);
extern void show_vnode(int, int);
extern void show_free(int, char **);
extern void PrintVnodeDiskObject(VnodeDiskObject *);

/* norton-recov.c */
extern int GetMaxVolId();
extern VolumeHeader *VolHeaderByIndex(int);
extern VolHead *VolByIndex(int);

/* norton-dir.c */
extern void show_dir(int, char **);
extern void show_dir(int, int, int);
extern void delete_name(int, char **);

/* norton-rds.c */
extern void show_heap(int, char **);
