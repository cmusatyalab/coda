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

#define CF (union config_val)
typedef union config_val {
	int   v;
	char *s;
} *pconfig_val;

/* one entry */
typedef struct config_entry {
	char *ent_key;
	union config_val ent_value;
	int ent_flag;
} *pconfig_entry;

/* flags for entries. */
#define CONFIG_SET 0x1 /* set if entry was set */
#define CONFIG_MALLOCED 0x2 /* set if entry has malloced string storage */
#define CONFIG_MANDATORY 0x1000

typedef struct config_table {
	char *tbl_name;
	pconfig_entry tbl_entries;
} *pconfig_table;

/* null terminated list of tablenames */
typedef struct config_rsr {
	char *cfg_tablename;
	pconfig_table cfg_stringtablep;
	pconfig_table cfg_consttablep;
} *pconfig_rsr;

typedef struct config_prog {
	char *cfg_file;          /* source of the values */
	pconfig_rsr *cfg_resources;
} *pconfig_cfg;

#define CFG_CONST 1
#define CFG_STRING 0

struct config_table *config_setup_table(struct config_table *table, struct config_entry *entries, char *file, int constorstr);
int config_find_key(char *key, struct config_table *table);
int config_set_string(char *key, char *value, struct config_table *table);
int config_set_const(char *key, int value, struct config_table *table);
char *config_get_name(char *key, struct config_table *table);
int config_get_const(char *key, struct config_table *table, int *result);
int config_next(char *line, char **key, char **val, char **rsr);
