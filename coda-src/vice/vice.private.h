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






extern int CLIENT_Build(RPC2_Handle, char *, RPC2_Integer, SecretToken *,
			ClientEntry **);
extern void CLIENT_Delete(ClientEntry *);
extern void CLIENT_CleanUpHost(HostTable *);
extern void CLIENT_GetWorkStats(int *, int *, unsigned int);
extern void CLIENT_PrintClients();
extern void CLIENT_CallBackCheck();
HostTable *CLIENT_FindHostEntry(RPC2_Handle CBCid);
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
