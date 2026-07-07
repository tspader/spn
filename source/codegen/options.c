#include "codegen/codegen.h"
#include "sp.h"

void spn_codegen_read_option_defaults(spn_codegen_ctx_t* ctx, toml_table_t* table, const c8* key, spn_option_defaults_t* out) {
  toml_array_t* array = toml_table_array(table, key);
  if (array) {
    *out = sp_da_new(ctx->mem, spn_option_default_t);
    spn_codegen_push_key(ctx, key);
    sp_for(it, (u32)toml_array_len(array)) {
      spn_codegen_push_index(ctx, it);
      toml_table_t* element = toml_array_table(array, (s32)it);
      if (!element) {
        spn_codegen_issue_at(ctx, SPN_CODEGEN_ERR_EXPECTED_OBJECT, sp_str_lit(""));
        spn_codegen_pop(ctx);
        continue;
      }

      sp_for(kt, (u32)toml_table_len(element)) {
        s32 len = 0;
        const c8* name = toml_table_key(element, (s32)kt, &len);
        if (!name) {
          break;
        }
        if (!sp_cstr_equal(name, "value") && !sp_cstr_equal(name, "when")) {
          spn_codegen_issue(ctx, SPN_CODEGEN_ERR_INVALID, name);
        }
      }

      spn_option_default_t entry = sp_zero;
      toml_value_t str = toml_table_string(element, "value");
      toml_value_t boolean = toml_table_bool(element, "value");
      if (str.ok) {
        entry.value = spn_codegen_value_str(ctx, str);
      }
      else if (boolean.ok) {
        entry.value = spn_codegen_value_bool(boolean);
      }
      else if (spn_codegen_field_present(element, "value")) {
        spn_codegen_issue(ctx, SPN_CODEGEN_ERR_INVALID, "value");
        spn_codegen_pop(ctx);
        continue;
      }
      else {
        spn_codegen_issue(ctx, SPN_CODEGEN_ERR_MISSING_KEY, "value");
        spn_codegen_pop(ctx);
        continue;
      }

      spn_codegen_read_when(ctx, element, "when", &entry.when);
      sp_da_push(*out, entry);
      spn_codegen_pop(ctx);
    }
    spn_codegen_pop(ctx);
    return;
  }

  toml_value_t str = toml_table_string(table, key);
  toml_value_t boolean = toml_table_bool(table, key);
  if (str.ok || boolean.ok) {
    spn_option_default_t entry = {
      .value = str.ok ? spn_codegen_value_str(ctx, str) : spn_codegen_value_bool(boolean),
    };
    *out = sp_da_new(ctx->mem, spn_option_default_t);
    sp_da_push(*out, entry);
    return;
  }

  if (spn_codegen_field_present(table, key)) {
    spn_codegen_issue(ctx, SPN_CODEGEN_ERR_INVALID, key);
  }
}

void spn_codegen_write_option_defaults(sp_io_writer_t* out, const spn_option_defaults_t* in) {
  sp_io_write_c8(out, '[');
  sp_da_for(*in, it) {
    const spn_option_default_t* entry = &(*in)[it];
    if (it) {
      sp_io_write_c8(out, ',');
    }
    sp_io_write_c8(out, '{');
    bool first = true;
    spn_codegen_json_key(out, &first, sp_str_lit("value"));
    spn_codegen_write_option_value(out, entry->value);
    if (spn_codegen_when_present(&entry->when)) {
      spn_codegen_json_key(out, &first, sp_str_lit("when"));
      spn_codegen_write_when(out, &entry->when);
    }
    sp_io_write_c8(out, '}');
  }
  sp_io_write_c8(out, ']');
}

bool spn_codegen_option_defaults_present(const spn_option_defaults_t* in) {
  return !sp_da_empty(*in);
}
