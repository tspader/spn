#include "sp.h"
#include "sp/macro.h"
#include "app/app.h"
#include "app/types.h"
#include "ctx/types.h"
#include "error/types.h"
#include "event/types.h"
#include "forward/types.h"
#include "unit/types.h"

#include "api/api.h"
#include "dag/occ.h"
#include "enum/enum.h"
#include "event/event.h"
#include "external/wasm/wasm.h"
#include "log/log.h"
#include "session/invocation.h"
#include "session/session.h"
#include "target/closure.h"
#include "task/build/build.h"
#include "task/build/dag.h"
#include "task/build/nodes/nodes.h"
#include "task/task.h"
#include "unit/package.h"

typedef struct {
  spn_target_unit_t* target;
  sp_da(spn_dag_id_t) objects;
} spn_dag_link_ctx_t;

typedef struct {
  spn_pkg_unit_t* pkg;
  sp_da(spn_dag_obs_t) obs;
} spn_dag_tree_ctx_t;

typedef struct {
  spn_target_unit_t* target;
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

static spn_dag_build_t* dag_of(spn_pkg_unit_t* pkg) {
  return pkg->session->dag;
}

static sp_str_t dag_artifact_path(spn_dag_build_t* b, spn_dag_id_t id) {
  return spn_dag_find_artifact(b->graph, id)->path;
}

static void dag_emit_err(spn_err_union_t err) {
  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_ERR,
    .err = err,
  });
}

////////////////
// IDENTITIES //
////////////////
static spn_dag_digest_t dag_invocation_identity(sp_str_t salt, spn_toolchain_unit_t* toolchain, spn_invocation_t* invocation) {
  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  spn_dag_hash_str(&ctx, salt);
  spn_dag_hash_u64(&ctx, toolchain->identity);
  spn_dag_hash_str(&ctx, invocation->program);
  spn_dag_hash_str(&ctx, invocation->cwd);
  spn_dag_hash_strs(&ctx, invocation->args);
  return spn_dag_hash_final(&ctx);
}

static spn_err_t dag_compile_identity(spn_compile_unit_t* unit, spn_dag_digest_t* identity) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_invocation_t invocation = sp_zero;
  spn_err_union_t err = spn_build_render_compile(s.mem, unit, sp_str_lit(""), sp_str_lit(""), &invocation);
  if (!err.kind) {
    *identity = dag_invocation_identity(sp_str_lit("spn.build.compile.v1"), unit->target->pkg->build->toolchain, &invocation);
  }
  sp_mem_end_scratch(s);
  if (err.kind) {
    dag_emit_err(err);
    return SPN_ERROR;
  }
  return SPN_OK;
}

static spn_err_t dag_link_identity(spn_target_unit_t* target, spn_dag_digest_t* identity) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_invocation_t invocation = sp_zero;
  sp_da(sp_str_t) objects = sp_da_new(s.mem, sp_str_t);
  spn_err_union_t err = spn_build_render_target(s.mem, target, sp_str_lit(""), objects, &invocation);
  if (!err.kind) {
    *identity = dag_invocation_identity(sp_str_lit("spn.build.link.v1"), target->pkg->build->toolchain, &invocation);
  }
  sp_mem_end_scratch(s);
  if (err.kind) {
    dag_emit_err(err);
    return SPN_ERROR;
  }
  return SPN_OK;
}

static spn_dag_digest_t dag_embed_identity(spn_target_unit_t* target) {
  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  spn_dag_hash_str(&ctx, sp_str_lit("spn.build.embed.v1"));
  spn_dag_hash_str(&ctx, target->pkg->info->qualified);
  spn_dag_hash_str(&ctx, target->info->name);
  sp_da_for(target->info->embed, it) {
    spn_embed_t* embed = &target->info->embed[it];
    spn_dag_hash_u8(&ctx, (u8)embed->kind);
    spn_dag_hash_str(&ctx, embed->symbol);
    spn_dag_hash_str(&ctx, embed->types.data);
    spn_dag_hash_str(&ctx, embed->types.size);
    switch (embed->kind) {
      case SPN_EMBED_FILE: {
        spn_dag_hash_str(&ctx, embed->file.path);
        break;
      }
      case SPN_EMBED_DIR: {
        spn_dag_hash_str(&ctx, embed->dir.path);
        spn_dag_hash_str(&ctx, embed->dir.dest);
        break;
      }
      case SPN_EMBED_MEM: {
        spn_dag_hash_u64(&ctx, embed->memory.size);
        spn_dag_hash_bytes(&ctx, embed->memory.buffer, embed->memory.size);
        break;
      }
    }
  }
  return spn_dag_hash_final(&ctx);
}

static bool dag_copy_to_include(spn_publish_copy_t* copy, sp_str_t* rest) {
  sp_str_pair_t to = sp_str_cleave_c8(copy->to, '/');
  if (!sp_str_equal(to.first, sp_str_lit("include"))) {
    return false;
  }
  if (rest) {
    *rest = to.second;
  }
  return true;
}

static bool dag_path_within(sp_str_t path, sp_str_t dir) {
  if (path.len <= dir.len + 1 || !sp_str_starts_with(path, dir)) {
    return false;
  }
  return path.data[dir.len] == '/';
}

static spn_dag_digest_t dag_tree_identity(spn_pkg_unit_t* unit) {
  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  spn_dag_hash_str(&ctx, sp_str_lit("spn.build.tree.v1"));
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
    if (!dag_copy_to_include(copy, SP_NULLPTR)) {
      continue;
    }
    spn_dag_hash_str(&ctx, copy->from);
    spn_dag_hash_str(&ctx, copy->to);
  }

  sp_da_for(unit->nodes.user, it) {
    spn_user_node_t* node = &unit->nodes.user[it];
    sp_da_for(node->outputs, ot) {
      if (dag_path_within(node->outputs[ot], unit->paths.include)) {
        spn_dag_hash_str(&ctx, node->outputs[ot]);
      }
    }
  }

  return spn_dag_hash_final(&ctx);
}

static spn_dag_digest_t dag_package_identity(spn_pkg_unit_t* unit) {
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

static spn_dag_digest_t dag_user_identity(spn_user_node_t* node) {
  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  spn_dag_hash_str(&ctx, sp_str_lit("spn.build.user.v1"));
  spn_dag_hash_str(&ctx, node->pkg->info->qualified);
  spn_dag_hash_str(&ctx, node->tag);
  spn_dag_hash_str(&ctx, node->fn);
  spn_dag_hash_strs(&ctx, node->inputs);
  spn_dag_hash_strs(&ctx, node->outputs);
  return spn_dag_hash_final(&ctx);
}

//////////////
// EXECUTES //
//////////////
static sp_str_t dag_dep_path(sp_mem_t mem, sp_str_t object) {
  return sp_fmt(mem, "{}.d", sp_fmt_str(object)).value;
}

static s32 dag_compile_exec(spn_dag_action_t* action, void* user_data) {
  spn_compile_unit_t* unit = (spn_compile_unit_t*)user_data;
  spn_dag_build_t* b = dag_of(unit->target->pkg);

  sp_str_t object = dag_artifact_path(b, unit->dag.object);
  sp_str_t depfile = action->discover ? dag_dep_path(b->mem, object) : sp_str_lit("");
  return spn_compile_object_run(unit, object, depfile);
}

static spn_err_t dag_compile_discover(spn_dag_action_t* action, void* user_data, sp_mem_t mem, sp_da(spn_dag_obs_t)* out) {
  spn_compile_unit_t* unit = (spn_compile_unit_t*)user_data;
  spn_dag_build_t* b = dag_of(unit->target->pkg);

  sp_str_t object = dag_artifact_path(b, unit->dag.object);
  sp_str_t dep = dag_dep_path(mem, object);
  if (!sp_fs_exists(dep)) {
    return SPN_OK;
  }
  sp_str_t content = sp_zero;
  spn_try_as(sp_io_read_file(mem, dep, &content), SPN_ERROR);

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
    sp_da_push(*out, ((spn_dag_obs_t) {
      .kind = SPN_DAG_OBS_FILE,
      .path = sp_fs_normalize_path(mem, path),
    }));
  }

  return parser.err ? SPN_ERROR : SPN_OK;
}

static s32 dag_link_exec(spn_dag_action_t* action, void* user_data) {
  spn_dag_link_ctx_t* link = (spn_dag_link_ctx_t*)user_data;
  spn_target_unit_t* target = link->target;
  spn_dag_build_t* b = dag_of(target->pkg);

  sp_da(sp_str_t) objects = sp_da_new(b->mem, sp_str_t);
  sp_da_for(link->objects, it) {
    sp_da_push(objects, dag_artifact_path(b, link->objects[it]));
  }
  return spn_link_target_run(target, dag_artifact_path(b, target->dag.output), objects);
}

static s32 dag_embed_exec(spn_dag_action_t* action, void* user_data) {
  spn_dag_embed_ctx_t* ctx = (spn_dag_embed_ctx_t*)user_data;
  spn_target_unit_t* target = ctx->target;
  spn_dag_build_t* b = dag_of(target->pkg);

  sp_da_init(b->mem, ctx->obs);
  sp_str_t object = dag_artifact_path(b, target->dag.embed.object);
  sp_str_t header = dag_artifact_path(b, target->dag.embed.header);
  return spn_embed_write(target, object, header, b->mem, &ctx->obs);
}

static spn_err_t dag_embed_discover(spn_dag_action_t* action, void* user_data, sp_mem_t mem, sp_da(spn_dag_obs_t)* out) {
  spn_dag_embed_ctx_t* ctx = (spn_dag_embed_ctx_t*)user_data;
  sp_da_for(ctx->obs, it) {
    sp_da_push(*out, ctx->obs[it]);
  }
  return SPN_OK;
}

static s32 dag_user_exec(spn_dag_action_t* action, void* user_data) {
  spn_dag_user_ctx_t* ctx = (spn_dag_user_ctx_t*)user_data;
  spn_user_node_t* node = ctx->node;
  spn_pkg_unit_t* pkg = node->pkg;
  spn_dag_build_t* b = dag_of(pkg);

  spn_pkg_unit_announce_compile(pkg);

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_BUILD_SCRIPT_USER_FN,
    .pkg = pkg->info,
    .io = &pkg->logs.io,
    .node = { .info = node }
  });

  sp_da_init(b->mem, ctx->obs);
  if (!sp_str_empty(node->fn)) {
    spn_node_ctx_t node_ctx = { .user_data = node->user_data };
    if (spn_wasm_call_export_ex(pkg, node->fn, SPN_ABI_KIND_NODE_CTX, &node_ctx, (spn_wasm_obs_t) { .mem = b->mem, .out = &ctx->obs })) {
      return 1;
    }
  }

  spn_dag_t* g = b->graph;
  sp_da_for(action->produces, it) {
    spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, action->produces[it]);
    if (ctx->stamp) {
      sp_fs_create_file(artifact->path);
      continue;
    }
    if (!sp_fs_exists(artifact->target)) {
      spn_log_error("{.cyan} node {.yellow} declared output {.yellow}, which it did not produce",
        SP_FMT_STR(pkg->info->name), SP_FMT_STR(node->tag), SP_FMT_STR(artifact->target));
      return 1;
    }
    if (sp_fs_copy(artifact->target, artifact->path)) {
      return 1;
    }
  }

  return 0;
}

static s32 dag_tree_copy_headers(spn_dag_tree_ctx_t* ctx, sp_str_t root, spn_target_map_t targets) {
  spn_pkg_unit_t* unit = ctx->pkg;
  sp_om_for(targets, it) {
    spn_target_info_t* target = sp_str_om_at(targets, it);
    sp_da_for(target->headers, ht) {
      sp_str_t header = target->headers[ht];
      sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
      sp_str_t from = sp_fs_join_path(scratch.mem, unit->paths.source, header);
      sp_str_t to = sp_fs_join_path(scratch.mem, root, header);
      sp_fs_create_dir(sp_fs_parent_path(to));
      s32 err = sp_fs_copy(from, to);
      sp_mem_end_scratch(scratch);
      if (err) {
        spn_log_error("{.cyan} failed to publish header {.yellow}", SP_FMT_STR(unit->info->name), SP_FMT_STR(header));
        return 1;
      }
    }
  }
  return 0;
}

static s32 dag_tree_copy_publishes(spn_dag_tree_ctx_t* ctx, sp_str_t root) {
  spn_pkg_unit_t* unit = ctx->pkg;
  spn_dag_build_t* b = dag_of(unit);

  sp_da_for(unit->info->publish.copy, it) {
    spn_publish_copy_t* copy = &unit->info->publish.copy[it];
    sp_str_t rest = sp_zero;
    if (!dag_copy_to_include(copy, &rest)) {
      continue;
    }

    sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
    sp_str_pair_t from = sp_str_cleave_c8(copy->from, '/');
    sp_str_t from_root = spn_api_dir(unit, spn_cache_dir_kind_from_str(from.first));
    sp_str_t dest = sp_fs_join_path(scratch.mem, root, rest);

    s32 err = 0;
    sp_da(spn_dag_match_t) matches = sp_da_new(scratch.mem, spn_dag_match_t);
    if (spn_dag_glob(b->mem, from_root, from.second, &ctx->obs, &matches)) {
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
      err = 1;
    }
    else {
      sp_fs_create_dir(sp_fs_parent_path(dest));
      err = sp_fs_copy(matches[0].path, dest) ? 1 : 0;
    }

    sp_mem_end_scratch(scratch);
    if (err) {
      spn_log_error("{.cyan} failed to publish {.yellow} to {.yellow}", SP_FMT_STR(unit->info->name), SP_FMT_STR(copy->from), SP_FMT_STR(copy->to));
      return 1;
    }
  }
  return 0;
}

static s32 dag_tree_copy_user_outputs(spn_dag_tree_ctx_t* ctx, sp_str_t root) {
  spn_pkg_unit_t* unit = ctx->pkg;
  sp_da_for(unit->nodes.user, it) {
    spn_user_node_t* node = &unit->nodes.user[it];
    sp_da_for(node->outputs, ot) {
      sp_str_t path = node->outputs[ot];
      if (!dag_path_within(path, unit->paths.include)) {
        continue;
      }
      sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
      sp_str_t relative = sp_str_strip_left(sp_str_strip_left(path, unit->paths.include), sp_str_lit("/"));
      sp_str_t to = sp_fs_join_path(scratch.mem, root, relative);
      sp_fs_create_dir(sp_fs_parent_path(to));
      s32 err = sp_fs_copy(path, to);
      sp_mem_end_scratch(scratch);
      if (err) {
        return 1;
      }
    }
  }
  return 0;
}

static s32 dag_tree_exec(spn_dag_action_t* action, void* user_data) {
  spn_dag_tree_ctx_t* ctx = (spn_dag_tree_ctx_t*)user_data;
  spn_pkg_unit_t* unit = ctx->pkg;
  spn_dag_build_t* b = dag_of(unit);

  sp_da_init(b->mem, ctx->obs);

  sp_str_t root = dag_artifact_path(b, unit->dag.tree);

  if (dag_tree_copy_headers(ctx, root, unit->info->libs)) {
    return 1;
  }
  if (unit->source == SPN_PKG_SOURCE_ROOT) {
    if (dag_tree_copy_headers(ctx, root, unit->info->exes)) return 1;
    if (dag_tree_copy_headers(ctx, root, unit->info->scripts)) return 1;
    if (dag_tree_copy_headers(ctx, root, unit->info->tests)) return 1;
  }
  if (dag_tree_copy_publishes(ctx, root)) {
    return 1;
  }
  if (dag_tree_copy_user_outputs(ctx, root)) {
    return 1;
  }

  return 0;
}

static spn_err_t dag_tree_discover(spn_dag_action_t* action, void* user_data, sp_mem_t mem, sp_da(spn_dag_obs_t)* out) {
  spn_dag_tree_ctx_t* ctx = (spn_dag_tree_ctx_t*)user_data;
  sp_da_for(ctx->obs, it) {
    sp_da_push(*out, ctx->obs[it]);
  }
  return SPN_OK;
}

static spn_err_t dag_user_discover(spn_dag_action_t* action, void* user_data, sp_mem_t mem, sp_da(spn_dag_obs_t)* out) {
  spn_dag_user_ctx_t* ctx = (spn_dag_user_ctx_t*)user_data;
  sp_da_for(ctx->obs, it) {
    sp_da_push(*out, ctx->obs[it]);
  }
  return SPN_OK;
}

static s32 dag_package_exec(spn_dag_action_t* action, void* user_data) {
  spn_dag_package_ctx_t* ctx = (spn_dag_package_ctx_t*)user_data;
  spn_pkg_unit_t* unit = ctx->pkg;
  spn_dag_build_t* b = dag_of(unit);

  sp_da_init(b->mem, ctx->obs);

  sp_da_for(unit->info->publish.copy, it) {
    spn_publish_copy_t* copy = &unit->info->publish.copy[it];
    if (dag_copy_to_include(copy, SP_NULLPTR)) {
      continue;
    }
    sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
    sp_str_pair_t from = sp_str_cleave_c8(copy->from, '/');
    sp_str_pair_t to = sp_str_cleave_c8(copy->to, '/');
    s32 err = spn_copy(
      (spn_t*)unit,
      spn_cache_dir_kind_from_str(from.first), sp_str_to_cstr(scratch.mem, from.second),
      spn_cache_dir_kind_from_str(to.first), sp_str_to_cstr(scratch.mem, to.second)
    );
    sp_mem_end_scratch(scratch);
    if (err) {
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
    spn_node_ctx_t node_ctx = sp_zero;
    if (spn_wasm_script_call_ex(script, unit, sp_str_lit("package"), SPN_ABI_KIND_NODE_CTX, &node_ctx, (spn_wasm_obs_t) { .mem = b->mem, .out = &ctx->obs })) {
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

  spn_dag_artifact_t* stamp = spn_dag_find_artifact(b->graph, action->produces[0]);
  spn_pkg_unit_write_stamp(unit, stamp->path);

  return 0;
}

static spn_err_t dag_package_discover(spn_dag_action_t* action, void* user_data, sp_mem_t mem, sp_da(spn_dag_obs_t)* out) {
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
  spn_target_unit_t* configure = unit->metaprogram.configure.target;
  if (!configure) {
    return (spn_dag_id_t) sp_zero;
  }
  return spn_dag_add_file(b->graph, get_target_output_path(b->mem, configure));
}

static spn_err_t dag_add_user_nodes(spn_dag_build_t* b, spn_pkg_unit_t* unit) {
  spn_dag_t* g = b->graph;
  spn_target_unit_t* metaprogram = unit->metaprogram.build.target;
  spn_dag_id_t configure = dag_configure_reactor(b, unit);

  sp_da_init(b->mem, unit->dag.user_outputs);

  sp_da_for(unit->nodes.user, it) {
    spn_user_node_t* node = &unit->nodes.user[it];

    spn_dag_user_ctx_t* ctx = sp_alloc_type(b->mem, spn_dag_user_ctx_t);
    ctx->node = node;
    ctx->stamp = sp_da_empty(node->outputs);
    if (ctx->stamp) {
      sp_da_push(node->outputs, spn_pkg_unit_get_node_stamp_file(unit, node));
    }

    node->dag = spn_dag_add_action(g, (spn_dag_action_config_t) {
      .identity = dag_user_identity(node),
      .execute = dag_user_exec,
      .discover = dag_user_discover,
      .user_data = ctx,
    });

    if (metaprogram && metaprogram->dag.output.occupied) {
      spn_dag_action_add_input(g, node->dag, metaprogram->dag.output);
    }
    if (configure.occupied) {
      spn_dag_action_add_input(g, node->dag, configure);
    }

    sp_da_for(node->outputs, ot) {
      spn_dag_id_t file = spn_dag_add_file(g, node->outputs[ot]);
      spn_try(spn_dag_action_add_output(g, node->dag, file));
      sp_da_push(unit->dag.user_outputs, file);
    }
  }

  sp_da_for(unit->nodes.user, it) {
    spn_user_node_t* node = &unit->nodes.user[it];
    sp_da_for(node->inputs, jt) {
      spn_dag_action_add_input(g, node->dag, spn_dag_add_file(g, node->inputs[jt]));
    }
    sp_da_for(node->deps, jt) {
      spn_user_node_t* dep = spn_find_user_node(node->deps[jt]);
      sp_da_for(dep->outputs, ot) {
        spn_dag_action_add_input(g, node->dag, spn_dag_add_file(g, dep->outputs[ot]));
      }
    }
  }

  return SPN_OK;
}

static spn_err_t dag_add_objects(spn_dag_build_t* b, spn_target_unit_t* target) {
  spn_dag_t* g = b->graph;
  spn_cc_toolchain_t* toolchain = &target->pkg->build->toolchain->cc;
  bool discovery = toolchain->driver != SPN_CC_DRIVER_MSVC;

  sp_da_for(target->objects, it) {
    spn_compile_unit_t* unit = target->objects[it];
    if (unit->dag.action.occupied) {
      continue;
    }

    spn_dag_digest_t identity = sp_zero;
    spn_try(dag_compile_identity(unit, &identity));

    unit->dag.action = spn_dag_add_action(g, (spn_dag_action_config_t) {
      .identity = identity,
      .execute = dag_compile_exec,
      .discover = discovery ? dag_compile_discover : SP_NULLPTR,
      .user_data = unit,
    });
    spn_dag_action_add_input(g, unit->dag.action, spn_dag_add_file(g, unit->paths.file));

    unit->dag.object = spn_dag_add_file(g, unit->paths.object);
    spn_try(spn_dag_action_add_output(g, unit->dag.action, unit->dag.object));
  }

  return SPN_OK;
}

static spn_err_t dag_add_target(spn_dag_build_t* b, spn_target_unit_t* target) {
  spn_dag_t* g = b->graph;
  spn_pkg_unit_t* pkg = target->pkg;

  switch (target->lib_kind) {
    case SPN_LIB_KIND_SOURCE: {
      return SPN_OK;
    }
    case SPN_LIB_KIND_OBJECT: {
      return dag_add_objects(b, target);
    }
    case SPN_LIB_KIND_STATIC:
    case SPN_LIB_KIND_SHARED:
    case SPN_LIB_KIND_NONE: {
      break;
    }
  }

  if (target->dag.action.occupied) {
    return SPN_OK;
  }

  spn_try(dag_add_objects(b, target));

  if (sp_da_empty(target->objects)) {
    return SPN_OK;
  }

  if (!sp_da_empty(target->info->embed)) {
    spn_dag_embed_ctx_t* embed = sp_alloc_type(b->mem, spn_dag_embed_ctx_t);
    embed->target = target;
    sp_da_init(b->mem, embed->obs);

    target->dag.embed.action = spn_dag_add_action(g, (spn_dag_action_config_t) {
      .identity = dag_embed_identity(target),
      .execute = dag_embed_exec,
      .discover = dag_embed_discover,
      .user_data = embed,
    });
    target->dag.embed.object = spn_dag_add_file(g, get_embed_object_path(b->mem, target));
    target->dag.embed.header = spn_dag_add_file(g, get_embed_header_path(b->mem, target));
    spn_try(spn_dag_action_add_output(g, target->dag.embed.action, target->dag.embed.object));
    spn_try(spn_dag_action_add_output(g, target->dag.embed.action, target->dag.embed.header));

    sp_da_for(target->info->embed, it) {
      spn_embed_t* entry = &target->info->embed[it];
      if (entry->kind == SPN_EMBED_FILE) {
        spn_dag_action_add_input(g, target->dag.embed.action, spn_dag_add_file(g, entry->file.path));
      }
    }

    sp_da_for(pkg->dag.user_outputs, it) {
      spn_dag_action_add_input(g, target->dag.embed.action, pkg->dag.user_outputs[it]);
    }

    sp_da_for(target->objects, it) {
      spn_dag_action_add_input(g, target->objects[it]->dag.action, target->dag.embed.header);
    }
  }

  spn_dag_link_ctx_t* link = sp_alloc_type(b->mem, spn_dag_link_ctx_t);
  link->target = target;
  sp_da_init(b->mem, link->objects);
  sp_da_for(target->objects, it) {
    sp_da_push(link->objects, target->objects[it]->dag.object);
  }
  if (target->dag.embed.object.occupied) {
    sp_da_push(link->objects, target->dag.embed.object);
  }

  spn_dag_digest_t identity = sp_zero;
  spn_try(dag_link_identity(target, &identity));

  target->dag.action = spn_dag_add_action(g, (spn_dag_action_config_t) {
    .identity = identity,
    .execute = dag_link_exec,
    .user_data = link,
  });
  sp_da_for(link->objects, it) {
    spn_dag_action_add_input(g, target->dag.action, link->objects[it]);
  }
  target->dag.output = spn_dag_add_file(g, get_target_output_path(b->mem, target));
  spn_try(spn_dag_action_add_output(g, target->dag.action, target->dag.output));

  return SPN_OK;
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
    if (dag_copy_to_include(&unit->info->publish.copy[it], SP_NULLPTR)) {
      return true;
    }
  }

  sp_da_for(unit->nodes.user, it) {
    spn_user_node_t* node = &unit->nodes.user[it];
    sp_da_for(node->outputs, ot) {
      if (dag_path_within(node->outputs[ot], unit->paths.include)) {
        return true;
      }
    }
  }

  return false;
}

static spn_err_t dag_add_tree(spn_dag_build_t* b, spn_pkg_unit_t* unit) {
  spn_dag_t* g = b->graph;

  if (!dag_pkg_publishes(unit)) {
    return SPN_OK;
  }

  spn_dag_tree_ctx_t* ctx = sp_alloc_type(b->mem, spn_dag_tree_ctx_t);
  ctx->pkg = unit;
  sp_da_init(b->mem, ctx->obs);

  spn_dag_id_t action = spn_dag_add_action(g, (spn_dag_action_config_t) {
    .identity = dag_tree_identity(unit),
    .execute = dag_tree_exec,
    .discover = dag_tree_discover,
    .user_data = ctx,
  });
  unit->dag.tree = spn_dag_add_tree(g, unit->paths.include);
  spn_try(spn_dag_action_add_output(g, action, unit->dag.tree));

  spn_target_map_t maps [] = { unit->info->libs, unit->info->exes, unit->info->scripts, unit->info->tests };
  u32 num_maps = unit->source == SPN_PKG_SOURCE_ROOT ? 4 : 1;
  sp_for(mt, num_maps) {
    sp_om_for(maps[mt], it) {
      spn_target_info_t* target = sp_str_om_at(maps[mt], it);
      sp_da_for(target->headers, ht) {
        sp_str_t from = sp_fs_join_path(b->mem, unit->paths.source, target->headers[ht]);
        spn_dag_action_add_input(g, action, spn_dag_add_file(g, from));
      }
    }
  }

  sp_da_for(unit->dag.user_outputs, it) {
    spn_dag_action_add_input(g, action, unit->dag.user_outputs[it]);
  }

  return SPN_OK;
}

static spn_err_t dag_add_package_action(spn_dag_build_t* b, spn_pkg_unit_t* unit) {
  spn_dag_t* g = b->graph;

  spn_dag_package_ctx_t* ctx = sp_alloc_type(b->mem, spn_dag_package_ctx_t);
  ctx->pkg = unit;
  sp_da_init(b->mem, ctx->obs);

  unit->dag.package = spn_dag_add_action(g, (spn_dag_action_config_t) {
    .identity = dag_package_identity(unit),
    .execute = dag_package_exec,
    .discover = dag_package_discover,
    .user_data = ctx,
  });
  unit->dag.stamp = spn_dag_add_file(g, unit->paths.stamp.package);
  spn_try(spn_dag_action_add_output(g, unit->dag.package, unit->dag.stamp));

  if (unit->dag.tree.occupied) {
    spn_dag_action_add_input(g, unit->dag.package, unit->dag.tree);
  }
  sp_da_for(unit->dag.user_outputs, it) {
    spn_dag_action_add_input(g, unit->dag.package, unit->dag.user_outputs[it]);
  }

  spn_target_unit_t* metaprogram = unit->metaprogram.build.target;
  if (metaprogram && metaprogram->dag.output.occupied) {
    spn_dag_action_add_input(g, unit->dag.package, metaprogram->dag.output);
  }
  spn_dag_id_t configure = dag_configure_reactor(b, unit);
  if (configure.occupied) {
    spn_dag_action_add_input(g, unit->dag.package, configure);
  }

  sp_da_for(unit->targets, it) {
    spn_target_unit_t* target = unit->targets[it];
    if (target->lib_kind == SPN_LIB_KIND_OBJECT) {
      sp_da_for(target->objects, ot) {
        if (target->objects[ot]->dag.object.occupied) {
          spn_dag_action_add_input(g, unit->dag.package, target->objects[ot]->dag.object);
        }
      }
      continue;
    }
    if (target->dag.output.occupied) {
      spn_dag_action_add_input(g, unit->dag.package, target->dag.output);
    }
  }

  return SPN_OK;
}

static spn_err_t dag_add_package(spn_dag_build_t* b, spn_pkg_unit_t* unit) {
  spn_target_unit_t* metaprogram = unit->metaprogram.build.target;
  if (metaprogram) {
    spn_try(dag_add_target(b, metaprogram));
  }

  spn_try(dag_add_user_nodes(b, unit));

  sp_da_for(unit->targets, it) {
    spn_target_unit_t* target = unit->targets[it];
    if (sp_da_empty(target->info->source)) {
      continue;
    }
    spn_try(dag_add_target(b, target));
  }

  spn_try(dag_add_tree(b, unit));
  spn_try(dag_add_package_action(b, unit));

  return SPN_OK;
}

static void dag_add_link_deps(spn_dag_build_t* b, spn_target_unit_t* target) {
  if (!target || !target->dag.action.occupied) {
    return;
  }
  spn_dag_t* g = b->graph;
  sp_da_for(target->deps.target, it) {
    spn_target_unit_t* lib = target->deps.target[it];
    if (!lib->info->no_link && lib->dag.output.occupied) {
      spn_dag_action_add_input(g, target->dag.action, lib->dag.output);
    }
  }
  sp_da_for(target->link.libs, it) {
    spn_target_unit_t* lib = target->link.libs[it].lib;
    if (lib->dag.output.occupied) {
      spn_dag_action_add_input(g, target->dag.action, lib->dag.output);
    }
  }
}

static spn_err_t dag_add_edges(spn_dag_build_t* b, spn_pkg_unit_t* unit) {
  spn_dag_t* g = b->graph;

  sp_da_for(unit->targets, it) {
    spn_target_unit_t* target = unit->targets[it];
    if (target->lib_kind == SPN_LIB_KIND_SOURCE) {
      continue;
    }

    sp_da_for(target->objects, ot) {
      spn_compile_unit_t* object = target->objects[ot];
      if (!object->dag.action.occupied) {
        continue;
      }

      sp_da_for(unit->dag.user_outputs, ut) {
        spn_dag_action_add_input(g, object->dag.action, unit->dag.user_outputs[ut]);
      }

      sp_da_for(unit->deps, dt) {
        spn_pkg_dep_t* dep = &unit->deps[dt];
        if (!dep->unit) {
          continue;
        }
        if (dep->kind == SPN_DEP_KIND_TEST && target->info->kind != SPN_TARGET_TEST) {
          continue;
        }
        if (dep->unit->dag.stamp.occupied) {
          spn_dag_action_add_input(g, object->dag.action, dep->unit->dag.stamp);
        }
      }
    }

    dag_add_link_deps(b, target);

    if (target->dag.embed.action.occupied) {
      sp_da_for(target->info->embed, et) {
        spn_embed_t* embed = &target->info->embed[et];
        if (embed->kind != SPN_EMBED_DIR) {
          continue;
        }
        sp_da_for(unit->deps, dt) {
          spn_pkg_dep_t* dep = &unit->deps[dt];
          if (dep->unit && sp_str_starts_with(embed->dir.path, dep->unit->paths.store) && dep->unit->dag.stamp.occupied) {
            spn_dag_action_add_input(g, target->dag.embed.action, dep->unit->dag.stamp);
          }
        }
      }
    }
  }

  sp_da_for(unit->deps, dt) {
    spn_pkg_dep_t* dep = &unit->deps[dt];
    if (!dep->unit || !dep->unit->dag.stamp.occupied) {
      continue;
    }
    spn_dag_action_add_input(g, unit->dag.package, dep->unit->dag.stamp);
    sp_da_for(unit->nodes.user, ut) {
      spn_dag_action_add_input(g, unit->nodes.user[ut].dag, dep->unit->dag.stamp);
    }
  }

  return SPN_OK;
}

static spn_err_t dag_prepare(spn_dag_build_t* b) {
  spn_session_t* session = b->session;

  sp_da_for(session->plan.builds, it) {
    spn_build_unit_t* build = session->plan.builds[it].build;
    sp_da_for(build->packages, jt) {
      spn_try(dag_add_package(b, build->packages[jt]));
    }
  }

  sp_da_for(session->plan.builds, it) {
    spn_build_unit_t* build = session->plan.builds[it].build;
    sp_da_for(build->packages, jt) {
      spn_try(dag_add_edges(b, build->packages[jt]));
    }
  }

  if (session->units.metaprogram) {
    sp_da_for(session->units.metaprogram->packages, it) {
      dag_add_link_deps(b, session->units.metaprogram->packages[it]->metaprogram.build.target);
    }
  }

  return SPN_OK;
}

/////////
// RUN //
/////////
static s32 dag_run_thread(void* data) {
  spn_dag_build_t* b = (spn_dag_build_t*)data;
  b->result = spn_dag_run_executor(b->graph, &b->env, &b->pool.executor);
  sp_atomic_s32_set(&b->done, 1);
  return 0;
}

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

  sp_da_for(session->plan.builds, it) {
    spn_build_plan_t* plan = &session->plan.builds[it];
    sp_da_for(plan->roots, jt) {
      spn_target_unit_t* target = spn_session_get_target_unit(session, plan->roots[jt]);
      if (target->kind != SPN_CC_OUTPUT_EXE || !target->dag.output.occupied) {
        continue;
      }

      sp_str_t staged_path = get_target_staged_path(scratch.mem, target);
      dag_stage_file(b, &staged, target->dag.output, staged_path);

      sp_str_t dir = sp_fs_parent_path(staged_path);
      sp_da(spn_target_unit_t*) libs = spn_target_runtime_libs(scratch.mem, target);
      sp_da_for(libs, lt) {
        spn_target_unit_t* lib = libs[lt];
        if (!lib->dag.output.occupied) {
          continue;
        }
        sp_str_t from = get_target_output_path(scratch.mem, lib);
        dag_stage_file(b, &staged, lib->dag.output, sp_fs_join_path(scratch.mem, dir, sp_fs_get_name(from)));
      }
    }
  }

  sp_da_for(session->plan.builds, it) {
    spn_build_unit_t* build = session->plan.builds[it].build;
    sp_str_t root = sp_fs_join_path(scratch.mem, build->paths.root, sp_str_lit("store"));
    dag_stage_pkg_store(b, &staged, spn_session_find_pkg_unit(session, build, spn_session_root_pkg(session)), root);
    sp_da_for(build->packages, jt) {
      dag_stage_pkg_store(b, &staged, build->packages[jt], root);
    }
  }

  sp_mem_end_scratch(scratch);
}

static void dag_emit_reports(spn_dag_build_t* b, u64 elapsed) {
  spn_session_t* session = b->session;
  bool failed = b->result != SPN_OK;
  u32 hits = (u32)sp_atomic_s32_get(&b->progress.hits);
  u32 misses = (u32)sp_atomic_s32_get(&b->progress.misses);

  sp_da_for(session->plan.builds, it) {
    spn_build_unit_t* build = session->plan.builds[it].build;
    spn_pkg_unit_t* root = spn_session_find_pkg_unit(session, build, spn_session_root_pkg(session));
    spn_pkg_info_t* pkg = root ? root->info : session->pkg;
    spn_build_io_t* io = root ? &root->logs.io : SP_NULLPTR;
    spn_profile_info_t* profile = &build->profile;

    if (failed) {
      spn_event_buffer_push(session->events, (spn_build_event_t) {
        .kind = SPN_EVENT_BUILD_FAILED,
        .pkg = pkg,
        .io = io,
        .build_failed = {
          .profile = profile->name,
          .time = elapsed,
          .num_errors = 1,
        },
      });
    }
    else {
      spn_event_buffer_push(session->events, (spn_build_event_t) {
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

    spn_event_buffer_push(session->events, (spn_build_event_t) {
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

spn_task_step_t spn_dag_build_init(spn_app_t* app) {
  spn_session_t* session = &app->session;

  spn_dag_build_t* b = sp_alloc_type(session->mem, spn_dag_build_t);
  sp_mem_zero(b, sizeof(spn_dag_build_t));
  session->dag = b;
  b->session = session;
  b->mem = spn.mem;
  b->graph = spn_dag_new(spn.mem);

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
  spn_dag_discovery_init(&b->discovery, spn.mem, sp_fs_join_path(session->mem, root, sp_str_lit("weak")));

  b->env = (spn_dag_env_t) {
    .files = &b->files,
    .cache = &b->actions,
    .store = &b->store,
    .discovery = &b->discovery,
    .progress = &b->progress,
    .scratch = tmp,
  };

  if (dag_prepare(b)) {
    spn_log_error("failed to construct dag build graph");
    return spn_task_fail(SPN_ERROR);
  }

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_INIT_BUILD_GRAPH,
    .graph_init = {
      .profile = session->profile.name,
      .force = app->config.force,
    }
  });

  spn_dag_pool_init(&b->pool, spn.mem, (spn_dag_pool_config_t) {
    .workers = 16,
    .on_worker_exit = spn_wasm_thread_exit,
  });

  b->timer = sp_tm_start_timer();
  sp_thread_init(&b->runner, dag_run_thread, b);

  return spn_task_continue();
}

spn_task_step_t spn_dag_build_update(spn_app_t* app) {
  spn_session_t* session = &app->session;
  spn_dag_build_t* b = session->dag;

  if (!sp_atomic_s32_get(&b->done)) {
    return spn_task_continue();
  }

  sp_thread_join(&b->runner);
  spn_dag_pool_deinit(&b->pool);

  u64 elapsed = sp_tm_read_timer(&b->timer);

  if (!b->result) {
    if (!app->lock.some) {
      spn_app_update_lock_file(app);
    }
    dag_stage(b);
  }

  dag_emit_reports(b, elapsed);

  return b->result ? spn_task_fail(SPN_ERROR) : spn_task_done();
}
