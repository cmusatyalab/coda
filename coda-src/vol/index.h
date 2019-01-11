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

#ifndef _INDEX_H_
#define _INDEX_H_ 1

class vindex {
    friend class vindex_iterator;
    Device unix_dev;
    VolumeId vol_id; /* unique volume id of object's volume */
    int vol_index; /* index of object's volume in recoverable storage */
    int vtype; /* vLarge (= 0) or vSmall (= 1) (from cvnode.h) */
    int camindex;

public:
    vindex(VolumeId volid = -1, int vnodetype = -1, Device dev = -1,
           int size = -1, int volindex = -1);
    int operator=(vindex &);
    ~vindex();
    int elts();
    int vnodes();
    int IsEmpty(VnodeId);
    int get(VnodeId, Unique_t, VnodeDiskObject *);
    int oget(bit32, Unique_t, VnodeDiskObject *);
    int put(VnodeId, Unique_t, VnodeDiskObject *);
    int oput(bit32, Unique_t, VnodeDiskObject *);
};

class vindex_iterator {
    vindex *v_ind;
    rec_smolist *vlists;
    int clist;
    int nlists;
    rec_smolist_iterator *nextlink;

public:
    vindex_iterator(vindex &);
    ~vindex_iterator();
    int operator()(VnodeDiskObject *);
};

#endif /* _INDEX_H_ */
