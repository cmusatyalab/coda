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

