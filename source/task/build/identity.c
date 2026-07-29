#include "task/build/identity.h"

#include "dag/dag.h"
#include "session/types.h"
#include "sha256/sha256.h"

static void identity_hash_pin(spn_sha256_ctx_t* ctx, const spn_build_source_pin_t* pin) {
  spn_dag_hash_u8(ctx, (u8)pin->kind);
  spn_dag_hash_str(ctx, pin->rev);
  spn_dag_hash_str(ctx, pin->dir);
  spn_dag_hash_u64(ctx, pin->patches);
}

spn_build_source_pin_t spn_build_source_pin(spn_pkg_unit_t* unit) {
  spn_build_source_pin_t pin = sp_zero;
  if (!unit->session) {
    return pin;
  }
  spn_resolved_pkg_t* resolved = sp_ht_getp(unit->session->resolve, unit->id.pkg);
  if (!resolved || resolved->origin.source.kind != SPN_PKG_TREE_GIT) {
    return pin;
  }
  pin.kind = resolved->origin.source.kind;
  pin.rev = resolved->origin.source.git.rev;
  pin.dir = resolved->origin.source.git.dir;
  pin.patches = resolved->origin.source.git.patches.hash;
  return pin;
}

bool spn_build_copy_to_include(spn_publish_copy_t* copy, sp_str_t* rest) {
  sp_str_pair_t to = sp_str_cleave_c8(copy->to, '/');
  if (!sp_str_equal(to.first, sp_str_lit("include"))) {
    return false;
  }
  if (rest) {
    *rest = to.second;
  }
  return true;
}

bool spn_build_path_within(sp_str_t path, sp_str_t dir) {
  if (path.len <= dir.len + 1 || !sp_str_starts_with(path, dir)) {
    return false;
  }
  return path.data[dir.len] == '/';
}

spn_dag_digest_t spn_build_tree_identity(const spn_dag_roots_t* roots, spn_pkg_unit_t* unit, const spn_build_source_pin_t* pin) {
  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  spn_dag_hash_str(&ctx, sp_str_lit("spn.build.tree.v3"));
  spn_dag_hash_str(&ctx, unit->info->qualified);
  identity_hash_pin(&ctx, pin);

  spn_target_map_t maps [] = { unit->info->libs, unit->info->exes, unit->info->scripts, unit->info->tests };
  u32 num_maps = unit->source == SPN_PKG_SOURCE_ROOT ? 4 : 1;
  sp_for(mt, num_maps) {
    sp_om_for(maps[mt], it) {
      spn_target_info_t* target = sp_str_om_at(maps[mt], it);
      spn_dag_hash_strs(&ctx, target->headers);
    }
  }

  sp_da_for(unit->info->publish.copy, it) {
    spn_publish_copy_t* copy = &unit->info->publish.copy[it];
    if (!spn_build_copy_to_include(copy, SP_NULLPTR)) {
      continue;
    }
    spn_dag_hash_str(&ctx, copy->from);
    spn_dag_hash_str(&ctx, copy->to);
  }

  sp_da_for(unit->user_nodes, it) {
    spn_user_node_t* node = &unit->user_nodes[it];
    sp_da_for(node->outputs, ot) {
      if (spn_build_path_within(node->outputs[ot], unit->paths.include)) {
        spn_dag_hash_masked(&ctx, roots, node->outputs[ot]);
      }
    }
  }

  return spn_dag_hash_final(&ctx);
}

spn_dag_digest_t spn_build_package_identity(spn_pkg_unit_t* unit, const spn_build_source_pin_t* pin) {
  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  spn_dag_hash_str(&ctx, sp_str_lit("spn.build.package.v2"));
  spn_dag_hash_str(&ctx, unit->info->qualified);
  identity_hash_pin(&ctx, pin);
  sp_da_for(unit->info->publish.copy, it) {
    spn_publish_copy_t* copy = &unit->info->publish.copy[it];
    spn_dag_hash_str(&ctx, copy->from);
    spn_dag_hash_str(&ctx, copy->to);
  }
  return spn_dag_hash_final(&ctx);
}

spn_dag_digest_t spn_build_user_identity(const spn_dag_roots_t* roots, spn_user_node_t* node, const spn_build_source_pin_t* pin) {
  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  spn_dag_hash_str(&ctx, sp_str_lit("spn.build.user.v3"));
  spn_dag_hash_str(&ctx, node->pkg->info->qualified);
  identity_hash_pin(&ctx, pin);
  spn_dag_hash_str(&ctx, node->tag);
  spn_dag_hash_str(&ctx, node->fn);
  spn_dag_hash_masked_strs(&ctx, roots, node->inputs);
  spn_dag_hash_masked_strs(&ctx, roots, node->outputs);
  return spn_dag_hash_final(&ctx);
}
