/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
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
		print(logFile); 
		CHOKE("fsobj::dir_Rebuild: no data"); 
	}

	if ( ! DH_DirOK(&data.dir->dh)) {
		LOG(0, ("WARNING: Corrupt directory for %s\n", FID_(&fid)));
		DH_Print(&data.dir->dh, stdout);
	}

	DH_Convert(&data.dir->dh, data.dir->udcf->Name(), fid.Volume, fid.Realm);

	data.dir->udcfvalid = 1;
}


/* TRANS */
void fsobj::dir_Create(char *Name, VenusFid *Fid) 
{
	if (!HAVEALLDATA(this)) { 
		print(logFile); 
		CHOKE("fsobj::dir_Create: (%s, %s) no data", Name, FID_(Fid)); 
	}

	int oldlength = dir_Length();

	if (DH_Create(&data.dir->dh, Name, MakeViceFid(Fid)) != 0) { 
		print(logFile); CHOKE("fsobj::dir_Create: (%s, %s) Create failed!", 
				      Name, FID_(Fid)); 
	}

	data.dir->udcfvalid = 0;

	int newlength = dir_Length();
	int delta_blocks = NBLOCKS(newlength) - NBLOCKS(oldlength);
	UpdateCacheStats(&FSDB->DirDataStats, CREATE, delta_blocks);
}


int fsobj::dir_Length() 
{
	if (!HAVEALLDATA(this)) { 
		print(logFile); 
		CHOKE("fsobj::dir_Length: no data"); 
	}

	return(DH_Length(&data.dir->dh));
}


/* TRANS */
void fsobj::dir_Delete(char *Name) 
{
	if (!HAVEALLDATA(this)) { 
		print(logFile); 
		CHOKE("fsobj::dir_Delete: (%s) no data", Name); 
	}

	int oldlength = dir_Length();

	if (DH_Delete(&data.dir->dh, Name) != 0) { 
		print(logFile); 
		CHOKE("fsobj::dir_Delete: (%s) Delete failed!", Name); 
	}

	data.dir->udcfvalid = 0;

	int newlength = dir_Length();
	int delta_blocks = NBLOCKS(newlength) - NBLOCKS(oldlength);
	UpdateCacheStats(&FSDB->DirDataStats, REMOVE, delta_blocks);
}


/* TRANS */
void fsobj::dir_MakeDir() 
{

	FSO_ASSERT(this, !HAVEDATA(this));

	data.dir = (VenusDirData *)rvmlib_rec_malloc((int)sizeof(VenusDirData));
	RVMLIB_REC_OBJECT(*data.dir);
	memset((void *)data.dir, 0, (int)sizeof(VenusDirData));
	DH_Init(&data.dir->dh);


	if (DH_MakeDir(&data.dir->dh, MakeViceFid(&fid), MakeViceFid(&pfid)) != 0) {
		print(logFile); 
		CHOKE("fsobj::dir_MakeDir: MakeDir failed!"); 
	}

	data.dir->udcfvalid = 0;
}


int fsobj::dir_Lookup(char *Name, VenusFid *Fid, int flags) 
{
	
	if (!HAVEALLDATA(this)) { 
		print(logFile); 
		CHOKE("fsobj::dir_Lookup: (%s) no data", Name); 
	}

	int code = DH_Lookup(&data.dir->dh, Name, MakeViceFid(Fid), flags);
	if (code != 0) 
		return(code);

	Fid->Realm = fid.Realm;
	Fid->Volume = fid.Volume;
	return(0);
}


/* Name buffer had better be CODA_MAXNAMLEN bytes or more! */
int fsobj::dir_LookupByFid(char *Name, VenusFid *Fid) 
{
	if (!HAVEALLDATA(this)) { 
		print(logFile); 
		CHOKE("fsobj::dir_LookupByFid: %s no data", FID_(Fid));
	}

	return DH_LookupByFid(&data.dir->dh, Name, MakeViceFid(Fid));
}


/* return 1 if directory is empty, 0 otherwise */
int fsobj::dir_IsEmpty() 
{
	if (!HAVEALLDATA(this)) { 
		print(logFile); 
		CHOKE("fsobj::dir_IsEmpty: no data"); 
	}

	return(DH_IsEmpty(&data.dir->dh));
}

/* determine if target_fid is the parent of this */
int fsobj::dir_IsParent(VenusFid *target_fid) 
{
	if (!HAVEALLDATA(this)) { 
		print(logFile); 
		CHOKE("fsobj::dir_IsParent: (%s) no data", FID_(target_fid));
	}

	/* Volumes must be the same. */
	if (!FID_VolEQ(&fid, target_fid))
		return(0);

	/* Don't match "." or "..". */
	if (FID_EQ(target_fid, &fid) || FID_EQ(target_fid, &pfid)) 
		return(0);

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
		print(logFile); 
		CHOKE("fsobj::dir_TranslateFid: %s -> %s no data", 
		      FID_(OldFid), FID_(NewFid));
	}

	if ((!FID_VolEQ(&fid, OldFid) && !FID_IsLocalFake(OldFid) && !FID_IsLocalFake(&fid)) ||
	    (!FID_VolEQ(&fid, NewFid) && !FID_IsLocalFake(NewFid) && !FID_IsLocalFake(&fid))) {
		print(logFile); 
		CHOKE("fsobj::dir_TranslateFid: %s -> %s cross-volume", 
		      FID_(OldFid), FID_(NewFid)); 
	}

	if (FID_EQ(OldFid, NewFid)) 
		return;

	Name = (char *)malloc(CODA_MAXNAMLEN);
	CODA_ASSERT(Name);

	while ( !dir_LookupByFid(Name, OldFid) ) {
		dir_Delete(Name);
		dir_Create(Name, NewFid);
	}
	free(Name);
}



void fsobj::dir_Print() 
{
	if (!HAVEALLDATA(this)) { 
		print(logFile); 
		CHOKE("fsobj::dir_Print: no data"); 
	}

	if (LogLevel >= 1000) {
		LOG(1000, ("fsobj::dir_Print: %s, %d, %d\n",
			   data.dir->udcf->Name(), data.dir->udcf->Length(), 
			   data.dir->udcfvalid));

		DH_Print(&data.dir->dh, stdout);
	}
}

