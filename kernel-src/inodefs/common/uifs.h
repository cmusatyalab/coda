#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

/* definitions specifically needed for the user level inode filesystem */ 

#define	FNAMESIZE	256
#define MAX_NODES	99999
#define	MAX_LINKS	999
#define	FILEDATA	36
#define DATAOFFSET	(sizeof(long)*4)

#define VICEMAGIC   47114711




struct i_header {
    long    lnk;
	long	volume;
	long	vnode;
	long	unique;
	long	dataversion;
    long    magic;
};



int get_header(struct i_header *header, ino_t ino);
static int put_header(struct i_header *header, ino_t ino);
static int set_link(long *count, ino_t ino);
ino_t maxino();
static int inosort(const struct dirent **a, const struct dirent **b);


