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

extern void CLIENT_InitHostTable(void);
extern int CLIENT_Build(RPC2_Handle, char *, RPC2_Integer, SecretToken *,
			ClientEntry **);
extern void CLIENT_Delete(ClientEntry *);
extern void CLIENT_CleanUpHost(HostTable *);
extern void CLIENT_GetWorkStats(int *, int *, unsigned int);
extern void CLIENT_PrintClients();
extern void CLIENT_CallBackCheck();
int CLIENT_MakeCallBackConn(ClientEntry *Client);

char *ViceErrorMsg(int errorCode);

extern void Die (char *);
extern int GetEtherStats();
extern int InitCallBack();
extern void ViceLog (int ...);
extern void DeleteCallBack(HostTable *, ViceFid *);
extern void BreakCallBack(HostTable *, ViceFid *);
extern void DeleteVenus (HostTable *);
extern void DeleteFile (ViceFid *);
extern int InitCallBack ();

extern int check_reintegration_retry;

