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

#*/

#ifndef _GETSECRET_H_
#define _GETSECRET_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <rpc2/rpc2.h>

/* Copy the hash of 'secret' into the RPC2 key */
void HashSecret(unsigned char *secret, int len, RPC2_EncryptionKey key);

/* to avoid having to re-read and re-hash tokens from files... */
struct secret_state {
    time_t mtime;
    off_t  size;
    RPC2_EncryptionKey key;
};
/* Read a tokenfile and return an RPC2 encryption key. */
int GetSecret(char *file, RPC2_EncryptionKey key, struct secret_state *state);

/* Fill the RPC2 key with random values (for session keys) */
void GenerateSecret(RPC2_EncryptionKey key);

#ifdef __cplusplus
}
#endif

#endif /* _GETSECRET_H_ */

