#define CF (union config_val)
typedef union config_val {
	int   v;
	char *s;
} *pconfig_val;

typedef struct config_entry {
	char *ent_key;
	union config_val ent_value;
	int ent_flag;
} *pconfig_entry;

/* flags for entries. */
#define CONFIG_SET 0x1 /* set if entry was set */
#define CONFIG_MALLOCED 0x2 /* set if entry has malloced string storage */

/* null terminated list of keys */
typedef struct config_table {
	pconfig_entry tbl_entries;
	char *tbl_file;          /* source of the values */
} *pconfig_table;

#define CFG_CONST 1
#define CFG_STRING 0

struct config_table *config_setup_table(struct config_table *table, struct config_entry *entries, char *file, int constorstr);
int config_find_key(char *key, struct config_table *table);
int config_set_string(char *key, char *value, struct config_table *table);
int config_set_const(char *key, int value, struct config_table *table);
char *config_get_name(char *key, struct config_table *table);
int config_get_const(char *key, struct config_table *table, int *result);
int config_next(char *line, char **key, char **val);
