/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#include "coda_assert.h"

#ifdef __CYGWIN32__

#include <windows.h>
/*


 void* malloc(long size){  
        return (HeapAlloc(GetProcessHeap(), 0, size));
 }

 void free(void *p){
	 HeapFree(GetProcessHeap(), 0, p); 
 }
 
*/

#endif
