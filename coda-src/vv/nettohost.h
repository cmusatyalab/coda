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






#ifndef _NETTOHOST_H_ 
#define _NETTOHOST_H_ 1

void ntohfid(ViceFid *, ViceFid *);
void htonfid(ViceFid *, ViceFid *);
void ntohsid(ViceStoreId *, ViceStoreId *);
void htonsid(ViceStoreId *, ViceStoreId *);
void ntohvv(ViceVersionVector *, ViceVersionVector *);
void htonvv(ViceVersionVector *, ViceVersionVector *);
 
#endif /* _NETTOHOST_H_ */
