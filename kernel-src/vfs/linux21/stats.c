/*
 * stats.c
 * 
 * CODA operation statistics
 *
 * (c) March, 1998 Zhanyong Wan <zhanyong.wan@yale.edu>
 *
 */

#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/sysctl.h>
#include <linux/swapctl.h>
#include <linux/proc_fs.h>
#include <linux/malloc.h>
#include <linux/stat.h>
#include <linux/ctype.h>
#include <asm/bitops.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/utsname.h>

#include <linux/coda.h>
#include <linux/coda_linux.h>
#include <linux/coda_fs_i.h>
#include <linux/coda_psdev.h>
#include <linux/coda_cache.h>
#include <linux/coda_proc.h>

struct coda_vfs_stats		coda_vfs_stat;
struct coda_permission_stats	coda_permission_stat;
struct coda_cache_inv_stats	coda_cache_inv_stat;
struct coda_upcall_stats_entry coda_upcall_stat[CODA_NCALLS];

/* keep this in sync with coda.h! */
char *coda_upcall_names[] = {
	"totals      ",   /*  0 */
	"noop        ",   /*  1 */
	"root        ",   /*  2 */
	"sync        ",   /*  3 */
	"open        ",   /*  4 */
	"close       ",   /*  5 */
	"ioctl       ",   /*  6 */
	"getattr     ",   /*  7 */
	"setattr     ",   /*  8 */
	"access      ",   /*  9 */
	"lookup      ",   /* 10 */
	"create      ",   /* 11 */
	"remove      ",   /* 12 */
	"link        ",   /* 13 */
	"rename      ",   /* 14 */
	"mkdir       ",   /* 15 */
	"rmdir       ",   /* 16 */
	"readdir     ",   /* 17 */
	"symlink     ",   /* 18 */
	"readlink    ",   /* 19 */
	"fsync       ",   /* 20 */
	"inactive    ",   /* 21 */
	"vget        ",   /* 22 */
	"signal      ",   /* 23 */
	"replace     ",   /* 24 */
	"flush       ",   /* 25 */
	"purgeuser   ",   /* 26 */
	"zapfile     ",   /* 27 */
	"zapdir      ",   /* 28 */
	"zapvnode    ",   /* 28 */
	"purgefid    ",   /* 30 */
	"open_by_path"    /* 31 */
};




void reset_coda_vfs_stats( void )
{
	memset( &coda_vfs_stat, 0, sizeof( coda_vfs_stat ) );
}

#if 0
static void reset_upcall_entry( struct coda_upcall_stats_entry * pentry )
{
	pentry->count = 0;
	pentry->time_sum = pentry->time_squared_sum = 0;
}
#endif

void reset_coda_upcall_stats( void )
{
	memset( &coda_upcall_stat, 0, sizeof( coda_upcall_stat ) );
}

void reset_coda_permission_stats( void )
{
	memset( &coda_permission_stat, 0, sizeof( coda_permission_stat ) );
}

void reset_coda_cache_inv_stats( void )
{
	memset( &coda_cache_inv_stat, 0, sizeof( coda_cache_inv_stat ) );
}


void do_time_stats( struct coda_upcall_stats_entry * pentry, 
		    unsigned long runtime )
{
	
	unsigned long time = runtime * 1000 /HZ;	/* time in ms */
	CDEBUG(D_SPECIAL, "time: %ld\n", time);

	if ( pentry->count == 0 ) {
		pentry->time_sum = pentry->time_squared_sum = 0;
	}
	
	pentry->count++;
	pentry->time_sum += time;
	pentry->time_squared_sum += time*time;
}



void coda_upcall_stats(int opcode, long unsigned runtime) 
{
	struct coda_upcall_stats_entry * pentry;
	
	if ( opcode < 0 || opcode > CODA_NCALLS - 1) {
		printk("Nasty opcode %d passed to coda_upcall_stats\n",
		       opcode);
		return;
	}
		
	pentry = &coda_upcall_stat[opcode];
	do_time_stats(pentry, runtime);

        /* fill in the totals */
	pentry = &coda_upcall_stat[0];
	do_time_stats(pentry, runtime);

}

unsigned long get_time_average( const struct coda_upcall_stats_entry * pentry )
{
	return ( pentry->count == 0 ) ? 0 : pentry->time_sum / pentry->count;
}

static inline unsigned long absolute( unsigned long x )
{
	return x >= 0 ? x : -x;
}

static unsigned long sqr_root( unsigned long x )
{
	unsigned long y = x, r;
	int n_bit = 0;
  
	if ( x == 0 )
		return 0;
	if ( x < 0)
		x = -x;

	while ( y ) {
		y >>= 1;
		n_bit++;
	}
  
	r = 1 << (n_bit/2);
  
	while ( 1 ) {
		r = (r + x/r)/2;
		if ( r*r <= x && x < (r+1)*(r+1) )
			break;
	}
  
	return r;
}

unsigned long get_time_std_deviation( const struct coda_upcall_stats_entry * pentry )
{
	unsigned long time_avg;
  
	if ( pentry->count <= 1 )
		return 0;
  
	time_avg = get_time_average( pentry );
	return 
	        sqr_root( (pentry->time_squared_sum / pentry->count) - 
			    time_avg * time_avg );
}

int do_reset_coda_vfs_stats( ctl_table * table, int write, struct file * filp,
			     void * buffer, size_t * lenp )
{
	if ( write ) {
		reset_coda_vfs_stats();
	}
  
	*lenp = 0;
	return 0;
}

int do_reset_coda_upcall_stats( ctl_table * table, int write, 
				struct file * filp, void * buffer, 
				size_t * lenp )
{
	if ( write ) {
		reset_coda_upcall_stats();
	}
  
	*lenp = 0;
	return 0;
}

int do_reset_coda_permission_stats( ctl_table * table, int write, 
				    struct file * filp, void * buffer, 
				    size_t * lenp )
{
	if ( write ) {
		reset_coda_permission_stats();
	}
  
	*lenp = 0;
	return 0;
}

int do_reset_coda_cache_inv_stats( ctl_table * table, int write, 
				   struct file * filp, void * buffer, 
				   size_t * lenp )
{
	if ( write ) {
		reset_coda_cache_inv_stats();
	}
  
	*lenp = 0;
	return 0;
}

int coda_vfs_stats_get_info( char * buffer, char ** start, off_t offset,
			     int length, int dummy )
{
	int len=0;
	off_t begin;
	struct coda_vfs_stats * ps = & coda_vfs_stat;
  
  /* this works as long as we are below 1024 characters! */
	len += sprintf( buffer,
			"Coda VFS statistics\n"
			"===================\n\n"
			"File Operations:\n"
			"\tfile_read\t%9d\n"
			"\tfile_write\t%9d\n"
			"\tfile_mmap\t%9d\n"
			"\topen\t\t%9d\n"
			"\trelase\t\t%9d\n"
			"\tfsync\t\t%9d\n\n"
			"Dir Operations:\n"
			"\treaddir\t\t%9d\n\n"
			"Inode Operations\n"
			"\tcreate\t\t%9d\n"
			"\tlookup\t\t%9d\n"
			"\tlink\t\t%9d\n"
			"\tunlink\t\t%9d\n"
			"\tsymlink\t\t%9d\n"
			"\tmkdir\t\t%9d\n"
			"\trmdir\t\t%9d\n"
			"\trename\t\t%9d\n"
			"\tpermission\t%9d\n"
			"\treadpage\t%9d\n",

			/* file operations */
			ps->file_read,
			ps->file_write,
			ps->file_mmap,
			ps->open,
			ps->release,
			ps->fsync,

			/* dir operations */
			ps->readdir,
		  
			/* inode operations */
			ps->create,
			ps->lookup,
			ps->link,
			ps->unlink,
			ps->symlink,
			ps->mkdir,
			ps->rmdir,
			ps->rename,
			ps->permission,
			ps->readpage );
  
	begin = offset;
	*start = buffer + begin;
	len -= begin;

	if ( len > length )
		len = length;
	if ( len < 0 )
		len = 0;

	return len;
}

int coda_upcall_stats_get_info( char * buffer, char ** start, off_t offset,
				int length, int dummy )
{
	int len=0;
	int i;
	off_t begin;
	off_t pos = 0;
	char tmpbuf[80];
	int tmplen = 0;

	ENTRY;
	/* this works as long as we are below 1024 characters! */
	if ( offset < 80 ) 
		len += sprintf( buffer,"%-79s\n",	"Coda upcall statistics");
	if ( offset < 160) 
		len += sprintf( buffer + len,"%-79s\n",	"======================");
	if ( offset < 240) 
		len += sprintf( buffer + len,"%-79s\n",	"upcall\t\t    count\tavg time(ms)\tstd deviation(ms)");
	if ( offset < 320) 
		len += sprintf( buffer + len,"%-79s\n",	"------\t\t    -----\t------------\t-----------------");
	pos = 320; 
	for ( i = 0 ; i < CODA_NCALLS ; i++ ) {
		tmplen += sprintf(tmpbuf,"%s\t%9d\t%10ld\t%10ld", 
				  coda_upcall_names[i],
				  coda_upcall_stat[i].count, 
				  get_time_average(&coda_upcall_stat[i]),
				  coda_upcall_stat[i].time_squared_sum);
		pos += 80;
		if ( pos < offset ) 
			continue; 
		len += sprintf(buffer + len, "%-79s\n", tmpbuf);
		if ( len >= length ) 
			break; 
	}
  
	begin = len- (pos - offset);
	*start = buffer + begin;
	len -= begin;

	if ( len > length )
		len = length;
	if ( len < 0 )
		len = 0;
	EXIT;
	return len;
}

int coda_permission_stats_get_info( char * buffer, char ** start, off_t offset,
				    int length, int dummy )
{
	int len=0;
	off_t begin;
	struct coda_permission_stats * ps = & coda_permission_stat;
  
	/* this works as long as we are below 1024 characters! */
	len += sprintf( buffer,
			"Coda permission statistics\n"
			"==========================\n\n"
			"count\t\t%9d\n"
			"hit count\t%9d\n",

			ps->count,
			ps->hit_count );
  
	begin = offset;
	*start = buffer + begin;
	len -= begin;

	if ( len > length )
		len = length;
	if ( len < 0 )
		len = 0;

	return len;
}

int coda_cache_inv_stats_get_info( char * buffer, char ** start, off_t offset,
				   int length, int dummy )
{
	int len=0;
	off_t begin;
	struct coda_cache_inv_stats * ps = & coda_cache_inv_stat;
  
	/* this works as long as we are below 1024 characters! */
	len += sprintf( buffer,
			"Coda cache invalidation statistics\n"
			"==================================\n\n"
			"flush\t\t%9d\n"
			"purge user\t%9d\n"
			"zap_dir\t\t%9d\n"
			"zap_file\t%9d\n"
			"zap_vnode\t%9d\n"
			"purge_fid\t%9d\n"
			"replace\t\t%9d\n",
			ps->flush,
			ps->purge_user,
			ps->zap_dir,
			ps->zap_file,
			ps->zap_vnode,
			ps->purge_fid,
			ps->replace );
  
	begin = offset;
	*start = buffer + begin;
	len -= begin;

	if ( len > length )
		len = length;
	if ( len < 0 )
		len = 0;

	return len;
}

