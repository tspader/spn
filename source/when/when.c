#include "when/when.h"

#include "enum/enum.h"

spn_option_value_t spn_option_value_none(void) {
  return (spn_option_value_t) { .kind = SPN_OPTION_VALUE_NONE };
}

spn_option_value_t spn_option_value_bool(bool value) {
  return (spn_option_value_t) { .kind = SPN_OPTION_VALUE_BOOL, .b = value };
}

spn_option_value_t spn_option_value_str(sp_str_t value) {
  return (spn_option_value_t) { .kind = SPN_OPTION_VALUE_STR, .str = value };
}

bool spn_option_value_equal(spn_option_value_t a, spn_option_value_t b) {
  if (a.kind != b.kind) {
    return false;
  }
  switch (a.kind) {
    case SPN_OPTION_VALUE_NONE: return true;
    case SPN_OPTION_VALUE_BOOL: return a.b == b.b;
    case SPN_OPTION_VALUE_STR:  return sp_str_equal(a.str, b.str);
  }
  SP_UNREACHABLE_RETURN(false);
}

sp_str_t spn_option_value_to_str(sp_mem_t mem, spn_option_value_t value) {
  switch (value.kind) {
    case SPN_OPTION_VALUE_NONE: return sp_str_lit("none");
    case SPN_OPTION_VALUE_BOOL: return value.b ? sp_str_lit("true") : sp_str_lit("false");
    case SPN_OPTION_VALUE_STR:  return sp_fmt(mem, "\"{}\"", sp_fmt_str(value.str)).value;
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

void spn_when_env_init(sp_mem_t mem, spn_when_env_t* env) {
  sp_str_ht_init(mem, *env);
}

void spn_when_env_set(spn_when_env_t* env, sp_str_t key, spn_option_value_t value) {
  sp_str_ht_insert(*env, key, value);
}

void spn_when_env_set_facts(spn_when_env_t* env, spn_when_facts_t facts) {
  spn_when_env_set(env, sp_str_lit("os"), spn_option_value_str(spn_os_to_str(facts.os)));
  spn_when_env_set(env, sp_str_lit("arch"), spn_option_value_str(spn_arch_to_str(facts.arch)));
  spn_when_env_set(env, sp_str_lit("abi"), spn_option_value_str(spn_abi_to_str(facts.abi)));
  spn_when_env_set(env, sp_str_lit("mode"), spn_option_value_str(spn_build_mode_to_str(facts.mode)));
  spn_when_env_set(env, sp_str_lit("opt"), spn_option_value_str(spn_opt_level_to_str(facts.opt)));
  spn_when_env_set(env, sp_str_lit("sanitize_address"), spn_option_value_bool(facts.sanitizers & SPN_SANITIZER_ADDRESS));
  spn_when_env_set(env, sp_str_lit("sanitize_thread"), spn_option_value_bool(facts.sanitizers & SPN_SANITIZER_THREAD));
  spn_when_env_set(env, sp_str_lit("sanitize_undefined"), spn_option_value_bool(facts.sanitizers & SPN_SANITIZER_UNDEFINED));
  spn_when_env_set(env, sp_str_lit("sanitize_memory"), spn_option_value_bool(facts.sanitizers & SPN_SANITIZER_MEMORY));
  spn_when_env_set(env, sp_str_lit("sanitize_leak"), spn_option_value_bool(facts.sanitizers & SPN_SANITIZER_LEAK));
}

bool spn_when_eval(const spn_when_t* when, spn_when_env_t* env) {
  if (!when) {
    return true;
  }
  sp_da_for(when->clauses, it) {
    const spn_when_clause_t* clause = &when->clauses[it];
    spn_option_value_t* current = sp_str_ht_get(*env, clause->key);
    if (!current) {
      return false;
    }
    if (spn_option_value_equal(*current, clause->value) == clause->negated) {
      return false;
    }
  }
  return true;
}

sp_str_t spn_when_to_str(sp_mem_t mem, const spn_when_t* when) {
  if (!when || sp_da_empty(when->clauses)) {
    return sp_str_lit("always");
  }
  sp_io_dyn_mem_writer_t out;
  sp_io_dyn_mem_writer_init(mem, &out);
  sp_da_for(when->clauses, it) {
    const spn_when_clause_t* clause = &when->clauses[it];
    if (it) {
      sp_fmt_io(&out.base, ", ");
    }
    sp_fmt_io(&out.base, "{} {} {}",
      sp_fmt_str(clause->key),
      sp_fmt_cstr(clause->negated ? "!=" : "="),
      sp_fmt_str(spn_option_value_to_str(mem, clause->value)));
  }
  return sp_io_dyn_mem_writer_as_str(&out);
}

bool spn_option_value_ok(const spn_option_info_t* option, spn_option_value_t value) {
  switch (option->type) {
    case SPN_OPTION_TYPE_NONE: {
      return false;
    }
    case SPN_OPTION_TYPE_BOOL: {
      return value.kind == SPN_OPTION_VALUE_BOOL;
    }
    case SPN_OPTION_TYPE_ENUM: {
      if (value.kind != SPN_OPTION_VALUE_STR) {
        return false;
      }
      sp_da_for(option->values, it) {
        if (sp_str_equal(option->values[it], value.str)) {
          return true;
        }
      }
      return false;
    }
  }
  SP_UNREACHABLE_RETURN(false);
}

spn_option_value_t spn_option_resolve(const spn_option_info_t* option, spn_when_env_t* env) {
  sp_da_for(option->defaults, it) {
    if (spn_when_eval(&option->defaults[it].when, env)) {
      return option->defaults[it].value;
    }
  }
  return spn_option_value_none();
}
