#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 3.1

          Copyright (c) 1987-1995 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/utils-src/mond/advice_parser.c,v 3.3 1998/09/07 15:57:23 braam Exp $";
#endif /*_BLURB_*/


extern "C" {
#include <stdio.h>
#include <strings.h>
#include <sys/param.h>
#include <sysent.h>

extern int ffilecopy(FILE*, FILE*);
}
#include "util.h"
#include "advice_parser.h"

extern int LogLevel;
extern FILE *LogFile;


#define CommentsDBdir "/usr/mond/adviceCommentsDB"
#define LINE_LENGTH 256

char line[LINE_LENGTH];

static int SkipLines(FILE *fp, int number)
{
    int error = 0;

    for (int count = 0; count < number; count++) 
        if (fgets(line, LINE_LENGTH, fp) == NULL)
	    error = 1;
    return(error);
}

static int ReadVenusVersionLine(FILE *fp, int *Major, int *Minor)
{
    int error = 0;
    int rc = 0;

    if (fgets(line, LINE_LENGTH, fp) == NULL)
	error = 1;
    rc = sscanf(line, "VenusVersion: %d.%d", Major, Minor);
    if (rc != 2)
	error += 1;
    return(error);
}

static int ReadFIDLine(FILE *fp, char *id, int *volume, int *vnode, int *unique)
{
    char formatstring[LINE_LENGTH];
    int error = 0;
    int rc = 0;

    if (fgets(line, LINE_LENGTH, fp) == NULL)
	error = 1;

    (void) sprintf(formatstring, "%s: <%%x.%%x.%%x>", id);

    rc = sscanf(line, formatstring, volume, vnode, unique);
    if (rc != 3)
	error += 1;
    return(error);
}

static int ReadVIDLine(FILE *fp, char *id, int *volume)
{
    char formatstring[LINE_LENGTH];
    int error = 0;
    int rc = 0;

    if (fgets(line, LINE_LENGTH, fp) == NULL)
	error = 1;

    (void) sprintf(formatstring, "%s: 0x%%x", id);

    rc = sscanf(line, formatstring, volume);
    if (rc != 1)
	error += 1;
    return(error);
}

static int ReadFormat1Line(FILE *fp, char *id, int *value)
{
    char formatstring[LINE_LENGTH];
    int error = 0;
    int rc = 0;

    if (fgets(line, LINE_LENGTH, fp) == NULL)
	error = 1;

    (void) sprintf(formatstring, "%s: %%d", id);

    rc = sscanf(line, formatstring, value);
    if (rc != 1)
	error += 1;
    return(error);
}

static int ReadFormat1xLine(FILE *fp, char *id, int *value)
{
    char formatstring[LINE_LENGTH];
    int error = 0;
    int rc = 0;

    if (fgets(line, LINE_LENGTH, fp) == NULL)
	error = 1;

    (void) sprintf(formatstring, "%s: %%x", id);

    rc = sscanf(line, formatstring, value);
    if (rc != 1)
	error += 1;
    return(error);
}

static int ReadFormat2Line(FILE *fp, char *id, char *value)
{
    char formatstring[LINE_LENGTH];
    int error = 0;
    int rc = 0;

    if (fgets(line, LINE_LENGTH, fp) == NULL)
	error = 1;

    (void) sprintf(formatstring, "%s: %%s", id);

    rc = sscanf(line, formatstring, value);
    if (rc != 1)
	error += 1;
    return(error); 
}

static int ReadAwarenessLine(FILE *fp, char *id, int *value)
{
    char tmpstring[8];
    int error = 0;

    *value = 0;
    error = ReadFormat2Line(fp, id, tmpstring);
    if (strncmp(tmpstring, "Yes", strlen("Yes")) == 0)
	*value = 2;
    else if (strncmp(tmpstring, "Suspected", strlen("Suspected")) == 0)
	*value = 1;
    else if (strncmp(tmpstring, "No", strlen("No")) == 0)
	*value = 0;
    else {
	error++;
	*value = -1;
    }
    return(error);
}

static void MoveFile(char *hereFileName, char *thereFileName) 
{
    FILE *hereFile, *thereFile;
    int code = 0;
    int error = 0;

    // Open the file we will copy FROM
    hereFile = fopen(hereFileName, "r");
    if (hereFile == NULL) {
	LogMsg(0,LogLevel,LogFile, "ERROR: MoveFile(%s,%s) ==> Cannot open file %s", hereFileName, thereFileName, hereFileName);
	assert(1 == 0);
    }

    // Open the file we will copy TO
    thereFile = fopen(thereFileName, "w+");
    if (thereFile == NULL) {
	LogMsg(0,LogLevel,LogFile, "ERROR: MoveFile(%s,%s) ==> Cannot open file %s", hereFileName, thereFileName, thereFileName);
	assert(1 == 0);
    }
    
    // Copy the file
    LogMsg(100,LogLevel,LogFile, "Moving %s to %s", hereFileName, thereFileName);
    code = ffilecopy(hereFile, thereFile);
    if (code != 0) {
	LogMsg(0,LogLevel,LogFile, "ERROR: MoveFile(%s,%s) ==> Cannot ffilecopy", hereFileName, thereFileName);
	assert(1 == 0);
    }

    fclose(hereFile);
    fclose(thereFile);
    unlink(hereFileName);
}

static char *DetermineQuestionnaire(char *comment_id)
{
    static char questionnaire[32];

    if (strncmp(comment_id, "OtherComments", strlen("OtherComments")) == 0) 
	strcpy(questionnaire, "DisconnectedMiss");
    else
	strcpy(questionnaire, "Reconnection");

    return(questionnaire);
}

static char *DetermineCommentField(char *comment_id)
{
    static char field[32];

    if (strncmp(comment_id, "SuspectedOtherComments", strlen("SuspectedOtherComments")) == 0) 
	strcpy(field, "Suspected");
    else if (strncmp(comment_id, "KnownOtherComments", strlen("KnownOtherComments")) == 0) 
	strcpy(field, "Known");
    else if (strncmp(comment_id, "PreparationComments", strlen("PreparationComments")) == 0) 
	strcpy(field, "Preparation");
    else if (strncmp(comment_id, "FinalComments", strlen("FinalComments")) == 0) 
	strcpy(field, "Final");
    else if (strncmp(comment_id, "OtherComments", strlen("OtherComments")) == 0) 
	strcpy(field, "Other");
    else {
	LogMsg(0, LogLevel,LogFile, "DetermineCommentField:  Unrecognized comment type (id=%s)\n", comment_id);
	assert(1 == 0);
    }  

    return(field);
}

static int DetermineUnique(char *questionnaire, char *commentField)
{
    char UniqueFileName[MAXPATHLEN];
    FILE *UniqueFile;
    char NewValue[8];
    int error = 0;
    int rc = 0;
    int unique;

    sprintf(UniqueFileName, "%s/%s/%s.ID", CommentsDBdir, questionnaire, commentField);
    if ((UniqueFile = fopen(UniqueFileName, "r+")) == NULL)
	error++;
    if (fgets(line, LINE_LENGTH, UniqueFile) == NULL)
	error++;
    rc = sscanf(line, "%d", &unique);
    if (rc != 1)
	error++;
    if (fseek(UniqueFile, 0L, 0) != 0)
	error++;
    if (fprintf(UniqueFile, "%d", unique+1) != 0)
	error++;
    if (fflush(UniqueFile) != 0)
	error++;
    if (fclose(UniqueFile) != 0)
	error++;

    if (error != 0) {
	LogMsg(0,LogLevel,LogFile, "DetermineUnique(%s,%s):  File handling error\n", questionnaire, commentField);
	assert(1 == 0);
    }

    return(unique);
}

static int DepositCommentFile(char *tmpFileName, char *id)
{
    char newCommentFileName[MAXPATHLEN];
    char *questionnaire;
    char *commentField;
    int unique;

    questionnaire = DetermineQuestionnaire(id);
    commentField = DetermineCommentField(id);
    unique = DetermineUnique(questionnaire, commentField);
    sprintf(newCommentFileName, "%s/%s/%s/%d", CommentsDBdir, questionnaire, commentField, unique);

    MoveFile(tmpFileName, newCommentFileName);
    return(unique);
}

static int CopeWithCommentsField(FILE *fp, char *id, int *value)
{
    FILE *CommentFile;
    char CommentFileName[MAXPATHLEN];
    int error = 0;
    int rc = 0;
    int num_lines = 0;

    if (fgets(line, LINE_LENGTH, fp) == NULL)
	error++;
    if (strncmp(line, id, strlen(id)) != 0)
	error++;

    sprintf(CommentFileName, "/tmp/advice.comment.%d", getpid());
    CommentFile = fopen(CommentFileName, "w+");
    assert(CommentFile != NULL);

    while (fgets(line, LINE_LENGTH, fp) != NULL) { 
	if (strncmp(line, "EndComment.", strlen("EndComment.")) == 0)
	    break;
	else {
   	    num_lines++;
	    fprintf(CommentFile, "%s", line);
        }
    }
    error += SkipLines(fp, 1);

    fflush(CommentFile);
    fclose(CommentFile);
    if (num_lines > 1)
        *value = DepositCommentFile(CommentFileName, id);
    else {
	*value = -1;
	unlink(CommentFileName);
    }

    return(error);
}

int ParseDisconQfile(char *filename, DiscoMissQ *q) 
{
    FILE *fp;
    int error = 0;

    LogMsg(20,LogLevel,LogFile,"Parsing: %s", filename);

    fp = fopen(filename, "r");
    if (fp == NULL) {
        LogMsg(0,LogLevel,LogFile,"ParseDiscoQfile could not open %s",filename);
        return(1);
    }
    error += SkipLines(fp, 1);
    error += ReadFormat1xLine(fp, "hostid", &(q->hostid));
    error += ReadFormat1Line(fp, "user", &(q->uid));
    error += ReadVenusVersionLine(fp, &(q->venusmajorversion), &(q->venusminorversion));
    error += ReadFormat1Line(fp, "AdviceMonitorVersion", &(q->advicemonitorversion));
    error += ReadFormat1Line(fp, "ADSRVversion", &(q->adsrv_version));
    error += ReadFormat1Line(fp, "ADMONversion", &(q->admon_version));
    error += ReadFormat1Line(fp, "Qversion", &(q->q_version));
    error += ReadFormat1Line(fp, "TimeOfDisconnection", &(q->disco_time));
    error += ReadFormat1Line(fp, "TimeOfCacheMiss", &(q->cachemiss_time));
    error += ReadFIDLine(fp, "Fid", &(q->fid_volume), &(q->fid_vnode), &(q->fid_uniquifier));
    error += SkipLines(fp, 2); // Path and RequestingProgram
    error += ReadFormat1Line(fp, "Practice",  &(q->practice_session));
    error += ReadFormat1Line(fp, "ExpectedAffect", &(q->expected_affect));
    error += CopeWithCommentsField(fp, "OtherComments:", &(q->comment_id));

    return(error);
}

static void InitReconnQ(ReconnQ *q) {
    q->hostid = -1;
    q->uid = -1;
    q->venusmajorversion = -1;
    q->venusminorversion = -1;
    q->advicemonitorversion = -1;
    q->adsrv_version = -1;
    q->admon_version = -1;
    q->q_version = -1;
    q->volume_id = -1;
    q->cml_count = -1;
    q->disco_time = -1;
    q->reconn_time = -1;
    q->walk_time = -1;
    q->reboots = -1;
    q->hits = -1;
    q->misses = -1;
    q->unique_hits = -1;
    q->unique_misses = -1;
    q->not_referenced = -1;
    q->awareofdisco = -1;
    q->voluntary = -1;
    q->practicedisco = -1;
    q->codacon = -1;
    q->sluggish = -1;
    q->observed_miss = -1;
    q->known_other_comments = -1;
    q->suspected_other_comments = -1;
    q->nopreparation = -1;
    q->hoard_walk = -1;
    q->num_pseudo_disco = -1;
    q->num_practice_disco = -1;
    q->prep_comments = -1;
    q->overall_impression = -1;
    q->final_comments = -1;
}

int ParseReconnQfile(char *filename, ReconnQ *q) {
    FILE *fp;
    int error = 0;

    LogMsg(20,LogLevel,LogFile,"Parsing: %s", filename);

    fp = fopen(filename, "r");
    if (fp == NULL) {
	LogMsg(0,LogLevel,LogFile,"ParseReconnQfile could not open %s", filename);
        return(1);
    }

    InitReconnQ(q);
    error += SkipLines(fp, 1);
    error += ReadFormat1xLine(fp, "hostid", &(q->hostid));
    error += ReadFormat1Line(fp, "user", &(q->uid));
    error += ReadVenusVersionLine(fp, &(q->venusmajorversion), &(q->venusminorversion));
    error += ReadFormat1Line(fp, "AdviceMonitorVersion", &(q->advicemonitorversion));
    error += ReadFormat1Line(fp, "ADSRVversion", &(q->adsrv_version));
    error += ReadFormat1Line(fp, "ADMONversion", &(q->admon_version));
    error += ReadFormat1Line(fp, "Qversion", &(q->q_version));
    error += SkipLines(fp, 1);
    error += ReadVIDLine(fp, "VID", &(q->volume_id));
    error += ReadFormat1Line(fp, "CMLCount", &(q->cml_count));
    error += ReadFormat1Line(fp, "TimeOfDisconnection", &(q->disco_time));
    error += ReadFormat1Line(fp, "TimeOfReconnection", &(q->reconn_time)); 
    error += ReadFormat1Line(fp, "TimeOfLastDemandHoardWalk", &(q->walk_time)); 
    error += ReadFormat1Line(fp, "NumberOfReboots", &(q->reboots)); 
    error += ReadFormat1Line(fp, "NumberOfCacheHits", &(q->hits)); 
    error += ReadFormat1Line(fp, "NumberOfCacheMisses", &(q->misses)); 
    error += ReadFormat1Line(fp, "NumberOfUniqueCacheHits",  &(q->unique_hits)); 
    error += ReadFormat1Line(fp, "NumberOfObjectsNotReferenced",  &(q->not_referenced)); 
    error += ReadAwarenessLine(fp, "AwareOfDisconnection", &(q->awareofdisco));
    if (q->awareofdisco != 0) {
	if (q->awareofdisco == 2) {
            error += ReadFormat1Line(fp, "VoluntaryDisconnection", &(q->voluntary)); 
            error += ReadFormat1Line(fp, "PracticeDisconnection", &(q->practicedisco)); 
        }
        error += ReadFormat1Line(fp, "Codacon", &(q->codacon)); 
        error += ReadFormat1Line(fp, "Sluggish", &(q->sluggish)); 
        error += ReadFormat1Line(fp, "ObservedCacheMiss", &(q->observed_miss)); 
        if (q->awareofdisco == 2) {
            error += CopeWithCommentsField(fp, "KnownOtherComments:", &(q->known_other_comments));
	    error += ReadFormat1Line(fp, "NoPreparation", &(q->nopreparation)); 
            error += ReadFormat1Line(fp, "HoardWalk", &(q->hoard_walk)); 
            error += ReadFormat1Line(fp, "PseudoDisconnection", &(q->num_pseudo_disco)); 
            error += ReadFormat1Line(fp, "PracticeDisconnection(s)", &(q->num_practice_disco)); 
    	    if (q->num_practice_disco) 
	        error += ReadFormat1Line(fp, "NumberPracticeDisconnection(s)", &(q->num_practice_disco));
            error += CopeWithCommentsField(fp, "PreparationComments:", &(q->prep_comments));
        }
        if (q->awareofdisco == 1) 
            error += CopeWithCommentsField(fp, "SuspectedOtherComments:", &(q->suspected_other_comments));
        error += ReadFormat1Line(fp, "OveralImpression", &(q->overall_impression)); 
    }
    error += CopeWithCommentsField(fp, "FinalComments:", &(q->final_comments));

    return(error);
}

/*
main(int argc, char *argv[]) {
	DiscoMissQ dq;
	ReconnQ rq;

	if (strncmp(argv[1], "Disco", strlen("Disco")) == 0)
		printf("Errors = %d\n", ParseDisconQfile(argv[1], &dq));
	if (strncmp(argv[1], "Recon", strlen("Recon")) == 0)
		printf("Errors = %d\n", ParseReconnQfile(argv[1], &rq));

	fflush(stdout);
}
*/
