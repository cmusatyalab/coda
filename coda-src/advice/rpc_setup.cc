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

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <signal.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdlib.h>

#include <ports.h>
#include <lwp/lwp.h>
#include <rpc2/rpc2.h>
#include "coda_string.h"

#include "admon.h"
#include "adsrv.h"
#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>

#include "advice_srv.h"
#include "console_handler.h"
#include "globals.h"
#include "helpers.h"
#include "rpc_setup.h"


/* RPC Variables */
extern RPC2_PortIdent rpc2_LocalPort;
RPC2_Handle VenusCID = -1;


/******************************************************
 ***********  RPC2 Initialization Routines  ***********
 ******************************************************/

void Init_RPC(int *mainpid)
{
  char error_msg[BUFSIZ];
  RPC2_PortIdent port;
  RPC2_SubsysIdent sid;
  long rc ;

  if (LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, (PROCESS *)mainpid) != LWP_SUCCESS) {
    snprintf(error_msg, BUFSIZ, "Can't Initialize LWP");   /* Initialize LWP package */
    ErrorReport(error_msg);
  }

  /* Initialize RPC2 Package */
  port.Tag = RPC2_PORTBYINETNUMBER;
  port.Value.InetPortNumber = (int)NULL;

  rc = RPC2_Init(RPC2_VERSION, NULL, &port, -1, NULL) ;
  if (rc != RPC2_SUCCESS) {
    snprintf(error_msg, BUFSIZ, "%s:  Can't Initialize RPC2", RPC2_ErrorMsg((int)rc));
    ErrorReport(error_msg);
  }

  /* Export Venus subsystem */
  sid.Tag = RPC2_SUBSYSBYID;
  sid.Value.SubsysId = ADMONSUBSYSID;
  rc = RPC2_Export(&sid) != RPC2_SUCCESS ;
  if (rc != RPC2_SUCCESS) {
    snprintf(error_msg, BUFSIZ, "%s\nCan't export the advice subsystem", RPC2_ErrorMsg((int)rc));
    ErrorReport(error_msg);
  }
  //  VenusService_EnqueueRequest = 1; 
}


/* Establish a connection to the server running on machine machine_name */
RPC2_Handle connect_to_machine(char *machine_name)
{
  char error_msg[BUFSIZ];
  RPC2_Handle cid;
  RPC2_HostIdent hid;
  RPC2_PortIdent pid;
  RPC2_SubsysIdent sid;
  long rc;
  RPC2_BindParms bp;

  hid.Tag = RPC2_HOSTBYNAME;
  if (strlen(machine_name) >= 64) { /* Not MAXHOSTNAMELEN because rpc2.h uses "64"! */
      snprintf(error_msg, BUFSIZ, "Machine name %s too long!", machine_name);
      ErrorReport(error_msg);
    } ;

  strcpy(hid.Value.Name, machine_name);
  pid.Tag = RPC2_PORTBYINETNUMBER;
  pid.Value.InetPortNumber = htons(PORT_venus);
  sid.Tag = RPC2_SUBSYSBYID;
  sid.Value.SubsysId = ADSRVSUBSYSID; 

  bp.SecurityLevel = RPC2_OPENKIMONO;
  bp.EncryptionType = (int)NULL;
  bp.SideEffectType = (int)NULL;
  bp.ClientIdent = NULL;
  bp.SharedSecret = NULL;
  fprintf(stderr, "Returning Binding..."); fflush(stderr);
  rc = RPC2_NewBinding(&hid, &pid, &sid, &bp, &cid);
  fprintf(stderr, "Done.\n"); fflush(stderr);
  if (rc != RPC2_SUCCESS) {
      char msg[16];

      sprintf(msg, "ReallyQuit\n");
      SendToConsole(msg);

      snprintf(error_msg, BUFSIZ, "%s\nCan't connect to machine %s (rc = %d)", 
	       RPC2_ErrorMsg((int)rc), machine_name, rc);
      ErrorReport(error_msg);
    };
  return(cid);
} ;

void InformVenusOfOurExistance(char *hostname, int uid, int thisPGID) {
  long rc;
  RPC2_Integer Major;
  RPC2_Integer Minor;

  LogMsg(1000,LogLevel,LogFile,"InformVenusOfOurExistance: Binding to venus on %s", hostname);
  VenusCID = connect_to_machine(hostname);
  LogMsg(1000,LogLevel,LogFile,
        "InformVenusOfOurExistance: NewAdviceService(%s, %d, %d, %d, %d, %d)...", 
        hostname, uid, rpc2_LocalPort.Value.InetPortNumber, thisPGID, 
        ADSRV_VERSION, ADMON_VERSION);

  rc = C_NewAdviceService(VenusCID, (RPC2_String)hostname, (RPC2_Integer)uid, (RPC2_Integer)rpc2_LocalPort.Value.InetPortNumber, (RPC2_Integer)thisPGID, (RPC2_Integer)ADSRV_VERSION, (RPC2_Integer)ADMON_VERSION, &Major, &Minor);

  VenusMajorVersionNumber = (int)Major;
  VenusMinorVersionNumber = (int)Minor;

  if (rc != RPC2_SUCCESS) {
    char msg[16];

      sprintf(msg, "ReallyQuit\n");
      SendToConsole(msg);

      fprintf(stderr,"InformVenusOfOurExistance:  NewAdviceService call failed\n");
      fprintf(stderr,"\tCheck the venus.log file for the error condition.\n");
      fprintf(stderr,"\tIt is probably version skew between venus and advice_srv.\n");
      fflush(stderr);
      LogMsg(0,LogLevel,LogFile, "InformVenusOfOurExistance: ERROR ==> NewAdviceService call failed (%s)", RPC2_ErrorMsg((int)rc));
      LogMsg(0,LogLevel,LogFile, "\t Check the venus.log file for the error condition.");
      LogMsg(0,LogLevel,LogFile, "\t It is probably version skew between venus and advice_srv.");
      LogMsg(0,LogLevel,EventFile, "NewAdviceService: VersionSkew or UserNotExist");
      LogMsg(0,LogLevel,LogFile, "Information:  uid=%d, ADSRV_VERSION=%d, ADMON_VERSION=%d",
             uid, ADSRV_VERSION, ADMON_VERSION);
      fflush(LogFile);
      fflush(EventFile);
      fclose(LogFile);
      fclose(EventFile);
      kill(interfacePID, SIGKILL);
      exit(-1);
  }
  else {
    LogMsg(0,LogLevel,LogFile,"Version information:");
    LogMsg(0,LogLevel,LogFile,"\tAdvice Monitor Version = %d",ADVICE_MONITOR_VERSION);
    LogMsg(0,LogLevel,LogFile,"\tVenus Version = %d.%d", VenusMajorVersionNumber, VenusMinorVersionNumber);
    LogMsg(0,LogLevel,LogFile,"\tADSRV Version = %d",ADSRV_VERSION);
    LogMsg(0,LogLevel,LogFile,"\tADMON Version = %d\n",ADMON_VERSION);
    fflush(LogFile);
  }
}

