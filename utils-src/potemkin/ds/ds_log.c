#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <strings.h>
#include <sys/time.h>

#include <odytypes.h>

#include "ds_log.h"
#include "ds_log.private.h"

const magic_t ds_log_magic = 529146149;

static char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

/* Add a time stamp to a log file line */

static void
TimeStamp(ds_log_t *lp) {
    struct timeval tv;
    struct tm *tm;

    gettimeofday(&tv,NULL);
    tm = localtime(&(tv.tv_sec));

    if ((tm->tm_year > lp->oldyear) || (tm->tm_yday > lp->oldday)) {
	fprintf(lp->fp,"\nDate: %3s %02d/%02d/%02d\n\n", 
		days[tm->tm_wday], tm->tm_mon+1, tm->tm_mday, tm->tm_year);
	lp->oldyear = tm->tm_year;
	lp->oldday = tm->tm_yday;
    }
    fprintf(lp->fp,"%02d:%02d:%02d ", tm->tm_hour, tm->tm_min, tm->tm_sec);
    
    if (lp->taglen != 0) {
	fprintf(lp->fp,"[%s] ",lp->tag);
    }
}

bool
ds_log_valid(ds_log_t *pl) {
    if (DS_LOG_VALID(pl)) {
	return TRUE;
    } else {
	return FALSE;
    }
}

ds_log_t *
ds_log_create(int level, FILE *fp, int flushlevel, char *name) {
    ds_log_t *result;

    ASSERT(fp);
    ASSERT(name);

    ALLOC(result,ds_log_t);
    result->magic = ds_log_magic;
    result->fp = fp;
    result->log_level = level;
    result->flush_level = flushlevel;
    result->oldyear = -1;
    result->oldday = -1;
    result->tag = name;
    result->taglen = strlen(name);
    return result;
}

FILE *
ds_log_destroy(ds_log_t *pl) {
    FILE *result;

    ASSERT(DS_LOG_VALID(pl));
    result = pl->fp;
    
    pl->magic = 0;
    pl->fp = NULL;
    pl->flush_level = pl->oldyear = pl->oldday = 0;
    pl->tag = NULL;

    FREE(pl);
    return result;
}

void
ds_log_setlevel(ds_log_t *pl, int level) {
    ASSERT(DS_LOG_VALID(pl));
    pl->log_level = level;
}

void
ds_log_printmsg(ds_log_t *pl, int level, char *fmt, ...) {
    va_list ap;

    ASSERT(DS_LOG_VALID(pl));

    if (level > pl->log_level) return;
    
    TimeStamp(pl);

    va_start(ap,fmt);
    vfprintf(pl->fp,fmt,ap);
    fprintf(pl->fp,"\n");
    va_end(ap);

    if (level >= pl->flush_level) fflush(pl->fp);
}
	
