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

/* include file to make definitions available in auth2.cc */
#ifndef _PWSUPPORT_H
#define _PWSUPPORT_H

/* Coda Password support stuff */

extern RPC2_EncryptionKey FileKey;	/* unsigned char causes initialization probs */
extern char *PWFile;     /* name of password file */

extern void InitPW(int firsttime);
extern long PWGetKeys(RPC2_CountedBS *cIdent, RPC2_EncryptionKey hKey, RPC2_EncryptionKey sKey);
extern long PWChangePasswd(RPC2_Handle cid, RPC2_Integer viceId, RPC2_String Passwd);
extern long PWNewUser(RPC2_Handle cid, RPC2_Integer viceId, RPC2_EncryptionKey initKey, RPC2_String otherInfo);
extern long PWDeleteUser(RPC2_Handle cid, RPC2_Integer viceId);
extern long PWChangeUser(RPC2_Handle cid, RPC2_Integer viceId, RPC2_EncryptionKey newKey, RPC2_String otherInfo);

#endif
