/* sysctl entries for Coda! */
#include <linux/config.h>
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

#include <linux/utsname.h>

#include "namecache.h"
#include "sysctl.h"
extern int coda_debug;
extern int cfsnc_use;
extern int coda_print_entry;
extern int cfsnc_flushme;
extern int cfsnc_procsize;
extern void cfsnc_flush(void);
void coda_sysctl_init(void);
void coda_sysctl_clean(void);

int coda_dointvec(ctl_table *table, int write, struct file *filp,
                  void *buffer, size_t *lenp);

struct ctl_table_header *fs_table_header, *coda_table_header;
#define FS_CODA         1       /* Coda file system */

#define CODA_DEBUG  	1	    /* control debugging */
#define CODA_ENTRY	    2       /* control enter/leave pattern */
#define CODA_SYSFLUSH      3       /* flush the cache on next lookup */
#define CODA_MC         4       /* use/do not use the minicache */
#define CODA_PROCSIZE   5       /* resize the cache on next lookup */



static ctl_table coda_table[] = {
	{CODA_DEBUG, "debug", &coda_debug, sizeof(int), 0644, NULL, &coda_dointvec},
	{CODA_ENTRY, "printentry", &coda_print_entry, sizeof(int), 0644, NULL, &coda_dointvec},
	{CODA_MC, "minicache", &cfsnc_use, sizeof(int), 0644, NULL, &coda_dointvec},
	{CODA_SYSFLUSH, "flushme", &cfsnc_flushme, sizeof(int), 0644, NULL, &coda_dointvec},
	{CODA_PROCSIZE, "resize", &cfsnc_procsize, sizeof(int), 0644, NULL, &coda_dointvec},
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
                        while (left && isspace(get_user((char *) buffer)))
                                left--, ((char *) buffer)++;
                        if (!left)
                                break;
                        neg = 0;
                        len = left;
                        if (len > TMPBUFLEN-1)
                                len = TMPBUFLEN-1;
                        memcpy_fromfs(buf, buffer, len);
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
                        memcpy_tofs(buffer, buf, len);
                        left -= len;
                        buffer += len;
                }
        }

        if (!write && !first && left) {
                put_user('\n', (char *) buffer);
                left--, buffer++;
        }
        if (write) {
                p = (char *) buffer;
                while (left && isspace(get_user(p++)))
                        left--;
        }
        if (write && first)
                return -EINVAL;
        *lenp -= left;
        filp->f_pos += *lenp;
        return 0;
}


