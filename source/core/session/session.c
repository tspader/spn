#include "session/session.h"

#include "sp.h"
#include "ctx/types.h"
#include "spn/errors.h"
#include "core/types.h"
#include "resolve/types.h"
#include "session/types.h"
#include "spn/core.h"
#include "unit/types.h"
#include "unit/unit.h"

#include "compiler/driver.h"
#include "error/error.h"
#include "event/event.h"
#include "intern/intern.h"
#include "paths/paths.h"
#include "project/types.h"
#include "pkg/pkg.h"
#include "pkg/options.h"
#include "profile/profile.h"
#include "toolchain/select.h"
#include "toolchain/toolchain.h"
#include "triple/triple.h"

static sp_str_t resolve_macos_sdk(sp_mem_t mem, sp_env_t* env) {
  sp_str_t sdk = sp_env_get(env, sp_str_lit("SPN_MACOS_SDK"));
  if (!sp_str_empty(sdk)) {
    return sdk;
  }
  if (spn_triple_host().os != SPN_OS_MACOS) {
    return sp_str_lit("");
  }

  sp_ps_output_t result = sp_ps_run(mem, (sp_ps_config_t) {
    .command = sp_str_lit("xcrun"),
    .args = {
      sp_str_lit("--show-sdk-path"),
    },
    .io = {
      .in = { .mode = SP_PS_IO_MODE_NULL },
      .err = { .mode = SP_PS_IO_MODE_NULL },
    },
  });
  if (result.status.exit_code) {
    return sp_str_lit("");
  }
  return sp_str_trim(result.out);
}

static spn_target_rule_t copy_rule(sp_mem_t mem, spn_target_rule_t rule) {
  spn_target_rule_t result = { .kind = rule.kind };
  if (rule.kind == SPN_TARGET_RULE_NAMED) {
    result.names = (spn_str_arr_t) {
      .items = sp_alloc_n(mem, sp_str_t, rule.names.count),
      .count = rule.names.count,
    };
    sp_for(it, rule.names.count) {
      result.names.items[it] = sp_str_copy(mem, rule.names.items[it]);
    }
  }
  return result;
}

static spn_session_config_t copy_config(sp_mem_t mem, spn_session_config_t config) {
  return (spn_session_config_t) {
    .selection = {
      .bin = copy_rule(mem, config.selection.bin),
      .lib = copy_rule(mem, config.selection.lib),
      .test = copy_rule(mem, config.selection.test),
      .script = copy_rule(mem, config.selection.script),
      .example = copy_rule(mem, config.selection.example),
    },
    .profile = {
      .name = sp_str_copy(mem, config.profile.name),
      .toolchain = sp_str_copy(mem, config.profile.toolchain),
      .mode = config.profile.mode,
      .opt = config.profile.opt,
      .sanitizers = config.profile.sanitizers,
      .sanitizers_set = config.profile.sanitizers_set,
      .triple = config.profile.triple,
    },
    .force = config.force,
  };
}

spn_err_t spn_session_init(spn_session_t* s, spn_ctx_t* ctx, sp_mem_t mem, spn_project_t* project, spn_session_config_t config) {
  spn_pkg_info_t* root = &project->package;
  s->ctx = ctx;
  s->project = project;
  s->mem = mem;
  s->pkg = root;
  config = copy_config(mem, config);
  s->config = config;
  s->paths.root = spn_path_from_root(SPN_PATH_ROOT_PROJECT);
  s->paths.build = spn_path_join(s->mem, s->paths.root, sp_str_lit("build"));
  spn_triple_t host = ctx->host;

  sp_str_ht_init(s->mem, s->profiles);
  spn_profile_populate(&s->profiles, root);

  sp_ht_init(s->mem, s->registry);
  sp_ht_init(s->mem, s->packages);
  sp_ht_init(s->mem, s->options);
  sp_ht_init(s->mem, s->fingerprints);
  sp_da_init(s->mem, s->plans);
  sp_da_init(s->mem, s->units.toolchains);
  sp_om_new(s->units.builds);
  sp_om_new(s->units.packages);
  sp_om_new(s->units.targets);
  sp_om_new(s->units.objects);

  spn_try(spn_profile_resolve(s->profiles, &config.profile, host, root, &s->profile));

  spn_toolchain_selection_t target = sp_zero;
  spn_try(spn_toolchain_select(&ctx->catalog, spn_profile_query(&s->profile, host), &target));
  spn_profile_finalize(&s->profile, target.triple.abi);

  switch (s->profile.os) {
    case SPN_OS_MACOS: {
      s->profile.sysroot = spn_path_canonicalize(s->mem, &ctx->roots, spn_path_join(s->mem, spn_path_from_root(SPN_PATH_ROOT_NONE), resolve_macos_sdk(s->mem, ctx->env)));
      break;
    }
    default: {
      break;
    }
  }

  spn_profile_info_t metaprogram = spn_build_metaprogram_profile();
  spn_toolchain_selection_t script = sp_zero;
  spn_try(spn_toolchain_select(&ctx->catalog, spn_profile_query(&metaprogram, host), &script));

  spn_path_t target_root = spn_path_join(s->mem, s->paths.build, spn_profile_build_dir(s->mem, &s->profile));
  s->units.target = spn_build_add(s, s->profile, target_root, target.toolchain);

  spn_triple_t metaprogram_triple = { metaprogram.arch, metaprogram.os, metaprogram.abi };
  spn_path_t metaprogram_root = spn_path_join(s->mem, s->paths.build, spn_triple_to_str(s->mem, metaprogram_triple));
  s->units.metaprogram = spn_build_add(s, metaprogram, metaprogram_root, script.toolchain);
  sp_da_push(s->units.metaprogram->include, spn_path_join(s->mem, spn_path_from_root(SPN_PATH_ROOT_RUNTIME), sp_str_lit("include")));

  spn_path_t log_path = spn_path_join(s->mem, s->units.target->paths.root, sp_str_lit(".spn/build.jsonl"));
  sp_str_t log = spn_path_str(&ctx->roots, s->mem, log_path);
  if (spn_event_log_open(ctx->events, log)) {
    return spn_err_emit(ctx, (spn_err_union_t) {
      .kind = SPN_ERR_FS_WRITE,
      .fs = { .path = log },
    });
  }

  spn_build_plan_t plan = {
    .build = s->units.target,
    .selection = config.selection,
  };
  sp_da_init(s->mem, plan.roots);
  sp_da_push(s->plans, plan);

  return SPN_OK;
}

sp_opt_spn_linkage_t spn_session_config_kind(spn_session_t* session, sp_str_t pkg_name) {
  sp_opt_spn_linkage_t requested = sp_zero;

  spn_pkg_config_t* config = spn_pkg_config_find(session->pkg->config, pkg_name);
  if (config && !sp_opt_is_null(config->kind)) {
    sp_opt_set(requested, config->kind.value);
  }

  return requested;
}

void spn_session_export_toolchain_env(spn_session_t* s) {
  sp_env_init(s->mem, &s->env);
  spn_toolchain_unit_t* toolchain = s->units.target->toolchain;
  sp_env_insert(&s->env, sp_str_lit("CC"), spn_toolchain_launcher_to_str(&spn.roots, s->mem, toolchain->cc.compiler));
  sp_env_insert(&s->env, sp_str_lit("AR"), spn_toolchain_launcher_to_str(&spn.roots, s->mem, toolchain->cc.archiver));
  sp_env_insert(&s->env, sp_str_lit("LD"), spn_toolchain_launcher_to_str(&spn.roots, s->mem, toolchain->cc.linker));
  if (!spn_arg_empty(toolchain->cc.cxx.program)) {
    sp_env_insert(&s->env, sp_str_lit("CXX"), spn_toolchain_launcher_to_str(&spn.roots, s->mem, toolchain->cc.cxx));
  }
}

spn_err_t spn_session_validate_flags(spn_session_t* s) {
  sp_om_for(s->units.builds, it) {
    spn_build_unit_t* build = sp_om_at(s->units.builds, it);
    sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
    spn_cc_flags_t flags = sp_zero;
    spn_err_t err = spn_cc_render_flags(scratch.mem, &build->toolchain->cc, &build->profile, &flags);
    sp_mem_end_scratch(scratch);
    spn_try(err);
  }
  return SPN_OK;
}

spn_pkg_id_t spn_session_root_pkg(spn_session_t* session) {
  sp_ht_for_kv(session->resolve, it) {
    if (it.val->source == SPN_PKG_SOURCE_ROOT) {
      return it.val->id;
    }
  }
  return SP_ZERO_STRUCT(spn_pkg_id_t);
}

spn_pkg_unit_t* spn_session_find_pkg_unit_by_id(spn_session_t* session, spn_pkg_unit_id_t id) {
  return sp_om_has(session->units.packages, id) ? sp_om_get(session->units.packages, id) : SP_NULLPTR;
}

spn_pkg_unit_t* spn_session_find_pkg_unit(spn_session_t* session, spn_build_unit_t* build, spn_pkg_id_t pkg) {
  return spn_session_find_pkg_unit_by_id(session, (spn_pkg_unit_id_t) {
    .pkg = pkg,
    .build = build->id,
  });
}

spn_pkg_unit_t* spn_session_find_dep(spn_session_t* session, spn_pkg_unit_t* pkg, sp_str_t qualified, spn_dep_kind_t kind) {
  sp_intern_id_t name = sp_intern_get_or_insert(session->ctx->intern, qualified);

  sp_da_for(pkg->deps, it) {
    if (pkg->deps[it].kind != kind) {
      continue;
    }
    if (pkg->deps[it].unit && pkg->deps[it].unit->id.pkg.qualified == name) {
      return pkg->deps[it].unit;
    }
  }
  return SP_NULLPTR;
}

spn_target_unit_t* spn_session_find_target_in_pkg(spn_session_t* session, spn_pkg_unit_t* pkg, sp_str_t name, spn_target_kind_t kind) {
  spn_target_unit_id_t id = {
    .pkg = pkg->id,
    .target = sp_intern_get_or_insert(session->ctx->intern, name),
    .kind = kind,
  };
  return sp_om_has(session->units.targets, id) ? sp_om_get(session->units.targets, id) : SP_NULLPTR;
}

spn_target_unit_t* spn_session_get_target_unit(spn_session_t* session, spn_target_unit_id_t id) {
  sp_assert(sp_om_has(session->units.targets, id));
  return sp_om_get(session->units.targets, id);
}

