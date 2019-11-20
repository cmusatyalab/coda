/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2019 Carnegie Mellon University
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

#include <sys/types.h>
#include <sys/stat.h>

#ifdef __cplusplus
}
#endif

#include "venus.private.h"

static void ValidateDir(const char *dir, uid_t owner, mode_t mode)
{
    int code = 0;
    struct stat tstat;

    // Ensure that directory exists...
    code = ::stat(dir, &tstat);
    if (code < 0 || !S_ISDIR(tstat.st_mode)) {
        if (code == 0)
            CODA_ASSERT(::unlink(dir) == 0);
        CODA_ASSERT(::mkdir(dir, 0755) == 0);
        ::stat(dir, &tstat);
    }

    // ...and it has the correct attributes.
    if (tstat.st_uid != owner || tstat.st_gid != V_GID)
        ::chown(dir, owner, V_GID);

    if ((tstat.st_mode & ~S_IFMT) != mode)
        ::chmod(dir, mode);
}

void MakeUserSpoolDir(char *usd, uid_t owner)
{
    static const char *SpoolDir = GetVenusConf().get_value("checkpointdir");
    // Ensure that the spool directory exists...
    ValidateDir(SpoolDir, V_UID, 0755);

    // Ensure that user's spool (sub-)directory exists...
    sprintf(usd, "%s/%d", SpoolDir, owner);
    ValidateDir(usd, owner, 0700);
}
