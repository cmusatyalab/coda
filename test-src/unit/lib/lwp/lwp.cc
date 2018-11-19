/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2018 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/* external */
#include <gtest/gtest.h>

/* from test */
#include <test/rvm/rvm.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdint.h>
#include <stdlib.h>

/* from lwp */
#include <lwp/lock.h>

#ifdef __cplusplus
}
#endif

PROCESS lwpid;
bool initialized =  false;
uint32_t init_ref_count = 0;
FILE * _logFile = NULL;
rvm_perthread_t rvmptt;

void LWPInit() {
    int lwprc = 0;
    int iomgrrc = 0;

    if (initialized) {
        init_ref_count++;
        return;
    }

    /* Initialize the LWP subsystem. */
    lwprc = LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, &lwpid);
    ASSERT_EQ(lwprc, LWP_SUCCESS) << "LWP_Init failed";

    memset(&rvmptt, 0, sizeof(rvm_perthread_t));

    rvmlib_init_threaddata(&rvmptt);

    iomgrrc = IOMGR_Initialize();
    ASSERT_EQ(iomgrrc, LWP_SUCCESS) << "IOMGR_Initialize failed";

    _logFile = tmpfile();
    ASSERT_TRUE(_logFile) << "Unable to create log file";\

    LWP_SetLog(_logFile, 0);

    initialized = true;
    init_ref_count++;

}

void LWPUnInit() {
    int lwprc = 0;
    int iomgrrc = 0;
    int ret = 0;

    if (initialized) init_ref_count--;

    if (init_ref_count) return;

    ret = fclose(_logFile);
    ASSERT_EQ(ret, 0) << "Unable to close log file";

    iomgrrc = IOMGR_Finalize();
    ASSERT_EQ(iomgrrc, LWP_SUCCESS) << "IOMGR_Finalize failed";

    lwprc = LWP_TerminateProcessSupport();
    ASSERT_EQ(lwprc, LWP_SUCCESS) << "LWP_TerminateProcessSupport failed";


    initialized = false;
}