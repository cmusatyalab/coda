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





#ifndef _VVLIST_H_
#define _VVLIST_H_ 1
    
#include <vcrcommon.h>
#include <cvnode.h>
    
typedef struct vventry {
    int isThere;	/* We have seen an existing vnode for this entry */
    long unique;
    ViceStoreId StoreId;
    struct vventry *next;
    unsigned int dumplevel; /* dumplevel at which this vnode was last dumped */
} vvent;

#define ENDLARGEINDEX "End of the Large Vnode List.\n"
#define LISTLINESIZE 160

class vvtable {
    friend class vvent_iterator;
    vvent **vvlist;
    int nlists;
    
  public:
    vvtable(FILE *Ancient, VnodeClass vclass, int listsize);
    ~vvtable();
    int IsModified(int vnodeNumber, long unique, ViceStoreId *StoreId,
		   unsigned int current_dumplevel,
		   unsigned int *last_dumplevel);
};


// Iterate through 1 list! Just one!
class vvent_iterator {
    vvent *cvvent;		// current olist
	
  public:
    vvent_iterator(vvtable&, int);
    vvent *operator()();	// return next object or 0
};

extern int  ValidListVVHeader(FILE *, Volume *, int *);
extern void DumpListVVHeader(int, Volume *vp, unsigned int dumplevel, int);
extern void ListVV(int fd, int vnode, VnodeDiskObject *vnp,
		   unsigned int dumplevel);
extern void getlistfilename(char *, VolumeId, VolumeId, const char *);

#endif /* _VVLIST_H_ */
