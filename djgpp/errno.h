/* errno is not a global variable, because that would make using it
   non-reentrant.  Instead, its address is returned by the function
   __errno.  */

#ifndef _CYG_ERRNO_H_
#ifdef __cplusplus
extern "C" {
#endif
#define _CYG_ERRNO_H_

#include "/usr/lib/gcc-lib/i386-go32-msdos/2.7.2.1/include/errno.h"

/* compilation of coda-src/rpc2 failed because this definition is missing */
#ifndef ENOTBLK
#define ENOTBLK 666
#endif

#include <sys/socket.h>

#define ESUCCESS 0

#ifdef __cplusplus
}
#endif
#endif /* _SYS_ERRNO_H */
