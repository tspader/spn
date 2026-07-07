#ifndef SPN_TOML_TYPES_H
#define SPN_TOML_TYPES_H

#include "sp.h"

typedef enum {
  SPN_TOML_EDIT_VALUE_SCALAR,
  SPN_TOML_EDIT_VALUE_STRING,
  SPN_TOML_EDIT_VALUE_ARRAY,
  SPN_TOML_EDIT_VALUE_TABLE,
} spn_toml_edit_value_kind_t;

typedef struct {
  u32 start;
  u32 end;
} spn_toml_edit_span_t;

typedef struct spn_toml_edit_entry spn_toml_edit_entry_t;
struct spn_toml_edit_entry {
  sp_da(sp_str_t) path;
  spn_toml_edit_value_kind_t kind;
  spn_toml_edit_span_t key;
  spn_toml_edit_span_t value;
  u32 line_end;
  sp_da(spn_toml_edit_entry_t) entries;
};

typedef struct {
  sp_da(sp_str_t) path;
  bool array;
  u32 content;
  sp_da(spn_toml_edit_entry_t) entries;
} spn_toml_edit_section_t;

typedef struct {
  u32 at;
  u32 remove;
  u32 seq;
  sp_str_t insert;
} spn_toml_edit_splice_t;

typedef struct {
  sp_mem_t mem;
  sp_str_t source;
  sp_str_t eol;
  sp_da(spn_toml_edit_section_t) sections;
  sp_da(spn_toml_edit_splice_t) splices;
} spn_toml_edit_t;

#endif
