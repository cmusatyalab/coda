/* Written by Philip A. Nelson, March 5, 2000.

   This code is public domain code. 

*/


/* mkpath.c -- Given a file name, make sure the path to the file
   is made.  The file itself is not made.  It is relative to
   curdir unless the path starts with slash (/).

   Return value:  0 if path is successfuly made or exists and
                    is directories.

		  -1 if path is not made or path contains a file.
		     errno should be set to an approprate value.
*/

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

int
mkpath (const char *name, mode_t mode)
{
    struct stat statbuf;
    char *slash;

    /* Does the name contain a parent path. */
    slash = strrchr(name,'/');
    if (slash == NULL)
        return 0;

    /* It does contain a path.  See if it exists. */
    *slash = 0;
    if (!stat (name, &statbuf)) {
        *slash = '/'; 
        /* Path exists ... check out path. */
        if (S_ISDIR(statbuf.st_mode))
	    return 0;
	errno = ENOTDIR;
	return -1;
    }
    
    /* Name does not exist, try to make it. */
    if (mkpath (name, mode)) {
        /*  Could not make path up to last element. */
        *slash = '/';
        return -1;
    }
    
    /* Add the "current" entry. */
    if (mkdir (name, mode)) {
        /*  Could not make the current entry as a directory. */
      *slash = '/';
      return -1;
    }

    /* Path made successfully. */
    *slash = '/';
    return 0;
}
