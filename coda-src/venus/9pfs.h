/* BLURB gpl

                           Coda File System
                              Release 6

             Copyright (c) 2018 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/*
 * This header defined plan 9 file system specific datatypes and constants.
 * It will probably not be of much use to any code outside of 9pfs.cc
 */

#ifndef _VENUS_9PFS_H_
#define _VENUS_9PFS_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <stdint.h>


/* Magic to detect an incoming Tversion request message */
#define P9_MAGIC_LEN 19
// @ offset 1, assume message len < 256, opcode Tversion, tag NOTAG
const unsigned char plan9_magic1[] = "\0\0\0d"; // \377\377";
// @ offset 12, assume version len < 256, version string "9P2000"
const unsigned char plan9_magic12[] = "\09P2000";


/* Every message starts with a four byte message size which includes the size
 * field itself, a one byte message type and a two byte identifying tag. The
 * following tries to document the various messages in the 9P protocol, as
 * seen in http://man.cat-v.org/plan_9/5/intro */
enum plan9_message_types {
    /* Negotiate protocol version */
    Tversion = 100, /* tag[2] msize[4] s[2] version[s] */
    Rversion,       /* tag[2] msize[4] s[2] version[s] */
    /* Authenticate */
    Tauth =    102, /* legacy:   tag[2] afid[4] s[2] uname[s] s[2] aname[s] */
                    /* 9p2000.u: tag[2] afid[4] s[2] uname[s] s[2] aname[s]
                                                                  n_uname[4] */
    Rauth,          /* tag[2] aqid[13] */
    /* Establish a connection */
    Tattach =  104, /* legacy:   tag[2] fid[4] afid[4] s[2] uname[s]
                                                     s[2] aname[s] */
                    /* 9p2000.u: tag[2] fid[4] afid[4] s[2] uname[s]
                                                     s[2] aname[s] n_uname[4] */
    Rattach,        /* tag[2] qid[13] */
    /* Return an error */
    Terror,         /* illegal */
    Rerror =   107, /* legacy:    tag[2] s[2] ename[s] */
                    /* 9P2000.u:  tag[2] s[2] ename[s] errno[4] */
    /* Abort a message */
    Tflush =   108, /* tag[2] oldtag[2] */
    Rflush,         /* tag[2] */
    /* Traverse a directory hierarchy */
    Twalk =    110, /* tag[2] fid[4] newfid[4] nwname[2] nwname*(s[2] wname[s]) */
    Rwalk,          /* tag[2] nwqid[2] nwqid*(wqid[13]) */
    /* Open a handle for an existing file */
    Topen =    112, /* tag[2] fid[4] mode[1] */
    Ropen,          /* tag[2] qid[13] iounit[4] */
    /* Prepare a handle for a new file */
    Tcreate =  114, /* legacy:   tag[2] fid[4] name[s] perm[4] mode[1] */
                    /* 9P2000.u: tag[2] fid[4] name[s] perm[4] mode[1] extension[s] */
    Rcreate,        /* tag[2] qid[13] iounit[4] */
    /* Read from file handle */
    Tread =    116, /* tag[2] fid[4] offset[8] count[4] */
    Rread,          /* tag[2] count[4] data[count] */
    /* Write to file handle */
    Twrite =   118, /* tag[2] fid[4] offset[8] count[4] data[count] */
    Rwrite,         /* tag[2] count[4] */
    /* Forget about file handle */
    Tclunk =   120, /* tag[2] fid[4] */
    Rclunk,         /* tag[2] */
    /* Remove file from server */
    Tremove =  122, /* tag[2] fid[4] */
    Rremove,        /* tag[2] */
    /* Get file attributes */
    Tstat =    124, /* tag[2] fid[4] */
    Rstat,          /* tag[2] n[2] stat[n] */
    /* Update file attributes */
    Twstat =   126, /* tag[2] fid[4] n[2] stat[n] */
    Rwstat,         /* tag[2] */
};


enum plan9_dotl_message_types {
    Rlerror = 7,       /* errno[4] */
    Tstatfs = 8,       /* fid[4]   */
    Rstatfs,           /* struct p9_statfs[] */
    Tlopen = 12,       /* fid[4] flags[4]    */
    Rlopen,            /* qid[13] iounit[4]  */
    Tlcreate = 14,     /* fid[4] name[s] flags[4] mode[4] gid[4] */
    Rlcreate,          /* qid[13] iounit[4] */
    Tsymlink = 16,     /* fid[4] name[s] symtgt[s] gid[4] */
    Rsymlink,          /* qid[13] */
    Tmknod = 18,       /* not supported */
    Rmknod,
    Trename = 20,      /* fid[4] dfid[4] name[s] */
    Rrename,
    Treadlink = 22,    /* fid[4] */
    Rreadlink,         /* target[s] */
    Tgetattr = 24,     /* fid[4] request_mask[8] */
    Rgetattr,          /* valid[8] struct p9_stat_dotl[] */
    Tsetattr = 26,     /* fid[4] valid[4] mode[4] uid[4] gid[4] size[8]
                          atime_sec[8] atime_nsec[8] mtime_sec[8] mtime_nsec[8]
                        */
    Rsetattr,
    Txattrwalk = 30,    /* not supported */
    Rxattrwalk,
    Txattrcreate = 32,  /* not supported */
    Rxattrcreate,
    Treaddir = 40,      /* fid[4] offset[8] count[4] */
    Rreaddir,           /* count[4] data[count]
                          direntry format: qid[13] offset[8] type[1] name[s] */
    Tfsync = 50,        /* fid[4] */
    Rfsync,
    Tlock = 52,         /* not supported */
    Rlock,
    Tgetlock = 54,      /* not supported */
    Rgetlock,
    Tlink = 70,         /* dfid[4] fid[4] name[s] */
    Rlink,
    Tmkdir = 72,        /* dfid[4] name[s] mode[4] gid[4] */
    Rmkdir,             /* qid[13] */
    Trenameat = 74,     /* olddirfid[4] oldname[s] newdirfid[4] newname[s] */
    Rrenameat,
    Tunlinkat = 76,     /* dirfd[4] name[s] flags[4] */
    Runlinkat,
};

/* Plan9 protocol version */
#define	P9_PROTO_UNKNOWN 0x00
#define	P9_PROTO_2000    0x01  /* 9P2000 Legacy protocol */
#define	P9_PROTO_DOTU    0x02  /* 9P2000.u Unix Extensions */
#define	P9_PROTO_DOTL    0x04  /* 9P2000.L Linux Extensions */

#define P9_NOTAG ((uint16_t)~0) /* version message should use 'NOTAG' */
#define P9_NOFID ((uint32_t)~0) /* noauth attach uses 'NOFID' for 'afid' */
#define P9_MAX_NWNAME 16        /* max elements in walk message */

/* size of 9pfs message header */
#define P9_MIN_MSGSIZE (sizeof(uint32_t)+sizeof(uint8_t)+sizeof(uint16_t))

/* 9P permission mode filetype bits */
#define  P9_DMDIR             0x80000000
#define  P9_DMAPPEND          0x40000000  /* unsupported */
#define  P9_DMEXCL            0x20000000  /* unsupported */
#define  P9_DMMOUNT           0x10000000  /* unsupported */
#define  P9_DMAUTH            0x08000000
#define  P9_DMTMP             0x04000000  /* unsupported */
  /* 9P2000.u extensions */
#define  P9_DMSYMLINK         0x02000000
#define  P9_DMLINK            0x01000000
#define  P9_DMDEVICE          0x00800000  /* unsupported */
#define  P9_DMNAMEDPIPE       0x00200000  /* unsupported */
#define  P9_DMSOCKET          0x00100000  /* unsupported */
#define  P9_DMSETUID          0x00080000  /* unsupported */
#define  P9_DMSETGID          0x00040000  /* unsupported */
#define  P9_DMSETVTX          0x00010000  /* unsupported */

/* 9P qid.types = higher byte of permission modes */
#define  P9_QTDIR             0x80
#define  P9_QTAPPEND          0x40  /* unsupported */
#define  P9_QTEXCL            0x20  /* unsupported */
#define  P9_QTMOUNT           0x10  /* unsupported */
#define  P9_QTAUTH            0x08
#define  P9_QTTMP 	          0x04  /* unsupported */
#define  P9_QTSYMLINK         0x02
#define  P9_QTLINK            0x01
#define  P9_QTFILE            0x00


/* Plan9 open/create flags */
#define P9_OREAD   0x00
#define P9_OWRITE  0x01
#define P9_ORDWR   0x02
#define P9_OEXEC   0x03
#define P9_OTRUNC  0x10
#define P9_OREXEC  0x20   /* unsupported */
#define P9_ORCLOSE 0x40   /* unsupported */
#define P9_OAPPEND 0x80
#define P9_OEXCL   0x1000 /* unsupported */

/* 9p2000.L open flags */
#define P9_DOTL_RDONLY        00000000
#define P9_DOTL_WRONLY        00000001
#define P9_DOTL_RDWR          00000002
#define P9_DOTL_NOACCESS      00000003
#define P9_DOTL_CREATE        00000100
#define P9_DOTL_EXCL          00000200
#define P9_DOTL_NOCTTY        00000400
#define P9_DOTL_TRUNC         00001000
#define P9_DOTL_APPEND        00002000
#define P9_DOTL_NONBLOCK      00004000
#define P9_DOTL_DSYNC         00010000
#define P9_DOTL_FASYNC        00020000
#define P9_DOTL_DIRECT        00040000
#define P9_DOTL_LARGEFILE     00100000
#define P9_DOTL_DIRECTORY     00200000
#define P9_DOTL_NOFOLLOW      00400000
#define P9_DOTL_NOATIME       01000000
#define P9_DOTL_CLOEXEC       02000000
#define P9_DOTL_SYNC          04000000


struct plan9_qid {
    uint8_t type;
    uint32_t version;
    uint64_t path;
};

struct plan9_stat {
    //uint16_t size; // we compute this as needed
    uint16_t type;
    uint32_t dev;
    struct plan9_qid qid;
    uint32_t mode;
    uint32_t atime;
    uint32_t mtime;
    uint64_t length;
    char *name;
    char *uid;
    char *gid;
    char *muid;
    // fields for 9p2000.u extensions:
    char *extension;	/* data about special files (links, devices, pipes,...) */
  	uid_t n_uid;		  /* numeric IDs */
  	gid_t n_gid;
  	uid_t n_muid;
};

/* Plan9 stat "don't touch" values for writing stat */
#define P9_DONT_TOUCH_TYPE      ((uint16_t)(-1))
#define P9_DONT_TOUCH_DEV       ((uint32_t)(-1))
#define P9_DONT_TOUCH_QID_TYPE  ((uint8_t)(-1))
#define P9_DONT_TOUCH_QID_VERS  ((uint32_t)(-1))
#define P9_DONT_TOUCH_QID_PATH  ((uint64_t)(-1))
#define P9_DONT_TOUCH_MODE      ((uint32_t)(-1))
#define P9_DONT_TOUCH_ATIME     ((uint32_t)(-1))
#define P9_DONT_TOUCH_MTIME     ((uint32_t)(-1))
#define P9_DONT_TOUCH_LENGTH    ((uint64_t)(-1))
#define P9_DONT_TOUCH_NAME      ""
#define P9_DONT_TOUCH_UID       ""
#define P9_DONT_TOUCH_GID       ""
#define P9_DONT_TOUCH_MUID      ""
// fields for 9p2000.u extensions:
#define P9_DONT_TOUCH_EXTENSION ""
#define P9_DONT_TOUCH_NUID      ((uint32_t)(-1))
#define P9_DONT_TOUCH_NGID      ((uint32_t)(-1))
#define P9_DONT_TOUCH_NMUID     ((uint32_t)(-1))


struct plan9_stat_dotl {
	struct plan9_qid qid;
	uint32_t st_mode;
	uid_t st_uid;
	gid_t st_gid;
    uint64_t st_nlink;
	uint64_t st_rdev;
	uint64_t st_size;
  uint64_t st_blksize;
  uint64_t st_blocks;
	uint64_t st_atime_sec;
	uint64_t st_atime_nsec;
	uint64_t st_mtime_sec;
	uint64_t st_mtime_nsec;
	uint64_t st_ctime_sec;
	uint64_t st_ctime_nsec;
  /* reserved for future use */
	uint64_t st_btime_sec;
	uint64_t st_btime_nsec;
	uint64_t st_gen;
	uint64_t st_data_version;
};


/* bits for request_mask[8] and valid[8] in dotl Tgetattr and Rgetattr msgs */
#define P9_GETATTR_MODE         0x00000001ULL
#define P9_GETATTR_NLINK        0x00000002ULL
#define P9_GETATTR_UID          0x00000004ULL
#define P9_GETATTR_GID          0x00000008ULL
#define P9_GETATTR_RDEV         0x00000010ULL
#define P9_GETATTR_ATIME        0x00000020ULL
#define P9_GETATTR_MTIME        0x00000040ULL
#define P9_GETATTR_CTIME        0x00000080ULL
#define P9_GETATTR_INO          0x00000100ULL
#define P9_GETATTR_SIZE         0x00000200ULL
#define P9_GETATTR_BLOCKS       0x00000400ULL
//reserved for future use
#define P9_GETATTR_BTIME        0x00000800ULL
#define P9_GETATTR_GEN          0x00001000ULL
#define P9_GETATTR_DATA_VERSION 0x00002000ULL
//masks
#define P9_GETATTR_BASIC        0x000007ffULL /* Mask for fields up to BLOCKS */
#define P9_GETATTR_ALL          0x00003fffULL /* Mask for All fields above */


/* bits for valid[4] in dotl Tsetattr msgs */
#define P9_SETATTR_MODE         0x00000001UL
#define P9_SETATTR_UID          0x00000002UL
#define P9_SETATTR_GID          0x00000004UL
#define P9_SETATTR_SIZE         0x00000008UL
#define P9_SETATTR_ATIME        0x00000010UL
#define P9_SETATTR_MTIME        0x00000020UL
#define P9_SETATTR_CTIME        0x00000040UL
#define P9_SETATTR_ATIME_SET    0x00000080UL
#define P9_SETATTR_MTIME_SET    0x00000100UL


struct plan9_statfs {
	uint32_t type;
	uint32_t bsize;
	uint64_t blocks;
	uint64_t bfree;
	uint64_t bavail;
	uint64_t files;
	uint64_t ffree;
	uint64_t fsid;
	uint32_t namelen;
};

#define V9FS_MAGIC		0x01021997       //for type in struct plan9_statfs


#ifdef __cplusplus
}
#endif

#include <dlist.h>
#include <mariner.h>

#define P9_BUFSIZE 8192

class plan9server {
    mariner *conn;
    dlist fids;

    unsigned char buffer[P9_BUFSIZE];
    size_t max_msize;                   /* negotiated by Tversion/Rversion */
    int protocol;                       /* negotiated by Tversion/Rversion */

    int pack_header(unsigned char **buf, size_t *bufspace,
                    uint8_t type, uint16_t tag);
    int send_response(unsigned char *buf, size_t len);
    int send_error(uint16_t tag, const char *error, int errcode);

    int handle_request(unsigned char *buf, size_t len);
    int recv_version(unsigned char *buf, size_t len, uint16_t tag);
    int recv_auth(unsigned char *buf, size_t len, uint16_t tag);
    int recv_attach(unsigned char *buf, size_t len, uint16_t tag);
    int recv_flush(unsigned char *buf, size_t len, uint16_t tag);
    int recv_walk(unsigned char *buf, size_t len, uint16_t tag);
    int recv_open(unsigned char *buf, size_t len, uint16_t tag);
    int recv_create(unsigned char *buf, size_t len, uint16_t tag);
    int recv_read(unsigned char *buf, size_t len, uint16_t tag);
    int recv_write(unsigned char *buf, size_t len, uint16_t tag);
    int recv_clunk(unsigned char *buf, size_t len, uint16_t tag);
    int recv_remove(unsigned char *buf, size_t len, uint16_t tag);
    int recv_stat(unsigned char *buf, size_t len, uint16_t tag);
    int recv_wstat(unsigned char *buf, size_t len, uint16_t tag);

    int recv_getattr(unsigned char *buf, size_t len, uint16_t tag);
    int recv_setattr(unsigned char *buf, size_t len, uint16_t tag);
    int recv_lopen(unsigned char *buf, size_t len, uint16_t tag);
    int recv_readdir(unsigned char *buf, size_t len, uint16_t tag);
    int recv_readlink(unsigned char *buf, size_t len, uint16_t tag);
    int recv_statfs(unsigned char *buf, size_t len, uint16_t tag);
    int recv_fsync(unsigned char *buf, size_t len, uint16_t tag);
    int recv_unlinkat(unsigned char *buf, size_t len, uint16_t tag);
    int recv_link(unsigned char *buf, size_t len, uint16_t tag);

    struct fidmap *find_fid(uint32_t fid);
    struct fidmap *add_fid(uint32_t fid, struct venus_cnode *cnode,
                           struct attachment *root);
    int del_fid(uint32_t fid);

    int plan9_stat(struct venus_cnode *cnode, struct attachment *root,
                   struct plan9_stat *stat, const char *name = NULL);
    ssize_t plan9_read(struct fidmap *fm, unsigned char *buf,
                       size_t count, size_t offset);

    int cnode_getname(struct venus_cnode *cnode, char *name);
    int cnode_getparent(struct venus_cnode *cnode, struct venus_cnode *parent);

public:
    plan9server(mariner *conn);
    ~plan9server();

    void main_loop(unsigned char *initial_buffer = NULL, size_t len = 0);
    int pack_dirent(unsigned char **buf, size_t *len, size_t *offset,
                    size_t *packed_offset, struct venus_cnode *parent,
                    struct attachment *root, const char *name);
    int fidmap_replace_cfid(VenusFid * OldFid, VenusFid * NewFid);
};

#endif /* _VENUS_9PFS_H_ */
