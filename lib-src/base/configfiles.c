#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include "configfiles.h"



struct config_entry client_const[] = {
	{"cachefiles", CF 5000, 1},
	{"avgcachefilesize", CF 3, 1},
	{"primaryuser", CF -1, 0},
	{ NULL, CF 0, 0}
};

struct config_entry client_string[] = {
	{"configdir", CF "/usr/coda/etc", 1},
	{"cachedir", CF "/usr/coda/venus.cache", 1},
	{"hosts", CF 0, 0},
	{"kerndevice", CF "/dev/cfs0", 1},
	{NULL, CF 0, 0}
};

struct config_entry srv_const[] = {
	{"rvmsize", CF 0, 0},
	{NULL, CF 0, 0}
};

struct config_entry srv_string[] = {
	{"configdir", CF "/etc/vice", 1},
	{"rvmdata" , CF 0, 0},
	{"rvmlog", CF 0, 0},
	{NULL, CF 0, 0}
};


/* size of the entries array */
static int config_size(struct config_table *t) 
{
	int j = 0;

	if ( !t ) 
		return 0;

	while ((t->tbl_entries)[j].ent_key)
		j++;
	return j;
}


struct config_table *config_setup_table(pconfig_table table, pconfig_entry entries,  
					char *file, int constorstr)
{
	char line[256];
	char *theline = NULL;
	FILE *f = NULL;
	int rc = 0;
	char *key;
	char *val;
	char *rsr;
	int ent;
	int lineno = 0;
	
	if ( entries == NULL  || file == NULL || table == NULL) 
		return NULL;

	table->tbl_entries = entries;
	table->tbl_file = file;
	
	f = fopen(file, "r");
	if ( !f ) {
		perror("config_setup_table");
		return NULL;
	}
	
	while(feof(f) == 0)  {
		lineno++;
		theline = fgets(line, sizeof(line), f);

		if ( theline == NULL ) 
			break;

		/* printf("NOW doing: %s\n", theline); */
		rc = config_next(line, &key, &val, &rsr);

		/* line contains nothing useful */
		if (rc == 0)
			continue; 

		if ( rc == 1 ) {  
			fprintf(stderr, 
				"Error in configfile %s line %d, key %s has no value\n", 
				file, lineno, key); 
			return NULL;
		}
		
		if ( rc < 0 ) {
			fprintf(stderr, 
				"Error in configfile %s line %d:\n   %s\n", 
				file, lineno, line); 
			return NULL;
		}


		ent = config_find_key(key, table);
		if (ent < 0) {
			fprintf(stderr, 
				"Key %s found in %s (line %d) not is not configurable.\n",
				key, file, lineno);
			return NULL;
		}
		
		if (constorstr)
			rc = config_set_const(key, (int)strtol(val,0, 0), table);
		else 
			rc = config_set_string(key, val, table);
		if ( rc ) {
			fprintf(stderr, "Error configuring variable (%s, %s) in %s.\n",
				key, val, file);
			return NULL;
		}
	}

	return table;
}


/* return NULL on failure or the entry on success */
struct config_entry *config_find_entry(char *key, pconfig_table table)
{
	int j;
	struct config_entry *entries;

	if ( table == NULL || key == NULL ) 
		return -1;

	if ( *key == '\0' )
		return -1;

        entries = table->tbl_entries;

	j = 0;
	while ( entries[j].ent_key != NULL && 
		strcmp(key, entries[j].ent_key) != 0 )
		j++;

	/* found ? */
	if ( entries[j].ent_key == NULL ) 
		return NULL;
	else 
		return &(entries[j]);
}

	
/* config_set_string: return 0 on success, -1 on failure */
int config_set_string(char *key, char *value, struct config_table *table)
{
	int j;
	char **valp;
	int flag;
	char *cpy;

	if ( value == NULL || key == NULL || table == NULL) 
		return -1;

	j = config_find_key(key, table);
	if ( j < 0 )
		return -1;

	valp = &((table->tbl_entries)[j].ent_value.s);
	flag = (table->tbl_entries)[j].ent_flag;

	if ( (flag & CONFIG_MALLOCED) && *valp)
		free(*valp);
	
	*valp = (char *)malloc(strlen(value) + 1);
	assert(*valp);

	cpy = strcpy(*valp, value);
	table->tbl_entries[j].ent_flag = CONFIG_SET || CONFIG_MALLOCED;
	return 0;

}

/* config_set_const: return 0 on success, -1 on failure */
int config_set_const(char *key, int value, struct config_table *table)
{
	int j;

	j = config_find_key(key, table);
	if ( j < 0 )
		return -1;

	table->tbl_entries[j].ent_value.v = value;
	table->tbl_entries[j].ent_flag = CONFIG_SET;
	return 0;

}
	
/* config_get_name: return NULL on error, or char * to result */
char *config_get_string(char *key, struct config_table *table)
{
	int j;

	j = config_find_key(key, table);
	if ( j < 0 ) 
		return NULL;

	if ( (table->tbl_entries)[j].ent_flag & CONFIG_SET )
		return table->tbl_entries[j].ent_value.s;
	return NULL;
}

/* config_get_const: return 0 on success, -1 on failure, set *result */
int config_get_const(char *key, struct config_table *table, int *result)
{
	int j;

	if ( result == NULL )
		return -1;

	j = config_find_key(key, table);
	if ( j < 0 )
		return -1;

	if ( (table->tbl_entries)[j].ent_flag & CONFIG_SET ) {
		*result = table->tbl_entries[j].ent_value.v;
		return 0;
	} else
		return -1;
}

/* returns 
   0 if line appears empty
   1 if a key was found, but no value
   -1 if an equality sign if found but no key
   -2 if a key is found but no equality sign
   2 if a key and value was found
   3 if a resource, value and key was found */
int config_next(char *line, char **key, char **val, char **rsr)
{
	int rc=0;
	char *hash;
	char *eqsign;
	
	/* remove everything following a hash */
	hash = strchr(line, '#');
	if (hash)
		*hash = '\0';

	if (line[0] == '\0' )
		return 0;

	eqsign = strchr(line, '=');

	*key = strtok(line, "=\t \n");

	/* no key and no equality sign: line whitespace */
	if ( !*key && !eqsign)
		return 0;

	/* no equality sign: malformed */
	if ( !eqsign ) 
		return -1;
	
	/* there must be a key if we make it here*/
	assert(*key);

	/* whitespace before equality sign: malformed */
	if ( *key > eqsign )
		return -2;

	/* find the value */
	*val = strtok(NULL, "\t =\n");
	if ( !*val ) 
		return 1;

	printf("Found: %s, value %s, rc %d\n", *key, *val, rc);
	return 2;
}

void config_print_const(struct config_table *table, FILE *file) 
{
	int j;

	if ( !table  || !table->tbl_entries) 
		return;
	
	for (j = 0; j < config_size(table); j++) {
		fprintf(file, "key %s value %d (%#x)\n",
		       (table->tbl_entries)[j].ent_key,
		       (table->tbl_entries)[j].ent_value.v,
		       (table->tbl_entries)[j].ent_value.v);
	}
}

void config_print_string(struct config_table *table, FILE *file) 
{
	int j;

	if ( !table  || !table->tbl_entries) 
		return;
	
	for (j = 0; j < config_size(table); j++) {
		fprintf(file, "key %s value \"%s\"\n",
		       (table->tbl_entries)[j].ent_key,
		       (table->tbl_entries)[j].ent_value.s);
	}
}


	
pconfig_rsr *rsr = {
	{ "", client_strings, client_constants }, 
	{ "server", server_strings, server_constants },
	{ NULL , NULL, NULL }
};

struct config_prog conf = {
	NULL,
}

int main(int argc, char **argv)
{
	int rc = 0;
	int constant;
	char *str;
	struct config_table testtable;
	
	if ( argc !=3 ) {
		printf("Usage %s constfile stringfile", argv[0]);
		exit(1);
	}
		
	config_setup_table(&testtable, client_const, argv[1], CFG_CONST);
	rc = config_get_const("primaryuser", &testtable, &constant);
	if ( rc == 0) 
		printf("Lookup of primaryuser: %d\n", constant);
	else 
		printf("Error cannot find primary user!\n");

	config_print_const(&testtable, stdout);

	config_setup_table(&testtable, client_string, argv[2], CFG_STRING);
	str = config_get_string("cachedir", &testtable);
	if (str) 
		printf("Lookup of cachedir: %d\n", constant);
	else 
		printf("Error cannot find cachedir user!\n");

	config_print_string(&testtable, stdout);

	return 0;
}
