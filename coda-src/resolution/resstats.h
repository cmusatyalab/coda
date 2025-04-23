/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

// statistics collected for resolution
#include <olist.h>

extern olist ResStatsList;

struct fileresstats {
    int file_nresolves; // total number of file resolves
    int file_nsucc; // resolves that were successful
    int file_conf; // resolves that resulted in conflicts
    int file_runtforce; // resolves that involved forcing a runt
    int file_we; // resolves due to weak equality
    int file_reg; // regular file resolves
    int file_userresolver; // file resolves that invoked a user installed resolver
    int file_succuserresolve; // number of those resolves that were successful
    int file_incvsg; // file resolves done with incomplete VSG

    fileresstats();
};

// histogram for sizes of logs shipped during resolution has buckets for
// < 1K, 2K, 4K, 8K, ... < 1024K, > 1024K
#define SHIPHISTOSIZE 12
#define logshipsizebucketnum(size)                                         \
    ((!((size) >> 10)) ?                                                   \
         0 :                                                               \
         ((!((size) >> 11)) ?                                              \
              1 :                                                          \
              ((!((size) >> 12)) ?                                         \
                   2 :                                                     \
                   ((!((size) >> 13)) ?                                    \
                        3 :                                                \
                        ((!((size) >> 14)) ?                               \
                             4 :                                           \
                             ((!((size) >> 15)) ?                          \
                                  5 :                                      \
                                  ((!((size) >> 16)) ?                     \
                                       6 :                                 \
                                       ((!((size) >> 17)) ?                \
                                            7 :                            \
                                            ((!((size) >> 18)) ?           \
                                                 8 :                       \
                                                 ((!((size) >> 19)) ?      \
                                                      9 :                  \
                                                      ((!((size) >> 20)) ? \
                                                           10 :            \
                                                           11)))))))))))

// histogram for max entries has buckets for
// entries < 8, 16, 32, 64, 128, 256, 512, 1024, >1024
#define NENTRIESHISTOSIZE 9
#define maxentriesbucketnum(i)                     \
    ((!((i) >> 3)) ?                               \
         0 :                                       \
         ((!((i) >> 4)) ?                          \
              1 :                                  \
              ((!((i) >> 5)) ?                     \
                   2 :                             \
                   ((!((i) >> 6)) ?                \
                        3 :                        \
                        ((!((i) >> 7)) ?           \
                             4 :                   \
                             ((!((i) >> 8)) ?      \
                                  5 :              \
                                  ((!((i) >> 9)) ? \
                                       6 :         \
                                       ((!((i) >> 10)) ? 7 : 8))))))))

struct logshiphisto { // histogram of log sizes moved around during dir res
    int totalsize[SHIPHISTOSIZE]; // distr of total size of logs
    int maxentries
        [NENTRIESHISTOSIZE]; // distr of max entries amongst logs in each res

    logshiphisto();
    void add(int, int *, int);
    void update(logshiphisto *);
    void print(int);
};

struct dirresstats {
    int dir_nresolves; // total number of dir resolves
    int dir_succ; // resolves that returned successfully
    int dir_conf; // resolves that marked the object in conflict
    int dir_nowork; // resolve calls that didn't require any work
    int dir_problems; // resolve calls with problems(lock, unequal ancestors)
    int dir_incvsg; // resolve calls with incomplete VSG
    logshiphisto logshipstats; // info about sizes of logs shipped

    dirresstats();
};

struct conflictstats {
    int nn; // name/name conflicts
    int ru; // r/u conflicts
    int uu; // u/u conflicts
    int mv; // rename related conflicts
    int wrap; // conflicts due to log wrap around
    int other; // unaccounted conflicts

    conflictstats();
    void update(conflictstats *);
};

// log statistics

// log size has several parameters
// for each incarnation of the server the smon db has
//	- highest water mark (highest below) &
//	- size at time of death (currentsize below)
// the record is "recomputed" and shipped every hour
// 	currentsize is updated
//	if high is > highest then highest is updated
//	high is reset to currentsize
// when the server starts, highest is set to currentsize

#define SIZEBUCKETS 15

struct logsize {
    int currentsize; // size of log right now
    int high; // high water mark since previous record was sent
    int highest; // highest water mark since incarnation
    int bucket[SIZEBUCKETS]; // histogram of sizes of log when reported
    // < 1024, < 2048, < 4096,... <8192*1024, >8192*1024

    logsize(int sz = 0);
    void chgsize(int);
    void report();
    void print(int);
#define logsizebucketnum(size)                                                     \
    ((!((size) >> 10)) ?                                                           \
         0 :                                                                       \
         ((!((size) >> 11)) ?                                                      \
              1 :                                                                  \
              ((!((size) >> 12)) ?                                                 \
                   2 :                                                             \
                   ((!((size) >> 13)) ?                                            \
                        3 :                                                        \
                        ((!((size) >> 14)) ?                                       \
                             4 :                                                   \
                             ((!((size) >> 15)) ?                                  \
                                  5 :                                              \
                                  ((!((size) >> 16)) ?                             \
                                       6 :                                         \
                                       ((!((size) >> 17)) ?                        \
                                            7 :                                    \
                                            ((!((size) >> 18)) ?                   \
                                                 8 :                               \
                                                 ((!((size) >> 19)) ?              \
                                                      9 :                          \
                                                      ((!((size) >> 20)) ?         \
                                                           10 :                    \
                                                           ((!((size) >> 21)) ?    \
                                                                11 :               \
                                                                ((!((size) >>      \
                                                                    22)) ?         \
                                                                     12 :          \
                                                                     ((!((size) >> \
                                                                         23)) ?    \
                                                                          13 :     \
                                                                          14))))))))))))))
};

#define MAXSIZES 9
struct varlhisto { // distribution of variable length parts for this volume
    int vallocs; // # of allocs of variable length parts
    int vfrees; // $ of deallocs of variable length parts
    int bucket[MAXSIZES];

    varlhisto();
    void countalloc(int);
    void countdealloc(int);
    void print(int);

#define varlbucketnum(sz) (((sz) / 8 > 8) ? 8 : ((sz) / 8))
#define SIZELT8(l) (l).bucket[0]
#define SIZELT16(l) (l).bucket[1]
#define SIZELT24(l) (l).bucket[2]
#define SIZELT32(l) (l).bucket[3]
#define SIZELT40(l) (l).bucket[4]
#define SIZELT48(l) (l).bucket[5]
#define SIZELT56(l) (l).bucket[6]
#define SIZELT64(l) (l).bucket[7]
#define SIZEGT64(l) (l).bucket[8]
};

struct logstats {
    int nwraps; // number of wrap arounds
    int nadmgrows; // how many times was admin limit of volume changed
    varlhisto vdist; // distribution of variable length parts
    logsize lsize;

    logstats(int sz = 0);
    void print(int);
};

//typedef enum {project, user, structural, other} VolumeType;

class resstats : public olink {
public:
    unsigned long vid;
    fileresstats file;
    dirresstats dir;
    conflictstats conf;
    logstats lstats;

    resstats(unsigned long, int = 0);
    void precollect();
    void postcollect();
    void update(fileresstats *);
    void update(dirresstats *);
    //void update(conflictstats *);
    //void update(hierarchystats *);
    //void update(logstats *);
    void print();
    void print(FILE *);
    void print(int);
};

#define Lsize(l) (l).lstats.lsize
#define VarlHisto(l) (l).lstats.vdist
