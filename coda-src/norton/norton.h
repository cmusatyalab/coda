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



#include <cvnode.h>
#include <volume.h>
#include <index.h>
#include <recov.h>
#include <camprivate.h>

/* norton-setup.c */
extern unsigned int norton_debug;
extern int mapprivate;
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
extern VolHead *GetVol(VolumeId);
extern VolHead *GetVol(char *);
extern int GetVolIndex(VolumeId);
extern void list_vols(int, char **);
extern void list_vols();
extern void show_volume(int, char **);
extern void show_volume(VolumeId);
extern void show_volume(char *);
extern void show_volume_details(int, char **);
extern void show_volume_details(VolumeId);
extern void show_volume_details(char *);
extern void show_index(int, char **);
extern void show_index(VolumeId);
extern void show_index(char *);
extern void sh_delete_volume(int, char **);
extern void undelete_volume(int, char **);
extern void sh_rename_volume(int, char **);

/* norton-vnode.c */
extern void show_vnode(int, char **);
extern void show_vnode(VolumeId, VnodeId, Unique_t);
extern void show_vnode(VolumeId, Unique_t);
extern void show_free(int, char **);
extern void set_linkcount(int, char **);
extern void PrintVnodeDiskObject(VnodeDiskObject *);

/* norton-recov.c */
extern int GetMaxVolId();
extern VolumeHeader *VolHeaderByIndex(int);
extern VolHead *VolByIndex(int);

/* norton-dir.c */
extern void show_dir(int, char **);
extern void show_dir(VolumeId, VnodeId, Unique_t);
extern void delete_name(int, char **);
extern void sh_create_name(int, char **);


/* norton-rds.c */
extern void show_heap(int, char **);
