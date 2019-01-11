/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2018 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/*
*
*
*                   RVM Basic Test Program
*
*/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <rvm/rvm.h>
#include "testrvm.h"

char *map_data; /* mapped data array ptr */
rvm_options_t *options; /* options descriptor ptr */
rvm_region_t *region; /* region descriptor ptr */
rvm_bool_t in_recovery; /* true if this run is a recovery */

rvm_bool_t rds_testsw = rvm_false; /* temporary */

#define TRUNCATE_VAL 30 /* truncate threshold */
#define NUM_TIDS 5

rvm_tid_t *t1, *t2, *t3, *t4, *t5; /* tid's */
rvm_tid_t *tids[NUM_TIDS];
#define T1 1
#define T2 2
#define T3 4
#define T4 8
#define T5 16
long tid_bits[NUM_TIDS] = { T1, T2, T3, T4, T5 };

static void eat_lf(void)
{
    while (rvm_true)
        if (getc(stdin) == '\n')
            return;
}

/* get boolean answer */
static rvm_bool_t get_ans(char *prompt /* prompt string */,
                          rvm_bool_t sense /* false if default is 'no' */)
{
    char sense_char; /* default sense character */

    if (sense)
        sense_char = 'y';
    else
        sense_char = 'n';

    while (rvm_true) {
        printf("\n%s (y or n [%c])? ", prompt, sense_char);
        switch (getc(stdin)) {
        case 'y':
        case 'Y':
            eat_lf();
            return rvm_true;

        case 'n':
        case 'N':
            eat_lf();
            return rvm_false;

        case '\n':
            return sense;

        default:
            eat_lf();
        }
    }
}

static rvm_bool_t chk_file(char *filename, rvm_region_t *region)
{
    char *map_data;
    char *test_data;
    int c;
    long i; /* loop counters */
    FILE *F;

    F = fopen(filename, "rb");
    if (F == NULL) {
        printf("\n? Couldn't open %s\n", filename);
        return rvm_false;
    }

    test_data = malloc(region->length);
    if (test_data == NULL) {
        printf("\n? Could not allocate test_data buffer\n");
        fclose(F);
        return rvm_false;
    }
    for (i = 0; i < region->length; i++) {
        c = getc(F);
        if (c == EOF) {
            printf("\n? EOF encountered while reading %s; i = %ld\n", filename,
                   i);
            free(test_data);
            fclose(F);
            return rvm_false;
        }
        test_data[i] = c;
    }

    map_data = region->vmaddr;
    for (i = 0; i < region->length; i++) {
        if (test_data[i] != map_data[i]) {
            printf("\n? Error: mapped data doesn't match %s:\n", filename);
            printf("         map_data[%ld] = 0%o\n", i, map_data[i]);
            printf("         %s[%ld] = 0%o\n", filename, i, test_data[i]);
            free(test_data);
            fclose(F);
            return rvm_false;
        }
    }
    free(test_data);

    if (fclose(F)) {
        printf("\n? Error closing %s\n", filename);
        printf("    errno = %d\n", errno);
        return rvm_false;
    }

    printf("\n  Mapped data agrees with %s\n", filename);
    return rvm_true;
}

/* copy file */
static rvm_bool_t copy_file(char *file1 /* source file */,
                            char *file2 /* destination file */)
{
    FILE *F1, *F2; /* copy F1 to F2 */
    long i;

    /* open source file */
    F1 = fopen(file1, "r");
    if (F1 == NULL) {
        printf("\n? Trouble opening %s\n", file1);
        perror("  error is");
        return rvm_true;
    }

    /* open destination file */
    F2 = fopen(file2, "w");
    if (F2 == NULL) {
        printf("\n? Trouble opening %s\n", file2);
        perror("  error is");
        (void)fclose(F1);
        return rvm_true;
    }

    /* copy */
    while (rvm_true) {
        i = getc(F1);
        if (i == EOF)
            break;
        (void)putc((char)i, F2);
    }

    /* close both files */
    (void)fclose(F1);
    (void)fclose(F2);

    return rvm_false;
}

/* initialization tests */
static rvm_bool_t test_initialization(rvm_options_t *options)
{
    rvm_return_t retval;
    rvm_tid_t *t1;

    /* uninitialized system test */
    t1 = NULL;
    if ((retval = rvm_begin_transaction(t1, restore)) != RVM_EINIT) {
        printf("\n? Error: RVM not initialized, retval = %d\n", (int)retval);
        return rvm_true;
    }

    /* initialize it properly */
    if ((retval = rvm_initialize(RVM_VERSION, options)) != RVM_SUCCESS) {
        printf("? rvm_initialize failed, code: %s\n", rvm_return(retval));
        return rvm_true;
    }

    return rvm_false;
}

/* termination tests */
static rvm_bool_t test_termination(void)
{
    rvm_return_t retval;

    /* do actual termination termination */
    if ((retval = rvm_terminate()) != RVM_SUCCESS) {
        printf("\n? Error in rvm_terminate, retval = %s\n", rvm_return(retval));
        return rvm_true;
    }

    /* try to restart -- should fail */
    if ((retval = RVM_INIT(NULL)) != RVM_EINIT) {
        printf("\n? Error in rvm_initialize, retval = %s\n",
               rvm_return(retval));
        return rvm_true;
    }
    printf("\nRVM terminated; re-init denied\n");

    return rvm_false;
}

/* bad TID tests */
static rvm_bool_t bad_TID_tests(rvm_tid_t *tid)
{
    rvm_return_t retval; /* rvm return code */

    printf("\nInvalid TID tests\n");
    if ((retval = rvm_abort_transaction((rvm_tid_t *)NULL)) != RVM_ETID) {
        printf("\n? Error NULL tid not detected, retval = %s\n",
               rvm_return(retval));
        return rvm_true;
    }

    if ((retval = rvm_abort_transaction(tid)) != RVM_ETID) {
        printf("\n? Error invalid TID not detected, retval = %s\n",
               rvm_return(retval));
        exit(EXIT_FAILURE);
    }

    printf("    Bad TID's properly detected\n");

    return rvm_false;
}

/* basic transaction start-up tests */
static rvm_bool_t start_trans_tests(rvm_tid_t *tid1, rvm_tid_t *tid2,
                                    rvm_tid_t *tid3, rvm_tid_t *tid4,
                                    rvm_tid_t *tid5)
{
    rvm_return_t retval; /* rvm return code */

    printf("\nTransaction start tests\n");
    if ((retval = rvm_begin_transaction(tid1, (rvm_mode_t)123)) != RVM_EMODE) {
        printf(
            "\n? Error begin_transaction failed on invalid mode, retval = %s\n",
            rvm_return(retval));
        return rvm_true;
    }
    if ((retval = rvm_begin_transaction(tid1, no_restore)) != RVM_SUCCESS) {
        printf("\n? Error in begin_transaction tid1, retval = %s\n",
               rvm_return(retval));
        return rvm_true;
    }
    if ((retval = rvm_begin_transaction(tid2, restore)) != RVM_SUCCESS) {
        printf("\n? Error in begin_transaction tid2, retval = %s\n",
               rvm_return(retval));
        return rvm_true;
    }
    if ((retval = rvm_begin_transaction(tid3, no_restore)) != RVM_SUCCESS) {
        printf("\n? Error in begin_transaction tid3, retval = %s\n",
               rvm_return(retval));
        return rvm_true;
    }
    if ((retval = rvm_begin_transaction(tid4, restore)) != RVM_SUCCESS) {
        printf("\n? Error in begin_transaction tid4, retval = %s\n",
               rvm_return(retval));
        return rvm_true;
    }
    if ((retval = rvm_begin_transaction(tid5, restore)) != RVM_SUCCESS) {
        printf("\n? Error in begin_transaction tid5, retval = %s\n",
               rvm_return(retval));
        return rvm_true;
    }

    printf("    Transactions started\n");
    return rvm_false;
}

/* test null transaction */
static rvm_bool_t test_null_trans(rvm_tid_t *tid)
{
    rvm_return_t retval; /* rvm return code */

    printf("\nNull transaction tests\n");
    if ((retval = rvm_end_transaction(tid, (rvm_mode_t)456)) != RVM_EMODE) {
        printf(
            "\n? Error end_transaction failed on invalid mode, retval = %s\n",
            rvm_return(retval));
        return rvm_true;
    }

    if ((retval = rvm_end_transaction(tid, flush)) != RVM_SUCCESS) {
        printf("\n? Error in end_transaction, retval = %s\n",
               rvm_return(retval));
        return rvm_true;
    }

    if (chk_file(T2_CHK_FILE, region)) {
        printf("    Null transaction did not affect mapped data\n");
        return rvm_false;
    } else
        return rvm_true;
}

/* single range commit test */
static rvm_bool_t test_single_range_commit(rvm_tid_t *tid)
{
    rvm_return_t retval; /* rvm return code */
    long i; /* loop counter */

    /* build a range and commit */
    printf("\nSingle range commit test\n");
    if ((retval = rvm_set_range(tid, &map_data[T1_S1], T1_L1)) != RVM_SUCCESS) {
        printf("\n? Error in set_range 1, t1, retval = %s\n",
               rvm_return(retval));
        return rvm_true;
    }

    /* make the modifications */
    for (i = T1_S1; i < (T1_S1 + T1_L1); i++)
        map_data[i] = T1_V1;

    if ((retval = rvm_end_transaction(tid, flush)) != RVM_SUCCESS) {
        printf("\n? Error in end_transaction, retval = %s\n",
               rvm_return(retval));
        return rvm_true;
    }

    /* check results against check file */
    if (!chk_file(T1_CHK_FILE, region)) {
        printf("?  Error in single range commit test data\n");
        return rvm_true;
    }

    return rvm_false;
}

/* commit a transaction with several ranges */
static rvm_bool_t test_multi_range_commit(rvm_tid_t *tid)
{
    rvm_return_t retval; /* rvm return code */
    long i; /* loop counter */

    printf("\nMulti-range transaction tests\n");
    if ((retval = rvm_set_range(tid, &map_data[T3_S1], T3_L1)) != RVM_SUCCESS) {
        printf("\n? Error in set_range 1, retval = %s\n", rvm_return(retval));
        return rvm_true;
    }

    for (i = T3_S1; i < (T3_S1 + T3_L1); i++)
        map_data[i] = T3_V1;

    if ((retval = rvm_set_range(tid, &map_data[T3_S2], T3_L2)) != RVM_SUCCESS) {
        printf("\n? Error in set_range 2, retval = %s\n", rvm_return(retval));
        return rvm_true;
    }

    for (i = T3_S2; i < (T3_S2 + T3_L2); i++)
        map_data[i] = T3_V2;

    if ((retval = rvm_set_range(tid, &map_data[T3_S3], T3_L3)) != RVM_SUCCESS) {
        printf("\n? Error in set_range 3, retval = %s\n", rvm_return(retval));
        return rvm_true;
    }

    for (i = T3_S3; i < (T3_S3 + T3_L3); i++)
        map_data[i] = T3_V3;

    if ((retval = rvm_end_transaction(tid, flush)) != RVM_SUCCESS) {
        printf("\n? Error in end_transaction, retval = %s\n",
               rvm_return(retval));
        return rvm_true;
    }

    if (!chk_file(T3_CHK_FILE, region)) {
        printf("?  Error in multi-range commit\n");
        return rvm_true;
    }
    printf("\n  Transaction committed succesfully\n");

    return rvm_false;
}

/* test multi-range transaction with abort */
static rvm_bool_t test_multi_range_abort(rvm_tid_t *tid)
{
    rvm_return_t retval; /* rvm return code */
    long i;

    printf("\nMulti-range transaction with abort & restore\n");

    if ((retval = rvm_set_range(tid, &map_data[T4_S1], T4_L1)) != RVM_SUCCESS) {
        printf("\n? Error in set_range 1, retval = %s\n", rvm_return(retval));
        return rvm_true;
    }

    for (i = T4_S1; i < (T4_S1 + T4_L1); i++)
        map_data[i] = T4_V1;

    if ((retval = rvm_set_range(tid, &map_data[T4_S2], T4_L2)) != RVM_SUCCESS) {
        printf("\n? Error in set_range 2, retval = %s\n", rvm_return(retval));
        return rvm_true;
    }

    for (i = T4_S2; i < (T4_S2 + T4_L2); i++)
        map_data[i] = T4_V2;

    if ((retval = rvm_set_range(tid, &map_data[T4_S3], T4_L3)) != RVM_SUCCESS) {
        printf("\n? Error in set_range 3, retval = %s\n", rvm_return(retval));
        return rvm_true;
    }

    for (i = T4_S3; i < (T4_S3 + T4_L3); i++)
        map_data[i] = T4_V3;

    if ((retval = rvm_abort_transaction(tid)) != RVM_SUCCESS) {
        printf("\n? Error in abort_transaction, retval = %s\n",
               rvm_return(retval));
        return rvm_true;
    }
    if (!chk_file(T4_CHK_FILE, region)) {
        printf("?  Error in multi-range transaction abort\n");
        return rvm_true;
    }

    printf("\n  Aborted transaction restored data correctly\n");
    return rvm_false;
}

/* test complex multi-range transaction */
static rvm_bool_t test_complex_range_commit(rvm_tid_t *tid)
{
    rvm_return_t retval; /* rvm return code */
    long i;

    printf("\nComplex multi-range transaction tests\n");

    if ((retval = rvm_set_range(tid, &map_data[T5_S1], T5_L1)) != RVM_SUCCESS) {
        printf("\n? Error in set_range 1, retval = %s\n", rvm_return(retval));
        return rvm_true;
    }
    for (i = T5_S1; i < (T5_S1 + T5_L1); i++)
        map_data[i] = T5_V1;

    if ((retval = rvm_set_range(tid, &map_data[T5_S2], T5_L2)) != RVM_SUCCESS) {
        printf("\n? Error in set_range 2, retval = %s\n", rvm_return(retval));
        return rvm_true;
    }
    for (i = T5_S2; i < (T5_S2 + T5_L2); i++)
        map_data[i] = T5_V2;

    if ((retval = rvm_set_range(tid, &map_data[T5_S3], T5_L3)) != RVM_SUCCESS) {
        printf("\n? Error in set_range 3, retval = %s\n", rvm_return(retval));
        return rvm_true;
    }
    for (i = T5_S3; i < (T5_S3 + T5_L3); i++)
        map_data[i] = T5_V3;

    if ((retval = rvm_set_range(tid, &map_data[T5_S4], T5_L4)) != RVM_SUCCESS) {
        printf("\n? Error in set_range 4, retval = %s\n", rvm_return(retval));
        return rvm_true;
    }
    for (i = T5_S4; i < (T5_S4 + T5_L4); i++)
        map_data[i] = T5_V4;
    if ((retval = rvm_set_range(tid, &map_data[T5_S5], T5_L5)) != RVM_SUCCESS) {
        printf("\n? Error in set_range 5, retval = %s\n", rvm_return(retval));
        return rvm_true;
    }
    for (i = T5_S5; i < (T5_S5 + T5_L5); i++)
        map_data[i] = T5_V5;

    if ((retval = rvm_set_range(tid, &map_data[T5_S6], T5_L6)) != RVM_SUCCESS) {
        printf("\n? Error in set_range 6, retval = %s\n", rvm_return(retval));
        return rvm_true;
    }
    for (i = T5_S6; i < (T5_S6 + T5_L6); i++)
        map_data[i] = T5_V6;

    if ((retval = rvm_end_transaction(tid, flush)) != RVM_SUCCESS) {
        printf("\n? Error in end_transaction, retval = %s\n",
               rvm_return(retval));
        return rvm_true;
    }

    if (!chk_file(T5_CHK_FILE, region)) {
        printf("?  Error in complex multi-range transaction commit\n");
        return rvm_true;
    }
    printf("\n  Complex transaction committed succesfully\n");

    return rvm_false;
}

int main(int argc, char **argv)
{
    rvm_return_t retval; /* rvm return code */
    rvm_bool_t ans; /* response */
    char *map_test_file; /* file to test mapping against */
    rvm_offset_t offset; /* offset temporary */

    options           = rvm_malloc_options();
    options->truncate = TRUNCATE_VAL;
    /* options->flags |= RVM_COALESCE_RANGES; */
    /* options->flags |= (RVM_COALESCE_RANGES | RVM_COALESCE_TRANS); */
    options->flags |= RVM_ALL_OPTIMIZATIONS;
    if (get_ans("Do you want a private mapping?", rvm_false))
        options->flags |= RVM_MAP_PRIVATE;
    in_recovery = get_ans("Is this a recovery from previous crash", rvm_false);

    /* see if should create a log file */
    if (!in_recovery &&
        get_ans("Do you want to create a log file", rvm_false)) {
        /* minimal initialization */
        if (test_initialization(NULL)) {
            printf("\n? Error in initialization tests\n");
            exit(EXIT_FAILURE);
        }

        /* create a log file and run with it */
        options->log_dev = LOG_FILE;
        offset           = RVM_MK_OFFSET(0, 4096);
        if ((retval = rvm_create_log(options, &offset, 0644)) != RVM_SUCCESS) {
            printf("\n? Error in log initialization, retval = %s\n",
                   rvm_return(retval));
            exit(EXIT_FAILURE);
        }
        if ((retval = rvm_set_options(options)) != RVM_SUCCESS) {
            printf("\n? Error in setting options, retval = %s\n",
                   rvm_return(retval));
            exit(EXIT_FAILURE);
        }

    } else {
        options->log_dev = LOG_FILE;
        if (test_initialization(options)) {
            printf("\n? Error in initialization tests\n");
            exit(EXIT_FAILURE);
        }
    }

    /* if not in recovery,create working copy of test files */
    if (!in_recovery)
        if (copy_file(MAP_DATA_FILE, TEST_DATA_FILE)) {
            printf("\n? Error in creating test files\n");
            exit(EXIT_FAILURE);
        }

    /* map first region, RVM to allocate space */
    region           = rvm_malloc_region();
    region->data_dev = TEST_DATA_FILE;

    printf("Basic mapping test:\n");
    printf("Pre-map region descriptor:\n");
    printf("    region.vmaddr = %p\n", region->vmaddr);
    printf("    region->length = %ld\n", region->length);
    printf("    region->offset.high = %lx\n",
           RVM_OFFSET_HIGH_BITS_TO_LENGTH(region->offset));
    printf("    region->offset.low = %lx\n",
           RVM_OFFSET_TO_LENGTH(region->offset));

    if ((retval = rvm_map(region, NULL)) != RVM_SUCCESS) {
        printf("?rvm_map failed, code: %s\n", rvm_return(retval));
        exit(EXIT_FAILURE);
    }

    printf("\nPost-map region descriptor:\n");
    printf("    region.vmaddr = %p\n", region->vmaddr);
    printf("    region->length = %ld\n", region->length);
    printf("    region->offset.high = %lx\n",
           RVM_OFFSET_HIGH_BITS_TO_LENGTH(region->offset));
    printf("    region->offset.low = %lx\n",
           RVM_OFFSET_TO_LENGTH(region->offset));

    map_data = region->vmaddr;
    if (in_recovery)
        map_test_file = T5_CHK_FILE;
    else
        map_test_file = MAP_CHK_FILE;
    if (!chk_file(map_test_file, region)) {
        if (in_recovery)
            printf("\n?  Recovery did not restore data correctly\n");
        else
            printf("\n? Error map test failed\n");
        exit(EXIT_FAILURE);
    }
    if (in_recovery)
        exit(EXIT_SUCCESS);

    /* bad tid tests */
    tids[0] = t1 = rvm_malloc_tid();
    if (bad_TID_tests(t1)) {
        printf("\n? Error in invalid tid tests\n");
        exit(EXIT_FAILURE);
    }

    /* transaction start tests */
    tids[1] = t2 = rvm_malloc_tid();
    tids[2] = t3 = rvm_malloc_tid();
    tids[3] = t4 = rvm_malloc_tid();
    tids[4] = t5 = rvm_malloc_tid();
    if (start_trans_tests(t1, t2, t3, t4, t5)) {
        printf("\n? Error in invalid tid tests\n");
        exit(EXIT_FAILURE);
    }

    /* test null transaction */
    if (test_null_trans(t2)) {
        printf("\n? Error in null transaction tests\n");
        exit(EXIT_FAILURE);
    }

    /* single range w/ commit test */
    if (test_single_range_commit(t1)) {
        exit(EXIT_FAILURE);
    }

    /* try multi-range transaction */
    if (test_multi_range_commit(t3)) {
        exit(EXIT_FAILURE);
    }

    /* try multi-range transaction with abort */
    if (test_multi_range_abort(t4)) {
        exit(EXIT_FAILURE);
    }

    /* try complex multi-range transaction -- tests all forms of
       shadowed changes */
    if (test_complex_range_commit(t5)) {
        exit(EXIT_FAILURE);
    }

    /* truncation test */
    if (get_ans("Do truncation", rvm_true)) {
        if ((retval = rvm_truncate()) != RVM_SUCCESS) {
            printf("?rvm_truncate failed, code: %s\n", rvm_return(retval));
            exit(EXIT_FAILURE);
        }

        /* compare mapped region to segment file */
        if (!chk_file(TEST_DATA_FILE, region)) {
            printf("?  Error in truncation write-back to segment file\n");
            exit(EXIT_FAILURE);
        }
    } else {
        ans = get_ans("Crash", rvm_false);
        if (ans)
            exit(EXIT_FAILURE);
    }

    /* unmap the region */
    if ((retval = rvm_unmap(region)) != RVM_SUCCESS) {
        printf("\n? Error in rvm_unmap, retval = %s\n", rvm_return(retval));
        exit(EXIT_FAILURE);
    }
    printf("\nUnmap completed correctly\n");

    /* release mallocated structures */
    rvm_free_region(region);
    rvm_free_options(options);
    rvm_free_tid(t1);
    rvm_free_tid(t2);
    rvm_free_tid(t3);
    rvm_free_tid(t4);
    rvm_free_tid(t5);

    /* system termination tests */
    if (test_termination()) {
        printf("\n? Error in termination tests\n");
        exit(EXIT_FAILURE);
    }

    printf("\nAll tests finished correctly!\n");

    return 0;
}
