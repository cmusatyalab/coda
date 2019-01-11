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
*
*                       Common Declarations for RVM Tests
*
*
*
*/

#define MAP_DATA_FILE "map_data_file"
#define MAP_CHK_FILE "map_chk_file"
#define TEST_DATA_FILE "test_data_file"
#define T1_CHK_FILE "t1_chk_file"
#define T2_CHK_FILE MAP_CHK_FILE
#define T3_CHK_FILE "t3_chk_file"
#define T4_CHK_FILE T3_CHK_FILE
#define T5_CHK_FILE "t5_chk_file"

#define LOG_FILE "log_file"

#define T1_S1 100
#define T1_L1 100
#define T1_V1 0

#define T3_S1 300
#define T3_L1 50
#define T3_V1 0100

#define T3_S2 400
#define T3_L2 150
#define T3_V2 0101

#define T3_S3 1000
#define T3_L3 512
#define T3_V3 0102

#define T4_S1 2000
#define T4_L1 150
#define T4_V1 0141

#define T4_S2 2100
#define T4_L2 50
#define T4_V2 0142

#define T4_S3 2050
#define T4_L3 500
#define T4_V3 0143

#define T5_S1 T4_S1
#define T5_L1 T4_L1
#define T5_V1 T4_V1

#define T5_S2 T4_S2
#define T5_L2 T4_L2
#define T5_V2 T4_V2

#define T5_S3 T4_S3
#define T5_L3 T4_L3
#define T5_V3 T4_V3

#define T5_S4 2400
#define T5_L4 100
#define T5_V4 0144

#define T5_S5 2600
#define T5_L5 200
#define T5_V5 0145

#define T5_S6 2600
#define T5_L6 100
#define T5_V6 0146
