#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 3.1

          Copyright (c) 1987-1995 Carnegie Mellon University
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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/utils-src/mond/bbuf.c,v 3.3 1998/11/30 11:39:54 jaharkes Exp $";
#endif /*_BLURB_*/




// Multi-Threaded bounded buffer.

// bbuf.c
//
// method implementations

// This monitor uses an incredibly simple locking scheme -- only one
// thread can be in any monitor function at a time.  All exported methods of
// the bbuf class are protected by the lock, as they are all rather
// short.  (i.e.: all methods except for bound are protected.)  The only
// methods that require write locks are insert, remove and the destructor.
//
// All procedures except two will unconditionally terminate once the lock has
// been obtained.  The two exceptions are insert() and remove().
//
// Insert:
// insert obtains the lock, and then checks to see if the buffer is full.
// if the buffer is full, insert will release the lock and sleep on the
// condition variable full_tank.  Insert, after inserting, signals low_fuel
// if the number of items exceeds the low fuel mark.  Insert can starve if
// no one ever deletes.
//
// Delete:
// delete obtains the lock, and then checks to see if the number of items 
// in the buffer is less than low_fuel_mark.  If it is, delete releases
// the lock and sleeps on the condition variable low_fuel.  Delete signals
// full_tank whenever it deletes something from the queue.  Delete can
// starve if no one ever inserts.

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdlib.h>
#include "lwp.h"
#include "lock.h"

#ifdef __cplusplus
}
#endif __cplusplus

//#include <stream.h>
#include "mondgen.h"
#include "mond.h"
#include "report.h"
#include "data.h"
#include "bbuf.h"

bbuf::bbuf(int bound, int lfm)
{
  head = 0;
  tail = 0;
  count = 0;
  bnd = bound;
  low_fuel_mark = lfm;
  buf = new bbuf_item[bound];
  low_fuel = new(char);             // set valid addresses to the conditions;
  full_tank = new(char);
  Lock_Init(&lock);                 // initialize the lock;
  dbg = mfalse;
}

bbuf::~bbuf()
{
  ObtainWriteLock(&lock);           // this had better be the only thread;
  delete [] buf;
  delete low_fuel;                  // deallocate memory for free-store;
  delete full_tank;                 // objects;
  ReleaseWriteLock(&lock);
}

BbufStatus bbuf::insert(bbuf_item to_insert)
{
  int sigResult;
  
  ObtainWriteLock(&lock);
  while (count == bnd) {
    ReleaseWriteLock(&lock);
    LWP_WaitProcess(full_tank);
    ObtainWriteLock(&lock);
  }
  buf[tail++] = to_insert;
  tail = tail % bnd;
  count++;
  if (count > low_fuel_mark) {
    sigResult = LWP_SignalProcess(low_fuel);
    if ((sigResult != LWP_SUCCESS) && (sigResult != LWP_ENOWAIT)) {
      bbuf_error("bbuf->insert():",sigResult);
    }
  }
  ReleaseWriteLock(&lock);
  return BBUFOK;
}

BbufStatus bbuf::remove(bbuf_item *result)
{
  int sigResult;

  ObtainWriteLock(&lock);
  while (count <= low_fuel_mark) {
    ReleaseWriteLock(&lock);
    LWP_WaitProcess(low_fuel);
    ObtainWriteLock(&lock);
  }
  *result = buf[head++];
  head = head%bnd;
  count--;
  sigResult = LWP_SignalProcess(full_tank);
  if ((sigResult != LWP_SUCCESS) && (sigResult != LWP_ENOWAIT))
    bbuf_error("bbuf->remove():",sigResult);
  ReleaseWriteLock(&lock);
  return BBUFOK;
}

bool bbuf::full(void)
{
  ObtainReadLock(&lock);
  if (count == bnd) {
    ReleaseReadLock(&lock);
    return mtrue;
  }
  else {
    ReleaseReadLock(&lock);
    return mfalse;
  }
}

bool bbuf::empty(void)
{
  ObtainReadLock(&lock);
  if (count == 0) {
    ReleaseReadLock(&lock);
    return mtrue;
  }
  else {
    ReleaseReadLock(&lock);
    return mfalse;
  }
}

void bbuf::flush_the_tank(void)
{
  int sigResult;
  
  ObtainWriteLock(&lock);
  low_fuel_mark = 0;
  ReleaseWriteLock(&lock);
  sigResult = LWP_SignalProcess(low_fuel);
  if ((sigResult != LWP_SUCCESS) && (sigResult != LWP_ENOWAIT)) {
    bbuf_error("bbuf->insert():",sigResult);
  }
}

void bbuf::bbuf_error(char *string, int err)
{
/*  bool crash = mtrue;

  switch(err) {
  case LWP_SUCCESS:
    cerr << string << " inadvertently called.\n";
    break;
  case LWP_EBADEVENT:
    cerr << string << " signalled/waited event not initialized\n";
    break;
  case LWP_EINIT:
    cerr << string << " lwp package not initialized\n";
    break;
  case LWP_ENOWAIT:
    cerr << string << " signal() expected waiting process; none waiting\n";
    break;
  default:
    cerr << string << " unrecognized error code\n";
    break;
  }
  exit(-1);
*/
}

void bbuf::debug(bool dbg_value) {
  dbg = dbg_value;
}

