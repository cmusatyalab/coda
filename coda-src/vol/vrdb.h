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







#ifndef _VICE_VRDB_H_
#define	_VICE_VRDB_H_	1

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>

#include <vcrcommon.h>
#include <vice.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <ohash.h>
#include <inconsist.h>


#define VRTABHASHSIZE	128

class vrtab;
class vrtab_iterator;
class vrent;


class vrtab : public ohashtab {
    friend void PrintVRDB();
    char *name;
    ohashtab namehtb;
  public:
    vrtab(char * ="anonymous vrtab");
    ~vrtab();
    void add(vrent *);
    void remove(vrent *);
    vrent *find(VolumeId);
    vrent *find(char *);
    vrent *ReverseFind(VolumeId);
    void clear();
    void print();
    void print(FILE *);
    void print(int);
};


/* each vrent is in 2 hash tables; lookup by replicated id and by volume name */
class vrent : public olink {
  public:   /* made public (temporarily?) to avoid multiple header include problems */
    char key[33];
    VolumeId volnum;
    olink	namehtblink;
    /*byte*/unsigned char nServers;
    VolumeId ServerVolnum[VSG_MEMBERS];
    unsigned long addr;

    vrent();
    vrent(vrent&);
    int operator=(vrent&);	    /* not supported! */
    ~vrent();

//  public:
    void GetHosts(unsigned long *);
    int index(unsigned long);
    void HostListToVV(unsigned long *, vv_t *);
    int GetVolumeInfo(VolumeInfo *);
    void Canonicalize();	// note that the function should not
				// be confused with the global var.
    void hton();
    void ntoh();
    void print();
    void print(FILE *);
    void print(int);
};


extern const char *VRDB_PATH;
extern const char *VRDB_TEMP;
extern vrtab VRDB;
extern void CheckVRDB();
extern int XlateVid(VolumeId *);
extern int XlateVid(VolumeId *, int *, int *);
extern int ReverseXlateVid(VolumeId *);
extern unsigned long XlateVidToVSG(VolumeId);

#endif	not _VICE_VRDB_H_

