#ifndef _EXTRA_FILE_H
#define _EXTRA_FILE_H

#include "i586-pc-cygwin32/include/sys/file.h"

#ifndef __FLOCK
#define __FLOCK
/* Operations for the `flock' call.  */
#define LOCK_SH       1    /* Shared lock.  */
#define LOCK_EX       2    /* Exclusive lock.  */
#define LOCK_UN       8    /* Unlock.  */

/* Can be OR'd in to one of the above.  */
#define LOCK_NB       4    /* Don't block when locking.  */

static inline int flock (int __fd, int __operation) {
    return 0;
}
#endif
#endif
