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

/*
 *
 * olist.c -- Implementation of olist type.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
}
#endif

#include "coda_assert.h"
#include "olist.h"



/* DEBUGGING! -JJK */
/*
  extern FILE *logFile;
  extern void Die(char * ...);
  */


olist::olist() {
    tail = 0;
    cnt = 0;
}


olist::olist(olist& ol) {
    abort();
}


int olist::operator=(olist& ol) {
    abort();
    return(0);  /* to keep C++ happy!! */
}


olist::~olist() {
    /* This is dangerous! */
    /* Perhaps we should abort() if count() != 0?  -JJK */
    
    clear();
}


void olist::insert(olink *p) {
    if (p->next != 0) abort();
    /*	{ print(logFile); p->print(logFile); Die("olist::insert: p->next != 0"); }*/

    if (tail !=	0) {	// at least one entry exists
        p->next = tail->next;
        tail->next = p;
    }
    else {		// no existing entries
	p->next = p;
        tail = p;
    }

    cnt++;
}


void olist::append(olink *p) {
    if (p->next != 0) abort();
    /*	{ print(logFile); p->print(logFile); Die("olist::append: p->next != 0"); }*/

    if (tail !=	0) {	// at least one entry exists
	p->next = tail->next;
        tail->next = p;
        tail = p;
    }
    else {		// no existing entries
	p->next = p;
        tail = p;
    }

    cnt++;
}


olink *olist::remove(olink *p) {
    olink *q, *prev;
    
    if (tail ==	0) 
	return(0);	    // empty list

    prev = tail;
    q = tail->next;
    while ( q != tail && q != p) {
	prev = q;
	q = q->next;
    }

    // 3 cases: p found and p is not the only element in olist
    //          p found and p is the only element in the olist
    //          p not found
	
    if ( q == p ) {
	prev->next = p->next;
	p->next = 0;
	cnt--;
	if (tail == p) { // we have removed tail, reassign or 
			 // set tail = 0 if olist is now empty
	    tail = ( prev == tail ) ? 0 : prev; 
	}
	return p;
    } else {
	return(0);      // not found 
    }
    // not reached
}



olink *olist::first() {
    if (tail ==	0) return(0);	    // empty list

					   return(tail->next);
}


olink *olist::last() {
    return(tail);
}


olink *olist::get() {
    if (tail ==	0) return(0);	    // empty list
    return(remove(tail->next));
}


void olist::clear() {
    olink *p;
    while ((p = get())) ;
    if (cnt != 0) abort();
    /*	{ print(logFile); Die("olist::clear: cnt != 0 after gets"); }*/
}


int olist::count() {
    return(cnt);
}


int olist::IsMember(olink *p) {
    olist_iterator next(*this);
    olink *ol;
    while ((ol = next()))
	if (ol == p) return(1);
    return(0);
}


void olist::print() {
    print(stderr);
}


void olist::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void olist::print(int fd) {
    /* first print out the olist header */
    char buf[1000];
    sprintf(buf, "this: %p, tail: %p : Default Olist : count = %d\n",
	    this, this->tail, cnt);
    write(fd, buf, strlen(buf));

    /* then print out all of the olinks */
    olist_iterator next(*this);
    olink *p;
    while((p = next())) p->print(fd);
}


/* return pointer to matching object in olist, or NULL */

olink *olist::FindObject(void *tag, otagcompare_t cmpfn) {
    olist_iterator nextobj(*this);
    olink *ol;
    while ((ol = nextobj())){
      if (ol->otagmatch(tag, cmpfn)) return (ol);  /* found it! */
    }
    return(0); /* no matching object */
}



olist_iterator::olist_iterator(olist& l) {
    clist = &l;
    clink = (olink *)-1;
    nlink = (olink *)-1;
}


olink *olist_iterator::operator()() {
    if ( clist->last() == 0 )	{/* empty list */
	clink = (olink *)-1;
	nlink = (olink *)-1;
	return 0;
    }
    
    switch((unsigned int)clink) {
    case -1:		/* state == NOTSTARTED */
	clink = clist->first();
	if ( clink != 0 ) { 
	    nlink = clink->next; /* clink may be deleted before next iter.,
				    keep ptr to next olink now */
	    return clink;
	} else {
	    nlink = (olink *)-1;
	    return 0;
	}

    case 0:		/* not reached */
	return 0;

    default:		/* state == INPROGRESS */
	if (clink == clist->last()) {	/* last already done */
	    clink = (olink *)0;
	    nlink = (olink *)-1;
	    return 0;
	}
	CODA_ASSERT(nlink != (olink *)-1);
	clink = nlink;		/* we saved nlink in last iteration */
	nlink = clink->next;	/* clink may be del. before next iter.,
				   keep ptr to next olink now */
	return clink;
    }

}

/* reset internal state */
void olist_iterator::reset() {
    clink = (olink *)-1;
    nlink = (olink *)-1;
}


olink::olink() {
    next = 0;
}


olink::olink(olink& ol) {
    abort();
}


int olink::operator=(olink& ol) {
    abort();
    return(0);  /* to keep C++ happy!! */
}


olink::~olink() {
}


void olink::print() {
    print(stderr);
}


void olink::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void olink::print(int fd) {
    char buf[80];
    sprintf(buf, "this: %p, next: %p: Default Olink\n",
	    this, this->next);
    write(fd, buf, strlen(buf));
}


/* test an object of arbitrary class derived from olink for matching tag;
   return 1 if tag matches, 0 otherwise */
int olink::otagmatch(void *testtag, otagcompare_t cmpfn ){
  int result = 0;

  result = (*cmpfn)(this, testtag); 
  return(result);
}


