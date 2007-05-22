/* This file was generated from errordb.txt at Mon May 21 22:31:27 EDT 2007 */
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

/* Translations for common UNIX errno values */
#ifndef EPERM
#define EPERM (RPC2_ERRBASE+1) /* Operation not permitted */
#endif
#ifndef ENOENT
#define ENOENT (RPC2_ERRBASE+2) /* No such file or directory */
#endif
#ifndef ESRCH
#define ESRCH (RPC2_ERRBASE+3) /* No such process */
#endif
#ifndef EINTR
#define EINTR (RPC2_ERRBASE+4) /* Interrupted system call */
#endif
#ifndef EIO
#define EIO (RPC2_ERRBASE+5) /* Input/output error */
#endif
#ifndef ENXIO
#define ENXIO (RPC2_ERRBASE+6) /* Device not configured */
#endif
#ifndef E2BIG
#define E2BIG (RPC2_ERRBASE+7) /* Argument list too long */
#endif
#ifndef ENOEXEC
#define ENOEXEC (RPC2_ERRBASE+8) /* Exec format error */
#endif
#ifndef EBADF
#define EBADF (RPC2_ERRBASE+9) /* Bad file descriptor */
#endif
#ifndef ECHILD
#define ECHILD (RPC2_ERRBASE+10) /* No child processes */
#endif
#ifndef EDEADLK
#define EDEADLK (RPC2_ERRBASE+11) /* Resource deadlock avoided */
#endif
#ifndef ENOMEM
#define ENOMEM (RPC2_ERRBASE+12) /* Cannot allocate memory */
#endif
#ifndef EACCES
#define EACCES (RPC2_ERRBASE+13) /* Permission denied */
#endif
#ifndef EFAULT
#define EFAULT (RPC2_ERRBASE+14) /* Bad address */
#endif
#ifndef ENOTBLK
#define ENOTBLK (RPC2_ERRBASE+15) /* Not a block device */
#endif
#ifndef EBUSY
#define EBUSY (RPC2_ERRBASE+16) /* Device busy */
#endif
#ifndef EEXIST
#define EEXIST (RPC2_ERRBASE+17) /* File exists */
#endif
#ifndef EXDEV
#define EXDEV (RPC2_ERRBASE+18) /* Cross-device link */
#endif
#ifndef ENODEV
#define ENODEV (RPC2_ERRBASE+19) /* Operation not supported by device */
#endif
#ifndef ENOTDIR
#define ENOTDIR (RPC2_ERRBASE+20) /* Not a directory */
#endif
#ifndef EISDIR
#define EISDIR (RPC2_ERRBASE+21) /* Is a directory */
#endif
#ifndef EINVAL
#define EINVAL (RPC2_ERRBASE+22) /* Invalid argument */
#endif
#ifndef ENFILE
#define ENFILE (RPC2_ERRBASE+23) /* Too many open files in system */
#endif
#ifndef EMFILE
#define EMFILE (RPC2_ERRBASE+24) /* Too many open files */
#endif
#ifndef ENOTTY
#define ENOTTY (RPC2_ERRBASE+25) /* Inappropriate ioctl for device */
#endif
#ifndef ETXTBSY
#define ETXTBSY (RPC2_ERRBASE+26) /* Text file busy */
#endif
#ifndef EFBIG
#define EFBIG (RPC2_ERRBASE+27) /* File too large */
#endif
#ifndef ENOSPC
#define ENOSPC (RPC2_ERRBASE+28) /* No space left on device */
#endif
#ifndef ESPIPE
#define ESPIPE (RPC2_ERRBASE+29) /* Illegal seek */
#endif
#ifndef EROFS
#define EROFS (RPC2_ERRBASE+30) /* Read-only file system */
#endif
#ifndef EMLINK
#define EMLINK (RPC2_ERRBASE+31) /* Too many links */
#endif
#ifndef EPIPE
#define EPIPE (RPC2_ERRBASE+32) /* Broken pipe */
#endif
#ifndef EDOM
#define EDOM (RPC2_ERRBASE+33) /* Numerical argument out of domain */
#endif
#ifndef ERANGE
#define ERANGE (RPC2_ERRBASE+34) /* Result too large */
#endif
#ifndef EAGAIN
#define EAGAIN (RPC2_ERRBASE+35) /* Resource temporarily unavailable */
#endif
#ifndef EALREADY
#define EALREADY (RPC2_ERRBASE+37) /* Operation already in progress */
#endif
#ifndef ENOSYS
#define ENOSYS (RPC2_ERRBASE+38) /* Function not implemented */
#endif
#ifndef EPROTONOSUPPORT
#define EPROTONOSUPPORT (RPC2_ERRBASE+43) /* Protocol not supported */
#endif
#ifndef EOPNOTSUPP
#define EOPNOTSUPP (RPC2_ERRBASE+45) /* Operation not supported */
#endif
#ifndef ENOBUFS
#define ENOBUFS (RPC2_ERRBASE+55) /* No buffer space available */
#endif
#ifndef ENOTCONN
#define ENOTCONN (RPC2_ERRBASE+57) /* Socket is not connected */
#endif
#ifndef ESHUTDOWN
#define ESHUTDOWN (RPC2_ERRBASE+58) /* Can't send after socket shutdown */
#endif
#ifndef ETOOMANYREFS
#define ETOOMANYREFS (RPC2_ERRBASE+59) /* Too many references: can't splice */
#endif
#ifndef ETIMEDOUT
#define ETIMEDOUT (RPC2_ERRBASE+60) /* Operation timed out */
#endif
#ifndef ELOOP
#define ELOOP (RPC2_ERRBASE+62) /* Too many levels of symbolic links */
#endif
#ifndef ENAMETOOLONG
#define ENAMETOOLONG (RPC2_ERRBASE+63) /* File name too long */
#endif
#ifndef ENOTEMPTY
#define ENOTEMPTY (RPC2_ERRBASE+66) /* Directory not empty */
#endif
#ifndef EDQUOT
#define EDQUOT (RPC2_ERRBASE+69) /* Disc quota exceeded */
#endif

/* Coda client <-> server specific errors */
#ifndef VSALVAGE
#define VSALVAGE (RPC2_ERRBASE+101) /* Volume needs salvage */
#endif
#ifndef VNOVNODE
#define VNOVNODE (RPC2_ERRBASE+102) /* Bad vnode number quoted */
#endif
#ifndef VNOVOL
#define VNOVOL (RPC2_ERRBASE+103) /* Volume does not exist or not online */
#endif
#ifndef VVOLEXISTS
#define VVOLEXISTS (RPC2_ERRBASE+104) /* Volume already exists */
#endif
#ifndef VNOSERVICE
#define VNOSERVICE (RPC2_ERRBASE+105) /* Volume is not in service */
#endif
#ifndef VOFFLINE
#define VOFFLINE (RPC2_ERRBASE+106) /* Volume is off line */
#endif
#ifndef VONLINE
#define VONLINE (RPC2_ERRBASE+107) /* Volume is already on line */
#endif
#ifndef VBUSY
#define VBUSY (RPC2_ERRBASE+110) /* Volume temporarily unavailable */
#endif
#ifndef VMOVED
#define VMOVED (RPC2_ERRBASE+111) /* Volume has moved */
#endif
#ifndef VNOSERVER
#define VNOSERVER (RPC2_ERRBASE+112) /* File server lwp is not running */
#endif
#ifndef VLOGSTALE
#define VLOGSTALE (RPC2_ERRBASE+113) /* CML head previously reintegrated */
#endif
#ifndef EVOLUME
#define EVOLUME (RPC2_ERRBASE+158) /* Volume error */
#endif
#ifndef EINCOMPATIBLE
#define EINCOMPATIBLE (RPC2_ERRBASE+198) /* Version vectors are incompatible */
#endif
#ifndef EINCONS
#define EINCONS (RPC2_ERRBASE+199) /* File is inconsistent */
#endif
#ifndef VFAIL
#define VFAIL (RPC2_ERRBASE+200) /* Unknown Coda error */
#endif

/* Advice monitor related errors, we should really not use these anymore */
// #define CAEFAIL		300	/* Unknown error related to the Advice Monitor */
// #define CAEVERSIONSKEW	301	/* Version skew between Venus and the Advice Monitor */
#ifndef CAENOSUCHUSER
#define CAENOSUCHUSER (RPC2_ERRBASE+302) /* Advice Monitor attempted to connect on behalf of an unknown user */
#endif
#ifndef CAENOTVALID
#define CAENOTVALID (RPC2_ERRBASE+303) /* Advice Monitor attempted to test liveness of an invalid connection */
#endif
#ifndef CAENOASR
#define CAENOASR (RPC2_ERRBASE+304) /* Advice Monitor returned the result of an ASR, but not ASR is pending */
#endif
#ifndef CAEUNEXPECTEDASR
#define CAEUNEXPECTEDASR (RPC2_ERRBASE+305) /* Advice Monitor returned the result of the wrong ASR */
#endif
// #define CAEASRINPROGRESS	306	/* Venus requested an ASR while another one is in progress */
#ifndef CAEADVICEPENDING
#define CAEADVICEPENDING (RPC2_ERRBASE+307) /* Advice Monitor requested a new connection while a request is pending */
#endif
#ifndef CAENOSERVERS
#define CAENOSERVERS (RPC2_ERRBASE+308) /* No servers are known to Venus */
#endif


#endif /* _ERRORS_H_ */
