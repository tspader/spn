#include "codegen/codegen.h"
#include "sp.h"
#include "yyjson.h"

spn_option_value_t spn_toml_loader_value_str(spn_toml_loader_t* ctx, toml_value_t value) {
  return (spn_option_value_t) {
    .kind = SPN_OPTION_VALUE_STR,
    .str = spn_toml_loader_intern_value(ctx, value),
  };
}

spn_option_value_t spn_toml_loader_value_bool(toml_value_t value) {
  return (spn_option_value_t) {
    .kind = SPN_OPTION_VALUE_BOOL,
    .b = value.u.b,
  };
}

void spn_toml_loader_write_option_value(sp_io_writer_t* out, spn_option_value_t value) {
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

void spn_toml_loader_read_when(spn_toml_loader_t* ctx, toml_table_t* table, const c8* key, spn_when_t* out) {
  toml_table_t* child = toml_table_table(table, key);
  if (!child) {
    if (spn_toml_loader_field_present(table, key)) {
      spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_EXPECTED_OBJECT, key);
    }
    return;
  }

  out->clauses = sp_da_new(ctx->mem, spn_when_clause_t);
  spn_toml_loader_push_key(ctx, key);
  sp_for(it, (u32)toml_table_len(child)) {
    s32 len = 0;
    const c8* name = toml_table_key(child, (s32)it, &len);
    if (!name) {
      break;
    }

    spn_when_clause_t clause = { .key = spn_toml_loader_intern(ctx, sp_str(name, (u32)len)) };
    toml_value_t str = toml_table_string(child, name);
    toml_value_t boolean = toml_table_bool(child, name);
    toml_table_t* negation = toml_table_table(child, name);

    if (str.ok) {
      clause.value = spn_toml_loader_value_str(ctx, str);
    }
    else if (boolean.ok) {
      clause.value = spn_toml_loader_value_bool(boolean);
    }
    else if (negation) {
      toml_value_t not_str = toml_table_string(negation, "not");
      toml_value_t not_bool = toml_table_bool(negation, "not");
      if (toml_table_len(negation) != 1 || (!not_str.ok && !not_bool.ok)) {
        if (not_str.ok) {
          free(not_str.u.s);
        }
        spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_INVALID, name);
        continue;
      }
      clause.negated = true;
      clause.value = not_str.ok ? spn_toml_loader_value_str(ctx, not_str) : spn_toml_loader_value_bool(not_bool);
    }
    else {
      spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_INVALID, name);
      continue;
    }

    sp_da_push(out->clauses, clause);
  }
  spn_toml_loader_pop(ctx);
}

spn_option_value_t spn_json_option_value(yyjson_val* value, sp_mem_t mem) {
  if (yyjson_is_bool(value)) {
    return (spn_option_value_t) { .kind = SPN_OPTION_VALUE_BOOL, .b = yyjson_get_bool(value) };
  }
  if (yyjson_is_str(value)) {
    return (spn_option_value_t) {
      .kind = SPN_OPTION_VALUE_STR,
      .str = sp_str_from_cstr_n(mem, yyjson_get_str(value), (u32)yyjson_get_len(value)),
    };
  }
  return (spn_option_value_t) { .kind = SPN_OPTION_VALUE_NONE };
}

void spn_json_read_when(yyjson_val* obj, const c8* key, spn_when_t* out, sp_mem_t mem) {
  yyjson_val* child = yyjson_obj_get(obj, key);
  if (!yyjson_is_obj(child)) {
    return;
  }

  out->clauses = sp_da_new(mem, spn_when_clause_t);
  size_t idx, max;
  yyjson_val *name, *value;
  yyjson_obj_foreach(child, idx, max, name, value) {
    spn_when_clause_t clause = {
      .key = sp_str_from_cstr_n(mem, yyjson_get_str(name), (u32)yyjson_get_len(name)),
    };

    yyjson_val* negation = yyjson_is_obj(value) ? yyjson_obj_get(value, "not") : SP_NULLPTR;
    if (negation) {
      clause.negated = true;
      value = negation;
    }

    clause.value = spn_json_option_value(value, mem);
    if (clause.value.kind == SPN_OPTION_VALUE_NONE) {
      continue;
    }
    sp_da_push(out->clauses, clause);
  }
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
      spn_toml_loader_write_option_value(out, clause->value);
      sp_io_write_c8(out, '}');
    }
    else {
      spn_toml_loader_write_option_value(out, clause->value);
    }
  }
  sp_io_write_c8(out, '}');
}

bool spn_codegen_when_present(const spn_when_t* in) {
  return !sp_da_empty(in->clauses);
}
