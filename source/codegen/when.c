#include "codegen/codegen.h"
#include "sp.h"

spn_option_value_t spn_codegen_value_str(spn_codegen_ctx_t* ctx, toml_value_t value) {
  return (spn_option_value_t) {
    .kind = SPN_OPTION_VALUE_STR,
    .str = spn_codegen_intern(ctx, sp_str(value.u.s, (u32)value.u.sl)),
  };
}

spn_option_value_t spn_codegen_value_bool(toml_value_t value) {
  return (spn_option_value_t) {
    .kind = SPN_OPTION_VALUE_BOOL,
    .b = value.u.b,
  };
}

void spn_codegen_write_option_value(sp_io_writer_t* out, spn_option_value_t value) {
  switch (value.kind) {
    case SPN_OPTION_VALUE_STR: {
      spn_codegen_json_str(out, value.str);
      break;
    }
    case SPN_OPTION_VALUE_BOOL: {
      spn_codegen_json_bool(out, value.b);
      break;
    }
    case SPN_OPTION_VALUE_NONE: {
      sp_io_write_str(out, sp_str_lit("null"), SP_NULLPTR);
      break;
    }
  }
}

void spn_codegen_read_when(spn_codegen_ctx_t* ctx, toml_table_t* table, const c8* key, spn_when_t* out) {
  toml_table_t* child = toml_table_table(table, key);
  if (!child) {
    if (spn_codegen_field_present(table, key)) {
      spn_codegen_issue(ctx, SPN_CODEGEN_ERR_EXPECTED_OBJECT, key);
    }
    return;
  }

  out->clauses = sp_da_new(ctx->mem, spn_when_clause_t);
  spn_codegen_push_key(ctx, key);
  sp_for(it, (u32)toml_table_len(child)) {
    s32 len = 0;
    const c8* name = toml_table_key(child, (s32)it, &len);
    if (!name) {
      break;
    }

    spn_when_clause_t clause = { .key = spn_codegen_intern(ctx, sp_str(name, (u32)len)) };
    toml_value_t str = toml_table_string(child, name);
    toml_value_t boolean = toml_table_bool(child, name);
    toml_table_t* negation = toml_table_table(child, name);

    if (str.ok) {
      clause.value = spn_codegen_value_str(ctx, str);
    }
    else if (boolean.ok) {
      clause.value = spn_codegen_value_bool(boolean);
    }
    else if (negation) {
      toml_value_t not_str = toml_table_string(negation, "not");
      toml_value_t not_bool = toml_table_bool(negation, "not");
      if (toml_table_len(negation) != 1 || (!not_str.ok && !not_bool.ok)) {
        spn_codegen_issue(ctx, SPN_CODEGEN_ERR_INVALID, name);
        continue;
      }
      clause.negated = true;
      clause.value = not_str.ok ? spn_codegen_value_str(ctx, not_str) : spn_codegen_value_bool(not_bool);
    }
    else {
      spn_codegen_issue(ctx, SPN_CODEGEN_ERR_INVALID, name);
      continue;
    }

    sp_da_push(out->clauses, clause);
  }
  spn_codegen_pop(ctx);
}

void spn_codegen_write_when(sp_io_writer_t* out, const spn_when_t* in) {
  sp_io_write_c8(out, '{');
  sp_da_for(in->clauses, it) {
    const spn_when_clause_t* clause = &in->clauses[it];
    if (it) {
      sp_io_write_c8(out, ',');
    }
    spn_codegen_json_str(out, clause->key);
    sp_io_write_c8(out, ':');
    if (clause->negated) {
      sp_io_write_c8(out, '{');
      bool first = true;
      spn_codegen_json_key(out, &first, sp_str_lit("not"));
      spn_codegen_write_option_value(out, clause->value);
      sp_io_write_c8(out, '}');
    }
    else {
      spn_codegen_write_option_value(out, clause->value);
    }
  }
  sp_io_write_c8(out, '}');
}

bool spn_codegen_when_present(const spn_when_t* in) {
  return !sp_da_empty(in->clauses);
}
