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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/norton/norton-reinit.cc,v 4.5 1998/10/02 13:27:41 braam Exp $";
#endif /*_BLURB_*/



#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <strings.h>
#include <sys/file.h>
#include <struct.h>

#ifdef __cplusplus
}
#endif __cplusplus

//#include <reslog.h>
#include <remotelog.h>
#include <rsle.h>
#include <parselog.h>
#include <cvnode.h>
#include <volume.h>
#include <index.h>
#include <recov.h>
#include <codadir.h>
#include <camprivate.h>
#include <coda_globals.h>

#include <rvm.h>
#include "norton.h"


void usage(char * name) {
    fprintf(stderr, "Usage: %s -rvm <log_device> <data_device> <length>\n", name);
    fprintf(stderr, "       [-dump <file> [skip <volid1> <volid2>...] | -load <file>]\n");
}


static int DumpGlobalState(int fd) {
    int maxvolid = VGetMaxVolumeId();
    
    if (write(fd, (void *)&maxvolid, (int)sizeof(int)) == -1) {
	perror("Writing max volume ID\n");
    	return 0;
    }
    return 1;
}

static int ReadGlobalState(int fd) {
    VolumeId maxvolid;
    int status;

    if (read(fd, &maxvolid, (int)sizeof(int)) == -1) {
	perror("Reading max volume ID\n");
	return 0;
    }

    if (norton_debug) printf("Read maximum vol ID: 0x%x\n", maxvolid);

    RVMLIB_BEGIN_TRANSACTION(restore);
    VSetMaxVolumeId(maxvolid);
    RVMLIB_END_TRANSACTION(flush, &(status));
    if (norton_debug) printf("Set max vol id to 0x%08x.  RVM Status = %d\n",
		      maxvolid, status);
    return 1;
}


static int DumpVolHead(int fd, VolHead *vol) {
    if (write(fd, (void *)vol, (int)sizeof(VolHead)) == -1) {
	perror("Writing volume header");
	return 0;
    }
    return 1;
}

static int ReadVolHead(int fd, VolHead *vol) {
    int ret;

    if ((ret = read(fd, (void *)vol, (int)sizeof(VolHead))) == -1) {
	/* An error occured */
	perror("Reading volume header");
	return 0;
    } 	/* We have a header or EOF */
    return ret;
}

int DumpVolDiskData(int fd, VolumeDiskData *data){
    if (write(fd, (void *)data, (int)sizeof(VolumeDiskData)) == -1) {
	perror("Writing volume disk data");
	return 0;
    }

    /* Check that the resolution flag is on before writing any log information */
    if (data->ResOn & 4) {
	if (write(fd, (void *)&data->log->admin_limit, (int)sizeof(int)) == -1) {
	    perror("Writing resolution log admin limit");
	    return 0;
	}
    }

    return 1;
}

static int ReadVolDiskData(int fd, VolumeDiskData *data, int *adm_limit){
    int ret;

    if ((ret = read(fd, (void *)data, (int)sizeof(VolumeDiskData))) == -1) {
	perror("Reading volume disk data\n");
	return 0;
    }

    if (data->ResOn & 4) {
	if ((ret = read(fd, (void *)adm_limit, (int)sizeof(int))) == -1) {
	    perror("Reading resolution log admin limit");
	    return 0;
	}
    }

    return ret;
}


static int DumpResLog(int fd, struct VolumeData *voldata, struct
		       VnodeDiskObject *vnode) {
    Volume    vol, *vp;		// We need to fake out the DumpLog routine
    volHeader vh;
    char      *buf = NULL;
    int	      size = 0,
	      nentries = 0,
	      ret = 1;


    /* Check that resolution is on, otherwise return error */
    if (!(voldata->volumeInfo->ResOn & 4)) {
	fprintf(stderr, "DumpResLog called on volume that doesn't ");
	fprintf(stderr, "have resolution turned on.");
	return 0;
    }
    
    /* Mush the data into the right data structure */
    bcopy((const char *)voldata->volumeInfo, (char *)&vh.diskstuff, (int)sizeof(VolumeDiskData));
    vp = &vol;
    vp->header = &vh;

    DumpLog(vnode->log, vp, &buf, &size, &nentries);

    /* Write out the number of entries and the size */
    if (write(fd, &nentries, (int)sizeof(nentries)) == -1) {
	perror("Writing number of entries");
	if (buf) free(buf);
	return 0;
    }

    if (write(fd, &size, (int)sizeof(size)) == -1) {
	perror("Writing resolution log buffer size");
	if (buf) free(buf);
	return 0;
    }
    
    if (write(fd, buf, size) == -1) {
	perror("Writing resolution log");
	ret = 0;
    }

    if (norton_debug)
	printf("    Wrote resolution log of %d entries for %d bytes\n",
	       nentries, size);
    
    if (buf) free(buf);
    return ret;
}



// Must be called withing a transaction.
static recle *AllocLogEntry(Volume *vp, rsle *entry) {
    recle *rle;
    int   index = -1;
    int   seqno = -1;

    if (V_VolLog(vp)->AllocRecord(&index, &seqno) != 0) {
	fprintf(stderr, "Can't allocate a vollog record! Aborting.\n");
	return NULL;
    }

    rle = V_VolLog(vp)->RecovPutRecord(index);
    if (!rle) {
	fprintf(stderr, "Can't put a vollog record! Aborting.\n");
	return NULL;
    }

    // Cram the correct index into the log entery.
    entry->index = index;
    rle->InitFromsle(entry);
    if (norton_debug) rle->print();

    return rle;
}


// If an error occures, set *err = 1, otherwise *err = 0
static int BuildResLog(Volume *vp, rec_dlist *log,
			 VnodeId pvn, /*Parent vnode */ 
			 Unique_t pu, /*parent uniquifier */
			 int nentries, int start,
			 rsle *logentries, int *err) {
    recle *rle;
    int   i = start;

    *err = 0;
    
    while (i <  nentries &&
	   ((pvn == 0) ||
	    ((logentries[i].dvn == pvn) && (logentries[i].du == pu)))) {

	if (!(rle = AllocLogEntry(vp, &logentries[i]))) {
	    *err = 1;
	    return -1;
	}
	      
	log->append(rle);
	i++;
	
	if ((rle->opcode == ViceRemoveDir_OP) ||
	    (rle->opcode == ResolveViceRemoveDir_OP)) {

	    // Should I check that there is actually a list?
	    // Should I allocate on entry to BuildResLog?
	    rmdir_rle *r = (rmdir_rle *)(&(rle->vle->vfld[0]));
	    r->childlist = new rec_dlist();

	    i += BuildResLog(vp, r->childlist, r->cvnode,
			      r->cunique, nentries, i,
			      logentries, err);
	    if (err) return -1;
	}

    }
    return i - start;
}


/* Warning: ReadResLog better not be called for a volume that does not
 * have resolution turned on.
 */
static int ReadResLog(int fd, Volume *vp, Vnode *vnp) {
    int   nentries,
	  size,
	  err;
    char  *buf;
    olist *hlist;
    rsle  *logentries;
    
    /* Read in the number of entries and the size */
    if (read(fd, &nentries, (int)sizeof(nentries)) == -1) {
	perror("Reading number of resolution log entries");
	return 0;
    }

    if (read(fd, &size, (int)sizeof(size)) == -1) {
	perror("Reading size of resolution log");
	return 0;
    }

    if (!(buf = (char *)malloc(size))) {
	fprintf(stderr, "Unable to malloc resolution log buffer\n");
	return 0;
    }

    if (read(fd, buf, size) == -1) {
	perror("Reading resolution log");
	free(buf);
	return 0;
    }
    if (norton_debug) {
	printf("    Read resolution log of %d entries for %d bytes\n",
	       nentries, size);
    }

    ParseRemoteLogs(buf, size, nentries, &hlist, &logentries);

    // Initialize the log list.
    VnLog(vnp) = new rec_dlist();

    // Now copy it to RVM.
    BuildResLog(vp, VnLog(vnp), 0, 0, nentries, 0, logentries, &err);
    
    free(buf);
    if (err) return 0;
    else return 1;
}

#define VNODESIZE(vclass) ((vclass) == vLarge ? \
			   SIZEOF_LARGEDISKVNODE :\
			   SIZEOF_SMALLDISKVNODE)

#define VNODECLASS(vclass) ((vclass) == vLarge ? "large" : "small")

static int DumpVnodeList(int fd, struct VolumeData *vol, int vol_index,
			  VnodeClass vclass) {
    char   buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *) buf;
    int    nvnodes,
	   vnode_index,
	   vnode_num;
    DirInode *inode;
    int	   npages;
    
    /* Send in a 0 for device, since noone uses it anyway. */
    vindex v_index(vol->volumeInfo->id, vclass, 0, VNODESIZE(vclass),
		   vol_index);
    vindex_iterator vnext(v_index);

    nvnodes = v_index.vnodes();
    if (norton_debug) {
	printf("Volume 0x%x, has %d %s vnodes\n", vol->volumeInfo->id,
	       nvnodes, VNODECLASS(vclass));
    }
    
    if (write(fd, (void *)&nvnodes, (int)sizeof(nvnodes)) == -1) {
	perror("Writing number of vnodes\n");
	return 0;
    }
    
    while ((vnode_index = vnext(vnode)) != -1) {

	/* Write out the vnode number */
	vnode_num = bitNumberToVnodeNumber(vnode_index, vclass);
	if (norton_debug) printf("Writing vnode number 0x%x\n", vnode_num);
	
	if (write(fd, (void *)&vnode_num, (int)sizeof(vnode_num)) == -1) {
	    perror("Writing vnode number\n");
	    return 0;
	}

	if (write(fd, (void *)vnode, VNODESIZE(vclass)) == -1) {
	    perror("Writing vnode\n");
	    return 0;
	}

	if (vclass == vLarge) {
	    /* Now write the inode information and directory pages. */
	    inode = (DirInode *)vnode->inodeNumber;
	    for (npages = 0; inode->di_pages[npages]; npages++);

	    if (norton_debug) {
		printf("    Inode %d has %d pages\n", inode, npages);
	    }
	    
	    if (write(fd, (void *)&npages, (int)sizeof(npages)) == -1) {
		perror("Writing inode\n");
		return 0;
	    }

	    /* Write the reference count */
	    if (write(fd, (void *)&inode->di_refcount,
		      (int)sizeof(inode->di_refcount)) == -1) { 
		perror("Writing inode reference count\n");
		return 0;
	    }

	    for (npages = 0; inode->di_pages[npages]; npages++) {
		    if (write(fd, (void *)inode->di_pages[npages], DIR_PAGESIZE) == -1) {
		    perror("Writing directory pages\n");
		    return 0;
		}
	    }

	    /* Finally write the resolution log */
	    if (vol->volumeInfo->ResOn & 4) {
		if (!DumpResLog(fd, vol, vnode)) return 0;
	    }
	}
    }
    return 1;
}
    
static int DumpVolVnodes(int fd, struct VolumeData *vol, int vol_index) { 
    if (!DumpVnodeList(fd, vol, vol_index, vLarge)) return 0;
    if (!DumpVnodeList(fd, vol, vol_index, vSmall)) return 0;
    return 1;
}

int CopyDirInode(DirInode *oldinode, DirInode **newinode)
{
    DirInode    shadowInode;

    if (!oldinode){
       LogMsg(29, DirDebugLevel, stdout, "CopyDirInode: Null oldinode");
       return -1;
    }
    *newinode = (DirInode *)rvmlib_rec_malloc(sizeof(DirInode));
    bzero((void *)&shadowInode, sizeof(DirInode));
    for(int i = 0; i < DIR_MAXPAGES; i++)
        if (oldinode->di_pages[i]){
            shadowInode.di_pages[i] = (long *)rvmlib_rec_malloc(DIR_PAGESIZE);
            rvmlib_modify_bytes(shadowInode.di_pages[i], 
				oldinode->di_pages[i], DIR_PAGESIZE);
        }
    shadowInode.di_refcount = oldinode->di_refcount;
    rvmlib_modify_bytes(*newinode, &shadowInode, sizeof(DirInode));
    return 0;

}

static int ReadVnodeList(int fd, Volume *vp, VnodeClass vclass, int ResOn) {
    char   buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *) buf;
    Vnode 	*vnp;
    int 	nvnodes;
    VnodeId 	vnode_num;
    int 	npages;
    int		i;
    int 	status;
    Error	err;

    /* We need to allocate this in RVM! */
    DirInode *inode = (DirInode *)malloc(sizeof(DirInode));
    DirInode *newinode;

    if (!inode) {
	fprintf(stderr, "Cannot allocate VM inode.\n");
	return 0;
    }
    
    if (read(fd, (void *)&nvnodes, (int)sizeof(nvnodes)) == -1) {
	sprintf(buf, "Reading number of %s vnodes\n", VNODECLASS(vclass));
	perror(buf);
	return 0;
    }
    if (norton_debug) printf("Reading %d vnodes...\n", nvnodes);

    while (nvnodes--) {
	if (read(fd, (void *)&vnode_num, (int)sizeof(vnode_num)) == -1) {
	    perror("Reading vnode number\n");
	    return 0;
	}
	if (norton_debug) printf("Reading vnode number 0x%x\n", vnode_num);

	bzero((void *)(void *)vnode, VNODESIZE(vclass));
	
	if (read(fd, (void *) vnode, VNODESIZE(vclass)) == -1) {
	    perror("Reading vnode\n");
	    return 0;
	}

	/* Sanity check what we just read by looking for the magic number */
	if (vnode->vnodeMagic !=
	    (vclass == vLarge ? LARGEVNODEMAGIC : SMALLVNODEMAGIC)) {
	    fprintf(stderr, "ERROR!  Bad %s magic number for vnode 0x%x\n",
		    VNODECLASS(vclass), vnode_num);
	    return 0;
	}
	
	vnp = VAllocVnode(&err, vp, vnode->type, vnode_num, vnode->uniquifier);
	if (err) {
	    fprintf(stderr, "Error code %d, while allocating vnode 0x%x\n",
		    err, vnode_num);
	    return(0);
	}

	// Save the volume index
	vnode->vol_index = vnp->disk.vol_index;

	/* Now copy the disk data into the vnode */
	bcopy((void *)vnode, (void *)&vnp->disk, VNODESIZE(vclass));
	vnp->changed = 1;
	vnp->delete_me = 0;
	
	RVMLIB_BEGIN_TRANSACTION(restore);
	if (vclass == vLarge) {
	    bzero((void *)inode, (int)sizeof(DirInode));
		
	    /* Read in the directory pages */
	    if (read(fd, (void *)&npages, (int)sizeof(npages)) == -1) {
		perror("Reading inode");
		rvmlib_abort(VFAIL);
		return 0;
	    }

	    if (read(fd, (void *)&inode->di_refcount,
		     (int)sizeof(inode->di_refcount)) == -1) {
		perror("Reading inode reference count");
		rvmlib_abort(VFAIL);
		return 0;
	    }

	    for (i = 0; i < npages; i++) {
		inode->di_pages[i] = (long *)malloc(DIR_PAGESIZE);
		if (!inode->di_pages[i]) {
		    fprintf(stderr, "Unable to allocate a new directory page.\n");
		    rvmlib_abort(VFAIL);
		    return 0;
		}

		if (read(fd, (void *)inode->di_pages[i], DIR_PAGESIZE) == -1) {
		    perror("Reading directory page\n");
		    rvmlib_abort(VFAIL);
		    return 0;
		}
	    }

	    if (CopyDirInode(inode, &newinode)) {
		fprintf(stderr, "Cannot copy inode to RVM, aborting\n");
		rvmlib_abort(VFAIL);
		return 0;
	    }
		
	    vnp->disk.inodeNumber = (Inode)newinode;
	    
	    /* Read in the resolution log */
	    if (ResOn) {
		if (!ReadResLog(fd, vp, vnp)) {
		    rvmlib_abort(VFAIL);
		    return 0;
		}
	    } else {
		/* Create an empty log.  DO NOT ALLOW A RES LOG TO BE
		 * CREATED for read only volumes.  at this point we
		 * think they are read/write.
		 */
		fprintf(stderr, "WARNING: Volume ResOn = FALSE, fix norton ");
		fprintf(stderr, "to create empty log.\n");
	    }
	}
	if (norton_debug) PrintVnodeDiskObject(vnode);

	/* Write the vnode to disk */
	VPutVnode(&err, vnp);
	if (err) {
	    fprintf(stderr, "VPutVnode Error: %d, on vnode 0x%x\n", err,
		    vnp->vnodeNumber);
	    return 0;
	}
	RVMLIB_END_TRANSACTION(flush, &(status));
	if (status != 0) {
	    fprintf(stderr,
		    "Transaction exited with status %d!, aborting\n",
		    status);
	    return 0;
	}
    }
    return 1;
}
    

static int ReadVolVnodes(int fd, Volume *vp, int ResOn) {
    if (!ReadVnodeList(fd, vp, vLarge, ResOn)) return 0;
    if (!ReadVnodeList(fd, vp, vSmall, ResOn)) return 0;
    return 1;
}


static void NortonSetupVolume(VolHead *vh, Volume *vp, int volindex) {
    // This turns out to be a global since its static, but if the
    // compiler semantics ever change, we need to move it.
    static struct volHeader header;

    bzero((void *)&header, sizeof(struct volHeader));
    bcopy((const void *)vh->data.volumeInfo, (void *)&header.diskstuff, 
	  sizeof(VolumeDiskData));
    header.back = vp;
    
    bzero((void *)vp, sizeof(Volume));
    vp->hashid = vh->header.id;
    vp->header = &header;
    vp->vol_index = volindex;
}

// Can't re-use code in vol-salvage since this comes from command line.
static void GetSkipVols(int num, VolumeId *ids, char *vol_nums[]) {
    while(num > 0) {
	sscanf(*vol_nums, "%x", ids);
	printf("Skipping volume: 0x%x\n", *ids);
	vol_nums++;
	ids++;
	num--;
    }
}

// In vol-salvage.c  GetSkipVols above builds the correct list.
extern int InSkipVolumeList(VolumeId v, VolumeId *vl, int nvols);

static int HasBackVols(VolumeId *skipvols, int nskipvols) {
    VolumeHeader *header;
    VolHead	 *vol;
    int          i,
		 maxid = GetMaxVolId();
    int 	 ret = FALSE;

    /* Iterate over all of the volumes */
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
	
	if (vol->data.volumeInfo->type == BACKVOL) {
	    // Check if the parent volume is being skipped.  If it is,
	    // the we dont care about it.
	    if (InSkipVolumeList(vol->data.volumeInfo->parentId,
				 skipvols, nskipvols)) {
		continue;
	    } else {
		ret = TRUE;
		printf("backup volume: %s(0x%x)\n",
		       vol->data.volumeInfo->name,
		       vol->data.volumeInfo->id);
	    }
	}
    }

    return ret;
}
	
    
// So we can add volume headers back to LRU.
extern void FreeVolumeHeader(register Volume *vp);

static int load_server_state(char *dump_file) {
    int dump_fd, volindex;
    int res_adm_limit;
    int status;
    VolumeDiskData data;
    VolHead	   vol_head;
    Volume	   *vp;
    Error	   err;
    int 	   vol_type;
    
    if ((dump_fd = open(dump_file, O_RDONLY, 0x600)) < 0) {
	perror(dump_file);
	return 0;
    }

    /* First thing, initiliaze the global server state */
    coda_init();
    
    if (!ReadGlobalState(dump_fd)) {
	fprintf(stderr, "Error reading reinit file, Aborting...\n");
	return 0;
    }
    
    bzero((void *)&vol_head, (int)sizeof(VolHead));
    while (ReadVolHead(dump_fd, &vol_head)) {
	/* Check the magic number */
	if (vol_head.header.stamp.magic != VOLUMEHEADERMAGIC) {
	    fprintf(stderr,
		    "ERROR! Bad volume header magic number, Aborting.\n"); 
	    return 0;
	}

	printf("Reading volume 0x%x\n", vol_head.header.id);
	
	RVMLIB_BEGIN_TRANSACTION(restore);

	if ((vol_type = vol_head.header.type) != RWVOL) {
	    // Pretend that this is a read-write volume, restore
	    // correct type later.
	    vol_head.header.type = RWVOL;
	}

	if ((volindex = NewVolHeader(&vol_head.header, &err)) == -1) {
	    if (err == VVOLEXISTS) {
		fprintf(stderr, "Volume 0X%x already exists!  Aborting\n",
			vol_head.header.id);
	    } else {
		fprintf(stderr,
			"Error code %d while creating volume %x, Aborting\n",
			err, vol_head.header.id);
	    }
	    rvmlib_abort(VFAIL);
	    return 0;
	}


	bzero((void *)&data, (int)sizeof(VolumeDiskData));
	if (!ReadVolDiskData(dump_fd, &data, &res_adm_limit)) {
	    fprintf(stderr, "Aborting...\n");
	    rvmlib_abort(VFAIL);
	    return 0;
	}

	/* Check the magic number */
	if (data.stamp.magic != VOLUMEINFOMAGIC) {
	    fprintf(stderr, "ERROR! Bad volume disk data magic number.\n");
	    rvmlib_abort(VFAIL);
	    return 0;
	}

	/* Allocate space for the volume resolution log if needed */
	if (data.ResOn & 4) {
	    data.log = new recov_vol_log(vol_head.header.id, res_adm_limit);
	}

	if (vol_type != RWVOL) {
	    // Pretend we are RW.
	    data.type = RWVOL;
	}

	vol_head.data.volumeInfo = &data;

	ReplaceVolDiskInfo(&err, volindex, &data);
	if (err != 0) {
	    fprintf(stderr,
		    "ReplaceVolDiskInfo returned %d, aborting\n", err);
	    rvmlib_abort(VFAIL);
	    return 0;
	}

	vp = VAttachVolume(&err, vol_head.header.id, V_SECRETLY);
	if (err) {
	    fprintf(stderr,
		    "VAttachVolume returns error %d when attaching 0x%x\n",
		    err, vol_head.header.id);
	    rvmlib_abort(VFAIL);
	    return 0;
	}

	RVMLIB_END_TRANSACTION(flush, &(status));
	if (status != 0) {
	    fprintf(stderr,
		    "Transaction exited with status %d!, aborting\n",
		    status);
	    return 0;
	}

	// If the volume isnt in use, VPutVnode gets upset.
	V_inUse(vp) = TRUE;
	
	if (norton_debug) {
	    printf("\n\n");
	    print_volume(&vol_head);
	    print_volume_details(&vol_head);
	}
	if (!ReadVolVnodes(dump_fd, vp, data.ResOn & 4)) {
	    fprintf(stderr, "Aborting...\n");
	    FreeVolumeHeader(vp);
	    return 0;
	}

	if (vol_type != RWVOL) {
	    // Need to change the volume type.
	    RVMLIB_BEGIN_TRANSACTION(restore);

	    vol_head.header.type = vol_type;
	    vp->header->diskstuff.type = vol_type;
	    rvmlib_modify_bytes(&(SRV_RVM(VolumeList[volindex]).header),
				&vol_head.header, 
				sizeof(struct VolumeHeader));
	    ReplaceVolDiskInfo(&err, volindex, &vp->header->diskstuff);
	    RVMLIB_END_TRANSACTION(flush, &(status));
	    if (status != 0) {
		fprintf(stderr,
			"Transaction exited with status %d!, aborting\n",
			status);
		return 0;
	    }
	}
	    
	    
	FreeVolumeHeader(vp);
	bzero((void *)&vol_head, (int)sizeof(VolHead));

	// Truncate the RVM log.
	rvm_truncate();
    }

    return 1;
}

static int dump_server_state(char *dump_file, char *skipvollist[], int nskipvols) {
    VolumeHeader *header;
    VolHead	 *vol;
    VolumeId	 *skipvols = NULL;
    int		 i,
		 maxid = GetMaxVolId();
    int 	 dump_fd;

    if (nskipvols > 0) {
	skipvols = (VolumeId *)malloc(nskipvols * sizeof(VolumeId));
	GetSkipVols(nskipvols, skipvols, skipvollist);
    }
	
    if (HasBackVols(skipvols, nskipvols)) {
	fprintf(stderr, "This server has backup volumes.  They must be ");
	fprintf(stderr, "purged before proceeding\n");
	if (skipvols) free(skipvols);
	return 0;
    }
    
    if ((dump_fd = open(dump_file, O_CREAT | O_EXCL | O_WRONLY, 0600)) < 0) {
	perror(dump_file);
	if (skipvols) free(skipvols);
	return 0;
    }

    if (!DumpGlobalState(dump_fd)) {
	fprintf(stderr, "Error writing reinit file, Aborting...\n");
	if (skipvols) free(skipvols);
	return 0;
    }
    
    /* Iterate over all of the volumes */
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
	
	// Do we need to skip this volume?
	if (InSkipVolumeList(vol->data.volumeInfo->id, skipvols, nskipvols))
	    continue;

	// Check that its not a backup of something we are skipping
	// This check should probably go in InSkipVolumeList()
	if ((vol->data.volumeInfo->type == BACKVOL) && 
	    InSkipVolumeList(vol->data.volumeInfo->parentId, 
			     skipvols, nskipvols)) {
	  continue;
	}
	    
	printf("Writing volume 0x%x\n", vol->data.volumeInfo->id);

	if (!DumpVolHead(dump_fd, vol) ||
	    !DumpVolDiskData(dump_fd, vol->data.volumeInfo) ||
	    !DumpVolVnodes(dump_fd, &vol->data, i)) {
	    fprintf(stderr, "Aborting...\n");
	    if (skipvols) free(skipvols);
	    return 0;
	}
    }

    if (skipvols) free(skipvols);
    return 1;
}




int main(int argc, char * argv[]) {
    char *rvm_log;
    char *rvm_data;
    int  data_len;
    int  ok;
    char *dump_file;
    rvm_return_t 	err;
    
    if (argc < 7) {
	usage(argv[0]);
	exit(1);
    }
    
    if (strcmp(argv[1], "-rvm")) {
	usage(argv[0]);
	exit(1);
    } else {
	rvm_log  = argv[2];
	rvm_data = argv[3];
	data_len = atoi(argv[4]);
    }
    
    if (!strcmp(argv[5], "-dump")) {
	if (argc > 8) {
	    if (strcmp(argv[7], "skip")) {
		usage(argv[0]);
		exit(1);
	    }
	}
	NortonInit(rvm_log, rvm_data, data_len);
	ok = dump_server_state(argv[6], &argv[8], argc - 8);
    } else if (!strcmp(argv[5], "-load")) {
	NortonInit(rvm_log, rvm_data, data_len);
	ok = load_server_state(argv[6]);
    } else {
	usage(argv[0]);
	exit(1);
    }
    
    err = rvm_terminate();
    if (ok) exit(0);
    else exit (1);
}





