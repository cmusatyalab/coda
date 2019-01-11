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

#ifndef _RES_FORCE_H
#define _RES_FORCE_H 1
#include "olist.h"
typedef enum
{
    CreateD = 0,
    CreateF = 1,
    CreateS = 2,
    CreateL = 3
} dirop_t;

class diroplink : public olink {
public:
    dirop_t op;
    long vnode;
    long unique;
    char name[DIROPNAMESIZE];

    diroplink(dirop_t, long, long, char *);
    ~diroplink();
    void hton();
    void ntoh();
    int write(int);
};

/* 
class commitlink : public olink {
  public:
    ViceFid	Fid;
    Vnode 	*vptr;

    commitlink(ViceFid *, Vnode *);
    ~commitlink();
};
*/

/* getdiropParm - struct to pack all the arguments to the enumerate dir proc*/
struct getdiropParm {
    Volume *volptr;
    olist *oplist;
};

extern void UpdateRunts(res_mgrpent *, ViceVersionVector **, ViceFid *);
extern int RuntExists(ViceVersionVector **, int, int *, int *);

#endif /* _RES_FORCE_H_ */
