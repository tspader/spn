#include "session/session.h"

#include "sp.h"
#include "ctx/types.h"
#include "error/types.h"
#include "filter/types.h"
#include "forward/types.h"
#include "resolve/types.h"
#include "session/types.h"
#include "spn.h"
#include "unit/types.h"
#include "unit/unit.h"

#include "compiler/driver.h"
#include "intern/intern.h"
#include "pkg/pkg.h"
#include "pkg/options.h"
#include "profile/profile.h"
#include "spn.embed.h"
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

static bool is_shared_linkage(spn_pkg_info_t* pkg) {
  sp_da_for(pkg->config, it) {
    spn_pkg_config_t* config = &pkg->config[it].value;
    if (!sp_opt_is_null(config->kind) && config->kind.value == SPN_LIB_KIND_SHARED) {
      return true;
    }
  }
  sp_str_om_for(pkg->libs, it) {
    spn_linkage_set_t linkages = sp_str_om_at(pkg->libs, it)->linkages;
    if (linkages.shared && !linkages.static_lib && !linkages.source) {
      return true;
    }
  }
  return false;
}

spn_err_union_t spn_session_init(spn_session_t* s, spn_ctx_t* ctx, sp_mem_t mem, spn_pkg_info_t* root, spn_app_config_t config) {
  s->ctx = ctx;
  s->mem = mem;
  s->pkg = root;
  s->paths.root = ctx->paths.project;
  s->paths.build = sp_fs_join_path(s->mem, s->paths.root, sp_str_lit("build"));
  spn_triple_t host = spn_triple_host();

  sp_str_t builtins = sp_str((const c8*)toolchains_json, toolchains_json_size);
  try_as_union(spn_toolchain_catalog_init(&s->catalog, builtins, s->mem));
  sp_str_om_for(root->toolchains, it) {
    spn_toolchain_catalog_add(&s->catalog, *sp_str_om_at(root->toolchains, it));
  }

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

  try_union(spn_profile_resolve(s->profiles, &config.overrides, host, is_shared_linkage(root), &s->profile));

  switch (s->profile.os) {
    case SPN_OS_MACOS: {
      s->profile.sysroot = resolve_macos_sdk(s->mem, ctx->env);
      break;
    }
    default: {
      break;
    }
  }

  try_union(spn_build_add(s, spn_build_config_target(host, s->profile), &s->units.target));
  try_union(spn_build_add(s, spn_build_config_metaprogram(host), &s->units.metaprogram));
  sp_da_push(s->units.metaprogram->include, ctx->paths.include);

  spn_build_plan_t plan = {
    .build = s->units.target,
    .selection = config.selection,
  };
  sp_da_init(s->mem, plan.roots);
  sp_da_push(s->plans, plan);

  return spn_result(SPN_OK);
}

// The root manifest can pin the lib kind of any package in the build with [config.<pkg>] kind
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
  sp_env_insert(&s->env, sp_str_lit("CC"), spn_toolchain_launcher_to_str(s->mem, toolchain->cc.compiler));
  sp_env_insert(&s->env, sp_str_lit("AR"), spn_toolchain_launcher_to_str(s->mem, toolchain->cc.archiver));
  sp_env_insert(&s->env, sp_str_lit("LD"), spn_toolchain_launcher_to_str(s->mem, toolchain->cc.linker));
  if (spn_toolchain_has_cxx(toolchain->info)) {
    sp_env_insert(&s->env, sp_str_lit("CXX"), spn_toolchain_launcher_to_str(s->mem, toolchain->cc.cxx));
  }
}

spn_err_union_t spn_session_validate_flags(spn_session_t* s) {
  sp_om_for(s->units.builds, it) {
    spn_build_unit_t* build = sp_om_at(s->units.builds, it);
    sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
    spn_cc_flags_t flags = sp_zero;
    spn_err_union_t err = spn_cc_render_flags(scratch.mem, &build->toolchain->cc, &build->profile, &flags);
    sp_mem_end_scratch(scratch);
    if (err.kind) {
      return err;
    }
  }
  return spn_result(SPN_OK);
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

spn_target_unit_t* spn_session_find_target_in_pkg(spn_session_t* session, spn_pkg_unit_t* pkg, sp_str_t name) {
  spn_target_unit_id_t id = {
    .pkg = pkg->id,
    .target = sp_intern_get_or_insert(session->ctx->intern, name),
  };
  return sp_om_has(session->units.targets, id) ? sp_om_get(session->units.targets, id) : SP_NULLPTR;
}

spn_target_unit_t* spn_session_get_target_unit(spn_session_t* session, spn_target_unit_id_t id) {
  sp_assert(sp_om_has(session->units.targets, id));
  return sp_om_get(session->units.targets, id);
}
