/* definitions specifically needed for the simple inode operations */ 

#ifndef SIMPLEIFS_INCLUDED
#define SIMPLEIFS_INCLUDED

#include <voltypes.h>
#include <partition.h>

#define	FNAMESIZE	256
#define MAX_NODES	99999
#define	MAX_LINKS	999
#define	FILEDATA	36

#define VICEMAGIC   47114711

struct part_simple_opts {
    Inode next;
};

#endif
