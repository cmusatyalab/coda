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

/* lookup.c
   Manual lookup of volume location data base information 
   for a particular volume.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <ctype.h>
#include <sys/param.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/file.h>

#include <unistd.h>
#include <stdlib.h>

#include <netinet/in.h>
#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <lwp/timer.h>
#include <rpc2/rpc2.h>
#include <rpc2/se.h>
#include <util.h>
#include <rvmlib.h>

#include <volutil.h>

#ifdef __cplusplus
}
#endif

#include <voltypes.h>
#include <vice.h>
#include <cvnode.h>
#include <volume.h>
#include <camprivate.h>
#include <al.h>
#include <voldefs.h>
#include <vldb.h>
#include <partition.h>
#include <viceinode.h>
#include <vutil.h>
#include <index.h>
#include <volhash.h>
#include <coda_globals.h>

#define INFOFILE    "/tmp/vollookup.tmp"
static FILE * infofile;    // descriptor for info file

struct hostent *gethostent();

char *voltypes[] = {"read/write", "read only", "backup", "unknown type", "unknown type"};

/*
  S_VolLookup: Return information for a volume specified 
  by name or volume-id
*/
long int S_VolLookup(RPC2_Handle rpcid, RPC2_String formal_vol, SE_Descriptor *formal_sed) {
    VolumeInfo info;
    Error error = 0;
    int status = 0;
    long rc = 0;
    SE_Descriptor sed;
    ProgramType *pt;

    /* To keep C++ 2.0 happy */
    char *vol = (char *)formal_vol;

    LogMsg(9, VolDebugLevel, stdout, "Entering S_VolLookup(%u, %s)", rpcid, vol);
    CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);


    VInitVolUtil(volumeUtility);

    infofile = fopen (INFOFILE, "w");

    /* See if user passed in volid rather than volname */
    long volid, index = 0;
    if ((sscanf(vol, "%lX", &volid) ==  1) && ((index = HashLookup(volid)) > 0)) {
	VolumeDiskData *vp = SRV_RVM(VolumeList[index]).data.volumeInfo;
	VGetVolumeInfo(&error, vp->name, &info);
    } else {
	VGetVolumeInfo(&error, vol, &info);
    }

    if (error) {
	LogMsg(0, VolDebugLevel, stdout, "SVolLookup: error code %d returned for volume \"%s\"",
		    error, vol);
	goto exit;
    }    else {
        register VolumeId *p;
	int printed, i;
	register RPC2_Unsigned *sptr;
	RPC2_Unsigned s;
	fprintf(infofile, "Info for vol \"%s\": volume id %lx, %s volume\n", vol, info.Vid, voltypes[info.Type]);
    	fprintf(infofile, "Associates: ");
	for (printed=0, p = &info.Type0, i = 0; i<MAXVOLTYPES; i++, p++) {
	    if (*p) {
		if (printed)
		    fprintf(infofile, ",");
		fprintf(infofile, " %s volume %lx", voltypes[i], *p);
		printed++;
	    }
	}
	fprintf(infofile, "\nOn servers: ");
	for (i = 0, sptr = &info.Server0; i< (int) info.ServerCount; i++,sptr++) {
	    struct hostent *h;
	    s = htonl(*sptr);
	    h = gethostbyaddr((char *)&s, sizeof(s), AF_INET);
	    if (h)
	        fprintf(infofile, "%s", h->h_name);
	    if (i < (int)info.ServerCount-1)
	        fprintf(infofile, ", ");
	}
	fprintf(infofile, "\n");
    }

    fclose(infofile);

    /* set up SE_Descriptor for transfer */
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.Tag = FILEBYNAME;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    strcpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, INFOFILE);
    sed.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0755;

    if ((rc = RPC2_InitSideEffect(rpcid, &sed)) <= RPC2_ELIMIT) {
	LogMsg(0, VolDebugLevel, stdout, "VolLookup: InitSideEffect failed with %s", RPC2_ErrorMsg(rc));
	rc = VFAIL;
	goto exit;
    }

    if ((rc = RPC2_CheckSideEffect(rpcid, &sed, SE_AWAITLOCALSTATUS)) <=
		RPC2_ELIMIT) {
	LogMsg(0, VolDebugLevel, stdout, "VolLookup: CheckSideEffect failed with %s", RPC2_ErrorMsg(rc));
	rc = VFAIL;
	goto exit;
    }

 exit: 
    VDisconnectFS();

    if (status)
	LogMsg(0, VolDebugLevel, stdout, "SVolLookup failed with %d", status);
    else
	LogMsg(9, VolDebugLevel, stdout, "SVolLookup returns %s", RPC2_ErrorMsg(rc));

    return (status?status:rc);

}

