#include "unit/unit.h"

#include "ctx/types.h"
#include "hash/digest/digest.h"
#include "paths/paths.h"
#include "profile/profile.h"
#include "sp/str.h"
#include "spn/core.h"
#include "toolchain/select.h"
#include "toolchain/toolchain.h"
#include "triple/triple.h"
#include "when/when.h"

spn_build_config_t spn_build_config_target(spn_triple_t host, spn_profile_info_t profile) {
  return (spn_build_config_t) {
    .host = host,
    .profile = profile,
    .role = SPN_TOOLCHAIN_ROLE_BUILD,
  };
}

spn_build_config_t spn_build_config_metaprogram(spn_triple_t host) {
  return (spn_build_config_t) {
    .host = host,
    .role = SPN_TOOLCHAIN_ROLE_SCRIPT,
    .profile = {
      .name = sp_str_lit("metaprogram"),
      .toolchain = sp_str_lit("auto"),
      .arch = SPN_ARCH_WASM32,
      .os = SPN_OS_WASI,
      .abi = SPN_ABI_MUSL,
      .mode = SPN_MODE_DEBUG,
      .opt = SPN_OPT_LEVEL_2,
      .standard = SPN_C99,
      .linkage = SPN_LIB_KIND_STATIC,
    },
  };
}

spn_build_id_t spn_build_id(const spn_build_config_t* config) {
  const spn_profile_info_t* profile = &config->profile;
  sp_hash_t parts [] = {
    spn_digest_hash_str(profile->name),
    spn_digest_hash_str(profile->toolchain),
    (sp_hash_t)profile->sysroot.root,
    spn_digest_hash_str(profile->sysroot.sub),
    (sp_hash_t)profile->os,
    (sp_hash_t)profile->arch,
    (sp_hash_t)profile->abi,
    (sp_hash_t)profile->linkage,
    (sp_hash_t)profile->standard,
    (sp_hash_t)profile->mode,
    (sp_hash_t)profile->opt,
    (sp_hash_t)profile->targeted,
    spn_digest_hash(&profile->sanitizers, sizeof(profile->sanitizers)),
    (sp_hash_t)config->host.arch,
    (sp_hash_t)config->host.os,
    (sp_hash_t)config->host.abi,
    (sp_hash_t)config->role,
  };
  return spn_digest_hash_combine(parts, sp_carr_len(parts));
}

static bool same_triple(spn_triple_t lhs, spn_triple_t rhs) {
  return lhs.arch == rhs.arch && lhs.os == rhs.os && lhs.abi == rhs.abi;
}

static spn_err_t bind_toolchain(spn_session_t* s, spn_toolchain_query_t query, spn_toolchain_unit_t** out) {
  spn_toolchain_resolution_t resolution = sp_zero;
  spn_try(spn_toolchain_select(&s->ctx->catalog, query, s->mem, &resolution));

  sp_da_for(s->units.toolchains, it) {
    spn_toolchain_unit_t* unit = s->units.toolchains[it];
    if (unit->info == resolution.info && same_triple(unit->host, query.host)) {
      *out = unit;
      return SPN_OK;
    }
  }

  spn_toolchain_unit_t* unit = sp_alloc_type(s->mem, spn_toolchain_unit_t);
  *unit = (spn_toolchain_unit_t) {
    .info = resolution.info,
    .host = query.host,
    .artifact = resolution.artifact,
  };
  sp_da_push(s->units.toolchains, unit);
  *out = unit;
  return SPN_OK;
}

static spn_path_t build_root(spn_session_t* s, const spn_build_config_t* config) {
  switch (config->role) {
    case SPN_TOOLCHAIN_ROLE_BUILD: {
      return spn_path_join(s->mem, s->paths.build, spn_profile_build_dir(s->mem, &config->profile));
    }
    case SPN_TOOLCHAIN_ROLE_SCRIPT: {
      spn_triple_t triple = { config->profile.arch, config->profile.os, config->profile.abi };
      return spn_path_join(s->mem, s->paths.build, spn_triple_to_str(s->mem, triple));
    }
  }
  sp_unreachable_return(sp_zero_struct(spn_path_t));
}

spn_err_t spn_build_add(spn_session_t* s, spn_build_config_t config, spn_build_unit_t** out) {
  spn_build_id_t id = spn_build_id(&config);
  if (sp_om_has(s->units.builds, id)) {
    *out = sp_om_get(s->units.builds, id);
    return SPN_OK;
  }

  spn_toolchain_unit_t* toolchain = SP_NULLPTR;
  spn_try(bind_toolchain(s, (spn_toolchain_query_t) {
    .name = config.profile.toolchain,
    .target = { config.profile.arch, config.profile.os, config.profile.abi },
    .host = config.host,
    .role = config.role,
  }, &toolchain));

  sp_om_insert(s->units.builds, id, sp_zero_struct(spn_build_unit_t));
  spn_build_unit_t* build = sp_om_back(s->units.builds);
  build->id = id;
  build->profile = config.profile;
  build->toolchain = toolchain;
  build->paths.root = build_root(s, &config);
  sp_da_init(s->mem, build->include);
  build->define = spn_when_facts_to_defines(s->mem, spn_profile_facts(&build->profile));
  sp_da_init(s->mem, build->packages);

  *out = build;
  return SPN_OK;
}
