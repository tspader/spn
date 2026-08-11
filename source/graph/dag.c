#include "sp.h"
#include "sp/macro.h"
#include "project/project.h"
#include "ctx/types.h"
#include "error/error.h"
#include "error/types.h"
#include "event/types.h"
#include "core/types.h"
#include "unit/types.h"

#include "api/api.h"
#include "compiler/driver.h"
#include "core/core.h"
#include "dag/occ.h"
#include "enum/enum.h"
#include "event/event.h"
#include "external/wasm/wasm.h"
#include "op/op.h"
#include "session/session.h"
#include "thread_pool/thread_pool.h"
#include "unit/unit.h"
#include "graph/build.h"
#include "graph/dag.h"
#include "graph/identity.h"
#include "graph/nodes/nodes.h"
#include "triple/triple.h"
#include "unit/package.h"

typedef struct {
  spn_target_unit_t* target;
  sp_da(spn_dag_id_t) objects;
  spn_dag_id_t exports;
} spn_dag_link_ctx_t;

typedef struct {
  spn_pkg_unit_t* pkg;
  sp_da(spn_dag_obs_t) obs;
} spn_dag_tree_ctx_t;

typedef struct {
  spn_target_unit_t* target;
  spn_dag_id_t object;
  spn_dag_id_t header;
  sp_da(spn_dag_obs_t) obs;
} spn_dag_embed_ctx_t;

typedef struct {
  spn_user_node_t* node;
  bool stamp;
  sp_da(spn_dag_obs_t) obs;
} spn_dag_user_ctx_t;

typedef struct {
  spn_pkg_unit_t* pkg;
  sp_da(spn_dag_obs_t) obs;
} spn_dag_package_ctx_t;

static sp_str_t dag_artifact_path(spn_dag_t* g, spn_dag_id_t id) {
  return spn_dag_find_artifact(g, id)->path;
}

////////////////
// IDENTITIES //
////////////////
static void hash_compiler_invocation(spn_sha256_ctx_t* ctx, const spn_dag_roots_t* roots, const spn_toolchain_unit_t* toolchain, const spn_invocation_t* invocation) {
  spn_dag_hash_u64(ctx, toolchain->identity);
  spn_dag_hash_masked(ctx, roots, invocation->program);
  spn_dag_hash_masked(ctx, roots, invocation->cwd);
  spn_dag_hash_masked_strs(ctx, roots, invocation->args);
}

static spn_dag_digest_t hash_compile_unit(spn_dag_build_t* b, const spn_compile_unit_t* unit) {
  sp_assert(!sp_str_empty(unit->invocation.program));
  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  spn_dag_hash_cstr(&ctx, "spn.build.compile.v3");
  hash_compiler_invocation(&ctx, &b->roots, unit->target->pkg->build->toolchain, &unit->invocation);
  spn_dag_hash_masked(&ctx, &b->roots, unit->paths.file);
  return spn_dag_hash_final(&ctx);
}

static spn_err_union_t dag_link_identity(spn_dag_build_t* b, spn_dag_link_ctx_t* link, sp_str_t output, spn_dag_digest_t* identity) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  spn_cc_link_files_t files = {
    .output = output,
  };
  sp_da_init(s.mem, files.objects);
  sp_da_for(link->objects, it) {
    sp_da_push(files.objects, dag_artifact_path(b->graph, link->objects[it]));
  }
  if (link->exports.occupied) {
    files.exports.path = dag_artifact_path(b->graph, link->exports);
  }

  spn_invocation_t invocation = sp_zero;
  spn_err_union_t err = spn_build_link_invocation(s.mem, link->target, &files, &invocation);
  if (!err.kind) {
    spn_sha256_ctx_t ctx = sp_zero;
    spn_sha256_init(&ctx);
    spn_dag_hash_str(&ctx, sp_str_lit("spn.build.link.v3"));
    hash_compiler_invocation(&ctx, &b->roots, link->target->pkg->build->toolchain, &invocation);
    *identity = spn_dag_hash_final(&ctx);
  }
  sp_mem_end_scratch(s);
  return err;
}

static spn_err_union_t dag_exports_identity(spn_dag_build_t* b, spn_dag_link_ctx_t* link, sp_str_t output, spn_dag_digest_t* identity) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_target_unit_t* target = link->target;
  spn_build_unit_t* build = target->pkg->build;

  spn_cc_archive_files_t files = {
    .output = sp_fmt(s.mem, "{}.a", SP_FMT_STR(output)).value,
  };
  sp_da_init(s.mem, files.objects);
  sp_da_for(link->objects, it) {
    sp_da_push(files.objects, dag_artifact_path(b->graph, link->objects[it]));
  }

  spn_invocation_t invocation = sp_zero;
  spn_err_union_t err = spn_cc_render_archive(s.mem, &build->toolchain->cc, &build->profile, &files, &invocation);
  if (!err.kind) {
    invocation.cwd = target->pkg->paths.work;
    spn_sha256_ctx_t ctx = sp_zero;
    spn_sha256_init(&ctx);
    spn_dag_hash_str(&ctx, sp_str_lit("spn.build.exports.v1"));
    hash_compiler_invocation(&ctx, &b->roots, build->toolchain, &invocation);
    spn_dag_hash_u8(&ctx, (u8)target->kind);
    spn_dag_hash_u8(&ctx, (u8)build->profile.os);
    spn_dag_hash_masked_strs(&ctx, &b->roots, target->link.cc.whole_archives);
    *identity = spn_dag_hash_final(&ctx);
  }
  sp_mem_end_scratch(s);
  return err;
}

static spn_dag_digest_t hash_embedding(spn_dag_build_t* b, spn_target_unit_t* target) {
  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  spn_dag_hash_str(&ctx, sp_str_lit("spn.build.embed.v3"));
  spn_dag_hash_str(&ctx, target->pkg->info->qualified);
  spn_dag_hash_str(&ctx, target->info->name);
  spn_profile_info_t* profile = &target->pkg->build->profile;
  spn_dag_hash_u8(&ctx, (u8)profile->os);
  spn_dag_hash_u8(&ctx, (u8)profile->arch);
  spn_dag_hash_u8(&ctx, (u8)profile->abi);
  sp_da_for(target->info->embed, it) {
    spn_embed_t* embed = &target->info->embed[it];
    spn_dag_hash_u8(&ctx, (u8)embed->kind);
    spn_dag_hash_str(&ctx, embed->symbol);
    spn_dag_hash_str(&ctx, embed->types.data);
    spn_dag_hash_str(&ctx, embed->types.size);
    switch (embed->kind) {
      case SPN_EMBED_FILE: {
        spn_dag_hash_masked(&ctx, &b->roots, embed->file.path);
        break;
      }
      case SPN_EMBED_DIR: {
        spn_dag_hash_masked(&ctx, &b->roots, embed->dir.path);
        spn_dag_hash_str(&ctx, embed->dir.dest);
        break;
      }
    }
  }
  return spn_dag_hash_final(&ctx);
}

//////////////
// EXECUTES //
//////////////
static sp_str_t dag_dep_path(sp_mem_t mem, sp_str_t object) {
  return sp_fmt(mem, "{}.d", sp_fmt_str(object)).value;
}

static s32 compile_object(spn_dag_t* g, spn_dag_action_t* action, void* user_data) {
  spn_compile_unit_t* unit = (spn_compile_unit_t*)user_data;

  sp_str_t object = dag_artifact_path(g, action->produces[0]);
  sp_str_t depfile = action->discover ? dag_dep_path(spn.mem, object) : sp_str_lit("");
  return spn_compile_object_run(unit, object, depfile);
}

static spn_err_t discover_compilation_deps(spn_dag_t* g, spn_dag_action_t* action, void* user_data, sp_mem_t mem, sp_da(spn_dag_obs_t)* out) {
  spn_compile_unit_t* unit = (spn_compile_unit_t*)user_data;

  sp_str_t object = dag_artifact_path(g, action->produces[0]);
  sp_str_t dep = dag_dep_path(mem, object);
  if (!sp_fs_exists(dep)) {
    return SPN_OK;
  }
  sp_str_t content = sp_zero;
  if (sp_io_read_file(mem, dep, &content)) {
    return SPN_ERROR;
  }

  occ_parser_t parser = sp_zero;
  if (occ_init(&parser, content)) {
    return SPN_ERROR;
  }

  sp_str_t prereq = sp_zero;
  while (occ_next(&parser, &prereq)) {
    sp_str_t path = prereq;
    if (!sp_fs_is_absolute(path)) {
      path = sp_fs_join_path(mem, unit->target->pkg->paths.work, path);
    }
    sp_str_t canonical = sp_fs_canonicalize_path(mem, path);
    sp_da_push(*out, ((spn_dag_obs_t) {
      .kind = SPN_DAG_OBS_FILE,
      .path = sp_str_empty(canonical) ? sp_fs_normalize_path(mem, path) : canonical,
    }));
  }

  return parser.err ? SPN_ERROR : SPN_OK;
}

static s32 dag_link_exec(spn_dag_t* g, spn_dag_action_t* action, void* user_data) {
  spn_dag_link_ctx_t* link = (spn_dag_link_ctx_t*)user_data;
  spn_target_unit_t* target = link->target;

  sp_da(sp_str_t) objects = sp_da_new(spn.mem, sp_str_t);
  sp_da_for(link->objects, it) {
    sp_da_push(objects, dag_artifact_path(g, link->objects[it]));
  }
  sp_str_t exports = link->exports.occupied ? dag_artifact_path(g, link->exports) : sp_str_lit("");
  return spn_link_target_run(target, dag_artifact_path(g, action->produces[0]), objects, exports);
}

static s32 dag_exports_exec(spn_dag_t* g, spn_dag_action_t* action, void* user_data) {
  spn_dag_link_ctx_t* link = (spn_dag_link_ctx_t*)user_data;

  sp_da(sp_str_t) objects = sp_da_new(spn.mem, sp_str_t);
  sp_da_for(link->objects, it) {
    sp_da_push(objects, dag_artifact_path(g, link->objects[it]));
  }
  return spn_link_exports_run(link->target, objects, dag_artifact_path(g, action->produces[0]));
}

static s32 generate_embedding(spn_dag_t* g, spn_dag_action_t* action, void* user_data) {
  spn_dag_embed_ctx_t* ctx = (spn_dag_embed_ctx_t*)user_data;
  spn_target_unit_t* target = ctx->target;

  sp_da_init(spn.mem, ctx->obs);
  sp_str_t object = dag_artifact_path(g, ctx->object);
  sp_str_t header = dag_artifact_path(g, ctx->header);
  return spn_embed_write(target, object, header, spn.mem, &ctx->obs);
}

static spn_err_t dag_embed_discover(spn_dag_t* g, spn_dag_action_t* action, void* user_data, sp_mem_t mem, sp_da(spn_dag_obs_t)* out) {
  spn_dag_embed_ctx_t* ctx = (spn_dag_embed_ctx_t*)user_data;
  sp_da_for(ctx->obs, it) {
    sp_da_push(*out, ctx->obs[it]);
  }
  return SPN_OK;
}

static s32 dag_user_exec(spn_dag_t* g, spn_dag_action_t* action, void* user_data) {
  spn_dag_user_ctx_t* ctx = (spn_dag_user_ctx_t*)user_data;
  spn_user_node_t* node = ctx->node;
  spn_pkg_unit_t* pkg = node->pkg;

  spn_pkg_unit_announce_compile(pkg);

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_BUILD_SCRIPT_USER_FN,
    .pkg = pkg->info,
    .io = &pkg->logs.io,
    .node = { .info = node }
  });

  sp_da_init(spn.mem, ctx->obs);
  if (!sp_str_empty(node->fn)) {
    if (spn_wasm_call_export_ex(pkg, node->fn, SPN_ABI_KIND_NONE, SP_NULLPTR, (spn_wasm_obs_t) { .mem = spn.mem, .out = &ctx->obs })) {
      return 1;
    }
  }

  sp_da_for(action->produces, it) {
    spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, action->produces[it]);
    if (ctx->stamp) {
      sp_fs_create_file(artifact->path);
      continue;
    }
    if (!sp_fs_exists(artifact->target)) {
      spn_event_buffer_push(spn.events, (spn_build_event_t) {
        .kind = SPN_EVENT_NODE_FAILED,
        .pkg = pkg->info,
        .io = &pkg->logs.io,
        .node_failed = {
          .path = artifact->target,
          .message = sp_fmt(spn.mem, "was declared as an output of node {} but was not produced", sp_fmt_str(node->tag)).value,
        },
      });
      return 1;
    }
    if (sp_fs_copy(artifact->target, artifact->path)) {
      spn_event_buffer_push(spn.events, (spn_build_event_t) {
        .kind = SPN_EVENT_NODE_FAILED,
        .pkg = pkg->info,
        .io = &pkg->logs.io,
        .node_failed = {
          .path = artifact->target,
          .message = sp_fmt(spn.mem, "output of node {} could not be copied into the build", sp_fmt_str(node->tag)).value,
        },
      });
      return 1;
    }
  }

  return 0;
}

spn_err_t spn_build_publish_copies(spn_pkg_unit_t* unit, sp_str_t root, bool strict, sp_da(spn_dag_obs_t)* obs) {
  sp_da_for(unit->info->publish.copy, it) {
    spn_publish_copy_t* copy = &unit->info->publish.copy[it];
    sp_str_t rest = sp_zero;
    if (!spn_build_copy_to_include(copy, &rest)) {
      continue;
    }

    sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
    sp_str_pair_t from = sp_str_cleave_c8(copy->from, '/');
    sp_str_t from_root = spn_api_dir(unit, spn_cache_dir_kind_from_str(from.first));
    sp_str_t dest = sp_fs_join_path(scratch.mem, root, rest);

    s32 err = 0;
    sp_da(spn_dag_match_t) matches = sp_da_new(scratch.mem, spn_dag_match_t);
    if (spn_dag_glob(spn.mem, from_root, from.second, obs, &matches)) {
      err = 1;
    }
    else if (sp_fs_is_glob(copy->from)) {
      sp_fs_create_dir(dest);
      sp_da_for(matches, mt) {
        sp_str_t to = sp_fs_join_path(scratch.mem, dest, sp_fs_get_name(matches[mt].path));
        if (sp_fs_copy(matches[mt].path, to)) {
          err = 1;
          break;
        }
      }
    }
    else if (sp_da_empty(matches)) {
      err = strict ? 1 : 0;
    }
    else {
      sp_fs_create_dir(sp_fs_parent_path(dest));
      err = sp_fs_copy(matches[0].path, dest) ? 1 : 0;
    }

    sp_mem_end_scratch(scratch);
    if (err) {
      spn_event_buffer_push(spn.events, (spn_build_event_t) {
        .kind = SPN_EVENT_NODE_FAILED,
        .pkg = unit->info,
        .io = &unit->logs.io,
        .node_failed = {
          .path = copy->from,
          .message = sp_fmt(spn.mem, "could not be published to {}", sp_fmt_str(copy->to)).value,
        },
      });
      return SPN_ERROR;
    }
  }
  return SPN_OK;
}

static s32 dag_tree_copy_user_outputs(spn_dag_tree_ctx_t* ctx, sp_str_t root) {
  spn_pkg_unit_t* unit = ctx->pkg;
  sp_da_for(unit->user_nodes, it) {
    spn_user_node_t* node = &unit->user_nodes[it];
    sp_da_for(node->outputs, ot) {
      sp_str_t path = node->outputs[ot];
      if (!spn_build_path_within(path, unit->paths.include)) {
        continue;
      }
      sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
      sp_str_t relative = sp_str_strip_left(sp_str_strip_left(path, unit->paths.include), sp_str_lit("/"));
      sp_str_t to = sp_fs_join_path(scratch.mem, root, relative);
      sp_fs_create_dir(sp_fs_parent_path(to));
      s32 err = sp_fs_copy(path, to);
      sp_mem_end_scratch(scratch);
      if (err) {
        spn_event_buffer_push(spn.events, (spn_build_event_t) {
          .kind = SPN_EVENT_NODE_FAILED,
          .pkg = unit->info,
          .io = &unit->logs.io,
          .node_failed = {
            .path = path,
            .message = sp_fmt(spn.mem, "output of node {} could not be published to the package store", sp_fmt_str(node->tag)).value,
          },
        });
        return 1;
      }
    }
  }
  return 0;
}

static s32 dag_tree_exec(spn_dag_t* g, spn_dag_action_t* action, void* user_data) {
  spn_dag_tree_ctx_t* ctx = (spn_dag_tree_ctx_t*)user_data;
  spn_pkg_unit_t* unit = ctx->pkg;

  sp_da_init(spn.mem, ctx->obs);

  sp_str_t root = dag_artifact_path(g, action->produces[0]);

  if (spn_pkg_unit_publish_headers(unit, root, true)) {
    return 1;
  }

  if (spn_build_publish_copies(unit, root, true, &ctx->obs)) {
    return 1;
  }
  if (dag_tree_copy_user_outputs(ctx, root)) {
    return 1;
  }

  return 0;
}

static spn_err_t dag_tree_discover(spn_dag_t* g, spn_dag_action_t* action, void* user_data, sp_mem_t mem, sp_da(spn_dag_obs_t)* out) {
  spn_dag_tree_ctx_t* ctx = (spn_dag_tree_ctx_t*)user_data;
  sp_da_for(ctx->obs, it) {
    sp_da_push(*out, ctx->obs[it]);
  }
  return SPN_OK;
}

static spn_err_t dag_user_discover(spn_dag_t* g, spn_dag_action_t* action, void* user_data, sp_mem_t mem, sp_da(spn_dag_obs_t)* out) {
  spn_dag_user_ctx_t* ctx = (spn_dag_user_ctx_t*)user_data;
  sp_da_for(ctx->obs, it) {
    sp_da_push(*out, ctx->obs[it]);
  }
  return SPN_OK;
}

static s32 dag_package_exec(spn_dag_t* g, spn_dag_action_t* action, void* user_data) {
  spn_dag_package_ctx_t* ctx = (spn_dag_package_ctx_t*)user_data;
  spn_pkg_unit_t* unit = ctx->pkg;

  spn_pkg_unit_create_layout(unit);
  sp_da_init(spn.mem, ctx->obs);

  sp_da_for(unit->info->publish.copy, it) {
    spn_publish_copy_t* copy = &unit->info->publish.copy[it];
    if (spn_build_copy_to_include(copy, SP_NULLPTR)) {
      continue;
    }
    sp_str_pair_t from = sp_str_cleave_c8(copy->from, '/');
    sp_str_pair_t to = sp_str_cleave_c8(copy->to, '/');
    s32 err = spn_api_copy_rooted(
      unit,
      spn_cache_dir_kind_from_str(from.first), from.second,
      spn_cache_dir_kind_from_str(to.first), to.second
    );
    if (err) {
      spn_event_buffer_push(spn.events, (spn_build_event_t) {
        .kind = SPN_EVENT_NODE_FAILED,
        .pkg = unit->info,
        .io = &unit->logs.io,
        .node_failed = {
          .path = copy->from,
          .message = sp_fmt(spn.mem, "could not be published to {}", sp_fmt_str(copy->to)).value,
        },
      });
      return 1;
    }
  }

  spn_wasm_script_t* script = SP_NULLPTR;
  if (spn_wasm_find_export(unit, sp_str_lit("package"), &script)) {
    return 1;
  }
  if (script) {
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_BUILD_SCRIPT_PACKAGE,
      .pkg = unit->info,
      .io = &unit->logs.io,
    });

    sp_tm_timer_t timer = sp_tm_start_timer();
    if (spn_wasm_script_call_ex(script, unit, sp_str_lit("package"), SPN_ABI_KIND_NONE, SP_NULLPTR, (spn_wasm_obs_t) { .mem = spn.mem, .out = &ctx->obs })) {
      return 1;
    }
    unit->time.package = sp_tm_read_timer(&timer);

    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_BUILD_SCRIPT_PACKAGE_OK,
      .pkg = unit->info,
      .io = &unit->logs.io,
      .package_ok = {
        .time = unit->time.package
      }
    });
  }

  spn_dag_artifact_t* stamp = spn_dag_find_artifact(g, action->produces[0]);
  spn_pkg_unit_write_stamp(unit, stamp->path);

  return 0;
}

static spn_err_t dag_package_discover(spn_dag_t* g, spn_dag_action_t* action, void* user_data, sp_mem_t mem, sp_da(spn_dag_obs_t)* out) {
  spn_dag_package_ctx_t* ctx = (spn_dag_package_ctx_t*)user_data;
  sp_da_for(ctx->obs, it) {
    sp_da_push(*out, ctx->obs[it]);
  }
  return SPN_OK;
}

//////////////////
// CONSTRUCTION //
//////////////////
static spn_dag_id_t dag_configure_reactor(spn_dag_build_t* b, spn_pkg_unit_t* unit) {
  spn_target_unit_t* configure = unit->metaprogram ? unit->metaprogram->scripts.configure : SP_NULLPTR;
  if (!configure) {
    return (spn_dag_id_t) sp_zero;
  }
  return spn_dag_add_file(b->graph, spn_target_output_path(b->mem, configure));
}

static spn_err_t dag_add_user_nodes(spn_dag_build_t* b, spn_pkg_unit_t* unit, spn_dag_pkg_ids_t* pkg) {
  spn_dag_t* g = b->graph;
  spn_dag_id_t configure = dag_configure_reactor(b, unit);
  spn_build_source_pin_t pin = spn_build_source_pin(unit);

  spn_dag_id_t metaprogram = sp_zero;
  if (unit->metaprogram && unit->metaprogram->scripts.build) {
    spn_dag_target_ids_t* ids = sp_ht_getp(b->ids.targets, unit->metaprogram->scripts.build);
    if (ids) {
      metaprogram = ids->output;
    }
  }

  sp_da_for(unit->user_nodes, it) {
    spn_user_node_t* node = &unit->user_nodes[it];

    spn_dag_user_ctx_t* ctx = sp_alloc_type(b->mem, spn_dag_user_ctx_t);
    ctx->node = node;
    ctx->stamp = sp_da_empty(node->outputs);
    if (ctx->stamp) {
      sp_da_push(node->outputs, spn_pkg_unit_get_node_stamp_file(unit, node));
    }

    spn_dag_id_t action = spn_dag_add_action(g, (spn_dag_action_config_t) {
      .identity = spn_build_user_identity(&b->roots, node, &pin),
      .execute = dag_user_exec,
      .discover = dag_user_discover,
      .user_data = ctx,
    });

    if (metaprogram.occupied) {
      spn_dag_action_add_input(g, action, metaprogram);
    }
    if (configure.occupied) {
      spn_dag_action_add_input(g, action, configure);
    }

    sp_da_for(node->outputs, ot) {
      spn_dag_id_t file = spn_dag_add_file(g, node->outputs[ot]);
      spn_try(spn_dag_action_add_output(g, action, file));
      sp_da_push(pkg->user_outputs, file);
    }

    sp_da_push(pkg->user_actions, action);
  }

  sp_da_for(unit->user_nodes, it) {
    spn_user_node_t* node = &unit->user_nodes[it];
    spn_dag_id_t action = pkg->user_actions[it];

    sp_da_for(node->inputs, jt) {
      spn_dag_action_add_input(g, action, spn_dag_add_file(g, node->inputs[jt]));
    }
    sp_da_for(node->deps, jt) {
      spn_user_node_t* dep = spn_node_deref(node->deps[jt]);
      sp_da_for(dep->outputs, ot) {
        spn_dag_action_add_input(g, action, spn_dag_add_file(g, dep->outputs[ot]));
      }
    }
  }

  return SPN_OK;
}

static spn_err_t add_object_compilation(spn_dag_build_t* b, spn_target_unit_t* target) {
  spn_dag_t* g = b->graph;
  spn_cc_toolchain_t* toolchain = &target->pkg->build->toolchain->cc;
  bool discovery = toolchain->driver != SPN_CC_DRIVER_MSVC;

  sp_da_for(target->objects, it) {
    spn_compile_unit_t* unit = target->objects[it];
    bool exists = sp_ht_getp(b->ids.objects, unit);
    sp_assert(!exists);

    spn_dag_object_ids_t ids = sp_zero;
    ids.action = spn_dag_add_action(g, (spn_dag_action_config_t) {
      .identity = hash_compile_unit(b, unit),
      .execute = compile_object,
      .discover = discovery ? discover_compilation_deps : SP_NULLPTR,
      .user_data = unit,
    });
    spn_dag_action_add_input(g, ids.action, spn_dag_add_file(g, unit->paths.file));

    ids.object = spn_dag_add_file(g, unit->paths.object);
    spn_try(spn_dag_action_add_output(g, ids.action, ids.object));

    sp_ht_insert(b->ids.objects, unit, ids);
  }

  return SPN_OK;
}

static sp_str_t embed_object_path(sp_mem_t mem, spn_target_unit_t* unit) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);
  sp_str_t name = sp_fmt(s.mem, "{}.embed.o", sp_fmt_str(unit->info->name)).value;
  sp_str_t path = sp_fs_join_path(mem, unit->pkg->paths.generated, name);
  sp_mem_end_scratch(s);
  return path;
}

static sp_str_t embed_header_path(sp_mem_t mem, spn_target_unit_t* unit) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);
  sp_str_t name = sp_fmt(s.mem, "{}.embed.h", sp_fmt_str(unit->info->name)).value;
  sp_str_t path = sp_fs_join_path(mem, unit->pkg->paths.generated, name);
  sp_mem_end_scratch(s);
  return path;
}

static sp_str_t target_exports_path(sp_mem_t mem, spn_target_unit_t* target) {
  spn_cc_exports_format_t format = spn_cc_exports_format(target->kind, target->pkg->build->profile.os);

  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);
  sp_str_t file_name = sp_fmt(s.mem, "{}.{}", sp_fmt_str(target->info->name), sp_fmt_cstr(spn_cc_exports_extension(format))).value;
  sp_str_t path = sp_fs_join_path(mem, target->pkg->paths.work, file_name);
  sp_mem_end_scratch(s);
  return path;
}

static spn_err_union_t dag_add_exports(spn_dag_build_t* b, spn_dag_link_ctx_t* link) {
  spn_dag_t* g = b->graph;
  spn_target_unit_t* target = link->target;

  sp_str_t output = target_exports_path(b->mem, target);
  spn_dag_digest_t identity = sp_zero;
  spn_try_union(dag_exports_identity(b, link, output, &identity));

  spn_dag_id_t action = spn_dag_add_action(g, (spn_dag_action_config_t) {
    .identity = identity,
    .execute = dag_exports_exec,
    .user_data = link,
  });
  link->exports = spn_dag_add_file(g, output);
  spn_try_union(spn_result(spn_dag_action_add_output(g, action, link->exports)));

  sp_da_for(link->objects, it) {
    spn_dag_action_add_input(g, action, link->objects[it]);
  }
  sp_da_for(target->link.cc.whole_archives, it) {
    spn_dag_action_add_input(g, action, spn_dag_add_file(g, target->link.cc.whole_archives[it]));
  }
  return spn_result(SPN_OK);
}

spn_err_union_t spn_dag_build_add_target(spn_dag_build_t* b, spn_target_unit_t* target) {
  spn_dag_t* g = b->graph;

  switch (target->lib_kind) {
    case SPN_LIB_KIND_SOURCE: {
      return spn_result(SPN_OK);
    }
    case SPN_LIB_KIND_OBJECT: {
      spn_try_union(spn_result(add_object_compilation(b, target)));
      return spn_result(SPN_OK);
    }
    case SPN_LIB_KIND_STATIC:
    case SPN_LIB_KIND_SHARED:
    case SPN_LIB_KIND_NONE: {
      break;
    }
  }

  bool exists = sp_ht_getp(b->ids.targets, target);
  sp_assert(!exists);

  spn_try_union(spn_result(add_object_compilation(b, target)));

  if (sp_da_empty(target->objects)) {
    return spn_result(SPN_OK);
  }

  spn_dag_target_ids_t ids = sp_zero;

  if (!sp_da_empty(target->info->embed)) {
    spn_dag_embed_ctx_t* embed = sp_alloc_type(b->mem, spn_dag_embed_ctx_t);
    embed->target = target;
    sp_da_init(b->mem, embed->obs);

    ids.embed.action = spn_dag_add_action(g, (spn_dag_action_config_t) {
      .identity = hash_embedding(b, target),
      .execute = generate_embedding,
      .discover = dag_embed_discover,
      .user_data = embed,
    });
    ids.embed.object = spn_dag_add_file(g, embed_object_path(b->mem, target));
    ids.embed.header = spn_dag_add_file(g, embed_header_path(b->mem, target));
    embed->object = ids.embed.object;
    embed->header = ids.embed.header;
    spn_try_union(spn_result(spn_dag_action_add_output(g, ids.embed.action, ids.embed.object)));
    spn_try_union(spn_result(spn_dag_action_add_output(g, ids.embed.action, ids.embed.header)));

    sp_da_for(target->info->embed, it) {
      spn_embed_t* entry = &target->info->embed[it];
      if (entry->kind == SPN_EMBED_FILE) {
        spn_dag_action_add_input(g, ids.embed.action, spn_dag_add_file(g, entry->file.path));
      }
    }

    sp_da_for(target->objects, it) {
      spn_dag_object_ids_t* object = sp_ht_getp(b->ids.objects, target->objects[it]);
      sp_assert(object);
      spn_dag_action_add_input(g, object->action, ids.embed.header);
    }
  }

  spn_dag_link_ctx_t* link = sp_alloc_type(b->mem, spn_dag_link_ctx_t);
  link->target = target;
  sp_da_init(b->mem, link->objects);
  sp_da_for(target->objects, it) {
    spn_dag_object_ids_t* object = sp_ht_getp(b->ids.objects, target->objects[it]);
    sp_assert(object);
    sp_da_push(link->objects, object->object);
  }
  if (ids.embed.object.occupied) {
    sp_da_push(link->objects, ids.embed.object);
  }

  if (target->kind == SPN_CC_OUTPUT_SHARED_LIB || target->kind == SPN_CC_OUTPUT_REACTOR) {
    spn_try_union(dag_add_exports(b, link));
  }

  sp_str_t output = spn_target_output_path(b->mem, target);

  spn_dag_digest_t identity = sp_zero;
  spn_try_union(dag_link_identity(b, link, output, &identity));

  ids.action = spn_dag_add_action(g, (spn_dag_action_config_t) {
    .identity = identity,
    .execute = dag_link_exec,
    .user_data = link,
  });
  sp_da_for(link->objects, it) {
    spn_dag_action_add_input(g, ids.action, link->objects[it]);
  }
  if (link->exports.occupied) {
    spn_dag_action_add_input(g, ids.action, link->exports);
  }
  ids.output = spn_dag_add_file(g, output);
  spn_try_union(spn_result(spn_dag_action_add_output(g, ids.action, ids.output)));

  sp_ht_insert(b->ids.targets, target, ids);
  return spn_result(SPN_OK);
}

static bool dag_pkg_publishes(spn_pkg_unit_t* unit) {
  spn_target_map_t maps [] = { unit->info->libs, unit->info->exes, unit->info->scripts, unit->info->tests };
  u32 num_maps = unit->source == SPN_PKG_SOURCE_ROOT ? 4 : 1;
  sp_for(mt, num_maps) {
    sp_om_for(maps[mt], it) {
      if (!sp_da_empty(sp_str_om_at(maps[mt], it)->headers)) {
        return true;
      }
    }
  }

  sp_da_for(unit->info->publish.copy, it) {
    if (spn_build_copy_to_include(&unit->info->publish.copy[it], SP_NULLPTR)) {
      return true;
    }
  }

  sp_da_for(unit->user_nodes, it) {
    spn_user_node_t* node = &unit->user_nodes[it];
    sp_da_for(node->outputs, ot) {
      if (spn_build_path_within(node->outputs[ot], unit->paths.include)) {
        return true;
      }
    }
  }

  return false;
}

static spn_err_t dag_add_tree(spn_dag_build_t* b, spn_pkg_unit_t* unit, spn_dag_pkg_ids_t* pkg) {
  spn_dag_t* g = b->graph;

  if (!dag_pkg_publishes(unit)) {
    return SPN_OK;
  }

  spn_dag_tree_ctx_t* ctx = sp_alloc_type(b->mem, spn_dag_tree_ctx_t);
  ctx->pkg = unit;
  sp_da_init(b->mem, ctx->obs);

  spn_build_source_pin_t pin = spn_build_source_pin(unit);
  spn_dag_id_t action = spn_dag_add_action(g, (spn_dag_action_config_t) {
    .identity = spn_build_tree_identity(&b->roots, unit, &pin),
    .execute = dag_tree_exec,
    .discover = dag_tree_discover,
    .user_data = ctx,
  });
  pkg->tree = spn_dag_add_tree(g, unit->paths.include);
  spn_try(spn_dag_action_add_output(g, action, pkg->tree));

  spn_target_map_t maps [] = { unit->info->libs, unit->info->exes, unit->info->scripts, unit->info->tests };
  u32 num_maps = unit->source == SPN_PKG_SOURCE_ROOT ? 4 : 1;
  sp_for(mt, num_maps) {
    sp_om_for(maps[mt], it) {
      spn_target_info_t* target = sp_str_om_at(maps[mt], it);
      sp_da_for(target->headers, ht) {
        sp_str_t from = spn_tree_path_resolve(b->mem, unit->paths.roots, target->headers[ht]);
        spn_dag_action_add_input(g, action, spn_dag_add_file(g, from));
      }
    }
  }

  sp_da_for(pkg->user_outputs, it) {
    spn_dag_action_add_input(g, action, pkg->user_outputs[it]);
  }

  return SPN_OK;
}

static spn_err_t dag_add_package_action(spn_dag_build_t* b, spn_pkg_unit_t* unit, spn_dag_pkg_ids_t* pkg) {
  spn_dag_t* g = b->graph;

  spn_dag_package_ctx_t* ctx = sp_alloc_type(b->mem, spn_dag_package_ctx_t);
  ctx->pkg = unit;
  sp_da_init(b->mem, ctx->obs);

  spn_build_source_pin_t pin = spn_build_source_pin(unit);
  pkg->action = spn_dag_add_action(g, (spn_dag_action_config_t) {
    .identity = spn_build_package_identity(unit, &pin),
    .execute = dag_package_exec,
    .discover = dag_package_discover,
    .user_data = ctx,
  });
  pkg->stamp = spn_dag_add_file(g, unit->paths.stamp.package);
  spn_try(spn_dag_action_add_output(g, pkg->action, pkg->stamp));

  if (pkg->tree.occupied) {
    spn_dag_action_add_input(g, pkg->action, pkg->tree);
  }
  sp_da_for(pkg->user_outputs, it) {
    spn_dag_action_add_input(g, pkg->action, pkg->user_outputs[it]);
  }

  spn_target_unit_t* metaprogram = unit->metaprogram ? unit->metaprogram->scripts.build : SP_NULLPTR;
  if (metaprogram && metaprogram->pkg != unit) {
    spn_dag_target_ids_t* ids = sp_ht_getp(b->ids.targets, metaprogram);
    if (ids) {
      spn_dag_action_add_input(g, pkg->action, ids->output);
    }
  }
  spn_dag_id_t configure = dag_configure_reactor(b, unit);
  if (configure.occupied) {
    spn_dag_action_add_input(g, pkg->action, configure);
  }

  sp_da_for(unit->targets, it) {
    spn_target_unit_t* target = unit->targets[it];
    if (target->lib_kind == SPN_LIB_KIND_OBJECT) {
      sp_da_for(target->objects, ot) {
        spn_dag_object_ids_t* object = sp_ht_getp(b->ids.objects, target->objects[ot]);
        if (object) {
          spn_dag_action_add_input(g, pkg->action, object->object);
        }
      }
      continue;
    }
    spn_dag_target_ids_t* ids = sp_ht_getp(b->ids.targets, target);
    if (ids) {
      spn_dag_action_add_input(g, pkg->action, ids->output);
    }
  }

  return SPN_OK;
}

static spn_err_t dag_add_package(spn_dag_build_t* b, spn_pkg_unit_t* unit) {
  spn_dag_pkg_ids_t ids = sp_zero;
  sp_da_init(b->mem, ids.user_outputs);
  sp_da_init(b->mem, ids.user_actions);
  spn_try(dag_add_user_nodes(b, unit, &ids));
  spn_try(dag_add_tree(b, unit, &ids));
  spn_try(dag_add_package_action(b, unit, &ids));
  sp_ht_insert(b->ids.packages, unit, ids);

  return SPN_OK;
}

static void dag_add_link_deps(spn_dag_build_t* b, spn_target_unit_t* target, spn_dag_id_t action) {
  spn_dag_t* g = b->graph;

  sp_da_for(target->link.libs, it) {
    spn_dag_target_ids_t* dep = sp_ht_getp(b->ids.targets, target->link.libs[it].lib);
    if (dep) {
      spn_dag_action_add_input(g, action, dep->output);
    }
  }
}

static void dag_add_target_edges(spn_dag_build_t* b, spn_target_unit_t* target) {
  spn_dag_t* g = b->graph;
  spn_pkg_unit_t* unit = target->pkg;

  if (target->lib_kind == SPN_LIB_KIND_SOURCE) {
    return;
  }

  spn_dag_pkg_ids_t* unit_ids = sp_ht_getp(b->ids.packages, unit);
  sp_da(spn_dag_id_t) user_outputs = unit_ids ? unit_ids->user_outputs : SP_NULLPTR;

  spn_dag_target_ids_t* found = sp_ht_getp(b->ids.targets, target);
  spn_dag_target_ids_t target_ids = found ? *found : (spn_dag_target_ids_t) sp_zero;

  sp_da_for(target->objects, ot) {
    spn_dag_object_ids_t* object = sp_ht_getp(b->ids.objects, target->objects[ot]);
    if (!object) {
      continue;
    }
    spn_dag_id_t action = object->action;

    sp_da_for(user_outputs, ut) {
      spn_dag_action_add_input(g, action, user_outputs[ut]);
    }

    sp_da_for(unit->deps, dt) {
      spn_pkg_dep_t* dep = &unit->deps[dt];
      if (!spn_dep_kind_applies(dep->kind, target->info->kind)) {
        continue;
      }
      spn_dag_pkg_ids_t* dep_ids = sp_ht_getp(b->ids.packages, dep->unit);
      sp_assert(dep_ids);
      spn_dag_action_add_input(g, action, dep_ids->stamp);
    }
  }

  if (target_ids.action.occupied) {
    dag_add_link_deps(b, target, target_ids.action);
  }

  if (target_ids.embed.action.occupied) {
    spn_dag_id_t embed_action = target_ids.embed.action;

    sp_da_for(user_outputs, ut) {
      spn_dag_action_add_input(g, embed_action, user_outputs[ut]);
    }

    sp_da_for(target->info->embed, et) {
      spn_embed_t* embed = &target->info->embed[et];
      if (embed->kind != SPN_EMBED_DIR) {
        continue;
      }
      sp_da_for(unit->deps, dt) {
        spn_pkg_dep_t* dep = &unit->deps[dt];
        if (!sp_str_starts_with(embed->dir.path, dep->unit->paths.store)) {
          continue;
        }
        spn_dag_pkg_ids_t* dep_ids = sp_ht_getp(b->ids.packages, dep->unit);
        sp_assert(dep_ids);
        spn_dag_action_add_input(g, embed_action, dep_ids->stamp);
      }
    }
  }
}

static spn_err_t dag_add_edges(spn_dag_build_t* b, spn_pkg_unit_t* unit) {
  spn_dag_t* g = b->graph;

  spn_dag_pkg_ids_t* unit_ids = sp_ht_getp(b->ids.packages, unit);
  sp_assert(unit_ids);
  spn_dag_id_t package = unit_ids->action;
  sp_da(spn_dag_id_t) user_actions = unit_ids->user_actions;

  sp_da_for(unit->deps, dt) {
    spn_dag_pkg_ids_t* dep = sp_ht_getp(b->ids.packages, unit->deps[dt].unit);
    sp_assert(dep);
    spn_dag_id_t stamp = dep->stamp;

    spn_dag_action_add_input(g, package, stamp);
    sp_da_for(user_actions, ut) {
      spn_dag_action_add_input(g, user_actions[ut], stamp);
    }
  }

  return SPN_OK;
}

static spn_err_union_t dag_add_unit_targets(spn_dag_build_t* b, sp_da(spn_pkg_unit_t*) units) {
  sp_da_for(units, it) {
    sp_da_for(units[it]->targets, jt) {
      spn_try_union(spn_dag_build_add_target(b, units[it]->targets[jt]));
    }
  }
  return spn_result(SPN_OK);
}

static void dag_add_unit_target_edges(spn_dag_build_t* b, sp_da(spn_pkg_unit_t*) units) {
  sp_da_for(units, it) {
    sp_da_for(units[it]->targets, jt) {
      dag_add_target_edges(b, units[it]->targets[jt]);
    }
  }
}

static spn_err_union_t prepare_graph(spn_dag_build_t* b) {
  spn_session_t* session = b->session;

  sp_om_for(session->units.builds, it) {
    spn_build_unit_t* build = sp_om_at(session->units.builds, it);
    spn_try_union(dag_add_unit_targets(b, build->packages));
  }

  sp_om_for(session->units.builds, it) {
    spn_build_unit_t* build = sp_om_at(session->units.builds, it);
    sp_da_for(build->packages, jt) {
      if (spn_pkg_unit_is_script_host(build->packages[jt])) {
        continue;
      }
      spn_try_union(spn_result(dag_add_package(b, build->packages[jt])));
    }
  }

  sp_om_for(session->units.builds, it) {
    spn_build_unit_t* build = sp_om_at(session->units.builds, it);
    dag_add_unit_target_edges(b, build->packages);
  }

  sp_om_for(session->units.builds, it) {
    spn_build_unit_t* build = sp_om_at(session->units.builds, it);
    sp_da_for(build->packages, jt) {
      if (spn_pkg_unit_is_script_host(build->packages[jt])) {
        continue;
      }
      spn_try_union(spn_result(dag_add_edges(b, build->packages[jt])));
    }
  }

  return spn_result(SPN_OK);
}

/////////
// RUN //
/////////
typedef sp_str_ht(u8) dag_staged_t;

static void dag_stage_link(spn_dag_build_t* b, dag_staged_t* staged, sp_str_t from, sp_str_t to) {
  if (sp_str_empty(to) || sp_str_ht_get(*staged, to)) {
    return;
  }
  sp_str_ht_insert(*staged, sp_str_copy(b->mem, to), (u8)true);

  sp_fs_create_dir(sp_fs_parent_path(to));
  if (sp_fs_exists(to)) {
    sp_fs_remove_file(to);
  }
  if (sp_fs_link(from, to, SP_FS_LINK_HARD) != SP_OK) {
    sp_fs_link(from, to, SP_FS_LINK_COPY);
  }
  spn_dag_file_cache_invalidate(&b->files, to);
}

static void dag_stage_file(spn_dag_build_t* b, dag_staged_t* staged, spn_dag_id_t artifact, sp_str_t to) {
  dag_stage_link(b, staged, spn_dag_find_artifact(b->graph, artifact)->path, to);
}

static void dag_stage_pkg_store(spn_dag_build_t* b, dag_staged_t* staged, spn_pkg_unit_t* unit, sp_str_t root) {
  if (!unit) {
    return;
  }
  switch (unit->source) {
    case SPN_PKG_SOURCE_ROOT:
    case SPN_PKG_SOURCE_FILE: {
      break;
    }
    case SPN_PKG_SOURCE_INDEX: {
      return;
    }
  }

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_da(sp_fs_entry_t) entries = sp_fs_collect_recursive(scratch.mem, unit->paths.store);
  sp_da_for(entries, it) {
    if (entries[it].kind == SP_FS_KIND_DIR) {
      continue;
    }
    sp_str_t relative = sp_str_strip_left(sp_str_strip_left(entries[it].path, unit->paths.store), sp_str_lit("/"));
    dag_stage_link(b, staged, entries[it].path, sp_fs_join_path(scratch.mem, root, relative));
  }
  sp_mem_end_scratch(scratch);
}

static void dag_stage(spn_dag_build_t* b) {
  spn_session_t* session = b->session;
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  dag_staged_t staged = SP_NULLPTR;
  sp_str_ht_init(b->mem, staged);

  sp_da_for(session->plans, it) {
    spn_build_plan_t* plan = &session->plans[it];
    sp_da_for(plan->roots, jt) {
      spn_target_unit_t* target = spn_session_get_target_unit(session, plan->roots[jt]);
      if (target->kind != SPN_CC_OUTPUT_EXE) {
        continue;
      }
      spn_dag_target_ids_t* ids = sp_ht_getp(b->ids.targets, target);
      if (!ids) {
        continue;
      }
      spn_dag_id_t output = ids->output;

      sp_str_t staged_path = spn_target_unit_staged_path(scratch.mem, target);
      dag_stage_file(b, &staged, output, staged_path);

      sp_str_t dir = sp_fs_parent_path(staged_path);
      sp_da(spn_target_unit_t*) libs = spn_target_runtime_libs(scratch.mem, target);
      sp_da_for(libs, lt) {
        spn_target_unit_t* lib = libs[lt];
        spn_dag_target_ids_t* lib_ids = sp_ht_getp(b->ids.targets, lib);
        if (!lib_ids) {
          continue;
        }
        spn_dag_id_t lib_output = lib_ids->output;
        sp_str_t from = spn_target_output_path(scratch.mem, lib);
        dag_stage_file(b, &staged, lib_output, sp_fs_join_path(scratch.mem, dir, sp_fs_get_name(from)));
      }
    }
  }

  sp_da_for(session->plans, it) {
    spn_build_unit_t* build = session->plans[it].build;
    sp_str_t root = sp_fs_join_path(scratch.mem, build->paths.root, sp_str_lit("store"));
    dag_stage_pkg_store(b, &staged, spn_session_find_pkg_unit(session, build, spn_session_root_pkg(session)), root);
    sp_da_for(build->packages, jt) {
      dag_stage_pkg_store(b, &staged, build->packages[jt], root);
    }
  }

  sp_mem_end_scratch(scratch);
}

static spn_err_union_t dag_result(spn_dag_build_t* b) {
  spn_dag_diag_t* diag = &b->env.diag;

  switch (b->result) {
    case SPN_OK: {
      return spn_result(SPN_OK);
    }
    case SPN_ERR_DAG_CANCELLED:
    case SPN_ERR_DAG_ACTION: {
      return spn_err_reported(b->result);
    }
    default: {
      break;
    }
  }

  sp_str_t path = diag->path;
  if (sp_str_empty(path) && diag->action.occupied) {
    spn_dag_action_t* action = spn_dag_find_action(b->graph, diag->action);
    if (!sp_da_empty(action->produces)) {
      spn_dag_artifact_t* artifact = spn_dag_find_artifact(b->graph, action->produces[0]);
      path = sp_str_empty(artifact->target) ? artifact->name : artifact->target;
    }
  }

  return (spn_err_union_t) {
    .kind = diag->err ? diag->err : b->result,
    .dag = { .path = path },
  };
}

static void dag_emit_reports(spn_dag_build_t* b, u64 elapsed) {
  spn_session_t* session = b->session;
  bool failed = b->result != SPN_OK;
  u32 hits = (u32)sp_atomic_s32_load(&b->progress.hits, SP_ATOMIC_SEQ_CST);
  u32 misses = (u32)sp_atomic_s32_load(&b->progress.misses, SP_ATOMIC_SEQ_CST);

  sp_da_for(session->plans, it) {
    spn_build_unit_t* build = session->plans[it].build;
    spn_pkg_unit_t* root = spn_session_find_pkg_unit(session, build, spn_session_root_pkg(session));
    spn_pkg_info_t* pkg = root ? root->info : session->pkg;
    spn_build_io_t* io = root ? &root->logs.io : SP_NULLPTR;
    spn_profile_info_t* profile = &build->profile;

    if (failed) {
      spn_event_buffer_push(session->ctx->events, (spn_build_event_t) {
        .kind = SPN_EVENT_BUILD_FAILED,
        .pkg = pkg,
        .io = io,
        .build_failed = {
          .profile = profile->name,
          .time = elapsed,
          .num_errors = 1,
          .first_error = sp_cstr_as_str(spn_err_to_str(b->result)),
        },
      });
    }
    else {
      spn_event_buffer_push(session->ctx->events, (spn_build_event_t) {
        .kind = SPN_EVENT_BUILD_PASSED,
        .pkg = pkg,
        .io = io,
        .build.passed = {
          .profile = profile,
          .time = elapsed,
          .hits = hits,
          .misses = misses,
        },
      });
    }

    spn_event_buffer_push(session->ctx->events, (spn_build_event_t) {
      .kind = SPN_EVENT_BUILD_SUMMARY,
      .pkg = pkg,
      .io = io,
      .build_summary = {
        .success = !failed,
        .hits = hits,
        .misses = misses,
        .total = (u32)sp_da_size(b->graph->actions),
        .time = elapsed,
        .profile = profile->name,
      },
    });
  }
}

static sp_str_t dag_root_dir(sp_mem_t mem, sp_str_t path) {
  if (sp_str_empty(path)) {
    return path;
  }
  sp_str_t canonical = sp_fs_canonicalize_path(mem, path);
  return sp_str_empty(canonical) ? sp_fs_normalize_path(mem, path) : canonical;
}

spn_dag_build_t* spn_dag_build_new(spn_op_t* op) {
  spn_session_t* session = op->session;
  spn_dag_build_t* b = sp_alloc_type(session->mem, spn_dag_build_t);
  sp_mem_zero(b, sizeof(spn_dag_build_t));
  b->session = session;
  b->mem = spn.mem;
  b->graph = spn_dag_new(spn.mem);
  b->roots.dirs[SPN_DAG_ROOT_PROJECT] = dag_root_dir(session->mem, session->paths.root);
  b->roots.dirs[SPN_DAG_ROOT_STORE] = dag_root_dir(session->mem, spn.paths.caches.store.dir);
  b->roots.dirs[SPN_DAG_ROOT_BUILD] = dag_root_dir(session->mem, spn.paths.caches.build.dir);
  b->roots.dirs[SPN_DAG_ROOT_CHECKOUT] = dag_root_dir(session->mem, spn.paths.caches.git.checkouts);
  b->roots.dirs[SPN_DAG_ROOT_TOOLCHAIN] = dag_root_dir(session->mem, session->units.target->toolchain->root);
  b->roots.dirs[SPN_DAG_ROOT_TOOLCHAIN_SCRIPT] = dag_root_dir(session->mem, session->units.metaprogram->toolchain->root);
  sp_ht_init(b->mem, b->ids.packages);
  sp_ht_init(b->mem, b->ids.targets);
  sp_ht_init(b->mem, b->ids.objects);

  sp_str_t root = sp_fs_join_path(session->mem, spn.paths.caches.dir, sp_str_lit("dag"));
  sp_fs_create_dir(root);
  sp_str_t tmp = sp_fs_join_path(session->mem, root, sp_str_lit("tmp"));
  sp_fs_create_dir(tmp);

  spn_dag_store_init(&b->store, (spn_dag_store_config_t) {
    .kind = SPN_DAG_STORE_FILESYSTEM,
    .mem = spn.mem,
    .dir = sp_fs_join_path(session->mem, root, sp_str_lit("store")),
  });
  spn_dag_file_cache_init(&b->files, spn.mem);
  spn_dag_action_cache_init(&b->actions, spn.mem, sp_fs_join_path(session->mem, root, sp_str_lit("strong")));
  spn_dag_obs_table_init(&b->discovery, spn.mem, sp_fs_join_path(session->mem, root, sp_str_lit("weak")), &b->roots);
  spn_dag_obs_table_init(&b->memos, spn.mem, sp_fs_join_path(session->mem, root, sp_str_lit("weak")), &b->roots);

  b->env = (spn_dag_env_t) {
    .files = &b->files,
    .cache = &b->actions,
    .store = &b->store,
    .discovery = &b->discovery,
    .memos = &b->memos,
    .roots = &b->roots,
    .progress = &b->progress,
    .wake = &op->ctx->wake,
    .cancel = &op->cancelled,
    .scratch = tmp,
  };

  return b;
}

spn_err_union_t spn_dag_build_run(spn_dag_build_t* b, u32 workers) {
  spn_thread_pool_init(&b->pool, spn.mem, (spn_thread_pool_config_t) {
    .workers = workers,
    .on_worker_exit = spn_wasm_thread_exit,
  });

  b->timer = sp_tm_start_timer();
  b->result = spn_dag_run_executor(b->graph, &b->env, &b->pool.executor);
  spn_thread_pool_deinit(&b->pool);
  return dag_result(b);
}

spn_err_union_t spn_dag_build_session(spn_op_t* op) {
  spn_session_t* session = op->session;
  spn_project_t* project = session->project;

  spn_dag_build_t* b = spn_dag_build_new(op);
  session->dag.build = b;

  spn_try_union(prepare_graph(b));

  spn_triple_t target = { session->profile.arch, session->profile.os, session->profile.abi };
  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_INIT_BUILD_GRAPH,
    .pkg = session->pkg,
    .graph_init = {
      .profile = session->profile.name,
      .target = spn_triple_to_str(session->mem, target),
      .toolchain = session->units.target->toolchain->info->name,
      .force = session->config.force,
    }
  });

  sp_atomic_ptr_store(&session->ctx->progress, &b->progress, SP_ATOMIC_SEQ_CST);
  spn_err_union_t result = spn_dag_build_run(b, 16);
  sp_atomic_ptr_store(&session->ctx->progress, SP_NULLPTR, SP_ATOMIC_SEQ_CST);
  u64 elapsed = sp_tm_read_timer(&b->timer);

  if (b->result == SPN_ERR_DAG_CANCELLED) {
    return result;
  }

  if (!result.kind) {
    if (!project->lock.some) {
      spn_try_union(spn_project_update_lock(project, session->ctx->intern, session->resolve));
    }
    dag_stage(b);
  }

  dag_emit_reports(b, elapsed);

  return result;
}
