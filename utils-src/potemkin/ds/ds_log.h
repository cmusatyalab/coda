#ifndef _DS_LOG_H_
#define _DS_LOG_H_

/* uses varargs */
#include <stdarg.h>

#include <odytypes.h>

/*
 * ds_log.h: public interface to the logging data structure
 */


/* 
 * An abstract log:
 *     A log consists of the following abstract entities:
 *           A level of detail, "log_level"
 *           A stream to which output should be sent
 *           A level of detail above which each statement should be
 *              immediately flushed.
 *           A (potentially empty) text name
 *
 * A log's current level of detail describes how "verbose" the logs
 * should be.  Each logging statement should have a detail associated
 * with it; only statements whose level of detail are less than or
 * equal to the log's level are printed.  Others are (quickly) ignored.
 * If a log's level is above the flush level, each statement is followed
 * by an explicit flush on the stream.
 *
 * Each logging statment is stamped with the time of day, and day changes
 * are noted in the log.  Each logging statement is also stamped
 * with the log's name, to allow multiple logs to (unambiguously) share
 * the same stream.  These routines are not thread safe.
 */

typedef struct ds_log_t ds_log_t;  /* logs are opaque */

/*** Observers ***/

extern bool
ds_log_valid(ds_log_t *lp);

/*** Mutators ***/

extern ds_log_t *
ds_log_create(int loglevel, FILE *fp, int flushlevel, char *name);

extern FILE *
ds_log_destroy(ds_log_t *lp);

extern void
ds_log_setlevel(ds_log_t *lp, int level);

/* 
 * printmsg appends a newline: you don't need one.
 * It's not a good idea to call this directly.  Use DS_LOG_MSG
 * instead;
 */

extern void
ds_log_printmsg(ds_log_t *lp, int level, char *fmt, ...);

/* Use the macro, DS_LOG_MSG to do logging: this will allow you to turn
   off logging in performance-sensitive spots. */

#define DS_LOG_MSG(args) \
      ds_log_printmsg args

#endif /* _DS_LOG_H_ */
