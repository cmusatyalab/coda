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

#ifndef	_TEST_RVM_H_
#define _TEST_RVM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#ifdef __cplusplus
}
#endif

/* from util */
#include <util/rvmlib.h>

const double DefaultStaticHeapRatio = 0.8;
const int DataToLogSizeRatio = 4;
const int DefaultDataDeviceSize = 10 * 1024 * 1024; // 10MB

const unsigned long MinDataDeviceSize = 0x080000;
const unsigned long MinLogDeviceSize = MinDataDeviceSize / DataToLogSizeRatio;

const int DFLT_RDSCS = 64;		/* RDS chunk size */
const int DFLT_RDSNL = 16;		/* RDS nlists */


#define INIT_RVM_CONFIG(config) config.configInit = true; \
                                (void*)tmpnam_r(config.LogDevice); \
                                (void*)tmpnam_r(config.DataDevice); \
                                config.staticHeapRatio = DefaultStaticHeapRatio; \
                                config.DataDeviceSize = DefaultDataDeviceSize; \
                                config.LogDeviceSize = config.DataDeviceSize / DataToLogSizeRatio;\
                                

struct rvm_config {
    bool configInit;
    char LogDevice[L_tmpnam];
    unsigned long LogDeviceSize;
    char DataDevice[L_tmpnam];
    unsigned long DataDeviceSize;
    double staticHeapRatio;
};

void RVMInit(struct rvm_config config);
void RVMDestroy(struct rvm_config config);

#endif /* _TEST_RVM_H_ */