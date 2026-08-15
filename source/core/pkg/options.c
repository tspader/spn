#include "pkg/options.h"

#include "paths/paths.h"
#include "resolve/types.h"
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
  spn_when_env_set_facts(env, (spn_when_facts_t) {
    .os = profile->os,
    .arch = profile->arch,
    .abi = profile->abi,
    .mode = profile->mode,
    .opt = profile->opt,
    .sanitizers = profile->sanitizers,
  });
}

void spn_when_env_add_options(spn_when_env_t* env, const spn_resolved_options_t* options) {
  sp_da_for(*options, it) {
    if ((*options)[it].value.kind == SPN_OPTION_VALUE_NONE) {
      continue;
    }
    spn_when_env_set(env, (*options)[it].name, (*options)[it].value);
  }
}

typedef struct {
  const spn_when_t* clauses;
  spn_option_setter_t setter;
  bool defaults_declined;
} option_set_t;

static option_set_t find_set(const spn_resolved_pkg_t* pkg, const spn_profile_info_t* profile, sp_da(spn_pkg_config_entry_t) root_config) {
  bool is_root = pkg->source == SPN_PKG_SOURCE_ROOT;
  spn_pkg_config_t* config = spn_pkg_config_find(root_config, pkg->name);
  return (option_set_t) {
    .clauses = is_root ? &profile->options : config ? &config->options : SP_NULLPTR,
    .setter = { .kind = is_root ? SPN_OPTION_SETTER_PROFILE : SPN_OPTION_SETTER_ROOT_MANIFEST },
    .defaults_declined = config && config->defaults_declined,
  };
}

typedef struct {
  sp_str_t option;
  spn_option_setter_t setter;
  spn_option_value_t value;
  bool negated;
} claim_t;

static spn_option_setter_t setter_consumer(sp_str_t name) {
  return (spn_option_setter_t) { .kind = SPN_OPTION_SETTER_CONSUMER, .name = name };
}

static void gather_claims(
  const spn_resolved_pkg_t* pkg,
  const spn_when_t* set,
  spn_option_setter_t setter,
  sp_da(claim_t)* claims,
  spn_option_violations_t* violations
) {
  sp_da_for(set->clauses, it) {
    const spn_when_clause_t* clause = &set->clauses[it];
    spn_option_info_t** option = sp_str_om_getp(pkg->options, clause->key);
    if (!option) {
      sp_da_push(*violations, ((spn_option_violation_t) {
        .kind = SPN_OPTION_ERR_UNDECLARED,
        .pkg = pkg->name,
        .option = clause->key,
        .a = setter,
      }));
      continue;
    }
    if (!spn_option_value_ok(*option, clause->value)) {
      sp_da_push(*violations, ((spn_option_violation_t) {
        .kind = SPN_OPTION_ERR_BAD_VALUE,
        .pkg = pkg->name,
        .option = clause->key,
        .value = clause->value,
        .a = setter,
      }));
      continue;
    }
    if (clause->negated && setter.kind != SPN_OPTION_SETTER_CONSUMER) {
      continue;
    }
    sp_da_push(*claims, ((claim_t) {
      .option = clause->key,
      .setter = setter,
      .value = clause->value,
      .negated = clause->negated,
    }));
  }
}

void spn_pkg_options_merge(
  sp_mem_t mem,
  const spn_resolved_pkg_t* pkg,
  const spn_profile_info_t* profile,
  sp_da(spn_pkg_config_entry_t) root_config,
  spn_option_requests_t requests,
  spn_merged_options_t* merged
) {
  option_set_t set = find_set(pkg, profile, root_config);

  merged->options = sp_da_new(mem, spn_resolved_option_t);
  merged->violations = sp_da_new(mem, spn_option_violation_t);

  sp_da(claim_t) claims = sp_da_new(mem, claim_t);
  if (set.clauses) {
    gather_claims(pkg, set.clauses, set.setter, &claims, &merged->violations);
  }
  sp_da_for(requests, rt) {
    gather_claims(pkg, requests[rt].options, setter_consumer(requests[rt].consumer), &claims, &merged->violations);
  }

  spn_when_env_t env;
  spn_when_env_from_profile(mem, profile, &env);

  sp_str_om_for(pkg->options, it) {
    spn_option_info_t* option = sp_str_om_at(pkg->options, it);

    spn_option_value_t fallback = sp_zero;
    if (!set.defaults_declined) {
      fallback = spn_option_resolve(option, &env);
    }
    if (fallback.kind == SPN_OPTION_VALUE_NONE && option->type == SPN_OPTION_TYPE_BOOL) {
      fallback = spn_option_value_bool(false);
    }

    spn_resolved_option_t resolved = { .name = option->name, .setter = { .kind = SPN_OPTION_SETTER_DEFAULT } };
    bool settled = false;
    bool value_union = false;
    const claim_t* last = SP_NULLPTR;

    sp_da_for(claims, ct) {
      const claim_t* claim = &claims[ct];
      if (!sp_str_equal(claim->option, option->name) || claim->negated) {
        continue;
      }
      if (claim->setter.kind != SPN_OPTION_SETTER_CONSUMER) {
        resolved.value = claim->value;
        resolved.setter = claim->setter;
        settled = true;
        break;
      }
      if (option->additive) {
        value_union |= claim->value.kind == SPN_OPTION_VALUE_BOOL && claim->value.b;
        resolved.value = spn_option_value_bool(value_union);
        resolved.setter = (spn_option_setter_t) { .kind = SPN_OPTION_SETTER_UNION };
        settled = true;
        continue;
      }
      if (last && !spn_option_value_equal(last->value, claim->value)) {
        sp_da_push(merged->violations, ((spn_option_violation_t) {
          .kind = SPN_OPTION_ERR_CONFLICT,
          .pkg = pkg->name,
          .option = option->name,
          .value = claim->value,
          .a = last->setter,
          .b = claim->setter,
        }));
      }
      resolved.value = claim->value;
      resolved.setter = claim->setter;
      settled = true;
      last = claim;
    }

    if (!settled) {
      resolved.value = fallback;
    }

    if (resolved.value.kind == SPN_OPTION_VALUE_NONE) {
      sp_da_push(merged->violations, ((spn_option_violation_t) {
        .kind = SPN_OPTION_ERR_NO_VALUE,
        .pkg = pkg->name,
        .option = option->name,
      }));
    }

    sp_da_for(claims, ct) {
      const claim_t* claim = &claims[ct];
      if (!sp_str_equal(claim->option, option->name) || !claim->negated) {
        continue;
      }
      if (spn_option_value_equal(resolved.value, claim->value)) {
        sp_da_push(merged->violations, ((spn_option_violation_t) {
          .kind = SPN_OPTION_ERR_VETO,
          .pkg = pkg->name,
          .option = option->name,
          .value = claim->value,
          .a = claim->setter,
          .b = resolved.setter,
        }));
      }
    }

    resolved.is_default = spn_option_value_equal(resolved.value, fallback);
    if (resolved.value.kind != SPN_OPTION_VALUE_NONE) {
      spn_when_env_set(&env, resolved.name, resolved.value);
    }
    sp_da_push(merged->options, resolved);
  }
}

void spn_pkg_options_env(
  sp_mem_t mem,
  const spn_resolved_pkg_t* pkg,
  const spn_profile_info_t* profile,
  sp_da(spn_pkg_config_entry_t) root_config,
  spn_option_requests_t requests,
  spn_when_env_t* env
) {
  spn_merged_options_t merged = sp_zero;
  spn_pkg_options_merge(mem, pkg, profile, root_config, requests, &merged);
  spn_when_env_from_profile(mem, profile, env);
  spn_when_env_add_options(env, &merged.options);
}

typedef struct {
  sp_mem_t mem;
  const spn_path_roots_t* roots;
  spn_tree_roots_t trees;
  spn_when_env_t* env;
} apply_ctx_t;

static void apply_gated(apply_ctx_t* ctx, sp_da(sp_str_t)* plain, spn_gated_list_t gated) {
  sp_da_for(gated, it) {
    if (!spn_when_eval(&gated[it].when, ctx->env)) {
      continue;
    }
    sp_da_push(*plain, gated[it].value);
  }
}

static void apply_gated_paths(apply_ctx_t* ctx, sp_da(spn_path_t)* plain, spn_gated_path_list_t gated) {
  sp_da_for(gated, it) {
    if (!spn_when_eval(&gated[it].when, ctx->env)) {
      continue;
    }
    spn_path_t path = spn_tree_path(ctx->mem, ctx->roots, ctx->trees, gated[it].tree, gated[it].path);
    sp_da_push(*plain, spn_path_canonicalize(ctx->mem, ctx->roots, path));
  }
}

static void apply_target(apply_ctx_t* ctx, spn_target_info_t* target) {
  apply_gated_paths(ctx, &target->source, target->gated.source);
  apply_gated_paths(ctx, &target->headers, target->gated.headers);
  apply_gated_paths(ctx, &target->include, target->gated.include);
  apply_gated(ctx, &target->define, target->gated.define);
  apply_gated(ctx, &target->flags, target->gated.flags);
  apply_gated(ctx, &target->system_deps, target->gated.system_deps);
  apply_gated(ctx, &target->deps, target->gated.deps);
}

void spn_pkg_apply_options(
  sp_mem_t mem,
  spn_pkg_info_t* info,
  const spn_path_roots_t* roots,
  spn_tree_roots_t trees,
  spn_when_env_t* env
) {
  if (info->applied) {
    return;
  }
  info->applied = true;

  apply_ctx_t ctx = { .mem = mem, .roots = roots, .trees = trees, .env = env };
  sp_str_om_for(info->libs, it) apply_target(&ctx, sp_str_om_at(info->libs, it));
  sp_str_om_for(info->exes, it) apply_target(&ctx, sp_str_om_at(info->exes, it));
  sp_str_om_for(info->scripts, it) apply_target(&ctx, sp_str_om_at(info->scripts, it));
  sp_str_om_for(info->tests, it) apply_target(&ctx, sp_str_om_at(info->tests, it));
  sp_str_om_for(info->examples, it) apply_target(&ctx, sp_str_om_at(info->examples, it));

  apply_gated(&ctx, &info->system_deps, info->gated.system_deps);
  apply_gated_paths(&ctx, &info->include, info->gated.include);

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
