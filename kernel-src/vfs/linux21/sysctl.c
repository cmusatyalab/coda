/*
 * Sysctl operations for Coda filesystem
 * Original version: (C) 1996 P. Braam and M. Callahan
 * Rewritten for Linux 2.1. (C) 1997 Carnegie Mellon University
 * 
 * Carnegie Mellon encourages users to contribute improvements to
 * the Coda project. Contact Peter Braam (coda@cs.cmu.edu).
 */
/* sysctl entries for Coda! */

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
#include <linux/coda_sysctl.h>
extern int coda_debug;
/* extern int cfsnc_use; */
extern int coda_print_entry;
/* extern int cfsnc_flushme; */
extern int cfsnc_procsize;
/* extern void cfsnc_flush(void); */
void coda_sysctl_init(void);
void coda_sysctl_clean(void);

int coda_dointvec(ctl_table *table, int write, struct file *filp,
                  void *buffer, size_t *lenp);

struct ctl_table_header *fs_table_header, *coda_table_header;
#define FS_CODA         1       /* Coda file system */

#define CODA_DEBUG  	 1	 /* control debugging */
#define CODA_ENTRY	 2       /* control enter/leave pattern */
#define CODA_TIMEOUT    3       /* timeout on upcalls to become intrble */
#define CODA_MC         4       /* use/do not use the access cache */
#define CODA_HARD       5       /* mount type "hard" or "soft" */



static ctl_table coda_table[] = {
	{CODA_DEBUG, "debug", &coda_debug, sizeof(int), 0644, NULL, &coda_dointvec},
	{CODA_ENTRY, "printentry", &coda_print_entry, sizeof(int), 0644, NULL, &coda_dointvec},
 	{CODA_MC, "accesscache", &coda_access_cache, sizeof(int), 0644, NULL, &coda_dointvec}, 
 	{CODA_TIMEOUT, "timeout", &coda_timeout, sizeof(int), 0644, NULL, &coda_dointvec},
 	{CODA_HARD, "hard", &coda_hard, sizeof(int), 0644, NULL, &coda_dointvec},
	{ 0 }
};


static ctl_table fs_table[] = {
       {FS_CODA, "coda",    NULL, 0, 0555, coda_table},
       {0}
};



void coda_sysctl_init()
{
	fs_table_header = register_sysctl_table(fs_table, 0);
/*	coda_table_header = register_sysctl_table(coda_table, 0);*/
}

void coda_sysctl_clean() {
	/*unregister_sysctl_table(coda_table_header);*/
	unregister_sysctl_table(fs_table_header);
}

int coda_dointvec(ctl_table *table, int write, struct file *filp,
                  void *buffer, size_t *lenp)
{
        int *i, vleft, first=1, len, left, neg, val;
        #define TMPBUFLEN 20
        char buf[TMPBUFLEN], *p;
        
        if (!table->data || !table->maxlen || !*lenp ||
            (filp->f_pos && !write)) {
                *lenp = 0;
                return 0;
        }
        
        i = (int *) table->data;
        vleft = table->maxlen / sizeof(int);
        left = *lenp;
        
        for (; left && vleft--; i++, first=0) {
                if (write) {
		        while (left) {
			        char c;
				if(get_user(c,(char *) buffer))
					return -EFAULT;
				if (!isspace(c))
					break;
				left--;
				((char *) buffer)++;
			}
                        if (!left)
                                break;
                        neg = 0;
                        len = left;
                        if (len > TMPBUFLEN-1)
                                len = TMPBUFLEN-1;
                        if (copy_from_user(buf, buffer, len))
			        return -EFAULT;
                        buf[len] = 0;
                        p = buf;
                        if (*p == '-' && left > 1) {
                                neg = 1;
                                left--, p++;
                        }
                        if (*p < '0' || *p > '9')
                                break;
                        val = simple_strtoul(p, &p, 0);
                        len = p-buf;
                        if ((len < left) && *p && !isspace(*p))
                                break;
                        if (neg)
                                val = -val;
                        buffer += len;
                        left -= len;
                        *i = val;
                } else {
                        p = buf;
                        if (!first)
                                *p++ = '\t';
                        sprintf(p, "%d", *i);
                        len = strlen(buf);
                        if (len > left)
                                len = left;
                        if (copy_to_user(buffer, buf, len))
			    return -EFAULT;
                        left -= len;
                        buffer += len;
                }
        }

        if (!write && !first && left) {
                if(put_user('\n', (char *) buffer))
		        return -EFAULT;
                left--, buffer++;
        }
        if (write) {
                p = (char *) buffer;
                while (left) {
			char c;
			if(get_user(c, p++))
				return -EFAULT;
			if (!isspace(c))
				break;
			left--;
		}
        }
        if (write && first)
                return -EINVAL;
        *lenp -= left;
        filp->f_pos += *lenp;
        return 0;
}


