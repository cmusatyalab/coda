#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/util/dict.h,v 4.3 98/11/02 16:45:45 rvb Exp $";
#endif /*_BLURB_*/







/*
 *
 *    Specification of an abstract dictionary facility.
 *
 *    The main functionality provided by this package over a standard dictionary is
 *    reference counting of dictionary entries.  An entry is removed by invoking its
 *    "suicide" member, and then "releasing" the holder's reference by "putting" the
 *    object back into the dictionary.  When the last reference to a "dying" entry is
 *    released, the object will physically remove itself from the dictionary and invoke
 *    its destructor.
 *
 *    A "helper" class (assocrefs) is also provided which maintains variable-sized
 *    arrays of references to dictionary entries.  It is useful for maintaining complex
 *    relationships between entries of the same or different type.
 *
 *    Notes:
 *       1. This package steals ideas from NIHCL; I did not derive from its base classes
 *          because we would have to buy into the whole library, which is currently
 *          inconvenient.  Eventually, we should probably do that.
 *       2. there is currently no (package-supplied) way to avoid "getting" a dying entry
 *          out of the dictionary or waiting for it to actually die.
 *       3. the package is NOT multi-thread capable (except for coroutines, of course).
 *
 */


#ifndef	_UTIL_DICT_H_
#define	_UTIL_DICT_H_    1


/* Forward declarations. */
class dictionary;
class assockey;
class assocval;
class assoc;
class assocrefs;
class assocrefs_iterator;



#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include "coda_assert.h"
#include <stdlib.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "dlist.h"


/* Should be built upon hash table rather than linked list? */
class dictionary : public dlist {
  protected:
    /* Protected ctor ensures this is abstract class! */
    dictionary() { ; }

  public:
    /* Derivers should not need to redefine these. */
    void Add(assoc *);
    void Remove(assoc *);
    assoc *Find(assockey&);
    void Put(assoc **);
    void Kill(assockey&);

    /* Note that Create() (and a Get() which creates if not found) cannot be defined */
    /* here since we don't know the type of the object that should be constructed. */
};


class assockey {
  protected:
    /* Protected ctor ensures this is abstract class! */
    assockey() { ; }

  public:
    virtual int	operator==(assockey& Key)   /* MUST be redefined by deriver! */
	{ CODA_ASSERT(0); return((char *)this == (char *)&Key); }
};


class assocval {
  protected:
    /* Protected ctor ensures this is abstract class! */
    assocval() { ; }

  public:
};


class assoc : private dlink {
 private:
    dictionary *dict;			    /* dictionary this object belongs to */
    int	refcnt;				    /* keep track of reference holders */
    unsigned dying : 1;			    /* T --> nuke this object when refcnt falls to zero */

  protected:
    assockey *key;
    assocval *val;

    /* Protected ctor ensures this is abstract class! */
    assoc(dictionary& Dict)
	{ dict = &Dict; dict->Add(this); refcnt = 1; dying = 0; }
    virtual ~assoc();			    /* MUST be redefined by deriver! */

  public:
    virtual void Hold();
    virtual void Release();
    virtual void Suicide();

    assockey& Key() { return(*key); }
    assocval& Val() { return(*val); }
    dictionary *Dict() { return(dict); }
    int Refcnt() { return(refcnt); }
    int Dying() { return(dying); }
};


/* Base class for maintaining collections of references to dictionary entries (assocs). */
/* Limited ordering of references can be obtained by "attach"'ing with a specified index. */
const int AR_DefaultInitialSize = 0;
const int AR_DefaultGrowSize = 4;

class assocrefs {
  friend class assocrefs_iterator;

  private:
    int max;				    /* size of assoc (pointer) array */
    int count;				    /* number of non-zero entries */
    int growsize;
    assoc **assocs;

  public:
    assocrefs(int = AR_DefaultInitialSize, int = AR_DefaultGrowSize);
    virtual ~assocrefs();

    virtual void Attach(assoc *, int = -1); /* add a reference (at specified index) */
    virtual void Detach(assoc * =0);	    /* delete a reference (or all) */
    virtual void Kill(assoc * =0);	    /* have a referenced assoc commit suicide (or all) */

    int Max() { return(max); }
    int Count() { return(count); }
    int GrowSize() { return(growsize); }
    assoc **Assocs() { return(assocs); }
    int Index(assoc *);
};


class assocrefs_iterator {
    assocrefs *a;
    int i;

  public:
    assocrefs_iterator(assocrefs&); /* parameter used to be a const -- meo 11/27/91 */
    const assoc *operator()(int * =0);
};

#endif	not _UTIL_DICT_H_
