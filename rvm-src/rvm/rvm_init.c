/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/*
*
*                   RVM Initialization and Termination
*
*/

#include "rvm_private.h"

/* global variables */
extern log_t        *default_log;       /* default log descriptor ptr */
extern int          errno;              /* kernel error number */
extern rvm_bool_t   rvm_utlsw;          /* true if call by rvmutl */

char                rvm_errmsg;         /* internal error message ptr */

/* initialization control */
/* Cannot statically initialize locks with pthreads. */
#ifndef RVM_USEPT
static RVM_MUTEX    init_lock = MUTEX_INITIALIZER;
#else
static RVM_MUTEX    init_lock;
#endif
static rvm_bool_t   inited = rvm_false;     /* initialization complete flag */
static rvm_bool_t   terminated = rvm_false; /* shutdown flag -- no
                                               restart allowed */

/* check that RVM properly initialized (for interface functions) */
rvm_bool_t bad_init(void)
{
    rvm_bool_t      init_val;

    CRITICAL(init_lock,                 /* begin init_lock crit sec */
        {
        init_val = inited;
        });                             /* end init_lock crit sec */

    if (init_val == rvm_true)           /* return reverse sense */
        return rvm_false;
    else
        return rvm_true;
}

/* rvm_initialize */
rvm_return_t rvm_initialize(char *rvm_version, rvm_options_t *rvm_options)
{
    rvm_return_t    retval = RVM_SUCCESS;

#ifdef RVM_USEPT
    /* have to init the init_lock */
    mutex_init(&init_lock);
#endif RVM_USEPT
    rvm_debug(0);                       /* only causes module loading */
    if (strcmp(rvm_version,RVM_VERSION) != 0)
        return RVM_EVERSION_SKEW;       /* version skew */
    CODA_ASSERT(sizeof(rvm_length_t) == sizeof(char *));
    if ((retval=bad_options(rvm_options,rvm_true)) != RVM_SUCCESS)
        return retval;                  /* bad options ptr or record */

    CRITICAL(init_lock,                 /* begin init_lock crit sec */
        {
        if (inited) goto err_exit;      /* did it all already ... */
        if (terminated)
            {
            retval = RVM_EINIT;         /* restart not allowed */
            goto err_exit;
            }

        cthread_init();                 /* init Cthreads */

        /* init basic structures */
        if ((init_utils()) != 0)
            {
            retval =  RVM_EIO;          /* can't get time stamp */
	    printf("Error in init_utils\n");
            goto err_exit;
            }
        init_map_roots();               /* mapping list and tree */
        init_log_list();                /* log device list */

        /* process options */
        if ((retval=do_rvm_options(rvm_options)) != RVM_SUCCESS) {
		printf("do_rvm_options failed\n");
		goto err_exit;
	}

        /* take care of default log */
        if (default_log == NULL) {
		if ((retval=do_log_options(NULL,NULL)) != RVM_SUCCESS) {
			printf("do_rvm_options failed\n");
			goto err_exit;
		}
	}
        inited = rvm_true;              /* all done */

err_exit:;
        });                             /* end init_lock crit sec */

    return retval;
    }

/* rvm_terminate */
rvm_return_t rvm_terminate(void)
{
    rvm_return_t    retval = RVM_SUCCESS;

    CRITICAL(init_lock,                 /* begin init_lock crit sec */
        {
        if (terminated) goto err_exit;  /* already terminated... */
        if (!inited)
            {
            retval = RVM_EINIT;         /* RVM not initialized */
            goto err_exit;
            }

        /* close log devices (will check for active transactions) */
        if ((retval=close_all_logs()) != RVM_SUCCESS)
            goto err_exit;

        /* close segment devices */
        if ((retval=close_all_segs()) != RVM_SUCCESS)
            goto err_exit;

        /* abort any further RVM function calls */
        inited = rvm_false;
        terminated = rvm_true;

err_exit:;
        });                             /* end init_lock crit sec */

    return retval;
}
