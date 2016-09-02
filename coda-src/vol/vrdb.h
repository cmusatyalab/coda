/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2016 Carnegie Mellon University
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
#endif

#include <stdio.h>

#include <vcrcommon.h>
#include <vice.h>
#ifdef __cplusplus
}
#endif

#include <ohash.h>
#include <inconsist.h>

#include <vice_file.h>

#define VRDB_PATH   vice_config_path("db/VRDB")
#define VRDB_TEMP   vice_config_path("db/VRDB.new")

#define VRTABHASHSIZE	128

class vrtab_iterator;
class vrent;

class vrtab : public ohashtab {
    friend void PrintVRDB();
    char *name;
    ohashtab namehtb;
  public:
    vrtab(const char *name = "anonymous vrtab");
    ~vrtab();
    void add(vrent *);
    void remove(vrent *);
    vrent *find(VolumeId);
    vrent *find(char *);
    vrent *ReverseFind(VolumeId, int *);
    void clear();
    void print();
    void print(FILE *);
    void print(int);

    int dump(int);
};


/* each vrent is in 2 hash tables; lookup by replicated id and by volume name */
class vrent : public olink {
  public:   /* made public (temporarily?) to avoid multiple header include problems */
    char key[33];
    VolumeId volnum;
    olink	namehtblink;
    /*byte*/unsigned char nServers;
    VolumeId ServerVolnum[VSG_MEMBERS];
    uint32_t unused;

    vrent();
    vrent(vrent&);
    int operator=(vrent&);	    /* not supported! */
    ~vrent();

//  public:
    void GetHosts(unsigned long *) __attribute__((deprecated("Prevents multihomed")));
    int index_by_hostaddr(unsigned long) __attribute__((deprecated("Prevents multihomed")));
    int index_by_serverid(uint8_t serverid);
    int index(void);               /* get the replica index for this server */
    void GetCheckVV(ViceVersionVector *);
    int GetVolumeInfo(VolumeInfo *);
    void hton();
    void ntoh();
    void print();
    void print(FILE *);
    void print(int);

    int dump(int);
};

extern vrtab VRDB;
extern void CheckVRDB();
extern int DumpVRDB(int outfd);
extern int XlateVid(VolumeId *, int * = NULL, int * = NULL);
extern int ReverseXlateVid(VolumeId *, int * = NULL);

#endif /* _VICE_VRDB_H_ */

