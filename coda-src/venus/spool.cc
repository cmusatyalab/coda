
#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/stat.h>


#ifdef __cplusplus
}
#endif __cplusplus

#include "venus.private.h"

extern char *SpoolDir;


void SpoolInit() {
    int code = 0;
    struct stat tstat;

    // Ensure that top-level spool directory exists... 
    code = ::stat(SpoolDir, &tstat);
    if (code < 0 || (tstat.st_mode & S_IFMT) != S_IFDIR) {
        if (code == 0)
            CODA_ASSERT(::unlink(SpoolDir) == 0);
        CODA_ASSERT(::mkdir(SpoolDir, 0755) == 0);
    }

    // ...and it has the correct attributes. 
    CODA_ASSERT(::chown(SpoolDir, V_UID, V_GID) == 0);
    CODA_ASSERT(::chmod(SpoolDir, 0755) == 0);
}

void MakeUserSpoolDir(char *usd, vuid_t owner) {
    int code = 0;
    struct stat tstat;

    // Ensure that user's spool (sub-)directory exists...
    sprintf(usd, "%s/%d", SpoolDir, owner);
    code = ::stat(usd, &tstat);
    if (code < 0 || (tstat.st_mode & S_IFMT) != S_IFDIR) {
	if (code == 0)
	    CODA_ASSERT(::unlink(usd) == 0);
	CODA_ASSERT(::mkdir(usd, 0755) == 0);
    }

    // ...and has the correct attributes.
    CODA_ASSERT(::chown(usd, owner, V_GID) == 0);
    CODA_ASSERT(::chmod(usd, 0700) == 0);
}
