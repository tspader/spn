#include "pkg/options.h"

#include "event/event.h"
#include "event/types.h"
#include "when/when.h"

spn_pkg_config_t* spn_pkg_config_find(sp_da(spn_pkg_config_entry_t) config, sp_str_t name) {
  sp_da_for(config, it) {
    if (sp_str_equal(config[it].key, name)) {
      return &config[it].value;
    }
  }
  return SP_NULLPTR;
}

void spn_when_env_from_profile(sp_mem_t mem, const spn_profile_info_t* profile, spn_when_env_t* env) {
  spn_when_env_init(mem, env);
  spn_when_env_set_facts(env, profile->os, profile->arch, profile->abi, profile->mode);
}

void spn_when_env_add_options(spn_when_env_t* env, const spn_resolved_options_t* options) {
  sp_da_for(*options, it) {
    if ((*options)[it].value.kind == SPN_OPTION_VALUE_NONE) {
      continue;
    }
    spn_when_env_set(env, (*options)[it].name, (*options)[it].value);
  }
}

static spn_err_t option_fail(spn_event_buffer_t* events, spn_evt_option_t evt) {
  if (events) {
    spn_event_buffer_push(events, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR_OPTION,
      .option = evt,
    });
  }
  return SPN_ERROR;
}

static const spn_when_clause_t* find_clause(const spn_when_t* set, sp_str_t key) {
  if (!set) {
    return SP_NULLPTR;
  }
  sp_da_for(set->clauses, it) {
    if (sp_str_equal(set->clauses[it].key, key)) {
      return &set->clauses[it];
    }
  }
  return SP_NULLPTR;
}

static spn_err_t validate_setter(sp_mem_t mem, sp_str_t pkg, spn_option_info_om_t options, const spn_when_t* set, sp_str_t setter, spn_event_buffer_t* events) {
  sp_da_for(set->clauses, it) {
    const spn_when_clause_t* clause = &set->clauses[it];
    spn_option_info_t** option = sp_str_om_getp(options, clause->key);
    if (!option) {
      return option_fail(events, (spn_evt_option_t) {
        .err = SPN_OPTION_ERR_UNDECLARED,
        .pkg = pkg,
        .option = clause->key,
        .a = setter,
      });
    }
    if (!spn_option_value_ok(*option, clause->value)) {
      return option_fail(events, (spn_evt_option_t) {
        .err = SPN_OPTION_ERR_BAD_VALUE,
        .pkg = pkg,
        .option = clause->key,
        .value = spn_option_value_to_str(mem, clause->value),
        .a = setter,
      });
    }
  }
  return SPN_OK;
}

spn_err_t spn_pkg_options_merge(
  sp_mem_t mem,
  sp_str_t pkg,
  spn_option_info_om_t options,
  const spn_profile_info_t* profile,
  sp_da(spn_pkg_config_entry_t) root_config,
  bool is_root,
  sp_da(spn_option_request_t) requests,
  spn_event_buffer_t* events,
  spn_resolved_options_t* out
) {
  spn_pkg_config_t* config = spn_pkg_config_find(root_config, pkg);
  const spn_when_t* set = is_root ? &profile->options : config ? &config->options : SP_NULLPTR;
  sp_str_t setter = is_root ? sp_str_lit("the profile") : sp_str_lit("the root manifest");
  bool defaults_declined = config && config->defaults_declined;

  if (events && set) {
    spn_try(validate_setter(mem, pkg, options, set, setter, events));
  }
  if (events) {
    sp_da_for(requests, it) {
      spn_try(validate_setter(mem, pkg, options, requests[it].options, requests[it].consumer, events));
    }
  }

  *out = sp_da_new(mem, spn_resolved_option_t);
  spn_when_env_t env;
  spn_when_env_from_profile(mem, profile, &env);

  sp_str_om_for(options, it) {
    spn_option_info_t* option = sp_str_om_at(options, it);
    const spn_when_clause_t* setter_clause = find_clause(set, option->name);
    if (setter_clause && setter_clause->negated) {
      setter_clause = SP_NULLPTR;
    }

    // The value this option lands on when nobody sets it, resolved against
    // the env as it stands before this option joins it; is_default compares
    // against this same value so the fingerprint and the merge can't disagree
    spn_option_value_t fallback = sp_zero;
    if (!defaults_declined) {
      fallback = spn_option_resolve(option, &env);
    }
    if (fallback.kind == SPN_OPTION_VALUE_NONE && option->type == SPN_OPTION_TYPE_BOOL) {
      fallback = spn_option_value_bool(false);
    }

    spn_resolved_option_t resolved = { .name = option->name };
    bool settled = false;
    sp_str_t winner = sp_str_lit("the default");

    if (setter_clause) {
      resolved.value = setter_clause->value;
      settled = true;
      winner = setter;
    }
    else if (option->additive) {
      bool value = false;
      sp_da_for(requests, rt) {
        const spn_when_clause_t* clause = find_clause(requests[rt].options, option->name);
        if (!clause || clause->negated) {
          continue;
        }
        value |= clause->value.kind == SPN_OPTION_VALUE_BOOL && clause->value.b;
        settled = true;
      }
      if (settled) {
        resolved.value = spn_option_value_bool(value);
        winner = sp_str_lit("the union of requests");
      }
    }
    else {
      sp_da_for(requests, rt) {
        const spn_when_clause_t* clause = find_clause(requests[rt].options, option->name);
        if (!clause || clause->negated) {
          continue;
        }
        if (settled && !spn_option_value_equal(resolved.value, clause->value)) {
          return option_fail(events, (spn_evt_option_t) {
            .err = SPN_OPTION_ERR_CONFLICT,
            .pkg = pkg,
            .option = option->name,
            .value = spn_option_value_to_str(mem, clause->value),
            .a = winner,
            .b = requests[rt].consumer,
          });
        }
        resolved.value = clause->value;
        winner = requests[rt].consumer;
        settled = true;
      }
    }

    if (!settled) {
      resolved.value = fallback;
      if (resolved.value.kind == SPN_OPTION_VALUE_NONE && events) {
        return option_fail(events, (spn_evt_option_t) {
          .err = SPN_OPTION_ERR_NO_VALUE,
          .pkg = pkg,
          .option = option->name,
        });
      }
    }

    sp_da_for(requests, rt) {
      const spn_when_clause_t* clause = find_clause(requests[rt].options, option->name);
      if (!clause || !clause->negated) {
        continue;
      }
      if (spn_option_value_equal(resolved.value, clause->value)) {
        return option_fail(events, (spn_evt_option_t) {
          .err = SPN_OPTION_ERR_VETO,
          .pkg = pkg,
          .option = option->name,
          .value = spn_option_value_to_str(mem, clause->value),
          .a = requests[rt].consumer,
          .b = winner,
        });
      }
    }

    resolved.is_default = spn_option_value_equal(resolved.value, fallback);
    if (resolved.value.kind != SPN_OPTION_VALUE_NONE) {
      spn_when_env_set(&env, resolved.name, resolved.value);
    }
    sp_da_push(*out, resolved);
  }

  return SPN_OK;
}

void spn_pkg_options_env(
  sp_mem_t mem,
  sp_str_t pkg,
  spn_option_info_om_t options,
  const spn_profile_info_t* profile,
  sp_da(spn_pkg_config_entry_t) root_config,
  bool is_root,
  sp_da(spn_option_request_t) requests,
  spn_when_env_t* env
) {
  spn_resolved_options_t resolved = sp_zero;
  spn_pkg_options_merge(mem, pkg, options, profile, root_config, is_root, requests, SP_NULLPTR, &resolved);
  spn_when_env_from_profile(mem, profile, env);
  spn_when_env_add_options(env, &resolved);
}

static void apply_gated(sp_da(sp_str_t)* plain, spn_gated_list_t gated, spn_when_env_t* env) {
  sp_da_for(gated, it) {
    if (!spn_when_eval(&gated[it].when, env)) {
      continue;
    }
    sp_da_push(*plain, gated[it].value);
  }
}

static void apply_target(spn_target_info_t* target, spn_when_env_t* env) {
  apply_gated(&target->source, target->gated.source, env);
  apply_gated(&target->define, target->gated.define, env);
  apply_gated(&target->flags, target->gated.flags, env);
  apply_gated(&target->system_deps, target->gated.system_deps, env);
  apply_gated(&target->deps, target->gated.deps, env);
}

void spn_pkg_apply_options(spn_pkg_info_t* info, spn_when_env_t* env) {
  if (info->applied) {
    return;
  }
  info->applied = true;

  sp_str_om_for(info->libs, it)    apply_target(sp_str_om_at(info->libs, it), env);
  sp_str_om_for(info->exes, it)    apply_target(sp_str_om_at(info->exes, it), env);
  sp_str_om_for(info->scripts, it) apply_target(sp_str_om_at(info->scripts, it), env);
  sp_str_om_for(info->tests, it)   apply_target(sp_str_om_at(info->tests, it), env);

  apply_gated(&info->system_deps, info->gated.system_deps, env);

  sp_str_om_for(info->options, it) {
    spn_option_info_t* option = sp_str_om_at(info->options, it);
    if (sp_str_empty(option->define)) {
      continue;
    }
    spn_option_value_t* value = sp_str_ht_get(*env, option->name);
    if (!value || value->kind != SPN_OPTION_VALUE_BOOL || !value->b) {
      continue;
    }
    sp_da_push(info->define, option->define);
    if (option->public) {
      sp_da_push(info->public_define, option->define);
    }
  }
}
