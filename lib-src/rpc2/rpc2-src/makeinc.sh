#!/bin/sh

echo "Generating ../include/rpc2/errors.h"
( cat << EOF
/* This file was generated from errordb.txt at $(date) */
/* It defines values for missing or RPC2/Coda specific errnos */

/* RPC2 converts errno values to it's own platform independent numbering
 * - Errors returned by RPC's are platform independent.
 * - Accomodate Coda specific errors (like VSALVAGE) as well as system errors.
 * - Quick and easy translation of errors:
 *   a) from RPC2/Coda to system errors (typically for clients)
 *   b) from system to RPC2/Coda errors (typically for servers)
 *   c) provide a "perror" like function.
 * - Coda servers should only return errors which all of the client
 *   platforms can handle.
 * - If errors arrive on certain clients/servers and are not recognized
 *   a log message is printed and a default error code (4711) is returned.
 */

#ifndef _ERRORS_H_
#define _ERRORS_H_

#include <errno.h>

/* Similar to perror but also knows about locally undefined errno values */
const char *cerror(int err);

/* Offset for undefined errors to avoid collision with existing errno values */
#define RPC2_ERRBASE 500

/* These are strange, they were defined in errordb.txt but their network
 * representation was set to the errno define and as such they would not be
 * correctly transferred over the wire. I guess it is better to define them
 * as true aliases so they are sent correctly across the wire */
#define VREADONLY	EROFS	/* Attempt to write to a read-only volume */
#define VDISKFULL	ENOSPC	/* Partition is full */
#define EWOULDBLOCK	EAGAIN	/* Operation would block */

EOF
sed 's/^#define \([^\t]*\)[\t]*\([^t]*\)\t\/\* \(.*\) \*\/$/#ifndef \1\n#define \1 (RPC2_ERRBASE+\2) \/* \3 *\/\n#endif/' < errordb.txt
echo
echo "#endif /* _ERRORS_H_ */"
) > ../include/rpc2/errors.h

echo "Generating switchc2s.h"
( cat << EOF
/* This file was generated from errordb.txt at $(date) */
/* It translates from on-the-wire RPC2 errors to system errno values */

EOF
sed 's/^#define \([^\t]*\)[\t]*\([^t]*\)\t\/\* \(.*\) \*\/$/  case \2:\tsys_err = \1; break;/' < errordb.txt
) > switchc2s.h

echo "Generating switchs2c.h"
( cat << EOF
/* This file was generated from errordb.txt at $(date) */
/* It translates from system errno values to on-the-wire RPC2 errors */

EOF
sed 's/^#define \([^\t]*\)[\t]*\([^t]*\)\t\/\* \(.*\) \*\/$/  case \1:\trpc2_err = \2; break;/' < errordb.txt
) > switchs2c.h

echo "Generating switchs2e.h"
( cat << EOF
/* This file was generated from errordb.txt at $(date) */
/* It translates from system (and Coda) errno values to error messages */

EOF
sed 's/^#define \([^\t]*\)[\t]*\([^t]*\)\t\/\* \(.*\) \*\/$/  case \1:\ttxt = \"\3\"; break;/' < errordb.txt
) > switchs2e.h

