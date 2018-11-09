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

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <assert.h>
#include <fcntl.h>
#include <pwd.h>
#include <unistd.h>
#include <struct.h>

#ifdef __cplusplus
}
#endif

#include "fso.h"
#include "mariner.h"
#include "venus.private.h"
#include "9pfs.h"
#include "SpookyV2.h"


#define DEBUG(...) do { fprintf(stderr, __VA_ARGS__); fflush(stderr); } while(0)


struct attachment {
    uint32_t refcount;
    struct venus_cnode cnode;
    const char *uname;
    const char *aname;
    uid_t userid;
};

struct fidmap {
    dlink link;
    uint32_t fid;
    struct venus_cnode cnode;
    int open_flags;
    struct attachment *root;
};


static int pack_le8(unsigned char **buf, size_t *len, uint8_t value)
{
    if (*len < 1) return -1;
    (*buf)[0] = value;
    (*buf) += 1; (*len) -= 1;
    return 0;
}

static int unpack_le8(unsigned char **buf, size_t *len, uint8_t *result)
{
    if (*len < 1) return -1;
    *result = (uint8_t)(*buf)[0];
    (*buf) += 1; (*len) -= 1;
    return 0;
}


static int pack_le16(unsigned char **buf, size_t *len, uint16_t value)
{
    if (*len < 2) return -1;
    (*buf)[0] = (uint8_t)(value >> 0);
    (*buf)[1] = (uint8_t)(value >> 8);
    (*buf) += 2; (*len) -= 2;
    return 0;
}

static int unpack_le16(unsigned char **buf, size_t *len, uint16_t *result)
{
    if (*len < 2) return -1;
    *result =
        ((uint16_t)(*buf)[0] << 0) |
        ((uint16_t)(*buf)[1] << 8);
    (*buf) += 2; (*len) -= 2;
    return 0;
}


static int pack_le32(unsigned char **buf, size_t *len, uint32_t value)
{
    if (*len < 4) return -1;
    (*buf)[0] = (uint8_t)(value >> 0);
    (*buf)[1] = (uint8_t)(value >> 8);
    (*buf)[2] = (uint8_t)(value >> 16);
    (*buf)[3] = (uint8_t)(value >> 24);
    (*buf) += 4; (*len) -= 4;
    return 0;
}

static int unpack_le32(unsigned char **buf, size_t *len, uint32_t *result)
{
    if (*len < 4) return -1;
    *result =
        ((uint32_t)(*buf)[0] << 0) |
        ((uint32_t)(*buf)[1] << 8) |
        ((uint32_t)(*buf)[2] << 16) |
        ((uint32_t)(*buf)[3] << 24);
    (*buf) += 4; (*len) -= 4;
    return 0;
}


static int pack_le64(unsigned char **buf, size_t *len, uint64_t value)
{
    if (*len < 8) return -1;
    (*buf)[0] = (uint8_t)(value >> 0);
    (*buf)[1] = (uint8_t)(value >> 8);
    (*buf)[2] = (uint8_t)(value >> 16);
    (*buf)[3] = (uint8_t)(value >> 24);
    (*buf)[4] = (uint8_t)(value >> 32);
    (*buf)[5] = (uint8_t)(value >> 40);
    (*buf)[6] = (uint8_t)(value >> 48);
    (*buf)[7] = (uint8_t)(value >> 56);
    (*buf) += 8; (*len) -= 8;
    return 0;
}

static int unpack_le64(unsigned char **buf, size_t *len, uint64_t *result)
{
    if (*len < 8) return -1;
    *result =
        ((uint64_t)(*buf)[0] << 0) |
        ((uint64_t)(*buf)[1] << 8) |
        ((uint64_t)(*buf)[2] << 16) |
        ((uint64_t)(*buf)[3] << 24) |
        ((uint64_t)(*buf)[4] << 32) |
        ((uint64_t)(*buf)[5] << 40) |
        ((uint64_t)(*buf)[6] << 48) |
        ((uint64_t)(*buf)[7] << 56);
    (*buf) += 8; (*len) -= 8;
    return 0;
}


static int pack_blob(unsigned char **buf, size_t *len,
                     const char *value, size_t size)
{
    if (*len < size) return -1;
    memcpy(*buf, value, size);
    (*buf) += size; (*len) -= size;
    return 0;
}

/* Important! Sort of like an 'unpack_blob', but returns a reference to the
 * blob in the original buffer so a copy can be made. */
static int get_blob_ref(unsigned char **buf, size_t *len,
                        unsigned char **result, size_t *result_len,
                        size_t size)
{
    if (*len < size) return -1;
    *result = *buf;
    if (result_len)
        *result_len = *len;
    (*buf) += size; (*len) -= size;
    return 0;
}


static int pack_string(unsigned char **buf, size_t *len, const char *value)
{
    uint16_t size = strlen(value);
    if (pack_le16(buf, len, size) ||
        pack_blob(buf, len, value, size))
        return -1;
    return 0;
}

/* Important! Allocates memory for the result string on success.
 * Caller is responsible for releasing the memory. */
static int unpack_string(unsigned char **buf, size_t *len, char **result)
{
    uint16_t size;
    unsigned char *blob;

    if (unpack_le16(buf, len, &size) ||
        get_blob_ref(buf, len, &blob, NULL, size))
        return -1;

    *result = ::strndup((char *)blob, size);
    if (*result == NULL)
        return -1;

    /* Check there is no embedded NULL character in the received string */
    if (::strlen(*result) != (size_t)size)
    {
        ::free(*result);
        return -1;
    }
    return 0;
}


static int pack_qid(unsigned char **buf, size_t *len,
                    const struct plan9_qid *qid)
{
    if (pack_le8(buf, len, qid->type) ||
        pack_le32(buf, len, qid->version) ||
        pack_le64(buf, len, qid->path))
        return -1;
    return 0;
}

static int unpack_qid(unsigned char **buf, size_t *len,
                      struct plan9_qid *qid)
{
    if (unpack_le8(buf, len, &qid->type) ||
        unpack_le32(buf, len, &qid->version) ||
        unpack_le64(buf, len, &qid->path))
        return -1;
    return 0;
}


static int pack_stat(unsigned char **buf, size_t *len,
                     const struct plan9_stat *stat, int proto)
{
    unsigned char *stashed_buf = NULL;
    size_t stashed_len = 0;

    /* get backpointer to beginning of the stat output so we can,
     * - fix up the length information after packing everything.
     * - rollback iff we run out of buffer space. */
    if (get_blob_ref(buf, len, &stashed_buf, &stashed_len, 2) ||
        pack_le16(buf, len, stat->type) ||
        pack_le32(buf, len, stat->dev) ||
        pack_qid(buf, len, &stat->qid) ||
        pack_le32(buf, len, stat->mode) ||
        pack_le32(buf, len, stat->atime) ||
        pack_le32(buf, len, stat->mtime) ||
        pack_le64(buf, len, stat->length) ||
        pack_string(buf, len, stat->name) ||
        pack_string(buf, len, stat->uid) ||
        pack_string(buf, len, stat->gid) ||
        pack_string(buf, len, stat->muid))
      goto pack_stat_err_out;

    if (proto == P9_PROTO_DOTU)
    { // 9P2000.u fields
      if (pack_string(buf, len, stat->extension) ||
          pack_le32(buf, len, stat->n_uid) ||
          pack_le32(buf, len, stat->n_gid) ||
          pack_le32(buf, len, stat->n_muid))
        goto pack_stat_err_out;
    }

    size_t tmplen;
    size_t stat_size;
    tmplen = 2;
    stat_size = stashed_len - *len - 2;
    pack_le16(&stashed_buf, &tmplen, stat_size);
    return 0;

  pack_stat_err_out:
    *buf = stashed_buf;
    *len = stashed_len;
    return -1;
}

static int unpack_stat(unsigned char **buf, size_t *len,
                       struct plan9_stat *stat, int proto)
{
    size_t stashed_length = *len;
    uint16_t size;
    stat->name = stat->uid = stat->gid = stat->muid = stat->extension = NULL;

    if (unpack_le16(buf, len, &size) ||
        unpack_le16(buf, len, &stat->type) ||
        unpack_le32(buf, len, &stat->dev) ||
        unpack_qid(buf, len, &stat->qid) ||
        unpack_le32(buf, len, &stat->mode) ||
        unpack_le32(buf, len, &stat->atime) ||
        unpack_le32(buf, len, &stat->mtime) ||
        unpack_le64(buf, len, &stat->length) ||
        unpack_string(buf, len, &stat->name) ||
        unpack_string(buf, len, &stat->uid) ||
        unpack_string(buf, len, &stat->gid) ||
        unpack_string(buf, len, &stat->muid))
      goto unpack_stat_err_out;

    if (proto == P9_PROTO_DOTU)
    {// 9P2000.u fields
      if (unpack_string(buf, len, &stat->extension) ||
          unpack_le32(buf, len, &stat->n_uid) ||
          unpack_le32(buf, len, &stat->n_gid) ||
          unpack_le32(buf, len, &stat->n_muid))
        goto unpack_stat_err_out;
    }

    if (size != (stashed_length - *len - 2))
      goto unpack_stat_err_out;

    return 0;

  unpack_stat_err_out:
    ::free(stat->extension);
    ::free(stat->muid);
    ::free(stat->gid);
    ::free(stat->uid);
    ::free(stat->name);
    return -1;
}


static int pack_dotl_direntry(unsigned char **buf, size_t *len,
                        const struct plan9_stat *stat, size_t *offset)
{
    unsigned char *stashed_buf = *buf;
    size_t stashed_len = *len;

    unsigned char *offset_buf = NULL;
    size_t offset_len = 8;

    if (pack_qid(buf, len, &stat->qid) ||
        get_blob_ref(buf, len, &offset_buf, NULL, 8)  ||
        pack_le8(buf, len, stat->qid.type) ||
        pack_string(buf, len, stat->name))
        goto pack_dotl_err_out;

    *offset += stashed_len - *len ;
    pack_le64(&offset_buf, &offset_len, *offset);
    return 0;

pack_dotl_err_out:
    *buf = stashed_buf;
    *len = stashed_len;
    return -1;
}


static int pack_stat_dotl(unsigned char **buf, size_t *len,
                     const struct plan9_stat_dotl *stat)
{
    if (pack_qid(buf, len, &stat->qid) ||
        pack_le32(buf, len, stat->st_mode) ||
        pack_le32(buf, len, stat->st_uid) ||
        pack_le32(buf, len, stat->st_gid) ||
        pack_le64(buf, len, stat->st_nlink) ||
        pack_le64(buf, len, stat->st_rdev) ||
        pack_le64(buf, len, stat->st_size) ||
        pack_le64(buf, len, stat->st_blksize) ||
        pack_le64(buf, len, stat->st_blocks) ||
        pack_le64(buf, len, stat->st_atime_sec) ||
        pack_le64(buf, len, stat->st_atime_nsec) ||
        pack_le64(buf, len, stat->st_mtime_sec) ||
        pack_le64(buf, len, stat->st_mtime_nsec) ||
        pack_le64(buf, len, stat->st_ctime_sec) ||
        pack_le64(buf, len, stat->st_ctime_nsec) ||
        pack_le64(buf, len, stat->st_btime_sec) ||
        pack_le64(buf, len, stat->st_btime_nsec) ||
        pack_le64(buf, len, stat->st_gen) ||
        pack_le64(buf, len, stat->st_data_version))
        return -1;
    return 0;
}


static int pack_statfs(unsigned char **buf, size_t *len,
                     const struct plan9_statfs *statfs)
{
    if (pack_le32(buf, len, statfs->type) ||
        pack_le32(buf, len, statfs->bsize) ||
        pack_le64(buf, len, statfs->blocks) ||
        pack_le64(buf, len, statfs->bfree) ||
        pack_le64(buf, len, statfs->bavail) ||
        pack_le64(buf, len, statfs->files) ||
        pack_le64(buf, len, statfs->ffree) ||
        pack_le64(buf, len, statfs->fsid) ||
        pack_le32(buf, len, statfs->namelen))
        return -1;
    return 0;
}


static void cnode2qid(struct venus_cnode *cnode, struct plan9_qid *qid)
{
    fsobj *f;

    qid->type = (cnode->c_type == C_VDIR) ? P9_QTDIR :
                (cnode->c_type == C_VLNK) ? P9_QTSYMLINK :
                (cnode->c_type == C_VREG) ? P9_QTFILE :
                // P9_QTFILE is defined as 0
                0;

    qid->path = SpookyHash::Hash64(&cnode->c_fid, sizeof(VenusFid), 0);

    qid->version = 0;
    f = FSDB->Find(&cnode->c_fid);
    if (f) {
        ViceVersionVector *vv = f->VV();
        for (int i = 0; i < VSG_MEMBERS; i++)
            qid->version += (&vv->Versions.Site0)[i];
    }
}


static uid_t getuserid(const char *username)
{
    struct passwd pwd, *res = NULL;
    char *buf;
    ssize_t bufsize;

    bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufsize == -1)
        bufsize = 16384;

    buf = (char *)malloc(bufsize);
    assert(buf != NULL);

    getpwnam_r(username, &pwd, buf, bufsize, &res);
    if (res) {
        free(buf);
        return res->pw_uid;
    }

    /* fall back to nobody */
    getpwnam_r("nobody", &pwd, buf, bufsize, &res);
    if (res) {
        free(buf);
        return res->pw_uid;
    }

    free(buf);
    return (uid_t)-2;
}


static char * getusername(uid_t uid)
{
    struct passwd pwd, *res = NULL;
    char *buf;
    ssize_t bufsize;
    char *uname = (char *)"nobody"; /* fall back name */

    bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufsize == -1)
        bufsize = 16384;

    buf = (char *)malloc(bufsize);
    assert(buf != NULL);

    getpwuid_r(uid, &pwd, buf, bufsize, &res);
    if (res)
        uname = strdup(res->pw_name);
    free(buf);
    return uname;
}


plan9server::plan9server(mariner *m)
{
    conn = m;
    max_msize = P9_BUFSIZE;
    protocol = P9_PROTO_UNKNOWN;
}

plan9server::~plan9server()
{
}


int plan9server::pack_header(unsigned char **buf, size_t *len,
                             uint8_t type, uint16_t tag)
{
    /* we will fix this value when sending the message */
    uint32_t msglen = P9_MIN_MSGSIZE;

    if (pack_le32(buf, len, msglen) ||
        pack_le8(buf, len, type) ||
        pack_le16(buf, len, tag))
        return -1;
    return 0;
}


int plan9server::send_response(unsigned char *buf, size_t len)
{
    /* fix up response length */
    unsigned char *tmpbuf = buf;
    size_t tmplen = 4;
    pack_le32(&tmpbuf, &tmplen, len);

    /* send response */
    if (conn->write_until_done(buf, len) != (ssize_t)len)
        return -1;
    return 0;
}


/* Error messages formats:
 * 9P2000:    Rerror[tag]   err_string
 * 9P2000.u:  Rerror[tag]   err_string  errno
 * 9P2000.L:  Rlerror[tag]  errno
 */
int plan9server::send_error(uint16_t tag, const char *error, int errcode)
{
    unsigned char *buf;
    size_t len;

    DEBUG("9pfs: Rerror[%x] '%s', errno: %d\n", tag, error, errcode);

    buf = buffer; len = max_msize;
    switch (protocol) {
      case P9_PROTO_2000:
        if (pack_header(&buf, &len, Rerror, tag) ||
            pack_string(&buf, &len, error))
            return -1;
        break;
      case P9_PROTO_DOTU:
        if (pack_header(&buf, &len, Rerror, tag) ||
            pack_string(&buf, &len, error) ||
            pack_le32(&buf, &len, (uint32_t)errcode))
            return -1;
        break;
      case P9_PROTO_DOTL:
        if (pack_header(&buf, &len, Rlerror, tag) ||
            pack_le32(&buf, &len, (uint32_t)errcode))
            return -1;
        break;
      default:
          return -1;
    }

    return send_response(buffer, max_msize - len);
}


void plan9server::main_loop(unsigned char *initial_buffer, size_t len)
{
    if (initial_buffer)
        memcpy(buffer, initial_buffer, len);

    while (1)
    {
        if (handle_request(buffer, len))
            break;

        /* get next request, anticipate we can read the 9pfs header */
        len = P9_MIN_MSGSIZE;
        if (conn->read_until_done(buffer, len) != (ssize_t)len)
            break;
    }
}



int plan9server::handle_request(unsigned char *buf, size_t read)
{
    unsigned char *unread = &buf[read];
    size_t len = read;

    uint32_t reqlen;
    uint8_t  opcode;
    uint16_t tag;

    if (unpack_le32(&buf, &len, &reqlen) ||
        unpack_le8(&buf, &len, &opcode) ||
        unpack_le16(&buf, &len, &tag))
        return -1;

    DEBUG("\n9pfs: got request length %u, type %u, tag %x\n", reqlen, opcode,
                                                                        tag);

    if (reqlen < read)
        return -1;

    if (reqlen > max_msize) {
        send_error(tag, "Message too long", EMSGSIZE);
        return -1;
    }

    /* read the rest of the request */
    len = reqlen - read;
    if (conn->read_until_done(unread, len) != (ssize_t)len)
        return -1;

    /* initialize request context */
    conn->u.Init();
    conn->u.u_priority = FSDB->StdPri();
    conn->u.u_flags = (FOLLOW_SYMLINKS | TRAVERSE_MTPTS | REFERENCE);

    len = reqlen - P9_MIN_MSGSIZE;
    switch (opcode)
    {
    case Tversion:  return recv_version(buf, len, tag);
    case Tauth:     return recv_auth(buf, len, tag);
    case Tattach:   return recv_attach(buf, len, tag);
    case Tflush:    return recv_flush(buf, len, tag);
    case Twalk:     return recv_walk(buf, len, tag);
    case Topen:     return recv_open(buf, len, tag);
    case Tcreate:   return recv_create(buf, len, tag);
    case Tread:     return recv_read(buf, len, tag);
    case Twrite:    return recv_write(buf, len, tag);
    case Tclunk:    return recv_clunk(buf, len, tag);
    case Tremove:   return recv_remove(buf, len, tag);
    case Tstat:     return recv_stat(buf, len, tag);
    case Twstat:    return recv_wstat(buf, len, tag);
    /* dotl messages */
    case Tgetattr:  return recv_getattr(buf, len, tag);
    case Tsetattr:  return recv_setattr(buf, len, tag);
    case Tlopen:    return recv_lopen(buf, len, tag);
    case Tlcreate:  return recv_lcreate(buf, len, tag);
    case Tsymlink:  return recv_symlink(buf, len, tag);
    case Tmkdir:    return recv_mkdir(buf, len, tag);
    case Treaddir:  return recv_readdir(buf, len, tag);
    case Tstatfs:   return recv_statfs(buf, len, tag);
    case Treadlink: return recv_readlink(buf, len, tag);
    case Tfsync:    return recv_fsync(buf, len, tag);
    case Tunlinkat: return recv_unlinkat(buf, len, tag);
    case Tlink:     return recv_link(buf, len, tag);
    case Trename:   return recv_rename(buf, len, tag);
    case Trenameat: return recv_renameat(buf, len, tag);
    /* unsupported dotl operations */
    case Tmknod:        return recv_mknod(buf, len, tag);
    case Txattrwalk:    return recv_xattrwalk(buf, len, tag);
    case Txattrcreate:  return recv_xattrcreate(buf, len, tag);
    case Tlock:         return recv_lock(buf, len, tag);
    case Tgetlock:      return recv_getlock(buf, len, tag);
    default:        return send_error(tag, "Operation not supported", EBADRQC);
    }
    return 0;
}


int plan9server::recv_version(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t msize;
    char *remote_version;
    const char *version;

    if (unpack_le32(&buf, &len, &msize) ||
        unpack_string(&buf, &len, &remote_version))
        return -1;

    DEBUG("9pfs: Tversion[%x] msize %d, version %s\n",
          tag, msize, remote_version);

    max_msize = (msize < P9_BUFSIZE) ? msize : P9_BUFSIZE;

    if (::strncmp(remote_version, "9P2000.L", 8) == 0) {
        version = "9P2000.L";
        protocol = P9_PROTO_DOTL;
      }
    else if (::strncmp(remote_version, "9P2000.u", 8) == 0) {
        version = "9P2000.u";
        protocol = P9_PROTO_DOTU;
      }
    else if (::strncmp(remote_version, "9P2000", 6) == 0) {
        version = "9P2000";
        protocol = P9_PROTO_2000;
      }
    else {
        version = "unknown";
        protocol = P9_PROTO_UNKNOWN;
      }
    ::free(remote_version);

    /* abort all existing I/O, clunk all fids */
    del_fid(P9_NOFID);

    /* send_Rversion */
    DEBUG("9pfs: Rversion[%x] msize %lu, version %s\n",
          tag, max_msize, version);

    buf = buffer; len = max_msize;
    if (pack_header(&buf, &len, Rversion, tag) ||
        pack_le32(&buf, &len, max_msize) ||
        pack_string(&buf, &len, version))
    {
        send_error(tag, "Message too long", EMSGSIZE);
        return -1;
    }
    return send_response(buffer, max_msize - len);
}


int plan9server::recv_auth(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t afid;
    char *uname;
    char *aname;
    uid_t uid = ~0;

    if (unpack_le32(&buf, &len, &afid) ||
        unpack_string(&buf, &len, &uname))
        return -1;
    if (unpack_string(&buf, &len, &aname)) {
        ::free(uname);
        return -1;
    }
    if (protocol == P9_PROTO_DOTU || protocol == P9_PROTO_DOTL) {
      if (unpack_le32(&buf, &len, &uid)) {
        ::free(uname);
        ::free(aname);
        return -1;
      }
    }

    DEBUG("9pfs: Tauth[%x] afid %u, uname %s, aname %s, uid %d\n",
          tag, afid, uname, aname, uid);

    ::free(uname);
    ::free(aname);
#if 0
    /* send_Rauth */
    DEBUG("9pfs: Rauth[%x] aqid %x.%x.%lx\n",
          tag, aqid->type, aqid->version, aqid->path);

    buf = buffer; len = max_msize;
    if (pack_header(&buf, &len, Rauth) ||
        pack_qid(&buf, &len, aqid))
    {
        send_error(tag, "Message too long");
        return -1;
    }
    return send_response(buffer, max_msize - len);
#endif
    return send_error(tag, "Operation not supported", EBADRQC);
}


int plan9server::recv_attach(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t fid;
    uint32_t afid;
    char *uname;
    char *aname;
    uid_t uid = ~0;  // default for legacy protocol

    if (unpack_le32(&buf, &len, &fid) ||
        unpack_le32(&buf, &len, &afid) ||
        unpack_string(&buf, &len, &uname))
        return -1;
    if (unpack_string(&buf, &len, &aname))
    {
        ::free(uname);
        return -1;
    }
    if (protocol == P9_PROTO_DOTU || protocol == P9_PROTO_DOTL) {
      if (unpack_le32(&buf, &len, &uid)) {
        ::free(uname);
        ::free(aname);
        return -1;
      }
    }

    DEBUG("9pfs: Tattach[%x] fid %u, afid %u, uname %s, aname %s, uid %d\n",
          tag, fid, afid, uname, aname, uid);

    if (find_fid(fid)) {
        ::free(uname);
        ::free(aname);
        return send_error(tag, "fid already in use", EBADF);
    }

    struct attachment *root = new struct attachment;
    struct plan9_qid qid;

    root->refcount = 0;
    root->uname = strlen(uname) == 0 ? getusername(uid) : uname;
    root->aname = aname;
    root->userid = uid == (uid_t)~0 ? getuserid(uname) : uid ;

    conn->u.u_uid = root->userid;
    conn->root(&root->cnode);

    if (add_fid(fid, &root->cnode, root) == NULL) {
        int errcode = errno;
        ::free((void *)root->uname);
        ::free((void *)root->aname);
        delete root;
        return send_error(tag, "failed to allocate new fid", errcode);
    }

    cnode2qid(&root->cnode, &qid);

    /* send_Rattach */
    DEBUG("9pfs: Rattach[%x] qid %x.%x.%lx\n",
          tag, qid.type, qid.version, qid.path);

    buf = buffer; len = max_msize;
    if (pack_header(&buf, &len, Rattach, tag) ||
        pack_qid(&buf, &len, &qid))
    {
        send_error(tag, "Message too long", EMSGSIZE);
        return -1;
    }
    return send_response(buffer, max_msize - len);
}


int plan9server::recv_flush(unsigned char *buf, size_t len, uint16_t tag)
{
    uint16_t oldtag;
    int rc;

    if (unpack_le16(&buf, &len, &oldtag))
        return -1;

    DEBUG("9pfs: Tflush[%x] oldtag %x\n", tag, oldtag);

    /* abort any outstanding request tagged with 'oldtag' */

    /* send_Rflush */
    DEBUG("9pfs: Rflush[%x]\n", tag);

    buf = buffer; len = max_msize;
    rc = pack_header(&buf, &len, Rflush, tag);
    assert(rc == 0);
    return send_response(buffer, max_msize - len);
}


int plan9server::recv_walk(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t fid;
    uint32_t newfid;
    uint16_t nwname;

    if (unpack_le32(&buf, &len, &fid) ||
        unpack_le32(&buf, &len, &newfid) ||
        unpack_le16(&buf, &len, &nwname))
        return -1;

    if (nwname > P9_MAX_NWNAME)
    {
        send_error(tag, "Argument list too long", E2BIG);
        return -1;
    }

    DEBUG("9pfs: Twalk[%x] fid %u, newfid %u, nwname %u\n",
          tag, fid, newfid, nwname);

    struct fidmap *fm;
    struct venus_cnode current, child;
    int i;
    char *wname;
    uint16_t nwqid;
    struct plan9_qid wqid[P9_MAX_NWNAME];

    fm = find_fid(fid);
    if (!fm) {
        return send_error(tag, "fid unknown or out of range", EBADF);
    }
    current = fm->cnode;

    for (i = 0; i < nwname; i++) {
        if (unpack_string(&buf, &len, &wname))
            return -1;

        DEBUG("9pfs: Twalk[%x] wname[%u] = '%s'\n", tag, i, wname);

        /* do not go up any further when we have reached the root of the
         * mounted subtree */
        if (strcmp(wname, "..") != 0 ||
            !FID_EQ(&current.c_fid, &fm->root->cnode.c_fid))
        {
            conn->u.u_uid = fm->root->userid;
            conn->lookup(&current, wname, &child,
                         CLU_CASE_SENSITIVE | CLU_TRAVERSE_MTPT);

            if (conn->u.u_error) {
                ::free(wname);
                break;
            }
            current = child;
        }

        ::free(wname);
        cnode2qid(&current, &wqid[i]);
    }

    /* report lookup errors only for the first path element */
    if (i == 0 && conn->u.u_error) {
        int errcode = conn->u.u_error;
        const char *errstr = VenusRetStr(errcode);
        return send_error(tag, errstr, errcode);
    }
    nwqid = i;

    /* only if nwqid == nwname do we set newfid */
    if (nwqid == nwname) {
        if (fid == newfid)
            del_fid(fid);

        else if (find_fid(newfid))
            return send_error(tag, "fid already in use", EBADF);

        add_fid(newfid, &current, fm->root);
    }

    /* send_Rwalk */
    DEBUG("9pfs: Rwalk[%x] nwqid %u\n", tag, nwqid);

    buf = buffer; len = max_msize;
    if (pack_header(&buf, &len, Rwalk, tag) ||
        pack_le16(&buf, &len, nwqid))
    {
        send_error(tag, "Message too long", EMSGSIZE);
        return -1;
    }
    for (i = 0; i < nwqid; i++)
    {
        DEBUG("9pfs: Rwalk[%x] wqid[%u] %x.%x.%lx\n",
              tag, i, wqid[i].type, wqid[i].version, wqid[i].path);

        if (pack_qid(&buf, &len, &wqid[i]))
        {
            send_error(tag, "Message too long", EMSGSIZE);
            return -1;
        }
    }
    return send_response(buffer, max_msize - len);
}


int plan9server::recv_open(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t fid;
    uint8_t mode;

    if (unpack_le32(&buf, &len, &fid) ||
        unpack_le8(&buf, &len, &mode))
        return -1;

    DEBUG("9pfs: Topen[%x] fid %u, mode 0x%x\n", tag, fid, mode);

    struct fidmap *fm;
    int flags;

    fm = find_fid(fid);
    if (!fm)
        return send_error(tag, "fid unknown or out of range", EBADF);

    if (fm->open_flags)
        return send_error(tag, "file already open for I/O", EIO);

    /* We're not handling ORCLOSE, OEXEC, OEXCL yet and the rest should be 0 */
    if (mode & ~0x93)
        return send_error(tag, "Invalid argument", EINVAL);

    switch (mode & 0x3) {
    case P9_OREAD:
    case P9_OEXEC:
        flags = C_O_READ;
        break;
    case P9_OWRITE:
        flags = C_O_WRITE;
        break;
    case P9_ORDWR:
        flags = C_O_READ | C_O_WRITE;
        break;
    }
    if (mode & P9_OTRUNC)
        flags |= C_O_TRUNC;

    /* vget and open yield, so we may lose fidmap, but we need to make sure we
     * can close opened cnodes if the fidmap was removed while we yielded. */
    struct venus_cnode cnode = fm->cnode;
    struct plan9_qid qid;

    if (flags & (C_O_WRITE | C_O_TRUNC)) {
        switch (cnode.c_type) {
        case C_VREG:
            break;
        case C_VDIR:
            return send_error(tag, "can't open directory for writing", EISDIR);
        case C_VLNK:
        default:
            return send_error(tag, "file is read only", EINVAL);
        }
    }

    conn->u.u_uid = fm->root->userid;
    if (cnode.c_type == C_VLNK) {
        struct venus_cnode tmp;
        conn->vget(&tmp, &cnode.c_fid, RC_STATUS|RC_DATA);
    }
    else {
        conn->open(&cnode, flags);
    }
    if (conn->u.u_error) {
        int errcode = conn->u.u_error;
        const char *errstr = VenusRetStr(errcode);
        return send_error(tag, errstr, errcode);
    }

    /* open yields, reobtain fidmap reference */
    fm = find_fid(fid);
    if (!fm) {
        if (cnode.c_type != C_VLNK)
            conn->close(&cnode, flags);
        return send_error(tag, "fid unknown or out of range", EBADF);
    }
    fm->open_flags = flags;

    cnode2qid(&cnode, &qid);

    uint32_t iounit = 4096;

    /* send_Ropen */
    DEBUG("9pfs: Ropen[%x] qid %x.%x.%lx, iounit %u\n",
          tag, qid.type, qid.version, qid.path, iounit);

    buf = buffer; len = max_msize;
    if (pack_header(&buf, &len, Ropen, tag) ||
        pack_qid(&buf, &len, &qid) ||
        pack_le32(&buf, &len, iounit))
    {
        send_error(tag, "Message too long", EMSGSIZE);
        return -1;
    }
    send_response(buffer, max_msize - len);
    return 0;
}


/* TODO fix leaking name and extension on error return paths
*/
int plan9server::recv_create(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t fid;
    char *name;
    uint32_t perm;
    uint8_t mode;
    char *extension = (char *)"";

    if (unpack_le32(&buf, &len, &fid) ||
        unpack_string(&buf, &len, &name))
        return -1;
    if (unpack_le32(&buf, &len, &perm) ||
        unpack_le8(&buf, &len, &mode))
    {
        free(name);
        return -1;
    }
    if (protocol == P9_PROTO_DOTU && unpack_string(&buf, &len, &extension))
    {
        free(name);
        return -1;
    }

    DEBUG("9pfs: Tcreate[%x] fid %u, name %s, perm %o, mode 0x%x, extension %s\n",
          tag, fid, name, perm, mode, extension);

    struct fidmap *fm;
    struct coda_vattr va = { 0 };
    int excl = 0;
    int flags;

    va.va_size = 0;
    va.va_mode = perm & 0777;

    fm = find_fid(fid);
    if (!fm)
        return send_error(tag, "fid unknown or out of range", EBADF);

    if (fm->open_flags)
        return send_error(tag, "file already open for I/O", EIO);

    /* we can only create in a directory */
    if (fm->cnode.c_type != C_VDIR)
        return send_error(tag, "Not a directory", ENOTDIR);

    /* We're not handling ORCLOSE, OEXEC, OEXCL yet and the rest should be 0 */
    if (mode & ~0x93)
        return send_error(tag, "Invalid argument", EINVAL);

    switch (mode & 0x3) {
    case P9_OREAD:
    case P9_OEXEC:
        flags = C_O_READ;
        break;
    case P9_OWRITE:
        flags = C_O_WRITE;
        break;
    case P9_ORDWR:
        flags = C_O_READ | C_O_WRITE;
        break;
    }
    if (mode & P9_OTRUNC) {
        flags |= C_O_TRUNC;
        va.va_size = 0; /* traditionally this is how Coda indicated truncate */
    }

    struct venus_cnode child;
    struct plan9_qid qid;

    conn->u.u_uid = fm->root->userid;

    if (perm & P9_DMDIR) {
      /* create a directory */
      conn->mkdir(&fm->cnode, name, &va, &child);
    }
    else if (perm & P9_DMSYMLINK) {
      /* create a symlink */
      conn->symlink(&fm->cnode, extension, &va, name);
      conn->lookup(&fm->cnode, name, &child,
                   CLU_CASE_SENSITIVE | CLU_TRAVERSE_MTPT);
    }
    else if (perm & P9_DMLINK) {
      /* create a hardlink */
      uint32_t src_fid = strtol(extension, NULL, 10); //undocumented in 9P
      struct fidmap * src_fm = find_fid(src_fid);     //fidmap of link src
      if (!src_fm)
          return send_error(tag, "source fid unknown or out of range", EBADF);
      conn->link(&src_fm->cnode, &fm->cnode, name);
      child = src_fm->cnode;
    }
    else {
      /* create a regular file */
      conn->create(&fm->cnode, name, &va, excl, flags, &child);
    }

    if (conn->u.u_error) {
        int errcode = conn->u.u_error;
        const char *errstr = VenusRetStr(errcode);
        return send_error(tag, errstr, errcode);
    }

    if (child.c_type != C_VLNK)
        conn->open(&child, flags);

    if (conn->u.u_error) {
        int errcode = conn->u.u_error;
        const char *errstr = VenusRetStr(errcode);
        return send_error(tag, errstr, errcode);
    }

    /* create yields, reobtain fidmap reference */
    fm = find_fid(fid);
    if (!fm) {
        if (child.c_type != C_VLNK)
            conn->close(&child, flags);
        return send_error(tag, "fid unknown or out of range", EBADF);
    }
    /* fid is replaced by the newly created file/directory/link */
    fm->cnode = child;
    fm->open_flags = flags;

    cnode2qid(&child, &qid);

    uint32_t iounit = 4096;

    /* send_Rcreate */
    DEBUG("9pfs: Rcreate[%x] qid %x.%x.%lx, iounit %u\n",
          tag, qid.type, qid.version, qid.path, iounit);

    buf = buffer; len = max_msize;
    if (pack_header(&buf, &len, Rcreate, tag) ||
        pack_qid(&buf, &len, &qid) ||
        pack_le32(&buf, &len, iounit))
    {
        send_error(tag, "Message too long", EMSGSIZE);
        return -1;
    }
    return send_response(buffer, max_msize - len);
}


int plan9server::recv_read(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t fid;
    uint64_t offset;
    uint32_t count;

    if (unpack_le32(&buf, &len, &fid) ||
        unpack_le64(&buf, &len, &offset) ||
        unpack_le32(&buf, &len, &count))
        return -1;

    DEBUG("9pfs: Tread[%x] fid %u, offset %lu, count %u\n",
          tag, fid, offset, count);

    struct fidmap *fm = find_fid(fid);
    if (!fm)
        return send_error(tag, "fid unknown or out of range", EBADF);

    if (!(fm->open_flags & C_O_READ))
        return send_error(tag, "Bad file descriptor", EBADF);

    if (protocol == P9_PROTO_DOTL && fm->cnode.c_type == C_VDIR)
        return send_error(tag, "Under 9P2000.L, Tread cannot be used on dirs",
                          EINVAL);

    /* send_Rread */
    unsigned char *tmpbuf;
    buf = buffer; len = max_msize;
    if (pack_header(&buf, &len, Rread, tag) ||
        get_blob_ref(&buf, &len, &tmpbuf, NULL, 4))
    {
        send_error(tag, "Message too long", EMSGSIZE);
        return -1;
    }

    if (count > len)
        count = len;

    ssize_t n = plan9_read(fm, buf, count, offset);
    if (n < 0) {
        int errcode = conn->u.u_error;
        const char *errstr = VenusRetStr(errcode);
        return send_error(tag, errstr, errcode);
    }

    /* fix up the actual size of the blob, and send */
    size_t tmplen = 4;
    pack_le32(&tmpbuf, &tmplen, n);

    DEBUG("9pfs: Rread[%x] %ld\n", tag, n);
    len -= n;
    return send_response(buffer, max_msize - len);
}


int plan9server::recv_write(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t fid;
    uint64_t offset;
    uint32_t count;

    if (unpack_le32(&buf, &len, &fid) ||
        unpack_le64(&buf, &len, &offset) ||
        unpack_le32(&buf, &len, &count))
        return -1;

    DEBUG("9pfs: Twrite[%x] fid %u, offset %lu, count %u\n",
          tag, fid, offset, count);

    if (len < count)
        return -1;

    struct fidmap *fm = find_fid(fid);
    if (!fm)
        return send_error(tag, "fid unknown or out of range", EBADF);

    if (!(fm->open_flags & C_O_WRITE) ||
        fm->cnode.c_type != C_VREG)
        return send_error(tag, "Bad file descriptor", EBADF);

    fsobj *f;
    int fd;
    ssize_t n;

    f = FSDB->Find(&fm->cnode.c_fid);
    assert(f); /* open file should have a reference */

    fd = f->data.file->Open(O_WRONLY);
    if (fd < 0)
        return send_error(tag, "I/O error", EIO);

    n = ::pwrite(fd, buf, count, offset);

    f->data.file->Close(fd);

    if (n < 0) {
        int errcode = conn->u.u_error;
        const char *errstr = VenusRetStr(errcode);
        return send_error(tag, errstr, errcode);
    }

    /* send_Rwrite */
    DEBUG("9pfs: Rwrite[%x] %lu\n", tag, n);

    buf = buffer; len = max_msize;
    if (pack_header(&buf, &len, Rwrite, tag) ||
        pack_le32(&buf, &len, n))
    {
        send_error(tag, "Message too long", EMSGSIZE);
        return -1;
    }
    return send_response(buffer, max_msize - len);
}


int plan9server::recv_clunk(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t fid;
    int rc;

    if (unpack_le32(&buf, &len, &fid))
        return -1;

    DEBUG("9pfs: Tclunk[%x] fid %u\n", tag, fid);

    rc = del_fid(fid);
    if (rc)
        return send_error(tag, "fid unknown or out of range", EBADF);

    /* send_Rclunk */
    DEBUG("9pfs: Rclunk[%x]\n", tag);

    buf = buffer; len = max_msize;
    rc = pack_header(&buf, &len, Rclunk, tag);
    assert(rc == 0); /* only sending header, should never be truncated */
    return send_response(buffer, max_msize - len);
}


int plan9server::recv_remove(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t fid;
    int rc;

    if (unpack_le32(&buf, &len, &fid))
        return -1;

    DEBUG("9pfs: Tremove[%x] fid %u\n", tag, fid);

    struct fidmap *fm = find_fid(fid);
    if (!fm)
        return send_error(tag, "fid unknown or out of range", EBADF);

    /* find the filename */
    char name[NAME_MAX];
    cnode_getname(&fm->cnode, name);

    /* Find the parent directory cnode */
    struct venus_cnode parent_cnode;
    int errcode = cnode_getparent(&fm->cnode, &parent_cnode);

    if (!errcode) {
        conn->u.u_uid = fm->root->userid;
        if (fm->cnode.c_type == C_VDIR)
            conn->rmdir(&parent_cnode, name);   /* remove a directory */
        else
            conn->remove(&parent_cnode, name);  /* remove a regular file */

        if (conn->u.u_error)
            errcode = conn->u.u_error;
    }

    /* 9p clunks the file, whether the actual server remove succeeded or not */
    del_fid(fid);

    if (errcode) {
        const char *errstr = VenusRetStr(errcode);
        return send_error(tag, errstr, errcode);
    }

    /* send_Rremove */
    DEBUG("9pfs: Rremove[%x]\n", tag);

    buf = buffer; len = max_msize;
    rc = pack_header(&buf, &len, Rremove, tag);
    assert(rc == 0);
    return send_response(buffer, max_msize - len);
}


int plan9server::recv_stat(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t fid;

    if (unpack_le32(&buf, &len, &fid))
        return -1;

    DEBUG("9pfs: Tstat[%x] fid %u\n", tag, fid);

    struct fidmap *fm;
    struct plan9_stat stat;
    int rc;

    fm = find_fid(fid);
    if (!fm)
        return send_error(tag, "fid unknown or out of range", EBADF);

    rc = plan9_stat(&fm->cnode, fm->root, &stat);
    if (rc) {
      int errcode = conn->u.u_error;
      const char *errstr = VenusRetStr(errcode);
      ::free(stat.name);
      return send_error(tag, errstr, errcode);
    }

    /* send_Rstat */
    DEBUG("9pfs: Rstat[%x]\n", tag);
    DEBUG("\
      mode: %o  length:  %lu name: '%s' \n \
      qid[type.ver.path]: %x.%x.%lx \n \
      extension: '%s' \n \
      uid: %s  gid: %s  muid: %s \n \
      n_uid: %d  n_gid: %d  n_muid: %d\n \
      atime: %u  mtime: %u \n ",
      stat.mode, stat.length, stat.name,
      stat.qid.type, stat.qid.version, stat.qid.path,
      stat.extension,
      stat.uid, stat.gid, stat.muid,
      stat.n_uid, stat.n_gid, stat.n_muid,
      stat.atime, stat.mtime);

    unsigned char *stashed_buf = NULL;
    size_t stashed_len = 0;

    buf = buffer; len = max_msize;
    if (pack_header(&buf, &len, Rstat, tag) ||
        get_blob_ref(&buf, &len, &stashed_buf, &stashed_len, 2) ||
        pack_stat(&buf, &len, &stat, protocol))
    {
        ::free(stat.name);
        send_error(tag, "Message too long", EMSGSIZE);
        return -1;
    }
    ::free(stat.name);

    size_t tmplen = 2;
    pack_le16(&stashed_buf, &tmplen, stashed_len - len - 2);
    return send_response(buffer, max_msize - len);
}


/*
 * @Unimplemented: setting gid
 */
int plan9server::recv_wstat(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t fid;
    uint16_t statlen;
    struct plan9_stat stat;
    struct coda_vattr attr;
    int errcode = 0;
    const char *strerr = NULL;
    int rc;

    if (unpack_le32(&buf, &len, &fid) ||
        unpack_le16(&buf, &len, &statlen) ||
        unpack_stat(&buf, &len, &stat, protocol))
        return -1;
    /* unpacked fields are freed before returning, at Send_Rwstat: */

    DEBUG("9pfs: Twstat[%x] fid %u, statlen %u\n", tag, fid, statlen);
    DEBUG("\
      mode:         %o \n \
      atime:        %d \n \
      mtime:        %d \n \
      length:       %d \n \
      name:         %s \n ",
      stat.mode, (int)stat.atime, (int)stat.mtime, (int)stat.length, stat.name);

    struct fidmap *fm;
    fm = find_fid(fid);
    if (!fm) {
        errcode = EBADF;
        strerr = "fid unknown or out of range";
        goto err_out;
    }

    /* modifications allowed for wstat:
      struct plan9_stat {
          uint16_t          type;    - illegal
          uint32_t          dev;     - illegal
          struct plan9_qid  qid;     - illegal
          uint32_t          mode;    - allowed by owner or group leader,
          uint32_t          atime;   - auto                     except dir bit
          uint32_t          mtime;   - allowed by owner or group leader
          uint64_t          length;  - allowed if write perm, not for dirs
          char *            name;    - allowed if write perm on parent dir
          char *            uid;     - illegal
          char *            gid;     - Unimplemented - allowed by owner or
          char *            muid;    - auto      or group leader, if in new gid

          char *      extensions;    - illegal?
          uid_t             n_uid;   - illegal
          uid_t             n_gid;   - Unimplemented (see gid)
          uid_t             n_muid;  - auto
      };
    */

    /* check for illegal modifcations */
    if ( stat.type != P9_DONT_TOUCH_TYPE
      || stat.dev != P9_DONT_TOUCH_DEV
      || stat.qid.type != P9_DONT_TOUCH_QID_TYPE
      || stat.qid.version != P9_DONT_TOUCH_QID_VERS
      || stat.qid.path != P9_DONT_TOUCH_QID_PATH
      || strcmp(stat.uid, P9_DONT_TOUCH_UID) != 0
      )
    {
        errcode = EINVAL;
        strerr = "Twstat tried to modify stat field illegally";
        goto err_out;
    }

    if (protocol == P9_PROTO_DOTU) {
      if (strcmp(stat.extension, P9_DONT_TOUCH_EXTENSION) != 0
        || stat.n_uid != P9_DONT_TOUCH_NUID)
      {
          errcode = EINVAL;
          strerr = "Twstat tried to modify stat field illegally";
          goto err_out;
      }
    }

    /* prepare to write the file attributes to Venus */
    conn->u.u_uid = fm->root->userid;

    /* if wstat involves a rename */
    if (strcmp(stat.name, P9_DONT_TOUCH_NAME) != 0) {
      /* get current name */
      char name[NAME_MAX];
      cnode_getname(&fm->cnode, name);

      /* Find the parent directory cnode */
      struct venus_cnode parent_cnode;
      if (cnode_getparent(&fm->cnode, &parent_cnode) < 0) {
          errcode = EINVAL;
          strerr = "tried to rename the mountpoint";
          goto err_out;
      }

      /* attempt rename */
      DEBUG("--- renaming %s to %s\n", name, stat.name);
      conn->rename(&parent_cnode, name,&parent_cnode, stat.name);
      if (conn->u.u_error) {
          errcode = conn->u.u_error;
          strerr = VenusRetStr(errcode);
          goto err_out;
      }
    }
    else if (stat.mode == P9_DONT_TOUCH_MODE
          && stat.length == P9_DONT_TOUCH_LENGTH
          && stat.mtime == P9_DONT_TOUCH_MTIME )  {
    /* If all legal wstat fields were "don't touch", then according to 9P
     * protocol, the server can interpret this wstat as a request to guarantee
     * that the contents of the associated file are committed to stable storage
     * before the Rwstat message is returned (i.e. "make the state of the file
     * exactly what it claims to be").
     */
      DEBUG("--- fsyncing fid %d\n", fid);
      //conn->fsync(&fm->cnode);
      if (conn->u.u_error) {
        errcode = conn->u.u_error;
        strerr = VenusRetStr(errcode);
        goto err_out;
      }
    }
    else {
      /* Update attr with the new stats to be written */
      DEBUG("--- writing attributes of fid %d\n", fid);
      /* must be set to IGNORE or will trigger error in vproc::setattr() */
      attr.va_fileid = VA_IGNORE_ID;
      attr.va_nlink = VA_IGNORE_NLINK;
      attr.va_blocksize = VA_IGNORE_BLOCKSIZE;
      attr.va_flags = VA_IGNORE_FLAGS;
      attr.va_rdev = VA_IGNORE_RDEV;
      attr.va_bytes = VA_IGNORE_STORAGE;

      /* vproc::setattr() can set the following 4 attributes */
      attr.va_uid = VA_IGNORE_UID;	   /* Cannot be modified through wstat */
      attr.va_mode = (stat.mode == P9_DONT_TOUCH_MODE) ?
                  VA_IGNORE_MODE : stat.mode & 0777;
      attr.va_size = (stat.length == P9_DONT_TOUCH_LENGTH) ?
                 VA_IGNORE_SIZE : stat.length;	  /* does this work? */
      attr.va_mtime.tv_sec = (stat.mtime == P9_DONT_TOUCH_MTIME) ?
                  VA_IGNORE_TIME1 : stat.mtime;
                                      /* rest of va_mtime is kept as-is */

      /* vproc::setattr() doesn't document what to do with the remaining
       * vattr so we just ignore them */

      /* attempt setattr */
      conn->setattr(&fm->cnode, &attr);
      if (conn->u.u_error) {
          errcode = conn->u.u_error;
          strerr = VenusRetStr(errcode);
          goto err_out;
      }
    }

    ::free(stat.muid);
    ::free(stat.gid);
    ::free(stat.uid);
    ::free(stat.name);
    if (protocol == P9_PROTO_DOTU)
        ::free(stat.extension);

    /* send_Rwstat */
    DEBUG("9pfs: Rwstat[%x]\n", tag);

    buf = buffer; len = max_msize;
    rc = pack_header(&buf, &len, Rwstat, tag);
    assert(rc == 0);
    return send_response(buffer, max_msize - len);

err_out:
    ::free(stat.muid);
    ::free(stat.gid);
    ::free(stat.uid);
    ::free(stat.name);
    if (protocol == P9_PROTO_DOTU)
        ::free(stat.extension);
    return send_error(tag, strerr, errcode);
}


struct filldir_args {
    plan9server *srv;
    unsigned char *buf;
    size_t count;
    size_t packed_offset;   // offset packed in dotl direntries
    size_t offset;
    struct venus_cnode parent;
    struct attachment *root;
};

static int filldir(struct DirEntry *de, void *hook)
{
    struct filldir_args *args = (struct filldir_args *)hook;
    if (strcmp(de->name, ".") == 0 || strcmp(de->name, "..") == 0)
        return 0;
    return args->srv->pack_dirent(&args->buf, &args->count, &args->packed_offset,
                                  &args->offset, &args->parent,
                                  args->root, de->name);
}

int plan9server::pack_dirent(unsigned char **buf, size_t *len, size_t *packed_offset,
                             size_t *offset, struct venus_cnode *parent,
                             struct attachment *root, const char *name)
{
    struct venus_cnode child;
    struct plan9_stat stat;
    int rc = 0;

    conn->u.u_uid = root->userid;
    conn->lookup(parent, name, &child, CLU_CASE_SENSITIVE | CLU_TRAVERSE_MTPT);
    if (conn->u.u_error)
        return conn->u.u_error;

    plan9_stat(&child, root, &stat, name);

    /* at first we iterate through already returned entries */
    if (*offset) {
        /* if there is an offset, we're guaranteed to be at the start of the
         * buffer. So there is 'room' for scratch space there. */
        unsigned char *scratch = *buf;
        if (protocol == P9_PROTO_DOTL) {
            rc = pack_dotl_direntry(&scratch, offset, &stat, packed_offset);
        }
        else {
            rc = pack_stat(&scratch, offset, &stat, protocol);
        }
        ::free(stat.name);
        if (rc) {
            /* was this a case of (offset < stat_length)? */
            /* -> i.e. 'bad offset in directory read' */
            return ESPIPE;
        }
        return 0;
    }

    /* and finally we pack until we cannot fit any more entries */
    if (protocol == P9_PROTO_DOTL) {
        rc = pack_dotl_direntry(buf, len, &stat, packed_offset);
        if (!rc) {
            DEBUG("      qid[%x.%x.%lx] off[%lu] typ[%x] '%s'\n",
                    stat.qid.type, stat.qid.version, stat.qid.path,
                    *packed_offset, stat.qid.type, stat.name);
        }
    }
    else {
        rc = pack_stat(buf, len, &stat, protocol);
    }
    ::free(stat.name);
    if (rc) /* failed to pack this entry. stop enumerating. */
        return ENOBUFS;
    return 0;
}


ssize_t plan9server::plan9_read(struct fidmap *fm, unsigned char *buf,
                                size_t count, size_t offset)
{
    fsobj *f;
    int fd;
    ssize_t n = 0;

    if (fm->cnode.c_type == C_VREG)
    {
        f = FSDB->Find(&fm->cnode.c_fid);
        assert(f); /* open file should have a reference */

        fd = f->data.file->Open(O_RDONLY);
        if (fd < 0) {
            conn->u.u_error = EIO;
            return -1;
        }

        n = ::pread(fd, buf, count, offset);
        if (n < 0) {
            conn->u.u_error = errno;
            n = -1;
        }

        f->data.file->Close(fd);
    }
    else if (fm->cnode.c_type == C_VDIR)
    {
        f = FSDB->Find(&fm->cnode.c_fid);
        assert(f); /* open directory should have a reference */

        int rc;
        struct filldir_args args;
        args.srv = this;
        args.offset = offset;
        args.buf = buf;
        args.count = count;
        args.packed_offset = 0;
        args.parent = fm->cnode;

        // when we allow concurrency we should bump the refcount on root
        // and handle cleanup when enumeratedir returns
        args.root = fm->root;

        rc = ::DH_EnumerateDir(&f->data.dir->dh, filldir, &args);
        if (rc && rc != ENOBUFS) {
            conn->u.u_error = rc;
            return -1;
        }
        n = count - args.count;
    }
    else if (fm->cnode.c_type == C_VLNK && offset == 0) {
        struct coda_string cstring;
        cstring.cs_buf = (char *)buf;
        cstring.cs_maxlen = count;

        conn->u.u_uid = fm->root->userid;
        conn->readlink(&fm->cnode, &cstring);

        n = conn->u.u_error ? -1 : cstring.cs_len;
    }
    return n;
}


int plan9server::plan9_stat(struct venus_cnode *cnode, struct attachment *root,
                            struct plan9_stat *stat, const char *name)
{
    struct coda_vattr attr;

    if (!name) {
      char buf[NAME_MAX];
      cnode_getname(cnode, buf);
      name = buf;
    }

    /* first fill in a mostly ok stat block, in case getattr fails */
    stat->type = 0;
    stat->dev = 0;
    cnode2qid(cnode, &stat->qid);
    stat->mode = (stat->qid.type << 24);
    stat->atime = 0;
    stat->mtime = 0;
    stat->length = 0;
    stat->name = strdup(name);
    stat->uid = (char *)root->uname;
    stat->gid = (char *)root->uname;
    stat->muid = (char *)root->uname;
    // 9P2000.u  extensions
    stat->extension = (char*)"";
    stat->n_uid = root->userid;
    stat->n_gid = root->userid;
    stat->n_muid = root->userid;

    conn->u.u_uid = root->userid;
    conn->getattr(cnode, &attr);

    /* check for getattr errors if we're not called from filldir */
    if (conn->u.u_error)
        return -1;

    stat->mode |= (attr.va_mode & (S_IRWXU|S_IRWXG|S_IRWXO));
    stat->atime = attr.va_atime.tv_sec;
    stat->mtime = attr.va_mtime.tv_sec;
    stat->length = (stat->qid.type == P9_QTDIR) ? 0 : attr.va_size;

    if (stat->qid.type == P9_QTSYMLINK) {
      char target_string[PATH_MAX + 1];
      struct coda_string cstring;
      cstring.cs_buf = target_string;
      cstring.cs_maxlen = PATH_MAX;
      conn->u.u_uid = root->userid;
      conn->readlink(cnode, &cstring);
      if (conn->u.u_error)
          return -1;
      stat->extension = strdup(cstring.cs_buf);
    }

    return 0;
}


/*
 * 9P2000.L operations
 */

int plan9server::recv_getattr(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t fid;
    uint64_t request_mask;

    if (unpack_le32(&buf, &len, &fid) ||
       unpack_le64(&buf, &len, &request_mask))
       return -1;

    DEBUG("9pfs: Tgetattr[%x] fid %u req mask 0x%lx\n", tag, fid, request_mask);

    struct fidmap *fm;
    struct plan9_stat_dotl stat;
    struct coda_vattr attr;

    fm = find_fid(fid);
    if (!fm)
       return send_error(tag, "fid unknown or out of range", EBADF);

    conn->u.u_uid = fm->root->userid;
    conn->getattr(&fm->cnode, &attr);
    if (conn->u.u_error) {
        int errcode = conn->u.u_error;
        const char *errstr = VenusRetStr(errcode);
        return send_error(tag, errstr, errcode);
    }

    cnode2qid(&fm->cnode, &stat.qid);
    switch (stat.qid.type) {
        case  P9_QTDIR:
            stat.st_mode = S_IFDIR;
            break;
        case  P9_QTSYMLINK:
            stat.st_mode = S_IFLNK;
            break;
        case  P9_QTFILE:
            stat.st_mode = S_IFREG;
            break;
        default:
            stat.st_mode = 0;
    }
    stat.st_mode |= (uint32_t)attr.va_mode;
    stat.st_uid = fm->root->userid;
    stat.st_gid = fm->root->userid;
    cnode_linkcount(&fm->cnode, &stat.st_nlink);
    stat.st_rdev = (uint64_t)attr.va_rdev;
    stat.st_size = attr.va_size;
    stat.st_blksize = (uint64_t)attr.va_blocksize;
    //number of 512-byte blocks allocated
    stat.st_blocks = (stat.st_size + 511) >> 9;
    stat.st_atime_sec = attr.va_atime.tv_sec;
    stat.st_atime_nsec = attr.va_atime.tv_nsec;
    stat.st_mtime_sec = attr.va_mtime.tv_sec;
    stat.st_mtime_nsec = attr.va_mtime.tv_nsec;
    stat.st_ctime_sec = attr.va_ctime.tv_sec;
    stat.st_ctime_nsec = attr.va_ctime.tv_nsec;
    /* reserved for future use */
    stat.st_btime_sec = 0;
    stat.st_btime_nsec = 0;
    stat.st_gen = attr.va_gen;
    stat.st_data_version = 0;

    // For now, the valid fields in response are the same as the ones requested
    uint64_t valid_mask = request_mask;

    /* send_Rgetattr */
    /* find the filename */
    char name[NAME_MAX];
    cnode_getname(&fm->cnode, name);
    DEBUG("9pfs: Rgetattr[%x] valid mask 0x%lx  (%s)\n", tag, valid_mask, name);
    DEBUG("\
            qid[type.ver.path]: %x.%x.%lx \n \
            mode: 0%o  n_uid: %d  n_gid: %d  nlink: %lu \n \
            rdev: %lu  size: %lu  blksize: %lu  blocks: %lu \n \
            atime_sec: %lu  atime_nsec: %lu \n \
            mtime_sec: %lu  mtime_nsec: %lu \n \
            ctime_sec: %lu  ctime_nsec: %lu \n \
            btime_sec: %lu  btime_nsec: %lu \n \
            gen: %lu  data_version: %lu \n",
            stat.qid.type, stat.qid.version, stat.qid.path,
            stat.st_mode, stat.st_uid, stat.st_gid, stat.st_nlink,
            stat.st_rdev, stat.st_size, stat.st_blksize, stat.st_blocks,
            stat.st_atime_sec, stat.st_atime_nsec, stat.st_mtime_sec,
            stat.st_mtime_nsec, stat.st_ctime_sec, stat.st_ctime_nsec,
            stat.st_btime_sec, stat.st_btime_nsec, stat.st_gen,
            stat.st_data_version);

    buf = buffer; len = max_msize;
    if (pack_header(&buf, &len, Rgetattr, tag) ||
       pack_le64(&buf, &len, valid_mask) ||
       pack_stat_dotl(&buf, &len, &stat))
    {
       send_error(tag, "Message too long", EMSGSIZE);
       return -1;
    }
    send_response(buffer, max_msize - len);
    return 0;
}


int plan9server::recv_setattr(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t fid;
    uint32_t valid_mask;
    struct plan9_stat_dotl stat;
    struct coda_vattr attr;

    if (unpack_le32(&buf, &len, &fid) ||
        unpack_le32(&buf, &len, &valid_mask) ||
        unpack_le32(&buf, &len, &stat.st_mode) ||
        unpack_le32(&buf, &len, &stat.st_uid) ||
        unpack_le32(&buf, &len, &stat.st_gid) ||
        unpack_le64(&buf, &len, &stat.st_size) ||
        unpack_le64(&buf, &len, &stat.st_atime_sec) ||
        unpack_le64(&buf, &len, &stat.st_atime_nsec) ||
        unpack_le64(&buf, &len, &stat.st_mtime_sec) ||
        unpack_le64(&buf, &len, &stat.st_mtime_nsec))
       return -1;

    DEBUG("9pfs: Tsetattr[%x] fid %u  valid mask 0x%x \n \
                 mode %o \n \
                 uid %u  gid %u \n \
                 size %lu \n \
                 atime_sec %lu  _nsec %lu \n \
                 mtime_sec %lu  _nsec %lu \n",
                 tag, fid, valid_mask, stat.st_mode, stat.st_uid, stat.st_gid,
                 stat.st_size, stat.st_atime_sec, stat.st_atime_nsec,
                 stat.st_mtime_sec, stat.st_mtime_nsec);

    struct timespec atime = {(time_t)stat.st_atime_sec,
                             (long)stat.st_atime_nsec };
    struct timespec mtime = {(time_t)stat.st_mtime_sec,
                             (long)stat.st_mtime_nsec };
    struct timespec ignore_time = {VA_IGNORE_TIME1,
                                   VA_IGNORE_TIME1 };
    struct timespec now_time;
    ::clock_gettime(CLOCK_REALTIME, &now_time);

    struct fidmap *fm = find_fid(fid);
    if (!fm)
       return send_error(tag, "fid unknown or out of range", EBADF);

    /* Build coda_vattr struct for vproc::setattr() */
    /* must be set to IGNORE or will trigger error in vproc::setattr() */
    attr.va_fileid = VA_IGNORE_ID;
    attr.va_nlink = VA_IGNORE_NLINK;
    attr.va_blocksize = VA_IGNORE_BLOCKSIZE;
    attr.va_flags = VA_IGNORE_FLAGS;
    attr.va_rdev = VA_IGNORE_RDEV;
    attr.va_bytes = VA_IGNORE_STORAGE;
    /* vproc::setattr() can set the following 4 attributes */
    attr.va_mode = valid_mask & P9_SETATTR_MODE ?
                    stat.st_mode & 0777 : VA_IGNORE_MODE;
    attr.va_uid = valid_mask & P9_SETATTR_UID ? stat.st_uid : VA_IGNORE_UID;
    attr.va_size = valid_mask & P9_SETATTR_SIZE ? stat.st_size : VA_IGNORE_SIZE;
    attr.va_mtime= valid_mask & P9_SETATTR_MTIME ?
                   valid_mask & P9_SETATTR_MTIME_SET ?
                   mtime : now_time : ignore_time;
    /* Currently vproc::setattr() ignores these attributes (oct. 2018 -AS) */
    attr.va_gid = valid_mask & P9_SETATTR_GID ? stat.st_gid : VA_IGNORE_GID;
    attr.va_atime= valid_mask & P9_SETATTR_ATIME ?
                   valid_mask & P9_SETATTR_ATIME_SET ?
                   atime : now_time : ignore_time;
    /* vproc::setattr() doesn't document what to do with the remaining vattr
        so we just ignore them */

    /* attempt setattr */
    conn->u.u_uid = fm->root->userid;
    conn->setattr(&fm->cnode, &attr);
    if (conn->u.u_error) {
        int errcode = conn->u.u_error;
        const char *errstr = VenusRetStr(errcode);
        return send_error(tag, errstr, errcode);
    }

    /* send_Rsetattr */
    /* find the filename for debug only */
    char name[NAME_MAX];
    cnode_getname(&fm->cnode, name);
    DEBUG("9pfs: Rsetattr[%x] (%s)\n", tag, name);

    buf = buffer; len = max_msize;
    if (pack_header(&buf, &len, Rsetattr, tag))
    {
       send_error(tag, "Message too long", EMSGSIZE);
       return -1;
    }
    send_response(buffer, max_msize - len);
    return 0;
}


 int plan9server::recv_lopen(unsigned char *buf, size_t len, uint16_t tag)
 {
     uint32_t fid;
     uint32_t lopen_flags;

     if (unpack_le32(&buf, &len, &fid) ||
         unpack_le32(&buf, &len, &lopen_flags))
         return -1;

     DEBUG("9pfs: Tlopen[%x] fid %u, flags 0%o\n", tag, fid, lopen_flags);

     struct fidmap *fm;
     int coda_flags;

     fm = find_fid(fid);
     if (!fm)
         return send_error(tag, "fid unknown or out of range", EBADF);

     if (fm->open_flags)
         return send_error(tag, "file already open for I/O", EIO);

     /* These are the flags that we support so far, the rest should be 0 */
     if (lopen_flags & ~00707503)
         return send_error(tag, "Unsupported lopen flags", ENOTSUP);

     switch (lopen_flags & 00000003) {
        case P9_DOTL_RDONLY:
           coda_flags = C_O_READ;
           break;
        case P9_DOTL_WRONLY:
           coda_flags = C_O_WRITE;
           break;
        case P9_DOTL_RDWR:
           coda_flags = C_O_READ | C_O_WRITE;
           break;
        default:
           return send_error(tag, "Invalid lopen access mode flags", EINVAL);
     }
     if (lopen_flags & P9_DOTL_TRUNC)
         coda_flags |= C_O_TRUNC;

     /* vget and open yield, so we may lose fidmap, but we need to make sure we
      * can close opened cnodes if the fidmap was removed while we yielded. */
     struct venus_cnode cnode = fm->cnode;
     struct plan9_qid qid;

     if (coda_flags & (C_O_WRITE | C_O_TRUNC)) {
         switch (cnode.c_type) {
         case C_VREG:
             break;
         case C_VDIR:
             return send_error(tag, "can't open directory for writing", EISDIR);
         case C_VLNK:
         default:
             return send_error(tag, "file is read only", EINVAL);
         }
     }

     conn->u.u_uid = fm->root->userid;
     if (cnode.c_type == C_VLNK) {
         struct venus_cnode tmp;
         conn->vget(&tmp, &cnode.c_fid, RC_STATUS|RC_DATA);
     }
     else {
         conn->open(&cnode, coda_flags);
     }
     if (conn->u.u_error) {
         int errcode = conn->u.u_error;
         const char *errstr = VenusRetStr(errcode);
         return send_error(tag, errstr, errcode);
     }

     /* open yields, reobtain fidmap reference */
     fm = find_fid(fid);
     if (!fm) {
         if (cnode.c_type != C_VLNK)
             conn->close(&cnode, coda_flags);
         return send_error(tag, "fid unknown or out of range", EBADF);
     }
     fm->open_flags = coda_flags;

     cnode2qid(&cnode, &qid);

     uint32_t iounit = 4096;

     /* send_Rlopen */
     DEBUG("9pfs: Rlopen[%x] qid %x.%x.%lx, iounit %u\n",
           tag, qid.type, qid.version, qid.path, iounit);

     buf = buffer; len = max_msize;
     if (pack_header(&buf, &len, Rlopen, tag) ||
         pack_qid(&buf, &len, &qid) ||
         pack_le32(&buf, &len, iounit))
     {
         send_error(tag, "Message too long", EMSGSIZE);
         return -1;
     }
     send_response(buffer, max_msize - len);
     return 0;
 }


/*
 * Note: CODA ignores the intent bits and the gid information.
 */
int plan9server::recv_lcreate(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t fid;
    char *name;
    uint32_t intent;   // ignored by CODA
    uint32_t mode;
    gid_t gid;         // ignored by CODA

    int errcode = 0;
    const char *errstr = NULL;

    if (unpack_le32(&buf, &len, &fid) ||
        unpack_string(&buf, &len, &name))
        return -1;
    if (unpack_le32(&buf, &len, &intent) ||
        unpack_le32(&buf, &len, &mode) ||
        unpack_le32(&buf, &len, &gid))
        {
            free(name);
            return -1;
        }

    DEBUG("9pfs: Tlcreate[%x] fid %u, name %s, intent %o, mode %o, gid %u\n",
       tag, fid, name, intent, mode, gid);

    struct fidmap *fm;
    struct coda_vattr va = { 0 };
    int excl = 0;
    int flags = C_O_WRITE | C_O_TRUNC;
    va.va_size = 0;
    va.va_mode = mode & 0777;

    uint32_t iounit = 4096;

    fm = find_fid(fid);
    if (!fm) {
        errcode = EBADF;
        errstr = "fid unknown or out of range";
        goto err_out;
    }

    /* we can only create in a directory */
    if (fm->cnode.c_type != C_VDIR) {
        errcode = ENOTDIR;
        errstr = "Not a directory";
        goto err_out;
    }

    struct venus_cnode child;
    struct plan9_qid qid;

    /* Attempt to create a regular file */
    conn->u.u_uid = fm->root->userid;
    conn->create(&fm->cnode, name, &va, excl, flags, &child);

    if (conn->u.u_error) {
        errcode = conn->u.u_error;
        errstr = VenusRetStr(errcode);
        goto err_out;
    }

    conn->open(&child, flags);

    if (conn->u.u_error) {
        errcode = conn->u.u_error;
        errstr = VenusRetStr(errcode);
        goto err_out;
    }

    /* create yields, reobtain fidmap reference */
    fm = find_fid(fid);
    if (!fm) {
        conn->close(&child, flags);
        errcode = EBADF;
        errstr = "fid unknown or out of range";
        goto err_out;
    }
    /* fid is replaced by the newly created file */
    fm->cnode = child;
    fm->open_flags = flags;

    cnode2qid(&child, &qid);

    /* send_Rlcreate */
    DEBUG("9pfs: Rlcreate[%x] qid %x.%x.%lx, iounit %u\n",
       tag, qid.type, qid.version, qid.path, iounit);

    buf = buffer; len = max_msize;
    if (pack_header(&buf, &len, Rlcreate, tag) ||
        pack_qid(&buf, &len, &qid) ||
        pack_le32(&buf, &len, iounit))
        {
            send_error(tag, "Message too long", EMSGSIZE);
            return -1;
        }
    return send_response(buffer, max_msize - len);

err_out:
    ::free(name);
    return send_error(tag, errstr, errcode);
}


/*
 * Note: CODA ignores the gid input parameter.
 */
int plan9server::recv_symlink(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t dfid;
    char *name;
    char *target;
    gid_t gid;      // ignored by CODA

    int errcode = 0;
    const char *errstr = NULL;

    if (unpack_le32(&buf, &len, &dfid) ||
        unpack_string(&buf, &len, &name))
        return -1;
    if (unpack_string(&buf, &len, &target)) {
        ::free(name);
        return -1;
    }
    if (unpack_le32(&buf, &len, &gid)) {
        ::free(name);
        ::free(target);
        return -1;
    }

    DEBUG("9pfs: Tsymlink[%x] dfid %u, name '%s', target '%s', gid %u\n",
          tag, dfid, name, target, gid);

    struct fidmap *fm;
    struct coda_vattr va = { 0 };

    fm = find_fid(dfid);
    if (!fm) {
        errcode = EBADF;
        errstr = "fid unknown or out of range";
        goto err_out;
    }

    /* we can only create in a directory */
    if (fm->cnode.c_type != C_VDIR) {
        errcode = ENOTDIR;
        errstr = "Not a directory";
        goto err_out;
    }

    struct venus_cnode child;
    struct plan9_qid qid;

    conn->u.u_uid = fm->root->userid;
    /* create a symlink and get a cnode for it */
    conn->symlink(&fm->cnode, target, &va, name);
    conn->lookup(&fm->cnode, name, &child,
               CLU_CASE_SENSITIVE | CLU_TRAVERSE_MTPT);

    if (conn->u.u_error) {
        errcode = conn->u.u_error;
        errstr = VenusRetStr(errcode);
        goto err_out;
    }

    /* symlink yields, reobtain fidmap reference */
    fm = find_fid(dfid);
    if (!fm) {
        errcode = EBADF;
        errstr = "fid unknown or out of range";
        goto err_out;
    }
    /* contrarily to what happens with legacy 9P or 9P2000.u, we do not
     * update fid to the newly created file
     */

    cnode2qid(&child, &qid);

    ::free(target);

    /* send_Rsymlink */
    DEBUG("9pfs: Rsymlink[%x] qid %x.%x.%lx\n",
          tag, qid.type, qid.version, qid.path);

    buf = buffer; len = max_msize;
    if (pack_header(&buf, &len, Rcreate, tag) ||
        pack_qid(&buf, &len, &qid))
    {
        send_error(tag, "Message too long", EMSGSIZE);
        return -1;
    }
    return send_response(buffer, max_msize - len);

err_out:
    ::free(name);
    ::free(target);
    return send_error(tag, errstr, errcode);
}


/*
 * Note: CODA ignores the gid input parameter.
 */
int plan9server::recv_mkdir(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t dfid;
    char *name;
    uint32_t mode;
    gid_t gid;      // ignored by CODA

    int errcode = 0;
    const char *errstr = NULL;

    if (unpack_le32(&buf, &len, &dfid) ||
        unpack_string(&buf, &len, &name))
        return -1;
    if (unpack_le32(&buf, &len, &mode) ||
        unpack_le32(&buf, &len, &gid))
    {
        free(name);
        return -1;
    }

    DEBUG("9pfs: Tmkdir[%x] dfid %u, name %s, mode %o, gid %u\n",
          tag, dfid, name, mode, gid);

    struct fidmap *fm;
    struct coda_vattr va = { 0 };

    va.va_size = 0;
    va.va_mode = mode & 0777;

    fm = find_fid(dfid);
    if (!fm) {
        errcode = EBADF;
        errstr = "fid unknown or out of range";
        goto err_out;
    }

    /* we can only create in a directory */
    if (fm->cnode.c_type != C_VDIR) {
        errcode = ENOTDIR;
        errstr = "Not a directory";
        goto err_out;
    }

    struct venus_cnode child;
    struct plan9_qid qid;

    /* Attempt to create the directory */
    conn->u.u_uid = fm->root->userid;
    conn->mkdir(&fm->cnode, name, &va, &child);

    if (conn->u.u_error) {
        errcode = conn->u.u_error;
        errstr = VenusRetStr(errcode);
        goto err_out;
    }

    /* mkdir yields, reobtain fidmap reference */
    fm = find_fid(dfid);
    if (!fm) {
        errcode = EBADF;
        errstr = "fid unknown or out of range";
        goto err_out;
    }
    /* contrarily to what happens with legacy 9P or 9P2000.u, we do not
     * update fid to the newly created directory
     */

    cnode2qid(&child, &qid);

    /* send_Rmkdir */
    DEBUG("9pfs: Rmkdir[%x] qid %x.%x.%lx\n",
          tag, qid.type, qid.version, qid.path);

    buf = buffer; len = max_msize;
    if (pack_header(&buf, &len, Rmkdir, tag) ||
        pack_qid(&buf, &len, &qid))
    {
        send_error(tag, "Message too long", EMSGSIZE);
        return -1;
    }
    return send_response(buffer, max_msize - len);

err_out:
    ::free(name);
    return send_error(tag, errstr, errcode);
}


 int plan9server::recv_readdir(unsigned char *buf, size_t len, uint16_t tag)
 {
     uint32_t fid;
     uint64_t offset;
     uint32_t count;

     if (unpack_le32(&buf, &len, &fid) ||
         unpack_le64(&buf, &len, &offset) ||
         unpack_le32(&buf, &len, &count))
         return -1;

     DEBUG("9pfs: Treaddir[%x] fid %u, offset %lu, count %u\n",
           tag, fid, offset, count);

     struct fidmap *fm = find_fid(fid);
     if (!fm)
         return send_error(tag, "fid unknown or out of range", EBADF);

     if (!(fm->open_flags & C_O_READ))
         return send_error(tag, "Bad file descriptor", EBADF);

     if (fm->cnode.c_type != C_VDIR)
         return send_error(tag, "Not a directory", ENOTDIR);

     /* send_Rreaddir */
     unsigned char *tmpbuf;
     buf = buffer; len = max_msize;
     if (pack_header(&buf, &len, Rreaddir, tag) ||
         get_blob_ref(&buf, &len, &tmpbuf, NULL, 4))
     {
         send_error(tag, "Message too long", EMSGSIZE);
         return -1;
     }

     if (count > len)
         count = len;

     ssize_t n = plan9_read(fm, buf, count, offset);
     if (n < 0) {
         int errcode = conn->u.u_error;
         const char *errstr = VenusRetStr(errcode);
         return send_error(tag, errstr, errcode);
     }

     /* fix up the actual size of the blob, and send */
     size_t tmplen = 4;
     pack_le32(&tmpbuf, &tmplen, n);

     DEBUG("9pfs: Rreaddir[%x] count %ld \n", tag, n);
     len -= n;
     return send_response(buffer, max_msize - len);
 }


int plan9server::recv_readlink(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t fid;

    if (unpack_le32(&buf, &len, &fid))
        return -1;

    DEBUG("9pfs: Treadlink[%x] fid %u\n", tag, fid);

    struct fidmap *fm = find_fid(fid);
    if (!fm)
        return send_error(tag, "fid unknown or out of range", EBADF);
    if (fm->cnode.c_type != C_VLNK)
        return send_error(tag, "not a symbolic link", EINVAL);

    char target[CODA_MAXPATHLEN];

    struct coda_string cstring;
    cstring.cs_buf = target;
    cstring.cs_len = 0;
    cstring.cs_maxlen = CODA_MAXPATHLEN;

    conn->u.u_uid = fm->root->userid;
    conn->readlink(&fm->cnode, &cstring);

    if (conn->u.u_error) {
        int errcode = conn->u.u_error;
        const char *errstr = VenusRetStr(errcode);
        return send_error(tag, errstr, errcode);
    }

    /* send_Rreadlink */
    DEBUG("9pfs: Readlink[%x] target '%s'\n", tag, target);

    buf = buffer; len = max_msize;
    if (pack_header(&buf, &len, Rreadlink, tag) ||
        pack_string(&buf, &len, target))
    {
     send_error(tag, "Message too long", EMSGSIZE);
     return -1;
    }

    return send_response(buffer, max_msize - len);
}


int plan9server::recv_statfs(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t fid;

    if (unpack_le32(&buf, &len, &fid))
        return -1;

    DEBUG("9pfs: Tstatfs[%x] fid %u\n", tag, fid);

    struct fidmap *fm = find_fid(fid);
    if (!fm)
        return send_error(tag, "fid unknown or out of range", EBADF);

    struct coda_statfs c_statfs;

    conn->u.u_uid = fm->root->userid;
    conn->statfs(&c_statfs);

    if (conn->u.u_error) {
        int errcode = conn->u.u_error;
        const char *errstr = VenusRetStr(errcode);
        return send_error(tag, errstr, errcode);
    }

    struct plan9_statfs p9_statfs;
    p9_statfs.blocks = (uint64_t)c_statfs.f_blocks;
    p9_statfs.bfree = (uint64_t)c_statfs.f_bfree;
    p9_statfs.bavail = (uint64_t)c_statfs.f_bavail;
    p9_statfs.files = (uint64_t)c_statfs.f_files;
    p9_statfs.ffree = (uint64_t)c_statfs.f_ffree;
    //taken from kernel code:
    p9_statfs.type = V9FS_MAGIC;
    p9_statfs.bsize = 4096;
    p9_statfs.namelen = CODA_MAXNAMLEN;
    //not reported
    p9_statfs.fsid = 0;

    /* send_Rstatfs */
    DEBUG("9pfs: Rstatfs[%x] typ[%u] bsize[%u] blocks[%lu] bfree[%lu] "
                "bavail[%lu] files[%lu] ffree[%lu] fsid[%lu] namelen[%u]\n",
                tag, p9_statfs.type, p9_statfs.bsize, p9_statfs.blocks,
                p9_statfs.bfree, p9_statfs.bavail, p9_statfs.files,
                p9_statfs.ffree, p9_statfs.fsid, p9_statfs.namelen);

    buf = buffer; len = max_msize;
    if (pack_header(&buf, &len, Rstatfs, tag) ||
        pack_statfs(&buf, &len, &p9_statfs))
    {
        send_error(tag, "Message too long", EMSGSIZE);
        return -1;
    }
    return send_response(buffer, max_msize - len);
}


int plan9server::recv_fsync(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t fid;

    if (unpack_le32(&buf, &len, &fid))
        return -1;

    DEBUG("9pfs: Tfsync[%x] fid %u\n", tag, fid);

    struct fidmap *fm = find_fid(fid);
    if (!fm)
        return send_error(tag, "fid unknown or out of range", EBADF);

    /* The current vproc::fsync() implementation is very heavy-handed, i.e. it
     * syncs and flushes everything.
     * Doing nothing seems to better achieve what we actually want here
     * because in case of a crash, the recovery log will still have the writes.
     * (Oct. 2018 -AS)

    conn->u.u_uid = fm->root->userid;
    conn->fsync(&fm->cnode);
    if (conn->u.u_error) {
        int errcode = conn->u.u_error;
        const char *errstr = VenusRetStr(errcode);
        return send_error(tag, errstr, errcode);
    }
    */

    /* send_Rfsync */
    DEBUG("9pfs: Rfsync[%x]\n", tag);
    buf = buffer; len = max_msize;
    int rc = pack_header(&buf, &len, Rfsync, tag);
    assert(rc == 0);
    return send_response(buffer, max_msize - len);
}


int plan9server::recv_unlinkat(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t dirfid;
    char *name;
    uint32_t flags;
    int rc;

    if (unpack_le32(&buf, &len, &dirfid) ||
        unpack_string(&buf, &len, &name))
        return -1;
    if (unpack_le32(&buf, &len, &flags))
    {
        free(name);
        return -1;
    }

    DEBUG("9pfs: Tunlinkat[%x] dirfid %u, name '%s', flags 0x%x\n",
            tag, dirfid, name, flags);

    struct fidmap *dirfm = find_fid(dirfid);
    if (!dirfm)
        return send_error(tag, "dirfid unknown or out of range", EBADF);
    if (dirfm->cnode.c_type != C_VDIR)
        return send_error(tag, "dirfid not a directory", ENOTDIR);

    /* Attempt unlinkat operation */
    conn->u.u_uid = dirfm->root->userid;
    if (flags == P9_DOTL_AT_REMOVEDIR)
        conn->rmdir(&dirfm->cnode, name);   /* remove a directory */
    else
        conn->remove(&dirfm->cnode, name);  /* remove a regular file */

    if (conn->u.u_error)
        goto err_out;

    /* Contrarily to what happens with the Remove operation, if the file name
     * is represented by a fid, that fid is not clunked. */

     ::free(name);

    /* send_Runlinkat */
    DEBUG("9pfs: Runlinkat[%x]\n", tag);

    buf = buffer; len = max_msize;
    rc = pack_header(&buf, &len, Runlinkat, tag);
    assert(rc == 0);
    return send_response(buffer, max_msize - len);

    err_out:
        ::free(name);
        int errcode = conn->u.u_error;
        const char *errstr = VenusRetStr(errcode);
        return send_error(tag, errstr, errcode);
}


int plan9server::recv_link(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t dfid;
    uint32_t src_fid;
    char *name;

    if (unpack_le32(&buf, &len, &dfid) ||
        unpack_le32(&buf, &len, &src_fid) ||
        unpack_string(&buf, &len, &name))
        return -1;

    DEBUG("9pfs: Tlink[%x] dfid %u, src fid %u, name %s\n",
            tag, dfid, src_fid, name);


    /* we can only create in a directory */
    struct fidmap *dfm = find_fid(dfid);
    if (!dfm)
        return send_error(tag, "directory fid unknown or out of range", EBADF);
    if (dfm->cnode.c_type != C_VDIR)
        return send_error(tag, "Not a directory", ENOTDIR);

    struct fidmap *src_fm = find_fid(src_fid);
    if (!src_fm)
        return send_error(tag, "Source fid unknown or out of range", EBADF);

    /* create the hardlink */
    conn->u.u_uid = dfm->root->userid;
    conn->link(&src_fm->cnode, &dfm->cnode, name);

    if (conn->u.u_error) {
        int errcode = conn->u.u_error;
        const char *errstr = VenusRetStr(errcode);
        return send_error(tag, errstr, errcode);
    }

    /* send_Rlink */
    DEBUG("9pfs: Rlink[%x]\n", tag);
    buf = buffer; len = max_msize;
    int rc = pack_header(&buf, &len, Rlink, tag);
    assert(rc == 0);
    return send_response(buffer, max_msize - len);
}


int plan9server::recv_rename(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t fid;
    uint32_t dfid;
    char *name;
    int errcode = 0;
    const char *errstr = NULL;
    int rc;

    if (unpack_le32(&buf, &len, &fid) ||
        unpack_le32(&buf, &len, &dfid) ||
        unpack_string(&buf, &len, &name))
        return -1;

    DEBUG("9pfs: Trename[%x] fid %u, dfid %u, name '%s'\n",
                    tag, fid, dfid, name);

    struct fidmap *fm = find_fid(fid);
    struct fidmap *dfm = find_fid(dfid);

    if (!fm) {
        errcode = EBADF;
        errstr = "fid unknown or out of range";
        goto err_out;
    }

    if (!dfm) {
        errcode = EBADF;
        errstr = "dfid unknown or out of range";
        goto err_out;
    }
    if (dfm->cnode.c_type != C_VDIR){
        errcode = ENOTDIR;
        errstr = "dfid not a directory";
        goto err_out;
    }

    /* get old name */
    char old_name[NAME_MAX];
    cnode_getname(&fm->cnode, old_name);

    /* get the old parent directory cnode */
    struct venus_cnode old_parent;
    if (cnode_getparent(&fm->cnode, &old_parent) < 0) {
        errcode = EINVAL;
        errstr = "tried to rename the mountpoint";
        goto err_out;
    }

    /* attempt rename */
    conn->u.u_uid = fm->root->userid;
    conn->rename(&old_parent, old_name, &dfm->cnode, name);
    if (conn->u.u_error) {
      errcode = conn->u.u_error;
      errstr = VenusRetStr(errcode);
      goto err_out;
    }

    ::free(name);

    /* send_Rrename */
    DEBUG("9pfs: Rrename[%x]\n", tag);

    buf = buffer; len = max_msize;
    rc = pack_header(&buf, &len, Rrename, tag);
    assert(rc == 0);
    return send_response(buffer, max_msize - len);

err_out:
    ::free(name);
    return send_error(tag, errstr, errcode);
}


int plan9server::recv_renameat(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t olddirfid;
    uint32_t newdirfid;
    char *oldname;
    char *newname;
    int errcode = 0;
    const char *errstr = NULL;
    int rc;

    if (unpack_le32(&buf, &len, &olddirfid) ||
        unpack_string(&buf, &len, &oldname))
        return -1;
    if (unpack_le32(&buf, &len, &newdirfid) ||
        unpack_string(&buf, &len, &newname)) {
        ::free(oldname);
        return -1;
    }

    DEBUG("9pfs: Trenameat[%x] olddirfid %u, oldname '%s', newdirfid %u, "
            "newname '%s'\n", tag, olddirfid, oldname, newdirfid, newname);

    struct fidmap *olddirfm = find_fid(olddirfid);
    struct fidmap *newdirfm = find_fid(newdirfid);

    if (!olddirfm) {
        errcode = EBADF;
        errstr = "old dir fid unknown or out of range";
        goto err_out;
    }
    if (!newdirfm) {
        errcode = EBADF;
        errstr = "new dir fid unknown or out of range";
        goto err_out;
    }

    if (olddirfm->cnode.c_type != C_VDIR){
        errcode = ENOTDIR;
        errstr = "old dir fid not a directory";
        goto err_out;
    }
    if (newdirfm->cnode.c_type != C_VDIR){
        errcode = ENOTDIR;
        errstr = "new dir fid not a directory";
        goto err_out;
    }

    /* attempt rename */
    conn->u.u_uid = newdirfm->root->userid;
    conn->rename(&olddirfm->cnode, oldname, &newdirfm->cnode, newname);
    if (conn->u.u_error) {
      errcode = conn->u.u_error;
      errstr = VenusRetStr(errcode);
      goto err_out;
    }

    ::free(oldname);
    ::free(newname);

    /* send_Rrenameat */
    DEBUG("9pfs: Rrenameat[%x]\n", tag);

    buf = buffer; len = max_msize;
    rc = pack_header(&buf, &len, Rrenameat, tag);
    assert(rc == 0);
    return send_response(buffer, max_msize - len);

err_out:
    ::free(oldname);
    ::free(newname);
    return send_error(tag, errstr, errcode);
}


int plan9server::recv_mknod(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t dfid;
    char *name;
    uint32_t mode;
    uint32_t major;
    uint32_t minor;
    gid_t gid;

    if (unpack_le32(&buf, &len, &dfid) ||
        unpack_string(&buf, &len, &name))
        return -1;
    if (unpack_le32(&buf, &len, &mode) ||
        unpack_le32(&buf, &len, &major) ||
        unpack_le32(&buf, &len, &minor) ||
        unpack_le32(&buf, &len, &gid)) {
        ::free(name);
        return -1;
    }

    DEBUG("9pfs: Tmknod[%x] dfid %u, name %s, major %u, minor %u, gid %d\n",
          tag, dfid, name, major, minor, gid);

    ::free(name);

#if 0
    /* send_Rmknod */
    DEBUG("9pfs: Rmknod[%x] qid %x.%x.%lx\n",
          tag, qid->type, qid->version, qid->path);

    buf = buffer; len = max_msize;
    if (pack_header(&buf, &len, Rmknod, tag) ||
        pack_qid(&buf, &len, qid))
    {
        send_error(tag, "Message too long");
        return -1;
    }
    return send_response(buffer, max_msize - len);
#endif
    return send_error(tag, "Operation not supported", ENOTSUP);
}


int plan9server::recv_xattrwalk(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t fid;
    uint32_t newfid;
    char *name;

    if (unpack_le32(&buf, &len, &fid) ||
        unpack_le32(&buf, &len, &newfid) ||
        unpack_string(&buf, &len, &name))
        return -1;

    DEBUG("9pfs: Txattrwalk[%x] fid %u, newfid %u name %s\n",
          tag, fid, newfid, name);

    ::free(name);

#if 0
    /* send_Rxattrwalk */
    DEBUG("9pfs: Rxattrwalk[%x] attr_size %lu\n", tag, attr_size);

    buf = buffer; len = max_msize;
    if (pack_header(&buf, &len, Rxattrwalk, tag) ||
        pack_len64(&buf, &len, attr_size))
    {
        send_error(tag, "Message too long");
        return -1;
    }
    return send_response(buffer, max_msize - len);
#endif
    return send_error(tag, "Operation not supported", ENOTSUP);
}


int plan9server::recv_xattrcreate(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t fid;
    char *name;
    uint64_t attr_size;
    uint32_t flags;

    if (unpack_le32(&buf, &len, &fid) ||
        unpack_string(&buf, &len, &name))
        return -1;
    if (unpack_le64(&buf, &len, &attr_size) ||
        unpack_le32(&buf, &len, &flags)) {
        ::free(name);
        return -1;
    }

    DEBUG("9pfs: Txattrcreate[%x] fid %u, name %s, attr_size %lu, flags %x\n",
          tag, fid, name, attr_size, flags);

    ::free(name);

#if 0
    /* send_Rxattrcreate */
    DEBUG("9pfs: Rxattrcreate[%x] \n", tag);

    buf = buffer; len = max_msize;
    if (pack_header(&buf, &len, Rxattrwalk, tag))
    {
        send_error(tag, "Message too long");
        return -1;
    }
    return send_response(buffer, max_msize - len);
#endif
    return send_error(tag, "Operation not supported", ENOTSUP);
}


int plan9server::recv_lock(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t fid;
    uint8_t type;
    uint32_t flags;
    uint64_t start;
    uint64_t length;
    uint32_t proc_id;
    char *client_id;

    if (unpack_le32(&buf, &len, &fid) ||
        unpack_le8(&buf, &len, &type) ||
        unpack_le32(&buf, &len, &flags) ||
        unpack_le64(&buf, &len, &start) ||
        unpack_le64(&buf, &len, &length) ||
        unpack_le32(&buf, &len, &proc_id) ||
        unpack_string(&buf, &len, &client_id))
        return -1;

    DEBUG("9pfs: Tlock[%x] fid %u  type %x  flags %x  start %lu  length %lu  "
                 "proc_id %d  client_id %s\n",
          tag, fid, type, flags, start, length, proc_id, client_id);

    ::free(client_id);

#if 0
    /* send_Rlock */
    DEBUG("9pfs: Rlock[%x] status %u\n", tag, status);

    buf = buffer; len = max_msize;
    if (pack_header(&buf, &len, Rlock, tag) ||
        pack_len8(&buf, &len, status))
    {
        send_error(tag, "Message too long");
        return -1;
    }
    return send_response(buffer, max_msize - len);
#endif
    return send_error(tag, "Operation not supported", ENOTSUP);
}


int plan9server::recv_getlock(unsigned char *buf, size_t len, uint16_t tag)
{
    uint32_t fid;
    uint8_t type;
    uint64_t start;
    uint64_t length;
    uint32_t proc_id;
    char *client_id;

    if (unpack_le32(&buf, &len, &fid) ||
        unpack_le8(&buf, &len, &type) ||
        unpack_le64(&buf, &len, &start) ||
        unpack_le64(&buf, &len, &length) ||
        unpack_le32(&buf, &len, &proc_id) ||
        unpack_string(&buf, &len, &client_id))
        return -1;

    DEBUG("9pfs: Tlock[%x] fid %u  type %x  start %lu  length %lu  "
                 "proc_id %d  client_id %s\n",
          tag, fid, type, start, length, proc_id, client_id);

    ::free(client_id);

#if 0
    /* send_Rgetlock */
    DEBUG("9pfs: Rgetlock[%x] type %x  start %lu  length %lu  proc_id %d  "
            "client_id %s\n", tag, type, start, length, proc_id, client_id);

    buf = buffer; len = max_msize;
    if (pack_header(&buf, &len, Rgetlock, tag) ||
        pack_len8(&buf, &len, type) ||
        pack_len64(&buf, &len, start) ||
        pack_len64(&buf, &len, length) ||
        pack_len32(&buf, &len, proc_id) ||
        pack_string(&buf, &len, client_id))
    {
        send_error(tag, "Message too long");
        return -1;
    }
    return send_response(buffer, max_msize - len);
#endif
    return send_error(tag, "Operation not supported", ENOTSUP);
}


/*
 * fidmap helper functions
 */

/*
* Given a cnode, returns its actual link count.
*/
int plan9server::cnode_linkcount(struct venus_cnode *cnode, uint64_t *linkcount)
{
fsobj *f = FSDB->Find(&cnode->c_fid);
if (f == NULL) return -EBADF;         /* Venus fid not found */
*linkcount = (uint64_t)f->stat.LinkCount;
return 0;
}


/*
 * Obtains the file or directory name given a cnode, and places it in the
 * location pointed to by name.
 * Coda's getattr doesn't return the path component because it supports
 * hardlinks so there may be multiple valid names for the same file.
 * 9pfs doesn't handle hardlinks so each file can maintain a unique name.
 * Coda does track the last name used to lookup the object, so return that.
 */
int plan9server::cnode_getname(struct venus_cnode *cnode, char *name)
{
  char buf[NAME_MAX] = "???";
  fsobj *f = FSDB->Find(&cnode->c_fid);
  if (f) f->GetPath(buf, PATH_COMPONENT);
  strcpy(name,buf);
  return 0;
}


/*
 * Given a cnode, constructs the parent directory cnode in the cnode struct
 * pointed to by parent.
 */
int plan9server::cnode_getparent(struct venus_cnode *cnode,
                              struct venus_cnode *parent)
{
  fsobj *f = FSDB->Find(&cnode->c_fid);
  if (f == NULL) return -EBADF;         /* Venus fid not found */
  if (f->IsMtPt()) return -EINVAL;      /* cnode was the mount point */
  MAKE_CNODE2(*parent, f->pfid, C_VDIR);
  return 0;
}

/*
 * Downcall to replace temporary Venus Fid once a server Fid has been assigned.
 * Replacement must happen for every entry in the 9p fidmap that contains the
 * temporary Venus Fid.
 */
int plan9server::fidmap_replace_cfid(VenusFid * OldFid, VenusFid * NewFid)
{
  dlist_iterator next(fids);
  dlink *cur;

  DEBUG("\n9pfs: Downcall received to replace c_fid %s with %s. Done for 9P fids: ", FID_(OldFid), FID_(NewFid));

  while ((cur = next()))
  {
      struct fidmap *fm = strbase(struct fidmap, cur, link);
      if (FID_EQ(&fm->cnode.c_fid, OldFid)) {
          fm->cnode.c_fid = *NewFid;
          DEBUG("%u ", fm->fid);
        }
  }
  DEBUG("\n");
  return 0;
}


struct fidmap *plan9server::find_fid(uint32_t fid)
{
    dlist_iterator next(fids);
    dlink *cur;

    while ((cur = next()))
    {
        struct fidmap *fm = strbase(struct fidmap, cur, link);
        if (fm->fid == fid)
            return fm;
    }
    return NULL;
}


struct fidmap *plan9server::add_fid(uint32_t fid, struct venus_cnode *cnode,
                                    struct attachment *root)
{
    struct fidmap *fm = new struct fidmap;
    if (!fm) return NULL;

    root->refcount++;
    fm->fid = fid;
    fm->cnode = *cnode;
    fm->open_flags = 0;
    fm->root = root;
    fids.prepend(&fm->link);

    return fm;
}

int plan9server::del_fid(uint32_t fid)
{
    dlist_iterator next(fids);
    dlink *cur = next();

    while (cur)
    {
        struct fidmap *fm = strbase(fidmap, cur, link);
        cur = next();

        if (fid != P9_NOFID && fid != fm->fid)
            continue;

        fids.remove(&fm->link);
        if (fm->open_flags && fm->cnode.c_type != C_VLNK) {
            conn->u.u_uid = fm->root->userid;
            conn->close(&fm->cnode, fm->open_flags);
        }

        if (--fm->root->refcount == 0) {
            ::free((void *)fm->root->uname);
            ::free((void *)fm->root->aname);
            delete fm->root;
        }
        delete fm;

        if (fid != P9_NOFID)
            return 0;
    }
    return (fid == P9_NOFID) ? 0 : -1;
}
