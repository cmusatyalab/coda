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

/*
 *
 * Implementation of the Venus File-System Object (fso) Directory subsystem.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <errno.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <coda.h>

/* interfaces */
#include <vice.h>

#ifdef __cplusplus
}
#endif

/* from dir */

#include "fso.h"
#include "local.h"
#include "venusrecov.h"
#include "venus.private.h"

/* *****  FSO Directory Interface  ***** */

/* Need not be called from within transaction. */
void fsobj::dir_Rebuild()
{
    if (!HAVEALLDATA(this)) {
        print(GetLogFile());
        CHOKE("fsobj::dir_Rebuild: no data");
    }

    if (!DH_DirOK(&data.dir->dh)) {
        LOG(0, ("WARNING: Corrupt directory for %s\n", FID_(&fid)));
        DH_Print(&data.dir->dh, stdout);
    }

    DH_Convert(&data.dir->dh, data.dir->udcf->Name(), fid.Volume, fid.Realm);

    data.dir->udcfvalid = 1;
}

/* TRANS */
void fsobj::dir_Create(const char *Name, VenusFid *Fid)
{
    if (!HAVEALLDATA(this)) {
        print(GetLogFile());
        CHOKE("fsobj::dir_Create: (%s, %s) no data", Name, FID_(Fid));
    }

    int rc, oldlength = dir_Length();

    /* XXX we need to strdup, because the dirops don't accept const char */
    char *entry = strdup(Name);
    CODA_ASSERT(entry);
    rc = DH_Create(&data.dir->dh, entry, MakeViceFid(Fid));
    if (rc) {
        print(GetLogFile());
        CHOKE("fsobj::dir_Create: (%s, %s) Create failed %d!", Name, FID_(Fid),
              rc);
    }
    free(entry);

    data.dir->udcfvalid = 0;

    int newlength    = dir_Length();
    int delta_blocks = NBLOCKS(newlength) - NBLOCKS(oldlength);
    UpdateCacheStats(&FSDB->DirDataStats, CREATE, delta_blocks);
}

int fsobj::dir_Length()
{
    if (!HAVEALLDATA(this)) {
        print(GetLogFile());
        CHOKE("fsobj::dir_Length: no data");
    }

    return (DH_Length(&data.dir->dh));
}

/* This is a hack to avoid an endless loop in GNU rm -rf.
 * When the number of directory entries exceeds some number it seeks back to 0
 * to unlink any entries that were added during the remove. However because of
 * Coda's session semantics the directory contents isn't changed until the
 * filedescriptor is closed and reopened. This function will clear the fileno
 * and namelen fields for the unlinked directory entry in the container file */
static void clear_dir_container_entry(CacheFile *cf, const char *name)
{
    char buf[4096];
    struct venus_dirent *vdir;
    int fd, len = strlen(name);
    int offset = 0, n = 0;

    if (!cf)
        return;
    fd = cf->Open(O_RDWR);

    while (1) {
        if (offset >= n) {
            n = read(fd, buf, sizeof(buf));
            if (n <= 0)
                break;
            offset = 0;
        }

        vdir = (struct venus_dirent *)&buf[offset];

        if (vdir->d_namlen == len && memcmp(vdir->d_name, name, len) == 0) {
            /* found matching entry, write it back with zero'd fileno/namlen */
            vdir->d_fileno = vdir->d_namlen = 0;
            lseek(fd, offset - n, SEEK_CUR);
            write(fd, vdir, sizeof(*vdir) - sizeof(vdir->d_name));
            break;
        }
        offset += vdir->d_reclen;
    }

    cf->Close(fd);
}

/* TRANS */
void fsobj::dir_Delete(const char *Name)
{
    if (!HAVEALLDATA(this)) {
        print(GetLogFile());
        CHOKE("fsobj::dir_Delete: (%s) no data", Name);
    }

    int oldlength = dir_Length();

    char *entry = strdup(Name);
    CODA_ASSERT(entry);
    if (DH_Delete(&data.dir->dh, entry)) {
        print(GetLogFile());
        CHOKE("fsobj::dir_Delete: (%s) Delete failed!", Name);
    }
    free(entry);

    clear_dir_container_entry(data.dir->udcf, Name);
    data.dir->udcfvalid = 0;

    int newlength    = dir_Length();
    int delta_blocks = NBLOCKS(newlength) - NBLOCKS(oldlength);
    UpdateCacheStats(&FSDB->DirDataStats, REMOVE, delta_blocks);
}

/* TRANS */
void fsobj::dir_MakeDir()
{
    FSO_ASSERT(this, !HAVEDATA(this));

    data.dir = (VenusDirData *)rvmlib_rec_malloc((int)sizeof(VenusDirData));
    FSO_ASSERT(this, data.dir);
    RVMLIB_REC_OBJECT(*data.dir);
    memset((void *)data.dir, 0, (int)sizeof(VenusDirData));
    DH_Init(&data.dir->dh);

    if (DH_MakeDir(&data.dir->dh, MakeViceFid(&fid), MakeViceFid(&pfid)) != 0) {
        print(GetLogFile());
        CHOKE("fsobj::dir_MakeDir: MakeDir failed!");
    }

    data.dir->udcfvalid = 0;

    stat.Length = dir_Length();
    UpdateCacheStats(&FSDB->DirDataStats, CREATE, BLOCKS(this));
}

int fsobj::dir_Lookup(const char *Name, VenusFid *Fid, int flags)
{
    if (!HAVEALLDATA(this)) {
        print(GetLogFile());
        CHOKE("fsobj::dir_Lookup: (%s) no data", Name);
    }

    int code = DH_Lookup(&data.dir->dh, Name, MakeViceFid(Fid), flags);
    if (code != 0)
        return (code);

    Fid->Realm  = fid.Realm;
    Fid->Volume = fid.Volume;
    return (0);
}

/* Name buffer had better be CODA_MAXNAMLEN bytes or more! */
int fsobj::dir_LookupByFid(char *Name, VenusFid *Fid)
{
    if (!HAVEALLDATA(this)) {
        print(GetLogFile());
        CHOKE("fsobj::dir_LookupByFid: %s no data", FID_(Fid));
    }

    return DH_LookupByFid(&data.dir->dh, Name, MakeViceFid(Fid));
}

/* return 1 if directory is empty, 0 otherwise */
int fsobj::dir_IsEmpty()
{
    if (!HAVEALLDATA(this)) {
        print(GetLogFile());
        CHOKE("fsobj::dir_IsEmpty: no data");
    }

    return (DH_IsEmpty(&data.dir->dh));
}

/* determine if target_fid is the parent of this */
int fsobj::dir_IsParent(VenusFid *target_fid)
{
    if (!HAVEALLDATA(this)) {
        print(GetLogFile());
        CHOKE("fsobj::dir_IsParent: (%s) no data", FID_(target_fid));
    }

    /* Volumes must be the same. */
    if (!FID_VolEQ(&fid, target_fid))
        return (0);

    /* Don't match "." or "..". */
    if (FID_EQ(target_fid, &fid) || FID_EQ(target_fid, &pfid))
        return (0);

    /* Lookup the target object. */
    char Name[MAXPATHLEN];

    return (!DH_LookupByFid(&data.dir->dh, Name, MakeViceFid(target_fid)));
}

/* local-repair modification */
/* TRANS */
void fsobj::dir_TranslateFid(VenusFid *OldFid, VenusFid *NewFid)
{
    char *Name = NULL;

    if (!HAVEALLDATA(this)) {
        print(GetLogFile());
        CHOKE("fsobj::dir_TranslateFid: %s -> %s no data", FID_(OldFid),
              FID_(NewFid));
    }

    if ((!FID_VolEQ(&fid, OldFid) && !FID_IsLocalFake(OldFid) &&
         !FID_IsLocalFake(&fid)) ||
        (!FID_VolEQ(&fid, NewFid) && !FID_IsLocalFake(NewFid) &&
         !FID_IsLocalFake(&fid))) {
        print(GetLogFile());
        CHOKE("fsobj::dir_TranslateFid: %s -> %s cross-volume", FID_(OldFid),
              FID_(NewFid));
    }

    if (FID_EQ(OldFid, NewFid))
        return;

    Name = (char *)malloc(CODA_MAXNAMLEN + 1);
    CODA_ASSERT(Name);

    while (!dir_LookupByFid(Name, OldFid)) {
        dir_Delete(Name);
        dir_Create(Name, NewFid);
    }
    free(Name);
}

void fsobj::dir_Print()
{
    if (!HAVEALLDATA(this)) {
        print(GetLogFile());
        CHOKE("fsobj::dir_Print: no data");
    }

    if (GetLogLevel() >= 1000) {
        LOG(1000, ("fsobj::dir_Print: %s, %d, %d\n", data.dir->udcf->Name(),
                   data.dir->udcf->Length(), data.dir->udcfvalid));

        DH_Print(&data.dir->dh, stdout);
    }
}
