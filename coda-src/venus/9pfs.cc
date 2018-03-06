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

#ifdef __cplusplus
}
#endif

#include "mariner.h"
#include "venus.private.h"
#include "9pfs.h"


#define DEBUG(...) do { fprintf(stderr, __VA_ARGS__); fflush(stderr); } while(0)


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
 * blob in the original buffer so a copy may need to be made. */
static int get_blob_ref(unsigned char **buf, size_t *len,
                        unsigned char **result, size_t size)
{
    if (*len < size) return -1;
    *result = *buf;
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
        get_blob_ref(buf, len, &blob, size))
        return -1;

    *result = strndup((char *)blob, size);
    if (*result == NULL)
        return -1;

    /* Check there is no embedded NULL character in the received string */
    if (strlen(*result) != (size_t)size) {
        free(*result);
        return -1;
    }
    return 0;
}


static int pack_qid(unsigned char **buf, size_t *len, const struct P9_qid *qid)
{
    if (pack_le8(buf, len, qid->type) ||
        pack_le32(buf, len, qid->version) ||
        pack_le64(buf, len, qid->path))
        return -1;
    return 0;
}

static int unpack_qid(unsigned char **buf, size_t *len, struct P9_qid *qid)
{
    if (unpack_le8(buf, len, &qid->type) ||
        unpack_le32(buf, len, &qid->version) ||
        unpack_le64(buf, len, &qid->path))
        return -1;
    return 0;
}


static int pack_stat(unsigned char **buf, size_t *len, struct P9_stat *stat)
{
    unsigned char *start_of_stat;
    size_t stashed_length = *len;

    /* get backpointer to beginning of the stat output so we can,
     * - fix up the length information after packing everything.
     * - rollback iff we run out of buffer space. */
    if (get_blob_ref(buf, len, &start_of_stat, 2) ||
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
    {
        *buf = start_of_stat;
        *len = stashed_length;
        return -1;
    }
    size_t tmplen = 2;
    size_t stat_size = stashed_length - *len - 2;
    pack_le16(&start_of_stat, &tmplen, stat_size);
    return 0;
}

static int unpack_stat(unsigned char **buf, size_t *len, struct P9_stat *stat)
{
    size_t stashed_length = *len;
    uint16_t size;
    stat->name = stat->uid = stat->gid = stat->muid = NULL;

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
        unpack_string(buf, len, &stat->muid) ||
        size != (stashed_length - *len - 2))
    {
        free(stat->muid);
        free(stat->gid);
        free(stat->uid);
        free(stat->name);
        return -1;
    }
    return 0;
}

static int pack_header(unsigned char **buf, size_t *len,
                       uint8_t type, uint16_t tag)
{
    uint32_t msglen = P9_MIN_MSGSIZE; /* we will fix this value when sending the message */

    if (pack_le32(buf, len, msglen) ||
        pack_le8(buf, len, type) ||
        pack_le16(buf, len, tag))
        return -1;
    return 0;
}


int mariner::handle_9pfs_request(size_t read)
{
    unsigned char *buf = (unsigned char *)commbuf;
    size_t len = read;

    uint32_t reqlen;
    uint8_t  opcode;
    uint16_t tag;

    if (unpack_le32(&buf, &len, &reqlen) ||
        unpack_le8(&buf, &len, &opcode) ||
        unpack_le16(&buf, &len, &tag))
        return -1;

    DEBUG("9pfs: got request length %u, type %u, tag %x", reqlen, opcode, tag);

    if (reqlen < P9_MIN_MSGSIZE)
        return -1;

    if (reqlen > max_9pfs_msize) {
        send_9pfs_Rerror(tag, "Message too long");
        return -1;
    }

    /* read the rest of the request */
    len = reqlen - read;
    if (read_until_done(&commbuf[read], len) != (ssize_t)len)
        return -1;


    switch (opcode)
    {
    case Tversion: {
        uint32_t msize;
        char *remote_version;
        const char *version;

        if (unpack_le32(&buf, &len, &msize) ||
            unpack_string(&buf, &len, &remote_version))
            return -1;
        DEBUG("9pfs: Tversion msize %d, version %s\n", msize, remote_version);

        max_9pfs_msize = (msize < MWBUFSIZE) ? msize : MWBUFSIZE;

        if (strncmp(remote_version, "9P2000", 6) == 0)
            version = "9P2000";
        else
            version = "unknown";
        free(remote_version);

        /* abort all existing I/O, clunk all fids */
        //conn->del_fids();
        //conn->attach_root = ...

        /* send_Rversion */
        DEBUG("9pfs: Rversion msize %lu, version %s\n", max_9pfs_msize, version);

        buf = (unsigned char *)commbuf; len = max_9pfs_msize;
        if (pack_header(&buf, &len, Rversion, tag) ||
            pack_le32(&buf, &len, max_9pfs_msize) ||
            pack_string(&buf, &len, version))
        {
            send_9pfs_Rerror(tag, "Message too long");
            return -1;
        }
        break;
    }

    default:
        return send_9pfs_Rerror(tag, "Operation not supported");
    }
    return send_9pfs_response(max_9pfs_msize - len);
}


int mariner::send_9pfs_Rerror(uint16_t tag, const char *error)
{
    unsigned char *buf = (unsigned char *)commbuf;
    size_t len = max_9pfs_msize;

    if (pack_header(&buf, &len, Rerror, tag) ||
        pack_string(&buf, &len, error))
        return -1;

    return send_9pfs_response(max_9pfs_msize - len);
}


int mariner::send_9pfs_response(size_t msglen)
{
    /* fix up response length */
    unsigned char *tmpbuf = (unsigned char *)commbuf;
    size_t tmplen = 4;
    pack_le32(&tmpbuf, &tmplen, msglen);

    /* send response */
    if (write_until_done(commbuf, msglen) != (ssize_t)msglen)
        return -1;
    return 0;
}
