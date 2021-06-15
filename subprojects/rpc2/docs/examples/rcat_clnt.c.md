---
title: rcat_clnt.c
---
``` c
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <assert.h>
#include "lwp.h"
#include "rpc2.h"
#include "rcat.h"

void main(int argc, char *argv[])
{
    RPC2_Handle the_connection;
    char error_msg[100];
    char local_cmd[100];
    int return_code;
    char *remotefilename, *servername, localfilename[30];
    SE_Descriptor sed;

    /* get a unique temporary file name */
    if (argc != 3)
        error_report("usage: rcat <server name> <remote file name>");
    servername     = argv[1];
    remotefilename = argv[2];
    sprintf(localfilename, "/tmp/rcat.%d", getpid());

    Init_RPC();

    the_connection = make_connection(servername);

    sed.Tag                                            = SMARTFTP;
    sed.Value.SmartFTPD.Tag                            = FILEBYNAME;
    sed.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0644;
    sed.Value.SmartFTPD.TransmissionDirection          = SERVERTOCLIENT;
    sed.Value.SmartFTPD.ByteQuota                      = -1;
    strcpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, localfilename);
    /* set the side effect descriptor to store the fetched file in filename */
    return_code = Fetch_Remote_File(the_connection, remotefilename, &sed);

    if (return_code == RCAT_FAILED) {
        sprintf(error_msg, "Failed to get the file %s  on machine %s\n",
                argv[2], argv[1]);
        error_report(error_msg);
    } else {
        sprintf(local_cmd, "cat %s", localfilename);
        system(local_cmd); /* display the fetched file to the standard output */
        sprintf(local_cmd, "rm -f %s", localfilename);
        system(local_cmd); /* remove the temporary tile */
    };
    return_code = RPC2_Unbind(the_connection);
    if (return_code != RPC2_SUCCESS) {
        sprintf(error_msg, "%s\nCant' close the connection!",
                RPC2_ErrorMsg(return_code));
        error_report(error_msg);
    };
}

void error_report(char *message)
{
    fprintf(stderr, message);
    fprintf(stderr, "\n");
    exit(1);
}

void Init_RPC()
{
    PROCESS mylpid;
    SFTP_Initializer sftpi;
    int return_code;
    char error_msg[100];

    if (LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, &mylpid) != LWP_SUCCESS)
        error_report("Can't Initialize LWP"); /* Initialize LWP package */

    SFTP_SetDefaults(&sftpi);
    SFTP_Activate(&sftpi); /* Initialize side effect SFTP package */

    /* Initialize RPC2 package */
    return_code = RPC2_Init(RPC2_VERSION, NULL, NULL, 0, -1, NULL);
    if (return_code != RPC2_SUCCESS) {
        sprintf(error_msg, "%s\nCan't Initialize RPC2",
                RPC2_ErrorMsg(return_code));
        error_report(error_msg);
    };
}

/* this routine tries to establish a connection to the server running on
   machine machine_name */
RPC2_Handle make_connection(char *machine_name)
{
    RPC2_HostIdent hident;
    RPC2_PortalIdent pident;
    RPC2_SubsysIdent sident;
    RPC2_Handle the_connection;
    char error_msg[100];
    int return_code;
    RPC2_BindParms bind_parms;

    hident.Tag = RPC2_HOSTBYNAME;
    if (strlen(machine_name) >= 64) {
        sprintf(error_msg, "Machine name %s too long!", machine_name);
        error_report(error_msg);
    };
    strcpy(hident.Value.Name, machine_name);

    pident.Tag                  = RPC2_PORTALBYINETNUMBER;
    pident.Value.InetPortNumber = htons(RCAT_PORTAL);

    sident.Tag            = RPC2_SUBSYSBYID;
    sident.Value.SubsysId = RCAT_SUBSYSID;

    bind_parms.SecurityLevel  = RPC2_OPENKIMONO;
    bind_parms.EncryptionType = NULL;
    bind_parms.SideEffectType = SMARTFTP;
    bind_parms.ClientIdent    = NULL;
    bind_parms.SharedSecret   = NULL;

    return_code = RPC2_NewBinding(&hident, &pident, &sident, &bind_parms,
                                  &the_connection);
    if (return_code != RPC2_SUCCESS) {
        sprintf(error_msg, "%s\nCan't connect to machine %s",
                RPC2_ErrorMsg(return_code), machine_name);
        error_report(error_msg);
    };
    return the_connection;
}
```
