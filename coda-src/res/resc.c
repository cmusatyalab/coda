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







/* Simple program to try multicast to the coda servers
   Puneet Kumar, June 1990
*/

#include <stdio.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/time.h>
#include <strings.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <lwp.h>
#include <rpc2.h>
#include "venusioctl.h"
#include "vice.h"
#include "callback.h"
#include "voldefs.h"
#include <ports.h>

#define MAXSRV 8
#define VSG_MEMBERS 8

extern int errno;
struct GetFid {
    ViceFid fid;
    ViceVersionVector vv;
};
ViceFid Fid;
char	mcname[256];
extern void cbserver();

/* get fid of object from venus */
void GetFidFromPath(char *path, ViceFid *fid)
{
    GetFid out;
    bzero(&out, sizeof(struct GetFid));

    struct ViceIoctl vi;
    vi.in = 0;
    vi.in_size = 0;
    vi.out = (char *)&out;
    vi.out_size = sizeof(struct GetFid);

    if (pioctl(path, VIOC_GETFID, &vi, 0) != 0) {
	perror("pioctl: GETFID");
	exit(-1);
    }
    *fid = out.fid;
    return;
}

void InitRPC()
    {
    int pid;
    int rc;


    /* Init RPC2 */
    rc = LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, (PROCESS *)&pid);
    if (rc != LWP_SUCCESS) 
    	{printf("LWP_Init() failed\n"); exit(-1);}

    RPC2_Init(RPC2_VERSION, 0, 0, 0, -1, NULL);

    

    /* Set up callback lwp; port 2115 hack doesn't work in Coda, because
       of a server bug  */
    LWP_CreateProcess((PFIC)cbserver, 8192, LWP_NORMAL_PRIORITY, 0, "cbserver", (PROCESS *)&pid);
    }

void main(int argc, char **argv)
{
    int rc;
    char path[1024];
    char mcname[256];
    ViceFid fid;
    RPC2_HostIdent hi;
    RPC2_PortalIdent pi;
    RPC2_SubsysIdent si;
    RPC2_Handle cid;
    VolumeInfo	Info, Info2;

    InitRPC();

    while(1){
	/* get object path name and server */
	printf("\nObject path name? ");
	scanf("%s", path);
	printf("\nMachine to get volinfo? ");
	scanf("%s", mcname);
	printf("Calling getfid with %s\n", path);
	GetFidFromPath(path, &fid);
	/* bind to a server and get the volume info */
	hi.Tag = RPC2_HOSTBYNAME;
	strcpy(hi.Value.Name, mcname);
	pi.Tag = RPC2_PORTALBYINETNUMBER;
	pi.Value.InetPortNumber = htons(PORT_codasrv); 
	si.Tag = RPC2_SUBSYSBYID;
	si.Value.Id = SUBSYS_SRV;
	rc = RPC2_Bind(RPC2_OPENKIMONO, NULL, &hi, &pi, &si, 
		       NULL, NULL, (RPC2_EncryptionKey)NULL, 
		       &cid);
	if (rc != RPC2_SUCCESS){
	    printf("resc: Couldnt bind (%d)\n", rc);
	    exit(-1);
	}

	char volstring[10];
	sprintf(volstring, "%u", fid.Volume);
	rc = ViceGetVolumeInfo(cid, volstring, &Info);
	if (rc != RPC2_SUCCESS){
	    printf("Couldnt get volume info for %s (%d)\n", 
		   volstring, rc);
	    exit(-1);
	}
	printf("VolumeId: %x\n", Info.Vid);
	printf("Volume Type: %d\n", Info.Type);
	int Nrwid = 0;
	VolumeId rwvid[MAXSRV];
	unsigned int haddr[MAXSRV];
	VolumeId *rwids = (VolumeId *) &(Info.RepVolMap);
	if (Info.Type == replicatedVolume){
	    for (int i = 0; i < MAXSRV; i++){
		if (*rwids){
		    printf("RWVolumeId[%d] = %x\n", i, *rwids);
		    rwvid[Nrwid] = *rwids;
		    Nrwid++;
		}
		rwids++;
	    }
	    for (i = 0; i < Nrwid; i++){
		sprintf(volstring, "%u", rwvid[i]);
		rc = ViceGetVolumeInfo(cid, volstring, &Info2);
		if (rc != RPC2_SUCCESS){
		    printf("Couldnt get vol info for %s(%s)\n",
			   volstring, RPC2_ErrorMsg(rc));
		    RPC2_Unbind(cid);
		    exit(-1);
		}
		printf("VolumeId = %x \nVolume Type = %d\nServerid= %x\n", 
		       Info2.Vid, Info2.Type, Info2.Server0);
		haddr[i] = Info2.Server0;
	    }
	    /* set up connection handles */
	    RPC2_Handle cid_arr[MAXSRV];
	    RPC2_Unsigned rc_arr[MAXSRV];
	    hi.Tag = RPC2_HOSTBYINETADDR;
	    for (i = 0; i < MAXSRV; i++)
		if (i < Nrwid){
		    hi.Value.InetAddress = haddr[i];
		    rc = RPC2_Bind(RPC2_OPENKIMONO, NULL, &hi, &pi, &si, 
				   NULL, NULL, (RPC2_EncryptionKey)NULL, 
				   &cid_arr[i]);
		    if (rc != RPC2_SUCCESS){
			printf("Couldnt bind %s\n", RPC2_ErrorMsg(rc));
			exit(-1);
		    }
		}
		else
		    cid_arr[i]= 0;

	    ViceFid bidfid;
	    bidfid.Volume = 0;
	    unsigned long ph = 1;
	    /* Dummy argument for ACL */
	    RPC2_BoundedBS dummybs, *acl;
	    dummybs.MaxSeqLen = 0;
	    dummybs.SeqLen = 0;
	    acl = &dummybs;

	    /* Status parameters. */
	    ViceStatus status;
	    bzero(&status, sizeof(ViceStatus));


	    /* COP2 Piggybacking. */
	    RPC2_CountedBS PiggyBS;
	    PiggyBS.SeqLen = 0;
	    PiggyBS.SeqBody = 0;

	    ARG_MARSHALL_BS(IN_OUT_MODE, RPC2_BoundedBS, aclvar, *acl, VSG_MEMBERS, 1024);
	    ARG_MARSHALL(OUT_MODE, ViceStatus, statusvar, status, VSG_MEMBERS);
	    /* make the multirpc call */
	    MRPC_MakeMulti(ViceFetch_OP, ViceFetch_PTR, MAXSRV, 
			   cid_arr, rc_arr, NULL, 0, 0, &fid, 
			   &bidfid, FetchNoData, aclvar_ptrs, 
			   statusvar_ptrs, ph, &PiggyBS, 0);
	    
	    ViceVersionVector *vv_ptrs[VSG_MEMBERS];
	    for (i = 0; i < VSG_MEMBERS; i++){
		vv_ptrs[i] = &((statusvar_ptrs[i])->VV);
		int *vv;
		vv = (int *)vv_ptrs[i];
		printf("VV[site %d]\n", i);
		for (int j = 0; j < VSG_MEMBERS; j++)
		    printf("%d ", vv[j]);
		printf("\n");
	    }
	    for (i = 0; i < Nrwid; i++)
		RPC2_Unbind(cid_arr[i]);
	    
	    
	}
	
	else 
	    printf("This is a non replicated Volume \n");
	RPC2_Unbind(cid);

    }
	
}

void cbserver()
    {
    RPC2_SubsysIdent cbs;
    RPC2_PacketBuffer *pkt;
    RPC2_Handle cid;
    int rc;

    cbs.Tag = RPC2_SUBSYSBYID;
    cbs.Value.Id = SUBSYS_CB;
    rc = RPC2_Export(&cbs);
    if (rc != RPC2_SUCCESS)
    	{printf("RPC2_Export: %s\n", RPC2_ErrorMsg(rc)); exit(-1);}
    
    while (1)
	{
	rc = RPC2_GetRequest(0, &cid, &pkt, 0, 0, 0, 0);
	if (rc != RPC2_SUCCESS)
	    {printf("RPC2_ExecuteRequest: %s\n", RPC2_ErrorMsg(rc)); continue;}	    
	rc = cb_ExecuteRequest(cid, pkt, 0);
	if (rc != RPC2_SUCCESS)
	    {printf("cb_ExecuteRequest: %s\n", RPC2_ErrorMsg(rc)); continue;}
	}
    }

long CallBack(RPC2_Handle cid, CallBackFid *fid)
    {
    return(0);
    }

/* None of these should be called -- here for the linker */
long CallBackConnect(RPC2_Handle cid, RPC2_Integer set, 
		      RPC2_Integer sel, RPC2_Integer et, 
		      RPC2_CountedBS *ci)
    {return(0);}

long CallBackConnectBack(RPC2_Handle cid)
    {return(0);}

long CallBackReceivedStore(RPC2_Handle cid, CallBackFid *Fid)
    {return(0);}


int ValidServer(char *s)
    {
    struct hostent *he;
    
    he = gethostbyname(s);
    if (he) return(1);
    else return(0);
    }

