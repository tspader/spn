#include "graph/identity.h"

#include "compiler/driver.h"
#include "dag/dag.h"
#include "graph/build.h"
#include "paths/paths.h"
#include "session/types.h"
#include "sha256/sha256.h"
#include "unit/package.h"
#include "unit/unit.h"

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
  if (!resolved || resolved->origin.source.kind != SPN_PKG_ROOT_GIT) {
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

spn_dag_digest_t spn_build_tree_identity(spn_pkg_unit_t* unit, const spn_build_source_pin_t* pin) {
  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  spn_dag_hash_str(&ctx, sp_str_lit("spn.build.tree.v6"));
  spn_dag_hash_str(&ctx, unit->info->qualified);
  identity_hash_pin(&ctx, pin);

  spn_pkg_unit_header_maps_t published = spn_pkg_unit_header_maps(unit);
  sp_for(mt, published.count) {
    sp_om_for(published.maps[mt], it) {
      spn_dag_hash_paths(&ctx, sp_str_om_at(published.maps[mt], it)->headers);
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
      if (spn_path_within(unit->paths.include, node->outputs[ot]).within) {
        spn_dag_hash_path(&ctx, node->outputs[ot]);
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

spn_dag_digest_t spn_build_user_identity(spn_user_node_t* node, const spn_build_source_pin_t* pin) {
  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  spn_dag_hash_str(&ctx, sp_str_lit("spn.build.user.v5"));
  spn_dag_hash_str(&ctx, node->pkg->info->qualified);
  identity_hash_pin(&ctx, pin);
  spn_dag_hash_str(&ctx, node->tag);
  spn_dag_hash_str(&ctx, node->fn);
  spn_dag_hash_paths(&ctx, node->inputs);
  spn_dag_hash_paths(&ctx, node->outputs);
  return spn_dag_hash_final(&ctx);
}

static void identity_hash_invocation(spn_sha256_ctx_t* ctx, const spn_toolchain_unit_t* toolchain, const spn_invocation_t* invocation) {
  spn_dag_hash_u64(ctx, toolchain->identity);
  spn_dag_hash_arg(ctx, invocation->program);
  spn_dag_hash_path(ctx, invocation->cwd);
  spn_dag_hash_args(ctx, invocation->args);
}

spn_dag_digest_t spn_build_compile_identity(const spn_compile_unit_t* unit) {
  sp_assert(!spn_arg_empty(unit->invocation.program));
  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  spn_dag_hash_str(&ctx, sp_str_lit("spn.build.compile.v5"));
  identity_hash_invocation(&ctx, unit->target->pkg->build->toolchain, &unit->invocation);
  spn_dag_hash_path(&ctx, unit->paths.file);
  return spn_dag_hash_final(&ctx);
}

spn_err_t spn_build_link_identity(sp_mem_t mem, spn_target_unit_t* target, spn_path_t output, sp_da(spn_path_t) objects, spn_path_t exports, spn_dag_digest_t* identity) {
  spn_cc_link_files_t files = {
    .output = output,
    .objects = objects,
    .exports.path = exports,
  };

  spn_invocation_t invocation = sp_zero;
  spn_try(spn_target_link_invocation(mem, target, &files, &invocation));

  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  spn_dag_hash_str(&ctx, sp_str_lit("spn.build.link.v5"));
  identity_hash_invocation(&ctx, target->pkg->build->toolchain, &invocation);
  *identity = spn_dag_hash_final(&ctx);
  return SPN_OK;
}

spn_err_t spn_build_exports_identity(sp_mem_t mem, spn_target_unit_t* target, spn_path_t output, sp_da(spn_path_t) objects, spn_dag_digest_t* identity) {
  spn_build_unit_t* build = target->pkg->build;
  spn_cc_archive_files_t files = {
    .output = spn_target_exports_archive(mem, output),
    .objects = objects,
  };

  spn_invocation_t invocation = sp_zero;
  spn_try(spn_cc_render_archive(mem, &build->toolchain->cc, &build->profile, &files, &invocation));
  invocation.cwd = target->pkg->paths.work;

  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  spn_dag_hash_str(&ctx, sp_str_lit("spn.build.exports.v3"));
  identity_hash_invocation(&ctx, build->toolchain, &invocation);
  spn_dag_hash_u8(&ctx, (u8)target->kind);
  spn_dag_hash_u8(&ctx, (u8)build->profile.os);
  spn_dag_hash_paths(&ctx, target->link.archives);
  *identity = spn_dag_hash_final(&ctx);
  return SPN_OK;
}
