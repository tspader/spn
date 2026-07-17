#include "codegen/codegen.h"
#include "config.gen.h"
#include "external/tom.h"
#include "intern/intern.h"
#include "manifest.gen.h"
#include "sp.h"
#include "sp/macro.h"
#include "spn.h"

static bool spn_toml_loader_read_version_part(spn_toml_loader_t* ctx, toml_table_t* table, const c8* key, s64 min, u16* out) {
  toml_value_t part = toml_table_int(table, key);
  if (!part.ok || part.u.i < min || part.u.i > UINT16_MAX) {
    return spn_toml_loader_issue(ctx, spn_toml_loader_field_present(table, key) ? SPN_CODEGEN_ERR_INVALID : SPN_CODEGEN_ERR_MISSING_KEY, key);
  }
  *out = (u16)part.u.i;
  return true;
}

void spn_toml_loader_read_os_version(spn_toml_loader_t* ctx, toml_table_t* table, const c8* key, spn_os_version_t* out) {
  toml_table_t* child = toml_table_table(table, key);
  if (!child) {
    if (spn_toml_loader_field_present(table, key)) {
      spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_EXPECTED_OBJECT, key);
    }
    return;
  }

  spn_toml_loader_push_key(ctx, key);
  spn_toml_loader_read_version_part(ctx, child, "major", 1, &out->major);
  if (spn_toml_loader_field_present(child, "minor")) {
    spn_toml_loader_read_version_part(ctx, child, "minor", 0, &out->minor);
  }
  spn_toml_loader_pop(ctx);
}

void spn_codegen_write_os_version(sp_io_writer_t* out, const spn_os_version_t* in) {
  bool first = true;
  sp_io_write_c8(out, '{');
  spn_codegen_json_key(out, &first, sp_str_lit("major"));
  sp_fmt_io(out, "{}", sp_fmt_uint(in->major));
  spn_codegen_json_key(out, &first, sp_str_lit("minor"));
  sp_fmt_io(out, "{}", sp_fmt_uint(in->minor));
  sp_io_write_c8(out, '}');
}

bool spn_codegen_os_version_present(const spn_os_version_t* in) {
  return in->major || in->minor;
}

spn_err_t spn_codegen_load(spn_toml_loader_t* ctx, sp_str_t path, spn_cg_manifest_t* out) {
  toml_table_t* table = spn_codegen_parse(ctx, path);
  if (table) {
    spn_manifest_read(ctx, table, out);
    toml_free(table);
  }
  return (sp_da_empty(ctx->issues)) ? SPN_OK : SPN_ERROR;
}

spn_err_t spn_codegen_load_config(spn_toml_loader_t* ctx, sp_str_t path, spn_cg_config_t* out) {
  toml_table_t* table = spn_codegen_parse(ctx, path);
  if (table) {
    spn_config_read(ctx, table, out);
    toml_free(table);
  }
  return (sp_da_empty(ctx->issues)) ? SPN_OK : SPN_ERROR;
}

spn_err_union_t spn_codegen_err(spn_toml_loader_t* ctx) {
  if (sp_da_empty(ctx->issues)) {
    return spn_result(SPN_OK);
  }

  return (spn_err_union_t) {
    .kind = SPN_ERR_MANIFEST_ISSUES,
    .issues = ctx->issues,
  };
}
