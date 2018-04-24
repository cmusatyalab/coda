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
    Tauth =    102, /* tag[2] afid[4] s[2] uname[s] s[2] aname[s] */
    Rauth,          /* tag[2] aqid[13] */
    /* Establish a connection */
    Tattach =  104, /* tag[2] fid[4] afid[4] s[2] uname[s] s[2] aname[s] */
    Rattach,        /* tag[2] qid[13] */
    /* Return an error */
    Terror,         /* illegal */
    Rerror =   107, /* tag[2] s[2] ename[s] */
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
    Tcreate =  114, /* tag[2] fid[4] name[s] perm[4] mode[1] */
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

#define P9_NOTAG ((uint16_t)~0) /* version message should use 'NOTAG' */
#define P9_NOFID ((uint32_t)~0) /* noauth attach uses 'NOFID' for 'afid' */
#define P9_MAX_NWNAME 16        /* max elements in walk message */

/* size of 9pfs message header */
#define P9_MIN_MSGSIZE (sizeof(uint32_t)+sizeof(uint8_t)+sizeof(uint16_t))

#define P9_QTDIR     0x80
#define P9_QTAUTH    0x08
#define P9_QTSYMLINK 0x02
#define P9_QTFILE    0x00

#define P9_OREAD   0x00
#define P9_OWRITE  0x01
#define P9_ORDWR   0x02
#define P9_OEXEC   0x03
#define P9_OTRUNC  0x10
#define P9_ORCLOSE 0x40


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
};

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
    size_t max_msize = P9_BUFSIZE; /* negotiated by Tversion/Rversion */

    int pack_header(unsigned char **buf, size_t *bufspace,
                    uint8_t type, uint16_t tag);
    int send_response(unsigned char *buf, size_t len);
    int send_error(uint16_t tag, const char *error);

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

    struct fidmap *find_fid(uint32_t fid);
    struct fidmap *add_fid(uint32_t fid, struct venus_cnode *cnode,
                           struct attachment *root);
    int del_fid(uint32_t fid);

    int plan9_stat(struct venus_cnode *cnode, struct attachment *root,
                   struct plan9_stat *stat, const char *name = NULL);
    ssize_t plan9_read(struct fidmap *fm, unsigned char *buf,
                       size_t count, size_t offset);
public:
    plan9server(mariner *conn);
    ~plan9server();

    void main_loop(unsigned char *initial_buffer = NULL, size_t len = 0);
    int pack_dirent(unsigned char **buf, size_t *len, size_t *offset,
                    struct venus_cnode *parent, struct attachment *root,
                    const char *name);
};

#endif /* _VENUS_9PFS_H_ */
