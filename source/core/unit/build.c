#include "unit/unit.h"

#include "ctx/types.h"
#include "hash/digest/digest.h"
#include "paths/paths.h"
#include "profile/profile.h"
#include "str/str.h"
#include "spn/core.h"
#include "toolchain/toolchain.h"
#include "triple/triple.h"
#include "when/when.h"

spn_build_id_t spn_build_id(const spn_profile_info_t* profile) {
  sp_hash_t parts [] = {
    spn_digest_hash_str(profile->name),
    (sp_hash_t)profile->toolchain.kind,
    spn_digest_hash_str(profile->toolchain.name),
    (sp_hash_t)profile->sysroot.root,
    spn_digest_hash_str(profile->sysroot.sub),
    (sp_hash_t)profile->os,
    (sp_hash_t)profile->arch,
    (sp_hash_t)profile->abi,
    (sp_hash_t)profile->driver,
    (sp_hash_t)profile->linkage,
    (sp_hash_t)profile->standard,
    (sp_hash_t)profile->mode,
    (sp_hash_t)profile->opt,
    (sp_hash_t)profile->targeted,
    spn_digest_hash(&profile->sanitizers, sizeof(profile->sanitizers)),
  };
  return spn_digest_hash_combine(parts, sp_carr_len(parts));
}

static spn_toolchain_unit_t* bind_toolchain(spn_session_t* s, spn_toolchain_info_t* toolchain) {
  sp_da_for(s->units.toolchains, it) {
    spn_toolchain_unit_t* unit = s->units.toolchains[it];
    if (unit->info == toolchain) {
      return unit;
    }
  }

  spn_toolchain_unit_t* unit = sp_alloc_type(s->mem, spn_toolchain_unit_t);
  *unit = (spn_toolchain_unit_t) { .info = toolchain };
  sp_da_push(s->units.toolchains, unit);
  return unit;
}

spn_build_unit_t* spn_build_add(spn_session_t* s, spn_profile_info_t profile, spn_path_t root, spn_toolchain_info_t* toolchain) {
  spn_build_id_t id = spn_build_id(&profile);
  sp_assert(!sp_om_has(s->units.builds, id));

  sp_om_insert(s->units.builds, id, sp_zero_struct(spn_build_unit_t));
  spn_build_unit_t* build = sp_om_back(s->units.builds);
  build->id = id;
  build->profile = profile;
  build->toolchain = bind_toolchain(s, toolchain);
  build->paths.root = root;
  sp_da_init(s->mem, build->include);
  build->define = spn_when_facts_to_defines(s->mem, spn_profile_facts(&build->profile));
  sp_da_init(s->mem, build->packages);
  return build;
}
