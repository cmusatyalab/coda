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







#ifndef _SIM_OPS_H_
#define _SIM_OPS_H_ 1
#include <voltypes.h>
typedef struct
{
	VolumeId	Volume;
	VnodeId		Vnode;
	Unique		Unique;
}	ViceFid;

typedef struct
{
	unsigned long	Host;
	unsigned long	Uniquifier;
}	ViceStoreId;

typedef struct
{
    int	Site0;
    int	Site1;
    int	Site2;
    int	Site3;
    int	Site4;
    int	Site5;
    int	Site6;
    int	Site7;
} ViceVersionArray;

typedef struct
{
    ViceVersionArray	Versions;
    ViceStoreId	StoreId;
    long Flags;
} ViceVersionVector;

struct common_log {
    unsigned long	serverid;
    ViceStoreId 	storeid;
    unsigned int  	opcode;
    VnodeId		dvnode;
    Unique		dunique;
    long 		nextptr;
    long 		prevptr;
};

struct rm_log {
    struct common_log cl;
    char 		*name;
    VnodeId 		cvnode;
    Unique 		cunique;
    ViceVersionVector 	cvv;
};

struct rmdir_log {
    struct common_log	cl;
    char		*name;
    VnodeId 		cvnode;
    Unique 		cunique;
    int 		head;	/* head of log for child being deleted */
    int			count;	/* number of log entries for child */
    ViceStoreId 	childLCP; /* childs lcp in log with other reps */
    ViceStoreId		csid;	/* storeid of directory when deleted */
};

struct create_log {
    struct common_log	cl;
    char		*name;
    VnodeId 		cvnode;
    Unique		cunique;
};

struct mkdir_log {
    struct common_log	cl;
    char		*name;
    VnodeId 		cvnode;
    Unique		cunique;
};

struct store_log {
    struct common_log	cl;
};

struct rename_log {
    struct common_log	cl;
    unsigned long srctgt;
    struct {
	char 	*oldname;
	VnodeId	cvnode;
	Unique	cunique;
	ViceVersionVector cvv;
    } rename_src;
    VnodeId OtherDirV;
    Unique OtherDirU;
    struct {
	char	*newname;
	int 	tgtexisted;
	VnodeId	TgtVnode;	/* vnode of target obj if it existed */
	Unique	TgtUnique;	/* uniquifier of target obj */
	union {
	    ViceVersionVector TgtGhostVV;
	    struct {
		int	head;
		int 	count;
	    } TgtGhostLog;
	} TgtGhost;
    } rename_tgt;
};

struct symlink_log {
    struct common_log	cl;
    char		*name;
    VnodeId 		cvnode;
    Unique		cunique;
};

struct hardlink_log {
    struct common_log	cl;
    char		*name;
    VnodeId 		cvnode;
    Unique		cunique;
};

#endif not _SIM_OPS_H_
