/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 2019 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _VENUSCONF_H_
#define _VENUSCONF_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>

#ifdef __cplusplus
}
#endif

#include <stringkeyvaluestore.h>
#include "dlist.h"
#include "rvmlib.h"

/*  *****  parameter defaults.  ***** */
#define DFLT_VR "/coda" /* venus root */
#define DFLT_CD "/usr/coda/venus.cache" /* cache directory */
/* the next two are relative to cachedir if they do not start with '/' */
#define DFLT_PIDFILE "pid" /* default pid file */
#define DFLT_CTRLFILE "VENUS_CTRL" /* default control file */

#define DFLT_LOGFILE "/usr/coda/etc/venus.log" /* venus log file */
#define DFLT_ERRLOG "/usr/coda/etc/console" /* venus error log */
#define MIN_CS "2MB"

#define UNSET_PRIMARYUSER 0 /* primary user of this machine */

/*  *****  Constants  *****  */

const int DFLT_RT  = 5; /* rpc2 retries */
const int UNSET_RT = 0;
const int DFLT_TO  = 60; /* rpc2 timeout */
const int UNSET_TO = 0;
const int DFLT_WS  = 32; /* sftp window size */
const int UNSET_WS = 0;
const int DFLT_SA  = 8; /* sftp send ahead */
const int UNSET_SA = -1;
const int DFLT_AP  = 8; /* sftp ack point */
const int UNSET_AP = -1;
const int DFLT_PS  = (1024 /*body*/ + 60 /*header*/); /* sftp packet size */
const int UNSET_PS = -1;
const int UNSET_ST = -1; /* do we time rpcs? */
const int UNSET_MT = -1; /* do we time mrpcs? */
#ifdef TIMING
const int DFLT_ST = 1;
const int DFLT_MT = 1;
#else
const int DFLT_ST = 0;
const int DFLT_MT = 0;
#endif
const unsigned int INIT_BW   = 10000000;
const unsigned int UNSET_WCT = 0;
const unsigned int DFLT_WCT  = 50000;
const int UNSET_WCS          = -1;
const int DFLT_WCS           = 1800; /* 30 minutes */

const int DFLT_MAXWORKERS     = 20;
const int UNSET_MAXWORKERS    = -1;
const int DFLT_MAXPREFETCHERS = 1;

const int DFLT_MAXCBSERVERS  = 5;
const int UNSET_MAXCBSERVERS = -1;

/*  *****  Constants  *****  */

const int UNSET_IMD = 0; /* do not initialize meta data */
#define DFLT_RVMT UFS /* meta data store type */
const unsigned long DFLT_VDDS  = 0x400000; /* Venus meta-data device size */
const unsigned long UNSET_VDDS = (unsigned long)-1;
const unsigned long MIN_VDDS   = 0x080000;
const int DataToLogSizeRatio   = 4;
const unsigned long DFLT_VLDS =
    DFLT_VDDS / DataToLogSizeRatio; /* Venus log device size */
const unsigned long UNSET_VLDS  = (unsigned long)-1;
const unsigned long MIN_VLDS    = MIN_VDDS / DataToLogSizeRatio;
const int DFLT_RDSCS            = 64; /* RDS chunk size */
const int UNSET_RDSCS           = -1;
const int DFLT_RDSNL            = 16; /* RDS nlists */
const int UNSET_RDSNL           = -1;
const int DFLT_CMFP             = 600; /* Connected-Mode Flush Period */
const int UNSET_CMFP            = -1;
const int DFLT_DMFP             = 30; /* Disconnected-Mode Flush Period */
const int UNSET_DMFP            = -1;
const int DFLT_MAXFP            = 3600; /* Maximum Flush Period */
const int UNSET_MAXFP           = -1;
const int DFLT_WITT             = 60; /* Worker-Idle time threshold */
const int UNSET_WITT            = -1;
const unsigned long DFLT_MAXFS  = 64 * 1024; /* Maximum Flush-Buffer Size */
const unsigned long UNSET_MAXFS = (unsigned long)-1;
const unsigned long DFLT_MAXTS  = 256 * 1024; /* Maximum Truncate Size */
const unsigned long UNSET_MAXTS = (unsigned long)-1;

const int DFLT_SWT  = 25;
const int UNSET_SWT = -1;
const int DFLT_MWT  = 75;
const int UNSET_MWT = -1;
const int DFLT_SSF  = 4;
const int UNSET_SSF = -1;

/* rule of thumb */
const int BLOCKS_PER_FILE = 24;
const int MLES_PER_FILE   = 4;
const int FILES_PER_HDBE  = 2;

const int MAX_PIGGY_VALIDATIONS = 50;

const int MIN_CB   = 2048;
const int MIN_CF   = MIN_CB / BLOCKS_PER_FILE;
const int MIN_MLE  = MIN_CF * MLES_PER_FILE;
const int MIN_HDBE = MIN_CF / FILES_PER_HDBE;

class VenusConf : public StringKeyValueStore {
private:
    bool already_loaded     = false;
    bool already_configured = false;
    dlist on_off_pairs_list;

    class on_off_pair : dlink {
    public:
        const char *on_val;
        const char *off_val;

        bool is_in_pair(const char *val);
        on_off_pair(const char *on, const char *off);
        ~on_off_pair();
    };
    on_off_pair *find_on_off_pair(const char *key);
    int add_int(const char *key, int64_t value);
    bool check_if_value_is_int(const char *key);
    bool check_all_int_values();
    void handle_relative_path(const char *key);

public:
    ~VenusConf();
    int64_t get_int_value(const char *key);
    void set_int(const char *key, int64_t value);
    const char *get_string_value(const char *key);
    bool get_bool_value(const char *key);
    int add_on_off_pair(const char *on_key, const char *off_key, bool on_value);
    void set(const char *key, const char *value);
    void load_default_config();
    void configure_cmdline_options();
    void apply_consistency_rules();
    int check();
};

uint64_t ParseSizeWithUnits(const char *SizeWUnits);

VenusConf &GetVenusConf();

#endif /* _VENUSCONF_H_ */
