#include "task/build/identity.h"

#include "dag/dag.h"
#include "sha256/sha256.h"

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

spn_dag_digest_t spn_build_tree_identity(const spn_dag_roots_t* roots, spn_pkg_unit_t* unit) {
  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  spn_dag_hash_str(&ctx, sp_str_lit("spn.build.tree.v2"));
  spn_dag_hash_str(&ctx, unit->info->qualified);

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

spn_dag_digest_t spn_build_package_identity(spn_pkg_unit_t* unit) {
  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  spn_dag_hash_str(&ctx, sp_str_lit("spn.build.package.v1"));
  spn_dag_hash_str(&ctx, unit->info->qualified);
  sp_da_for(unit->info->publish.copy, it) {
    spn_publish_copy_t* copy = &unit->info->publish.copy[it];
    spn_dag_hash_str(&ctx, copy->from);
    spn_dag_hash_str(&ctx, copy->to);
  }
  return spn_dag_hash_final(&ctx);
}

spn_dag_digest_t spn_build_user_identity(const spn_dag_roots_t* roots, spn_user_node_t* node) {
  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  spn_dag_hash_str(&ctx, sp_str_lit("spn.build.user.v2"));
  spn_dag_hash_str(&ctx, node->pkg->info->qualified);
  spn_dag_hash_str(&ctx, node->tag);
  spn_dag_hash_str(&ctx, node->fn);
  spn_dag_hash_masked_strs(&ctx, roots, node->inputs);
  spn_dag_hash_masked_strs(&ctx, roots, node->outputs);
  return spn_dag_hash_final(&ctx);
}
