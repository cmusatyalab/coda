#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/norton/norton-volume.cc,v 4.2 1997/10/15 15:53:02 braam Exp $";
#endif /*_BLURB_*/




#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <strings.h>
#ifdef	__MACH__
#include <mach/boolean.h>
#endif
    
#ifdef __cplusplus
}
#endif __cplusplus

#include <cvnode.h>
#include <volume.h>
#include <index.h>
#include <recov.h>
#include <camprivate.h>
#include <coda_globals.h>

#include <parser.h>
#include "norton.h"


// Stolen from vol-info.c
PRIVATE char * date(unsigned long date, char *result)
{
    struct tm *tm = localtime((long *)&date);
    sprintf(result, "%02d/%02d/%02d.%02d:%02d:%02d", 
	    tm->tm_year, tm->tm_mon+1, tm->tm_mday,
	    tm->tm_hour, tm->tm_min, tm->tm_sec);
    return(result);
}


void PrintVV(vv_t *vv) {
    int i;
    
    printf("{[");
    for (i = 0; i < VSG_MEMBERS; i++)
        printf(" %d", (&(vv->Versions.Site0))[i]);
    printf(" ] [ %d %d ] [ 0x%#x ]}\n",
             vv->StoreId.Host, vv->StoreId.Uniquifier, vv->Flags);
}



void print_volume(VolHead * vol) {
    printf("    Id: 0x%x  \tName: %s \tParent: 0x%x\n",
	   vol->header.id,
	   vol->data.volumeInfo->name,
	   vol->header.parent);
    printf("    GoupId: 0x%x \tPartition: %s\n",
	   vol->data.volumeInfo->groupId,
	   vol->data.volumeInfo->partition);
    printf("    Version Vector: ");
    PrintVV(&vol->data.volumeInfo->versionvector);
    printf("\n    \t\tNumber vnodes	Number Lists	Lists\n");
    printf("    \t\t-------------	------------	----------\n");
    printf("    small\t%13d\t%12d\t0x%8x\n",
	   vol->data.nsmallvnodes,
	   vol->data.nsmallLists,
	   vol->data.smallVnodeLists);
    printf("    large\t%13d\t%12d\t0x%8x\n",
	   vol->data.nlargevnodes,
	   vol->data.nlargeLists,
	   vol->data.largeVnodeLists);
}




void print_volume_details(VolHead *vol) {
    int i;
    char buf[20];
    
    printf("\n");
    printf("    inuse: %s\tinService: %s\tblessed: %s\tneedsSalvaged: %s\n",   
	   vol->data.volumeInfo->inUse ? "TRUE" : "FALSE",
	   vol->data.volumeInfo->inService ? "TRUE" : "FALSE",
	   vol->data.volumeInfo->blessed ? "TRUE" : "FALSE",
	   vol->data.volumeInfo->needsSalvaged ? "TRUE" : "FALSE");
    printf("    Uniquifier: 0x%x\t", vol->data.volumeInfo->uniquifier);
    printf("type: ");
    switch (vol->data.volumeInfo->type) {
      case RWVOL: 	printf("rw\n"); 	break;
      case ROVOL: 	printf("ro\n"); 	break;
      case BACKVOL:	printf("backup\n"); 	break;
      case REPVOL:	printf("rep\n"); 	break;
      default:	printf("*** UNKNOWN ***\n");
    }
    printf("    Clone: 0x%x\tbackupId: 0x%x\trestoredFromId: 0x%x\n", 
	   vol->data.volumeInfo->cloneId,
	   vol->data.volumeInfo->backupId,
	   vol->data.volumeInfo->restoredFromId);
    printf("    destroyMe: 0x%x\tdontSalvage: 0x%x\n",
	   vol->data.volumeInfo->destroyMe, vol->data.volumeInfo->dontSalvage);
//    PrintVV(vol->data.volumeInfo->versionvector);
    printf("    needsCallback: %s\tResOn: %s\tmaxlogentries: 0x%x\n",
	   vol->data.volumeInfo->needsCallback ? "TRUE" : "FALSE",
	   vol->data.volumeInfo->ResOn ? "TRUE" : "FALSE",
	   vol->data.volumeInfo->maxlogentries);
    printf("    minquota: %d\tmaxquota: %d\tmaxfiles: %d\n",
	   vol->data.volumeInfo->minquota,
	   vol->data.volumeInfo->maxquota,
	   vol->data.volumeInfo->maxfiles);
    printf("    accountNumber: %d\towner: %d\n",
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


VolHead *GetVol(int volid) {
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


int GetVolIndex(int volid) {
    VolHead	 *vol;
    int		 i,
		 maxid = GetMaxVolId();

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

	printf("%6d 0x%8x 0x%8x ", i, header->id, header->parent);
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

void show_volume(int argc, char *argv[]) {
    int volid;

    if (argc != 3) {
	fprintf(stderr, "Usage: show volume <volid> | <name> | *\n");
	return;
    }

    if (!strcmp(argv[2], "*")) show_all_volumes();
    else if (Parser_int(argv[2], &volid) == 1) show_volume(volid);
    else show_volume(argv[2]);
}

void show_volume(int volid) {
    VolHead	 *vol;

    vol = GetVol(volid);

    if (vol) print_volume(vol);
    else printf("Unable to find volume id 0x%x\n", volid);
}



void show_volume(char *name) {
    VolHead	 *vol;

    vol = GetVol(name);

    if (vol) print_volume(vol);
    else printf("Unable to find volume named %s\n", name);
}


void delete_volume(VolHead *vol) {
    byte destroyflag=0xD3;
    Error error;

    if (vol) {
	CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED);
	CAMLIB_MODIFY_BYTES(&(vol->data.volumeInfo->destroyMe), 
			    &destroyflag, sizeof(byte));
	CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, error);
	    }

}

void delete_volume_byid(int volid) 
{
    VolHead *vol = NULL;

    vol = GetVol(volid);
    if ( vol )
	delete_volume(vol);
    else
	printf("Unable to find volume %d\n", volid);


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
    int volid;

    if (argc != 3) {
	fprintf(stderr, "Usage: delete volume  <name> | <volid>");
	return;
    }
    else if (Parser_int(argv[2], &volid) == 1) 
	delete_volume_byid(volid);
    else 
	delete_volume_byname(argv[2]);
}


void show_volume_details(int argc, char *argv[]) {
    int volid;

    if (argc != 4) {
	fprintf(stderr, "Usage: show volume details <volid> | <name>\n");
	return;
    }

    if (Parser_int(argv[3], &volid) == 1) show_volume_details(volid);
    else show_volume_details(argv[3]);
}


void show_volume_details(int volid) {
    VolHead	*vol;

    vol = GetVol(volid);

    if (vol) {
	print_volume(vol);
	print_volume_details(vol);
    } else {
	printf("Unable to find volume id 0x%x\n", volid);
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



void show_index(int argc, char *argv[]) {
    int volid;

    if (argc != 3) {
	fprintf(stderr, "Usage: show index <volid> | <name>\n");
	return;
    }

    if (Parser_int(argv[2], &volid) == 1) show_index(volid);
    else show_index(argv[2]);
}


void show_index(int volid) {
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

	printf("    Volume '0x%x' is at index %d\n", volid, i);
	return;
    }

    printf("Unable to find volume id 0x%x\n", volid);
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






