/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-2010 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#include <pthread.h>
#include <rvm/rvm.h>

pthread_t       rvm_pthreadid;
void		*rvm_ptstat;
int             rvm_join_res;

int rvm_lock_free (pthread_mutex_t *m) 
{
    int result = pthread_mutex_trylock(m);
    if (result == 0) pthread_mutex_unlock(m);
    return (result == 0) ? rvm_true : rvm_false;
}

