#pragma once

#include "config_data.h"

struct ConfigData *nfcd_make(cd_realloc realloc, void *ud, int config_size, int stringtable_size);
void nfcd_free(struct ConfigData *cd);

cd_loc nfcd_root(struct ConfigData *cd);
int nfcd_type(struct ConfigData *cd, cd_loc loc);
double nfcd_to_number(struct ConfigData *cd, cd_loc loc);
const char *nfcd_to_string(struct ConfigData *cd, cd_loc loc);
void *nfcd_to_handle(struct ConfigData *cd, cd_loc);
cd_handle_dealloc nfcd_to_handle_deallocator(struct ConfigData *cd, cd_loc loc);

int nfcd_array_size(struct ConfigData *cd, cd_loc arr);
cd_loc nfcd_array_item(struct ConfigData *cd, cd_loc arr, int i);

int nfcd_object_size(struct ConfigData *cd, cd_loc object);
cd_loc nfcd_object_keyloc(struct ConfigData *cd, cd_loc object, int i);
const char *nfcd_object_key(struct ConfigData *cd, cd_loc object, int i);
cd_loc nfcd_object_value(struct ConfigData *cd, cd_loc object, int i);
cd_loc nfcd_object_lookup(struct ConfigData *cd, cd_loc object, const char *key);

cd_loc nfcd_null();
cd_loc nfcd_undefined();
cd_loc nfcd_false();
cd_loc nfcd_true();
cd_loc nfcd_add_number(struct ConfigData **cd, double n);
cd_loc nfcd_add_string(struct ConfigData **cd, const char *s);
cd_loc nfcd_add_handle(struct ConfigData **cd, void *handle, cd_handle_dealloc deallocator);
cd_loc nfcd_add_array(struct ConfigData **cd, int size);
cd_loc nfcd_add_object(struct ConfigData **cd, int size);
void nfcd_set_root(struct ConfigData *cd, cd_loc root);

void nfcd_push(struct ConfigData **cd, cd_loc array, cd_loc item);
void nfcd_set(struct ConfigData **cd, cd_loc object, const char *key, cd_loc value);
void nfcd_set_loc(struct ConfigData **cd, cd_loc object, cd_loc key, cd_loc value);

cd_realloc nfcd_allocator(struct ConfigData *cd, void **user_data);

// nf_json_parser.c


struct nfjp_Settings
{
	int unquoted_keys;
	int c_comments;
	int implicit_root_object;
	int optional_commas;
	int equals_for_colon;
	int python_multiline_strings;
};
const char *nfjp_parse(const char *s, struct ConfigData **cdp);
const char *nfjp_parse_with_settings(const char *s, struct ConfigData **cdp, struct nfjp_Settings *settings);

// nf_memory_tracker.c


struct nfmt_Buffer {
	char *start;
	char *end;
};

void nfmt_init();
void nfmt_record_malloc(void *p, int size, const char *tag, const char *file, int line);
void nfmt_record_free(void *p);
struct nfmt_Buffer nfmt_read();

// nf_string_table.c


#define NFST_STRING_TABLE_FULL (-1)

struct nfst_StringTable;

void nfst_init(struct nfst_StringTable *st, int bytes, int average_string_size);
void nfst_grow(struct nfst_StringTable *st, int bytes);
int  nfst_pack(struct nfst_StringTable *st);
int nfst_to_symbol(struct nfst_StringTable *st, const char *s);
int nfst_to_symbol_const(const struct nfst_StringTable *st, const char *s);
const char *nfst_to_string(struct nfst_StringTable *, int symbol);

