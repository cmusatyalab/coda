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
    if (pack_header(&buf, &len, Rerror, tag) ||
        pack_string(&buf, &len, error))
        return -1;
    if (protocol == P9_PROTO_DOTU) {
      if (pack_le32(&buf, &len, (uint32_t)errcode))
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

    if (::strncmp(remote_version, "9P2000.", 7) == 0) {
        //9P2000.L Linux extensions not yet supported: degrade to 9P2000.u
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
    if (protocol == P9_PROTO_DOTU) {
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
    if (protocol == P9_PROTO_DOTU) {
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
    root->uname = uname;
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
    /* fid is replaced by the newly created file/directory/symlink */
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
    if (cnode_getparent(&fm->cnode, &parent_cnode) < 0)
        return send_error(tag, "tried to remove the mountpoint", EBUSY);

    conn->u.u_uid = fm->root->userid;
    if (fm->cnode.c_type == C_VDIR) {
      /* remove a directory */
      conn->rmdir(&parent_cnode, name);
    }
    else {
      /* remove a regular file */
      conn->remove(&parent_cnode, name);
    }
    /* 9p clunks the file, whether the actual server remove succeeded or not */
    del_fid(fid);

    if (conn->u.u_error) {
        int errcode = conn->u.u_error;
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


    /* retrieve the current file attributes from Venus */
    conn->u.u_uid = fm->root->userid;
    conn->getattr(&fm->cnode, &attr);
    if (conn->u.u_error) {
        errcode = conn->u.u_error;
        strerr = VenusRetStr(errcode);
        goto err_out;
    }

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
      conn->u.u_uid = fm->root->userid;
      conn->rename(&parent_cnode, name,&parent_cnode, stat.name);
      if (conn->u.u_error) {
          errcode = conn->u.u_error;
          strerr = VenusRetStr(errcode);
          goto err_out;
      }
      DEBUG("renamed %s to %s\n", name, stat.name);
    }

    /* Update attr with the new stats to be written.
     * If all wstat fields were "don't touch", then according to 9P protocol,
     * the server can interpret this wstat as a request to guarantee that
     * the contents of the associated file are committed to stable storage
     * before the Rwstat message is returned (i.e. "make the state of the
     * file exactly what it claims to be.""), which we do by confirming at
     * least the uid in the setattr() call.
     */

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
     * so we just keep/affirm them as they are:
     *     attr.va_gid
     *     attr.va_atime
     *     attr.va_ctime
     *     attr.va_gen
     *     attr.va_type
     *     attr.va_filerev
     */

    /* attempt setattr */
    conn->u.u_uid = fm->root->userid;
    conn->setattr(&fm->cnode, &attr);
    if (conn->u.u_error) {
        errcode = conn->u.u_error;
        strerr = VenusRetStr(errcode);
        goto err_out;
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
    size_t offset;
    struct venus_cnode parent;
    struct attachment *root;
};

static int filldir(struct DirEntry *de, void *hook)
{
    struct filldir_args *args = (struct filldir_args *)hook;
    if (strcmp(de->name, ".") == 0 || strcmp(de->name, "..") == 0)
        return 0;
    return args->srv->pack_dirent(&args->buf, &args->count, &args->offset,
                                  &args->parent, args->root, de->name);
}

int plan9server::pack_dirent(unsigned char **buf, size_t *len, size_t *offset,
                             struct venus_cnode *parent,
                             struct attachment *root, const char *name)
{
    struct venus_cnode child;
    struct plan9_stat stat;
    int rc;

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
        rc = pack_stat(&scratch, offset, &stat, protocol);
        ::free(stat.name);
        if (rc) {
            /* was this a case of (offset < stat_length)? */
            /* -> i.e. 'bad offset in directory read' */
            return ESPIPE;
        }
        return 0;
    }

    /* and finally we pack until we cannot fit any more entries */
    rc = pack_stat(buf, len, &stat, protocol);
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
  assert(f);
  if (f->IsMtPt()) return -1;  /* cnode was the mount point */
  MAKE_CNODE2(*parent, f->pfid, C_VDIR);
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
