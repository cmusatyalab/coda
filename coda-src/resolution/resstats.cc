/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/




#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <resstats.h>

// globals
olist ResStatsList;	// list of all volume's res stats


fileresstats::fileresstats() {
    memset((void *)this, 0, sizeof(fileresstats));
}

logshiphisto::logshiphisto() {
    memset((void *)this, 0, sizeof(logshiphisto));
}

void logshiphisto::print(int fd) {
    char buf[512];

    sprintf(buf, "Log shipped stats:\n");
    write(fd, buf, (int)strlen(buf));
    sprintf(buf, "TotalSize Distribution: \n");
    write(fd, buf, (int)strlen(buf));
    
    sprintf(buf, "1K\t2K\t4K\t8K\t16K\t32K\t64K\t128K\t256K\t512K\t1024K\t> 1024K\n");
    write(fd, buf, (int)strlen(buf));
    
    sprintf(buf, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
	    totalsize[0], totalsize[1], totalsize[2], totalsize[3], 
	    totalsize[4], totalsize[5], totalsize[6], totalsize[7],
	    totalsize[8], totalsize[9], totalsize[10], totalsize[11]);
    write(fd, buf, (int)strlen(buf));

    sprintf(buf, "MaxEntries distribution:\n");
    write(fd, buf, (int)strlen(buf));
    sprintf(buf, "<8\t16\t32\t64\t128\t256\t512\t1024\t>1024\n");
    write(fd, buf, (int)strlen(buf));
    sprintf(buf, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
	    maxentries[0], maxentries[1], maxentries[2], 
	    maxentries[3], maxentries[4], maxentries[5], 
	    maxentries[6], maxentries[7], maxentries[8]);
    write(fd, buf, (int)strlen(buf));
}

void logshiphisto::add(int tsize, int *entries, int nentries) {
    // update totalsize
    totalsize[logshipsizebucketnum(tsize)]++;

    // update maxentries
    int maxentrysize = 0;
    for (int i = 0; i < nentries; i++) 
	if (entries[i] > maxentrysize) 
	    maxentrysize = entries[i];
    maxentries[maxentriesbucketnum(maxentrysize)]++;
}

void logshiphisto::update(logshiphisto *newlsh) {
    for (int i = 0; i < SHIPHISTOSIZE; i++) 
	if (newlsh->totalsize[i]) totalsize[i] += newlsh->totalsize[i];
  { /* drop scope for int i below; to avoid identifier clash */
    for (int i = 0; i < NENTRIESHISTOSIZE; i++)
	if (newlsh->maxentries[i]) maxentries[i] += newlsh->maxentries[i];
  } /* drop scope for int i above; to avoid identifier clash */
}
dirresstats::dirresstats() {
    memset((void *)this, 0, sizeof(dirresstats));
}

conflictstats::conflictstats() {
    memset((void *)this, 0, sizeof(conflictstats));
}

void conflictstats::update(conflictstats *cs) {
    nn += cs->nn;
    ru += cs->ru;
    uu += cs->uu;
    mv += cs->mv;
    wrap += cs->wrap;
    other += cs->other;
}

logsize::logsize(int size) {
    currentsize = high = highest = size;
}
void logsize::chgsize(int sz) {
    currentsize += sz;
    if (currentsize > high) high = currentsize;
    if (high > highest) highest = high;
}
void logsize::report(){
    bucket[logsizebucketnum(high)]++;
    high = currentsize;
}

void logsize::print(int fd) {
    char buf[256];
    sprintf(buf, "Logsize: %d curr, %d high in cur interval, %d highest\nDistribution:\n",
	    currentsize, high, highest);
    write(fd, buf, (int)strlen(buf));
    
    sprintf(buf, "<1K\t2K\t4K\t8K\t16K\t32K\t64K\t128K\t256K\t\t512K\t1024K\t2048K\t4096K\t8192K\t>8192K\n");
    write(fd, buf, (int)strlen(buf));
    
    sprintf(buf, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
	    bucket[0], bucket[1], bucket[2], bucket[3],
	    bucket[4], bucket[5], bucket[6], bucket[7],
	    bucket[8], bucket[9], bucket[10], bucket[11],
	    bucket[12], bucket[13], bucket[14]);
    write(fd, buf, (int)strlen(buf));
}

varlhisto::varlhisto() {
    memset((void *)this, 0, sizeof(varlhisto));
}

void varlhisto::countalloc(int varsz) {
    if (varsz) {
	bucket[varlbucketnum(varsz)]++;
	vallocs++;
    }
}
void varlhisto::countdealloc(int varsz) {
    if (varsz) 
	vfrees++;
}

void varlhisto::print(int fd) {
    char buf[512];

    sprintf(buf, "Var Histo: %d allocs %d frees\n",
	    vallocs, vfrees);
    write(fd, buf, (int)strlen(buf));
    
    sprintf(buf, "<8\t16\t24\t32\t40\t48\t56\t64\t>64\n");
    write(fd, buf, (int)strlen(buf));

    sprintf(buf, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
	    bucket[0], bucket[1], bucket[2], bucket[3], 
	    bucket[4], bucket[5], bucket[6], bucket[7], bucket[8]);
    write(fd, buf, (int)strlen(buf));
}

logstats::logstats(int size): lsize(size) {
    nwraps = 0; 
    //physsize = size;
    nadmgrows = 0;
}

void logstats::print(int fd) {
    char buf[64];

    sprintf(buf, "Log Stats: %d wraps, %d adm grows\n", 
	    nwraps, nadmgrows);
    write(fd, buf, (int)strlen(buf));
    
    vdist.print(fd);
    lsize.print(fd);
}
resstats::resstats(unsigned long id, 
		   int sz) : lstats(sz) {
    vid = id;
}

void resstats::precollect() {
    // update the log size histogram
    lstats.lsize.bucket[logsizebucketnum(lstats.lsize.high)]++;
}

void resstats::postcollect() {
    lstats.lsize.high = lstats.lsize.currentsize;
}

void resstats::print() {
    print(stdout);
    fflush(stdout);
}

void resstats::print(FILE *fp) {
    print(fileno(fp));
    fflush(fp);
}

void resstats::print(int fd) {
    char buf[512];
    sprintf(buf, "Res. stats for volume 0x%lx:\n", vid);
    write(fd, buf, (int)strlen(buf));

    sprintf(buf, "File Stats: %d Nresolves, %d Succ, %d Conf, %d runtforces, %d WeakEq, %d Reg FR, %d user resolves, %d successful user resolves, %d IncompleteVSG\n",
	    file.file_nresolves, file.file_nsucc, file.file_conf, file.file_runtforce,
	    file.file_we, file.file_reg, 
	    file.file_userresolver, file.file_succuserresolve, 
	    file.file_incvsg);
    write(fd, buf, (int)strlen(buf));

    sprintf(buf, "Dir Stats: %d res, %d succ, %d conf, %d nowork, %d problems, %d IncompleteVSG\n",
	    dir.dir_nresolves, dir.dir_succ, dir.dir_conf, dir.dir_nowork,
	    dir.dir_problems, dir.dir_incvsg);
    write(fd, buf, (int)strlen(buf));
    dir.logshipstats.print(fd);

    sprintf(buf, "Conflict stats: %d N/N, %d R/U, %d U/U, %d M/U, %d Wrap, %d other\n",
	    conf.nn, conf.ru, conf.uu, conf.mv, conf.wrap, conf.other);
    write(fd, buf, (int)strlen(buf));

    lstats.print(fd);
}

resstats *FindResStats(unsigned long id) {
    olist_iterator next(ResStatsList);
    resstats *r = NULL;
    while ((r = (resstats *)next())) {
	if (r->vid == id) break;
    }
    return(r);
}

void resstats::update(fileresstats *newstats) {
    file.file_nresolves += newstats->file_nresolves;
    file.file_nsucc += newstats->file_nsucc;
    file.file_conf += newstats->file_conf;
    file.file_runtforce += newstats->file_runtforce;
    file.file_we += newstats->file_we;
    file.file_reg += newstats->file_reg;
    file.file_incvsg += newstats->file_incvsg;
}

void resstats::update(dirresstats *newstats) {
    dir.dir_nresolves += newstats->dir_nresolves;
    dir.dir_succ += newstats->dir_succ;
    dir.dir_conf += newstats->dir_conf;
    dir.dir_nowork += newstats->dir_nowork;
    dir.dir_problems += newstats->dir_problems;
    dir.dir_incvsg += newstats->dir_incvsg;
    dir.logshipstats.update(&(newstats->logshipstats));
}
