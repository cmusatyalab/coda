#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/param.h>
#include <stdio.h>

#include <lwp.h>
#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>

#include "admon.h"
#include "adsrv.h"

void Init_RPC(int *);
RPC2_Handle connect_to_machine(char *);
void InformVenusOfOurExistance(char *, int, int);

/* RPC Variables */
extern RPC2_PortalIdent rpc2_LocalPortal;
RPC2_Handle VenusCID = -1;

int mainpid;
int thisPGID;
int thisPID;
char HostName[MAXHOSTNAMELEN];
char ShortHostName[MAXHOSTNAMELEN];
int uid = -1;

void InitPGID() {
#ifdef  __linux__
        (void) setpgrp();
#else
        (void) setpgrp(0, thisPID);
#endif
  thisPGID = thisPID;
}

void InitHostName() {
//  CODA_ASSERT(gethostname(HostName, MAXHOSTNAMELEN) == 0);
//  strcpy(ShortHostName, HostName);
//  for (int i = 0; i < strlen(HostName); i++)
//          if (ShortHostName[i] == '.')
//                  ShortHostName[i] = 0;
  snprintf(HostName, MAXHOSTNAMELEN, "telos.odyssey.cs.cmu.edu");
  printf("InitHostName: hostname = %s\n", HostName);
  fflush(stdout);
}


int main(int argc, char *argv[]) {

InitHostName();
  uid = getuid();
  InitPGID();

  Init_RPC(&mainpid);
printf("Informing Venus on %s for %d and PGID=%d\n",HostName,uid,thisPGID); fflush(stdout);
  InformVenusOfOurExistance(HostName, uid, thisPGID);
printf("Informed Venus\n"); fflush(stdout);

}

/******************************************************
 ***********  RPC2 Initialization Routines  ***********
 ******************************************************/

void Init_RPC(int *mainpid)
{
  char error_msg[BUFSIZ];
  RPC2_PortalIdent *portallist[1], portal1;
  RPC2_SubsysIdent sid;
  long rc ;

  if (LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, (PROCESS *)mainpid) != LWP_SUCCESS) {
    printf("Can't Initialize LWP");   /* Initialize LWP package */
    fflush(stdout);
    exit(-1);
  }

  /* Initialize RPC2 Package */
  portallist[0] = &portal1; 

  portal1.Tag = RPC2_PORTALBYINETNUMBER;
  portal1.Value.InetPortNumber = NULL;

  rc = RPC2_Init(RPC2_VERSION, NULL, portallist, 1, -1, NULL) ;
  if (rc != RPC2_SUCCESS) {
    printf("%s:  Can't Initialize RPC2", RPC2_ErrorMsg((int)rc));
    fflush(stdout);
    exit(-1);
  }

  /* Export Venus subsystem */
  sid.Tag = RPC2_SUBSYSBYID;
  sid.Value.SubsysId = ADMONSUBSYSID;
  rc = RPC2_Export(&sid) != RPC2_SUCCESS ;
  if (rc != RPC2_SUCCESS) {
    printf("%s\nCan't export the advice subsystem", RPC2_ErrorMsg((int)rc));
    fflush(stdout);
    exit(-1);
  }
  //  VenusService_EnqueueRequest = 1; 
}


/* Establish a connection to the server running on machine machine_name */
RPC2_Handle connect_to_machine(char *machine_name)
{
  char error_msg[BUFSIZ];
  RPC2_Handle cid;
  RPC2_HostIdent hid;
  RPC2_PortalIdent pid;
  RPC2_SubsysIdent sid;
  long rc;
  RPC2_BindParms bp;

  hid.Tag = RPC2_HOSTBYNAME;
  if (strlen(machine_name) >= 64) { /* Not MAXHOSTNAMELEN because rpc2.h uses "64"! */
      printf("Machine name %s too long!", machine_name);
      fflush(stdout);
      exit(-1);
    } ;

  strcpy(hid.Value.Name, machine_name);
  pid.Tag = RPC2_PORTALBYINETNUMBER;
//  pid.Value.InetPortNumber = htons(ntohs(ADSRVPORTAL));
  pid.Value.InetPortNumber = PORT_venus;
printf("Connecting to InetPortNumber = %d\n", pid.Value.InetPortNumber);
  sid.Tag = RPC2_SUBSYSBYID;
  sid.Value.SubsysId = ADSRVSUBSYSID; 
printf("Connecting to SubsysID=%d\n", sid.Value.SubsysId); fflush(stdout);

  bp.SecurityLevel = RPC2_OPENKIMONO;
  bp.EncryptionType = NULL;
  bp.SideEffectType = NULL;
  bp.ClientIdent = NULL;
  bp.SharedSecret = NULL;
  rc = RPC2_NewBinding(&hid, &pid, &sid, &bp, &cid);
printf("Returned from NewBinding\n"); fflush(stdout);
  if (rc != RPC2_SUCCESS) {
      char msg[16];

      printf("%s\nCan't connect to machine %s (rc = %d)", 
	       RPC2_ErrorMsg((int)rc), machine_name, rc);
      fflush(stdout);
      exit(-1);
    };
  return(cid);
} ;

void InformVenusOfOurExistance(char *hostname, int uid, int thisPGID) {
  long rc;
  RPC2_Integer Major;
  RPC2_Integer Minor;

  printf("InformVenusOfOurExistance: Binding to venus on %s\n", hostname);
  VenusCID = connect_to_machine(hostname);
  printf("InformVenusOfOurExistance: NewAdviceService(%s, %d, %d, %d, %d, %d)...\n", 
        hostname, uid, rpc2_LocalPortal.Value.InetPortNumber, thisPGID, 
        ADSRV_VERSION, ADMON_VERSION);

  rc = NewAdviceService(VenusCID, (RPC2_String)hostname, (RPC2_Integer)uid, (RPC2_Integer)rpc2_LocalPortal.Value.InetPortNumber, (RPC2_Integer)thisPGID, (RPC2_Integer)ADSRV_VERSION, (RPC2_Integer)ADMON_VERSION, &Major, &Minor);

  if (rc != RPC2_SUCCESS) {
      printf("InformVenusOfOurExistance: ERROR ==> NewAdviceService call failed (%s)\n", 
	     RPC2_ErrorMsg((int)rc));
      printf("\t Check the venus.log file for the error condition.\n");
      printf("\t It is probably version skew between venus and advice_srv.\n");
      printf("Information:  uid=%d, ADSRV_VERSION=%d, ADMON_VERSION=%d\n",
             uid, ADSRV_VERSION, ADMON_VERSION);
      fflush(stdout);
      exit(-1);
  }
  else {
    printf("Version information:\n");
    printf("\tADSRV Version = %d\n",ADSRV_VERSION);
    printf("\tADMON Version = %d\n",ADMON_VERSION);
    fflush(stdout);
  }
}

