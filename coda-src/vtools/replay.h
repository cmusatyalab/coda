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









/* The following is taken from tar(5). */

/* The operation is effectively encoded in the linkflag field. */
/* We extend the use of this field to accommodate additional operations. */
/* The new encoding is: */
/*     '\0'  :  StoreData, Create */
/*     '1'  :  Link */
/*     '2'  :  Symlink */
/*     '3'  :  StoreStatus */
/*     '4'  :  Remove */
/*     '5'  :  Rename */
/*     '6'  :  Mkdir */
/*     '7'  :  Rmdir */

/* Notes: */
/*     1. Tar understands only codes '0', '1', '2'. */
/*     2. For rename, name :: to, linkname :: from. */
/*     3. Gid is normally -1.  Gid of 0 indicates name overflow. */


#define	TBLOCK	512
#define NBLOCK	20
#define	NAMSIZ	100

union hblock {
    char dummy[TBLOCK];
    struct header {
	char name[NAMSIZ];
	char mode[8];
	char uid[8];
	char gid[8];
	char size[12];
	char mtime[12];
	char chksum[8];
	char linkflag;
	char linkname[NAMSIZ];
    } dbuf;
};


#define	STOREDATA   '\0'
#define	LINK	    '1'
#define	SYMLINK	    '2'
#define	STORESTATUS '3'
#define	REMOVE	    '4'
#define	RENAME	    '5'
#define	MKDIR	    '6'
#define	RMDIR	    '7'

