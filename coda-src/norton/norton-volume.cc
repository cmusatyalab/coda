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
                           none currently

#*/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <time.h>
#include "coda_string.h"
    
#ifdef __cplusplus
}
#endif

#include <cvnode.h>
#include <volume.h>
#include <index.h>
#include <recov.h>
#include <camprivate.h>
#include <coda_globals.h>

#include <parser.h>
#include "norton.h"


// Stolen from vol-info.c
static char * date(time_t date, char *result)
{
    struct tm *tm = localtime(&date);
    sprintf(result, "%02d/%02d/%02d.%02d:%02d:%02d", 
	    tm->tm_year, tm->tm_mon+1, tm->tm_mday,
	    tm->tm_hour, tm->tm_min, tm->tm_sec);
    return(result);
}


void PrintVV(vv_t *vv) {
    int i;
    
    printf("{[");
    for (i = 0; i < VSG_MEMBERS; i++)
        printf(" %ld", (&(vv->Versions.Site0))[i]);
    printf(" ] [ %ld %ld ] [ 0x%#lx ]}\n",
             vv->StoreId.Host, vv->StoreId.Uniquifier, vv->Flags);
}



void print_volume(VolHead * vol) {
    printf("    Id: 0x%lx  \tName: %s \tParent: 0x%lx\n",
	   vol->header.id,
	   vol->data.volumeInfo->name,
	   vol->header.parent);
    printf("    GroupId: 0x%lx \tPartition: %s\n",
	   vol->data.volumeInfo->groupId,
	   vol->data.volumeInfo->partition);
    printf("    Version Vector: ");
    PrintVV(&vol->data.volumeInfo->versionvector);
    printf("\n    \t\tNumber vnodes	Number Lists	Lists\n");
    printf("    \t\t-------------	------------	----------\n");
    printf("    small\t%13u\t%12u\t%p\n",
	   (unsigned int)vol->data.nsmallvnodes,
	   (unsigned int)vol->data.nsmallLists,
	   vol->data.smallVnodeLists);
    printf("    large\t%13u\t%12u\t%p\n",
	   (unsigned int)vol->data.nlargevnodes,
	   (unsigned int)vol->data.nlargeLists,
	   vol->data.largeVnodeLists);
}

void print_volume_details(VolHead *vol)
{
    int i;
    char buf[20];
    
    printf("\n");
    printf("    inuse: %s\tinService: %s\tblessed: %s\tneedsSalvaged: %s\n",   
	   vol->data.volumeInfo->inUse ? "TRUE" : "FALSE",
	   vol->data.volumeInfo->inService ? "TRUE" : "FALSE",
	   vol->data.volumeInfo->blessed ? "TRUE" : "FALSE",
	   vol->data.volumeInfo->needsSalvaged ? "TRUE" : "FALSE");
    printf("    Uniquifier: 0x%u\t", vol->data.volumeInfo->uniquifier);
    printf("type: ");
    switch (vol->data.volumeInfo->type) {
      case RWVOL: 	printf("rw\n"); 	break;
      case ROVOL: 	printf("ro\n"); 	break;
      case BACKVOL:	printf("backup\n"); 	break;
      case REPVOL:	printf("rep\n"); 	break;
      default:	printf("*** UNKNOWN ***\n");
    }
    printf("    Clone: 0x%lx\tbackupId: 0x%lx\trestoredFromId: 0x%lx\n", 
	   vol->data.volumeInfo->cloneId,
	   vol->data.volumeInfo->backupId,
	   vol->data.volumeInfo->restoredFromId);
    printf("    destroyMe: 0x%x\tdontSalvage: 0x%x\n",
	   vol->data.volumeInfo->destroyMe, vol->data.volumeInfo->dontSalvage);
//    PrintVV(vol->data.volumeInfo->versionvector);
    printf("    needsCallback: %s\tResOn: %s\n",
	   vol->data.volumeInfo->needsCallback ? "TRUE" : "FALSE",
	   vol->data.volumeInfo->ResOn ? "TRUE" : "FALSE");
    printf("    minquota: %d\tmaxquota: %d\tmaxfiles: %d\n",
	   vol->data.volumeInfo->minquota,
	   vol->data.volumeInfo->maxquota,
	   vol->data.volumeInfo->maxfiles);
    printf("    accountNumber: %u\towner: %u\n",
	   vol->data.volumeInfo->accountNumber,
	   vol->data.volumeInfo->owner);
    printf("    filecount: %d\tlinkcount: %d\tdiskused: %d\n",
	   vol->data.volumeInfo->filecount,
	   vol->data.volumeInfo->linkcount,
	   vol->data.volumeInfo->diskused);
    printf("    ");
    for (i = 0; i < 3; i++) {
	printf("weekUse[%d] = %d, ", i, vol->data.volumeInfo->weekUse[i]);
    }
    printf("\n    ");
    for (i = 3; i < 6; i++) {
	printf("weekUse[%d] = %d, ", i, vol->data.volumeInfo->weekUse[i]);
    }
    printf("\n    ");
    for (i = 6; i < 7; i++) {
	printf("weekUse[%d] = %d, ", i, vol->data.volumeInfo->weekUse[i]);
    }
    printf("\n");
    printf("    dayUseDate: %s\tcreationDate: %s\n",
	   date(vol->data.volumeInfo->dayUseDate, buf), 
	   date(vol->data.volumeInfo->creationDate, buf));
    printf("    accessDate: %s\texpirationDate: %s\n",
	   date(vol->data.volumeInfo->accessDate, buf),
	   date(vol->data.volumeInfo->expirationDate, buf));
    printf("    backupDate: %s\tcopyDate: %s\n",
	   date(vol->data.volumeInfo->backupDate, buf),
	   date(vol->data.volumeInfo->copyDate, buf));
//    PrintResLog(vol->data.volumeInfo->log);
    printf("    OfflineMessage: %s\n", vol->data.volumeInfo->offlineMessage);
    printf("    motd: %s\n", vol->data.volumeInfo->motd);
}
    
    



VolHead *GetVol(char *name) {
    VolHead *vol;
    int	    i,
	    maxid = GetMaxVolId();

    /* Loop until we find the volume */
    for (i = 0; (i < maxid) && (i < MAXVOLS); i++) {
	if ((vol = VolByIndex(i)) == NULL) {
	    printf("WARNING: Unable to get volume at index: %d\n", i);
	    continue;
	}
	if (vol->header.stamp.magic != VOLUMEHEADERMAGIC) {
	    continue;
	}

	if (!strcmp(vol->data.volumeInfo->name, name)) {
	    return(vol);
	}
    }

    return(NULL);
}


VolHead *GetVol(VolumeId volid)
{
    VolHead *vol;
    int	    i,
	    maxid = GetMaxVolId();

    /* Loop until we find the volume */
    for (i = 0; (i < maxid) && (i < MAXVOLS); i++) {
	if ((vol = VolByIndex(i)) == NULL) {
	    printf("WARNING: Unable to get volume at index: %d\n", i);
	    continue;
	}
	if (vol->header.stamp.magic != VOLUMEHEADERMAGIC) {
	    continue;
	}
	if (vol->header.id == volid) {
	    return(vol);
	}
    }

    return(NULL);
}


int GetVolIndex(VolumeId volid)
{
    VolHead	 *vol;
    int	 i, maxid = GetMaxVolId();

    /* Loop until we find the volume */
    for (i = 0; (i < maxid) && (i < MAXVOLS); i++) {
	if ((vol = VolByIndex(i)) == NULL) {
	    continue;
	}
	if (vol->header.stamp.magic != VOLUMEHEADERMAGIC) {
	    continue;
	}

	if (vol->header.id != volid) {
	    continue;
	}

	return i;
    }

    return -1;
}



void list_vols(int argc, char *argv[]) {
    list_vols();
}




void list_vols() {
    VolumeHeader *header;
    int 	 i,
		 maxid = GetMaxVolId();

    printf("Index  ID         Parent     Type\n");
    printf("------ ---------- ---------- ----\n");
    
    for (i = 0; (i < maxid) && (i < MAXVOLS); i++) {
	if ((header = VolHeaderByIndex(i)) == NULL) {
	    printf("WARNING: Unable to get volume header at index: %d\n", i);
	    continue;
	}
	if (header->stamp.magic != VOLUMEHEADERMAGIC) {
	    continue;
	}

	printf("%6d 0x%8lx 0x%8lx ", i, header->id, header->parent);
	switch (header->type) {
	  case RWVOL: 	printf("rw\n"); 	break;
	  case ROVOL: 	printf("ro\n"); 	break;
	  case BACKVOL:	printf("backup\n"); 	break;
	  case REPVOL:	printf("rep\n"); 	break;
	  default:	printf("*** UNKNOWN ***\n");
	}
	    
    }
}


void show_all_volumes() {
    VolumeHeader *header;
    VolHead	 *vol;
    int		 i,
		 maxid = GetMaxVolId();

    for (i = 0; (i < maxid) && (i < MAXVOLS); i++) {
	if ((header = VolHeaderByIndex(i)) == NULL) {
	    printf("WARNING: Unable to get volume header at index: %d\n", i);
	    continue;
	}
	if (header->stamp.magic != VOLUMEHEADERMAGIC) {
	    continue;
	}

	if ((vol = VolByIndex(i)) == NULL) {
	    printf("WARNING: Unable to get volume at index: %d\n", i);
	    continue;
	}

	print_volume(vol);
	printf("\n--------------------------------------------------------------\n");
    }
}

void show_volume(int argc, char *argv[])
{
    unsigned int volid;

    if (argc != 3) {
	fprintf(stderr, "Usage: show volume <volid> | <name> | *\n");
	return;
    }

    if (!strcmp(argv[2], "*")) show_all_volumes();
    else if (Parser_uint(argv[2], &volid) == 1) show_volume(volid);
    else show_volume(argv[2]);
}

void show_volume(VolumeId volid)
{
    VolHead	 *vol;

    vol = GetVol(volid);

    if (vol) print_volume(vol);
    else printf("Unable to find volume id 0x%lx\n", volid);
}



void show_volume(char *name) {
    VolHead	 *vol;

    vol = GetVol(name);

    if (vol) print_volume(vol);
    else printf("Unable to find volume named %s\n", name);
}


void delete_volume(VolHead *vol) {
    byte destroyflag=0xD3;
    rvm_return_t status;

    if (vol) {
	rvmlib_begin_transaction(restore);
	rvmlib_modify_bytes(&(vol->data.volumeInfo->destroyMe), 
			    &destroyflag, sizeof(byte));
	rvmlib_end_transaction(flush, &status);
	    }

}

void delete_volume_byid(VolumeId volid) 
{
    VolHead *vol = NULL;

    vol = GetVol(volid);
    if ( vol )
	delete_volume(vol);
    else
	printf("Unable to find volume 0x%lx\n", volid);


}
void delete_volume_byname(char *name) {
    VolHead *vol = NULL;

    vol = GetVol(name);

    if (vol) 
	delete_volume(vol);
    else 
	printf("Unable to find volume named %s\n", name);
}

void sh_delete_volume(int argc, char **argv)
{
    unsigned int volid;

    if (argc != 3) {
	fprintf(stderr, "Usage: delete volume  <name> | <volid>");
	return;
    }
    else if (Parser_uint(argv[2], &volid) == 1) 
	delete_volume_byid(volid);
    else 
	delete_volume_byname(argv[2]);
}


void show_volume_details(VolumeId volid)
{
    VolHead	*vol;

    vol = GetVol(volid);

    if (vol) {
	print_volume(vol);
	print_volume_details(vol);
    } else {
	printf("Unable to find volume id 0x%lx\n", volid);
    }
}


void show_volume_details(char *name) {
    VolHead	*vol;

    vol = GetVol(name);

    if (vol) {
	print_volume(vol);
	print_volume_details(vol);
    } else {
	printf("Unable to find volume named %s\n", name);
    }
}

void show_volume_details(int argc, char *argv[])
{
    unsigned int volid;

    if (argc != 4) {
	fprintf(stderr, "Usage: show volume details <volid> | <name>\n");
	return;
    }

    if (Parser_uint(argv[3], &volid) == 1)
	show_volume_details((VolumeId)volid);
    else
	show_volume_details(argv[3]);
}



void show_index(VolumeId volid)
{
    VolHead	 *vol;
    int		 i,
		 maxid = GetMaxVolId();

    /* Loop until we find the volume */
    for (i = 0; (i < maxid) && (i < MAXVOLS); i++) {
	if ((vol = VolByIndex(i)) == NULL) {
	    printf("WARNING: Unable to get volume at index: %d\n", i);
	    continue;
	}
	if (vol->header.stamp.magic != VOLUMEHEADERMAGIC) {
	    continue;
	}

	if (vol->header.id != volid) {
	    continue;
	}

	printf("    Volume '0x%lx' is at index %d\n", volid, i);
	return;
    }

    printf("Unable to find volume id 0x%lx\n", volid);
}



void show_index(char *name) {
    VolHead	 *vol;
    int		 i,
		 maxid = GetMaxVolId();

    /* Loop until we find the volume */
    for (i = 0; (i < maxid) && (i < MAXVOLS); i++) {
	if ((vol = VolByIndex(i)) == NULL) {
	    printf("WARNING: Unable to get volume at index: %d\n", i);
	    continue;
	}
	if (vol->header.stamp.magic != VOLUMEHEADERMAGIC) {
	    continue;
	}

	if (strcmp(vol->data.volumeInfo->name, name)) {
	    continue;
	}

	printf("    Volume '%s' is at index %d\n", name, i);
	return;
    }

    printf("Unable to find volume named %s\n", name);
}


void show_index(int argc, char *argv[])
{
    unsigned int volid;

    if (argc != 3) {
	fprintf(stderr, "Usage: show index <volid> | <name>\n");
	return;
    }

    if (Parser_uint(argv[2], &volid) == 1)
	show_index((VolumeId)volid);
    else
	show_index(argv[2]);
}

void rename_volume(VolHead *vol, char *newname)
{
    char namestr[V_MAXVOLNAMELEN];
    rvm_return_t status;

    if (!vol) return;

    memset(namestr, 0, sizeof(namestr));
    strncpy(namestr, newname, sizeof(namestr)-1);

    rvmlib_begin_transaction(restore);
    rvmlib_modify_bytes(vol->data.volumeInfo->name, namestr, sizeof(namestr));
    rvmlib_end_transaction(flush, &status);
}

void rename_volume_byid(VolumeId volid, char *newname) 
{
    VolHead *vol = NULL;

    vol = GetVol(volid);
    if ( vol )
	rename_volume(vol, newname);
    else
	printf("Unable to find volume 0x%lx\n", volid);


}

void rename_volume_byname(char *name, char *newname)
{
    VolHead *vol = NULL;

    vol = GetVol(name);

    if (vol) 
	rename_volume(vol, newname);
    else 
	printf("Unable to find volume named %s\n", name);
}

void sh_rename_volume(int argc, char **argv)
{
    unsigned int volid;

    if (argc != 4) {
	fprintf(stderr, "Usage: rename volume <name>|<volid> <newname>");
	return;
    }
    else if (Parser_uint(argv[2], &volid) == 1) 
	rename_volume_byid(volid, argv[3]);
    else 
	rename_volume_byname(argv[2], argv[3]);
}

