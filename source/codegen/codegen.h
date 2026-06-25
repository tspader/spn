#ifndef SPN_CODEGEN_H
#define SPN_CODEGEN_H

#include "sp.h"
#include "toml.h"
#include "intern/intern.h"

typedef enum {
  SPN_CODEGEN_OK = 0,
  SPN_CODEGEN_ERR_EXPECTED_BOOL,
  SPN_CODEGEN_ERR_EXPECTED_STR,
  SPN_CODEGEN_ERR_EXPECTED_OBJECT,
  SPN_CODEGEN_ERR_MISSING_KEY,
  SPN_CODEGEN_ERR_DUPLICATE_KEY,
  SPN_CODEGEN_ERR_PARSE,
  SPN_CODEGEN_ERR_FILE_MISSING,
} spn_codegen_err_t;

typedef struct {
  spn_codegen_err_t code;
  sp_str_t path;
  sp_str_t detail;
} spn_codegen_issue_t;

#define SPN_CODEGEN_PATH_MAX 32

typedef enum {
  SPN_CODEGEN_PATH_KEY,
  SPN_CODEGEN_PATH_INDEX,
} spn_codegen_path_kind_t;

typedef struct {
  spn_codegen_path_kind_t kind;
  const c8* key;
  u32 index;
} spn_codegen_path_seg_t;

typedef struct {
  sp_mem_t mem;
  sp_intern_t* intern;
  sp_str_t dir;
  spn_codegen_path_seg_t path[SPN_CODEGEN_PATH_MAX];
  u32 depth;
  sp_da(spn_codegen_issue_t) issues;
} spn_codegen_ctx_t;

void spn_codegen_ctx_init(spn_codegen_ctx_t* ctx, sp_mem_t mem, sp_intern_t* intern);

void spn_codegen_push_key(spn_codegen_ctx_t* ctx, const c8* key);
void spn_codegen_push_index(spn_codegen_ctx_t* ctx, u32 index);
void spn_codegen_pop(spn_codegen_ctx_t* ctx);
bool spn_codegen_issue(spn_codegen_ctx_t* ctx, spn_codegen_err_t code, const c8* key);
bool spn_codegen_issue_at(spn_codegen_ctx_t* ctx, spn_codegen_err_t code, sp_str_t detail);

sp_str_t spn_codegen_intern(spn_codegen_ctx_t* ctx, sp_str_t value);
sp_str_t spn_codegen_path_join(spn_codegen_ctx_t* ctx, sp_str_t raw);
sp_da(sp_str_t) spn_codegen_read_path_array(spn_codegen_ctx_t* ctx, toml_table_t* table, const c8* key);

sp_str_t spn_codegen_str_required(spn_codegen_ctx_t* ctx, toml_table_t* table, const c8* key);
bool     spn_codegen_str_optional(spn_codegen_ctx_t* ctx, toml_table_t* table, const c8* key, sp_str_t* value);
sp_str_t spn_codegen_raw_required(spn_codegen_ctx_t* ctx, toml_table_t* table, const c8* key);
bool     spn_codegen_raw_optional(spn_codegen_ctx_t* ctx, toml_table_t* table, const c8* key, sp_str_t* value);
bool     spn_codegen_read_bool(spn_codegen_ctx_t* ctx, toml_table_t* table, const c8* key, bool* value);
sp_da(sp_str_t) spn_codegen_read_str_array(spn_codegen_ctx_t* ctx, toml_table_t* table, const c8* key);
void     spn_codegen_launcher_from_str(spn_codegen_ctx_t* ctx, sp_str_t raw, sp_str_t* program, sp_da(sp_str_t)* args);

const c8* spn_codegen_err_name(spn_codegen_err_t code);

typedef struct {
  sp_io_writer_t base;
  sp_io_writer_t* inner;
  u32 depth;
  bool in_string;
  bool escape;
  bool pending;
} spn_codegen_json_writer_t;

void spn_codegen_json_writer_init(spn_codegen_json_writer_t* writer, sp_io_writer_t* inner);

void spn_codegen_json_key(sp_io_writer_t* out, bool* first, sp_str_t key);
void spn_codegen_json_str(sp_io_writer_t* out, sp_str_t value);
void spn_codegen_json_bool(sp_io_writer_t* out, bool value);
void spn_codegen_json_str_array(sp_io_writer_t* out, sp_da(sp_str_t) values);
void spn_codegen_json_issues(sp_io_writer_t* out, sp_da(spn_codegen_issue_t) issues);
sp_str_t spn_codegen_issues_to_str(sp_mem_t mem, sp_da(spn_codegen_issue_t) issues);

struct spn_cg_manifest;

bool spn_codegen_load(spn_codegen_ctx_t* ctx, sp_str_t path, struct spn_cg_manifest* out);

#endif
