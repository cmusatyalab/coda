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

/************************************* fid table entry */

#ifndef MAXNAMLEN
#define MAXNAMLEN  255
#endif  /* MAXNAMLEN */

#ifndef V_BLKSIZE  
#define V_BLKSIZE  8192
#endif  /* V_BLKSIZE */

typedef struct fid_ent_s {
    ViceFid           fid;
    enum coda_vtype        type;
    ds_list_t        *kids;
    struct fid_ent_s *parent;
    char              name[MAXNAMLEN+1];
} fid_ent_t;

#if defined(__BSD44__) && defined(__i386__)
#define SYS_STRING  "i386_nbsd1"
#endif

#ifdef __linux__
#define SYS_STRING "linux"
#endif

#ifdef DJGPP
#define SYS_STRING "dos"
#endif

#ifdef sun
#define SYS_STRING "solaris"
#endif

#ifdef LINUX
#define ATTR_MODE	1
#define ATTR_UID	2
#define ATTR_GID	4
#define ATTR_SIZE	8
#define ATTR_ATIME	16
#define ATTR_MTIME	32
#define ATTR_CTIME	64
#define ATTR_ATIME_SET	128
#define ATTR_MTIME_SET	256
#define ATTR_FORCE	512	/* Not a change, but a change it */
#define ATTR_ATTR_FLAG	1024
#define MS_MGC_VAL 0xC0ED0000	/* magic flag number to indicate "new" flags */
#define umode_t int
struct iattr {
	unsigned int	ia_valid;
	umode_t		ia_mode;
	uid_t		ia_uid;
	gid_t		ia_gid;
	off_t		ia_size;
	time_t		ia_atime;
	time_t		ia_mtime;
	time_t		ia_ctime;
	unsigned int	ia_attr_flags;
};

static void coda_iattr_to_vattr(struct iattr *, struct coda_vattr *);


#define sigcontext sigaction
#define MOUNT_CFS 0
#define d_namlen d_reclen
#define SYS_STRING "linux"
#define ts_sec tv_sec
#define ts_nsec tv_nsec
#else
#define MOUNT_CFS 1
#endif




