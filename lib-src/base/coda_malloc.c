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
