#include "sp.h"
#include "sp/atomic_file.h"
#include "sp/io.h"
#include "sp/macro.h"
#include "project/project.h"
#include "ctx/types.h"
#include "error/error.h"
#include "spn/errors.h"
#include "event/types.h"
#include "core/types.h"
#include "unit/types.h"

#include "api/api.h"
#include "compiler/driver.h"
#include "core/core.h"
#include "cpu/cpu.h"
#include "dag/wasi/canonicalize.h"
#include "enum/enum.h"
#include "event/event.h"
#include "external/wasm/wasm.h"
#include "op/op.h"
#include "paths/paths.h"
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
  spn_target_unit_t* target;
  spn_dag_id_t object;
  spn_dag_id_t header;
} spn_dag_embed_ctx_t;

typedef struct {
  spn_user_node_t* node;
  bool stamp;
} spn_dag_user_ctx_t;

static spn_path_t dag_artifact_path(spn_dag_t* g, spn_dag_id_t id) {
  return spn_dag_find_artifact(g, id)->materialized;
}

static sp_str_t dag_artifact_str(spn_dag_t* g, sp_mem_t mem, spn_dag_id_t id) {
  return spn_path_str(g->roots, mem, dag_artifact_path(g, id));
}

static spn_path_t dag_artifact_declared(spn_dag_t* g, spn_dag_id_t id) {
  return spn_dag_find_artifact(g, id)->path;
}

static sp_da(spn_path_t) dag_declared_paths(sp_mem_t mem, spn_dag_t* g, sp_da(spn_dag_id_t) ids) {
  sp_da(spn_path_t) paths = sp_da_new(mem, spn_path_t);
  sp_da_reserve(paths, sp_da_size(ids));
  sp_da_for(ids, it) {
    sp_da_push(paths, dag_artifact_declared(g, ids[it]));
  }
  return paths;
}

static spn_dag_digest_t hash_embedding(spn_target_unit_t* target) {
  spn_digest_ctx_t ctx = sp_zero;
  spn_digest_init_blake3(&ctx);
  spn_dag_hash_str(&ctx, sp_str_lit("spn.build.embed.v7"));
  spn_dag_hash_str(&ctx, target->pkg->info->qualified);
  spn_dag_hash_str(&ctx, target->info->name);
  spn_dag_hash_u8(&ctx, (u8)target->info->kind);
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
        spn_dag_hash_path(&ctx, embed->file.path);
        break;
      }
      case SPN_EMBED_DIR: {
        spn_dag_hash_path(&ctx, embed->dir.path);
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
static spn_path_t dag_dep_path(sp_mem_t mem, spn_path_t object) {
  return spn_path_suffix(mem, object, sp_str_lit(".d"));
}

static void observe_prereq(spn_dag_t* g, spn_compile_unit_t* unit, spn_dag_env_t* env, sp_mem_t mem, sp_da(spn_dag_obs_t)* obs, sp_str_t prereq) {
  sp_str_t path = prereq;
  if (!sp_fs_is_absolute(path)) {
    path = spn_path_str(g->roots, mem, spn_path_join(mem, unit->target->pkg->paths.work, path));
  }
  sp_str_t canonical = spn_dag_file_cache_canonical(env->files, path);
  if (sp_str_empty(canonical)) {
    canonical = spn_dag_wasi_canonicalize(mem, path);
  }
  sp_assert(!sp_str_empty(canonical));
  sp_da_push(*obs, ((spn_dag_obs_t) {
    .kind = SPN_DAG_OBS_FILE,
    .path = spn_path_make(g->roots, canonical),
  }));
}

static spn_err_t compile_object(spn_dag_t* g, spn_dag_action_t* action, void* user_data, spn_dag_env_t* env, sp_mem_t mem, sp_da(spn_dag_obs_t)* obs) {
  spn_compile_unit_t* unit = (spn_compile_unit_t*)user_data;
  const spn_cc_toolchain_t* toolchain = &unit->target->pkg->build->toolchain->cc;

  spn_path_t object = dag_artifact_path(g, action->produces[0]);
  spn_cc_depfile_t mode = spn_cc_depfile(toolchain, unit->lang);
  if (mode == SPN_CC_DEPFILE_NONE) {
    return spn_compile_object_run(unit, object, (spn_path_t) sp_zero) ? SPN_ERR_DAG_ACTION : SPN_OK;
  }

  spn_path_t depfile = dag_dep_path(mem, object);
  if (spn_compile_object_run(unit, object, depfile)) {
    return SPN_ERR_DAG_ACTION;
  }

  sp_str_t dep = spn_path_str(g->roots, mem, depfile);
  if (!sp_fs_exists(dep)) {
    return mode == SPN_CC_DEPFILE_REQUIRED ? SPN_ERR_DAG_DEPFILE : SPN_OK;
  }
  sp_str_t content = sp_zero;
  if (sp_io_read_file(mem, dep, &content)) {
    return SPN_ERR_DAG_DEPFILE;
  }
  sp_da(sp_str_t) prereqs = sp_zero;
  if (spn_cc_parse_depfile(mem, toolchain, content, &prereqs)) {
    return SPN_ERR_DAG_DEPFILE;
  }
  sp_da_for(prereqs, it) {
    observe_prereq(g, unit, env, mem, obs, prereqs[it]);
  }
  return SPN_OK;
}

static spn_err_t dag_link_exec(spn_dag_t* g, spn_dag_action_t* action, void* user_data, spn_dag_env_t* env, sp_mem_t mem, sp_da(spn_dag_obs_t)* obs) {
  spn_dag_link_ctx_t* link = (spn_dag_link_ctx_t*)user_data;
  spn_target_unit_t* target = link->target;

  sp_da(spn_path_t) objects = sp_da_new(mem, spn_path_t);
  sp_da_for(link->objects, it) {
    sp_da_push(objects, dag_artifact_path(g, link->objects[it]));
  }
  spn_path_t exports = link->exports.occupied ? dag_artifact_path(g, link->exports) : (spn_path_t) sp_zero;
  if (spn_link_target_run(target, dag_artifact_path(g, action->produces[0]), objects, exports)) {
    return SPN_ERR_DAG_ACTION;
  }
  return SPN_OK;
}

static spn_err_t dag_exports_exec(spn_dag_t* g, spn_dag_action_t* action, void* user_data, spn_dag_env_t* env, sp_mem_t mem, sp_da(spn_dag_obs_t)* obs) {
  spn_dag_link_ctx_t* link = (spn_dag_link_ctx_t*)user_data;

  sp_da(spn_path_t) objects = sp_da_new(mem, spn_path_t);
  sp_da_for(link->objects, it) {
    sp_da_push(objects, dag_artifact_path(g, link->objects[it]));
  }
  if (spn_link_exports_run(link->target, objects, dag_artifact_path(g, action->produces[0]))) {
    return SPN_ERR_DAG_ACTION;
  }
  return SPN_OK;
}

static spn_err_t generate_embedding(spn_dag_t* g, spn_dag_action_t* action, void* user_data, spn_dag_env_t* env, sp_mem_t mem, sp_da(spn_dag_obs_t)* obs) {
  spn_dag_embed_ctx_t* ctx = (spn_dag_embed_ctx_t*)user_data;
  spn_target_unit_t* target = ctx->target;

  if (spn_embed_write(target, dag_artifact_path(g, ctx->object), dag_artifact_path(g, ctx->header), mem, obs)) {
    return SPN_ERR_DAG_ACTION;
  }
  return SPN_OK;
}

static spn_err_t dag_user_exec(spn_dag_t* g, spn_dag_action_t* action, void* user_data, spn_dag_env_t* env, sp_mem_t mem, sp_da(spn_dag_obs_t)* obs) {
  spn_dag_user_ctx_t* ctx = (spn_dag_user_ctx_t*)user_data;
  spn_user_node_t* node = ctx->node;
  spn_pkg_unit_t* pkg = node->pkg;

  spn_pkg_unit_announce_compile(pkg);

  spn_event_buffer_push(spn.events, (spn_event_t) {
    .kind = SPN_EVENT_SCRIPT_USER_FN,
    .pkg = pkg->info->name,
    .script_user_fn = { .tag = node->tag }
  });

  if (!sp_str_empty(node->fn)) {
    if (spn_wasm_call_export_ex(pkg, node->fn, SPN_ABI_KIND_NONE, SP_NULLPTR, (spn_wasm_obs_t) { .mem = mem, .out = obs })) {
      return SPN_ERR_DAG_ACTION;
    }
  }

  sp_da_for(action->produces, it) {
    spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, action->produces[it]);
    sp_str_t target = spn_path_str(g->roots, spn.mem, artifact->materialized);
    if (ctx->stamp) {
      sp_fs_create_file(target);
      continue;
    }
    sp_str_t declared = spn_path_str(g->roots, spn.mem, artifact->path);
    if (!sp_fs_exists(declared)) {
      spn_event_buffer_push(spn.events, (spn_event_t) {
        .kind = SPN_EVENT_NODE_FAILED,
        .pkg = pkg->info->name,
        .node_failed = {
          .path = declared,
          .message = sp_fmt(spn.mem, "was declared as an output of node {} but was not produced", sp_fmt_str(node->tag)).value,
        },
      });
      return SPN_ERR_DAG_ACTION;
    }
    if (sp_fs_copy(declared, target)) {
      spn_event_buffer_push(spn.events, (spn_event_t) {
        .kind = SPN_EVENT_NODE_FAILED,
        .pkg = pkg->info->name,
        .node_failed = {
          .path = declared,
          .message = sp_fmt(spn.mem, "output of node {} could not be copied into the build", sp_fmt_str(node->tag)).value,
        },
      });
      return SPN_ERR_DAG_ACTION;
    }
  }

  return SPN_OK;
}

typedef enum {
  PUBLISH_COPY_OK,
  PUBLISH_COPY_ABSENT,
  PUBLISH_COPY_FAILED,
} publish_copy_result_t;

static publish_copy_result_t publish_copy(spn_pkg_unit_t* unit, sp_str_t root, spn_publish_copy_t* copy, sp_str_t rest, sp_da(spn_dag_obs_t)* obs) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  publish_copy_result_t result = PUBLISH_COPY_OK;

  sp_str_pair_t from = sp_str_cleave_c8(copy->from, '/');
  spn_path_t from_root = spn_api_dir_path(unit, spn_cache_dir_kind_from_str(from.first));
  sp_str_t dest = sp_fs_join_path(scratch.mem, root, rest);

  sp_da(spn_path_t) matches = sp_da_new(scratch.mem, spn_path_t);
  if (spn_dag_glob(spn.mem, &spn.roots, spn_path_join(spn.mem, from_root, from.second), obs, &matches)) {
    result = PUBLISH_COPY_FAILED;
  }
  else if (sp_fs_is_glob(copy->from)) {
    sp_fs_create_dir(dest);
    sp_da_for(matches, mt) {
      sp_str_t to = sp_fs_join_path(scratch.mem, dest, sp_fs_get_name(matches[mt].sub));
      if (spn_fs_update_file(spn_path_str(&spn.roots, scratch.mem, matches[mt]), to)) {
        result = PUBLISH_COPY_FAILED;
        break;
      }
    }
  }
  else if (sp_da_empty(matches)) {
    result = PUBLISH_COPY_ABSENT;
  }
  else {
    sp_fs_create_dir(sp_fs_parent_path(dest));
    result = spn_fs_update_file(spn_path_str(&spn.roots, scratch.mem, matches[0]), dest) ? PUBLISH_COPY_FAILED : PUBLISH_COPY_OK;
  }

  sp_mem_end_scratch(scratch);
  return result;
}

static spn_err_t publish_copy_failed(spn_pkg_unit_t* unit, spn_publish_copy_t* copy) {
  spn_event_buffer_push(spn.events, (spn_event_t) {
    .kind = SPN_EVENT_NODE_FAILED,
    .pkg = unit->info->name,
    .node_failed = {
      .path = copy->from,
      .message = sp_fmt(spn.mem, "could not be published to {}", sp_fmt_str(copy->to)).value,
    },
  });
  return SPN_ERROR;
}

spn_err_t spn_build_publish_copies(spn_pkg_unit_t* unit, sp_str_t root, sp_da(spn_dag_obs_t)* obs) {
  sp_da_for(unit->info->publish.copy, it) {
    spn_publish_copy_t* copy = &unit->info->publish.copy[it];
    sp_str_t rest = sp_zero;
    if (!spn_build_copy_to_include(copy, &rest)) {
      continue;
    }
    if (publish_copy(unit, root, copy, rest, obs) != PUBLISH_COPY_OK) {
      return publish_copy_failed(unit, copy);
    }
  }
  return SPN_OK;
}

spn_err_t spn_build_publish_existing_copies(spn_pkg_unit_t* unit, sp_str_t root) {
  sp_da_for(unit->info->publish.copy, it) {
    spn_publish_copy_t* copy = &unit->info->publish.copy[it];
    sp_str_t rest = sp_zero;
    if (!spn_build_copy_to_include(copy, &rest)) {
      continue;
    }
    if (publish_copy(unit, root, copy, rest, SP_NULLPTR) == PUBLISH_COPY_FAILED) {
      return publish_copy_failed(unit, copy);
    }
  }
  return SPN_OK;
}

static s32 dag_tree_copy_user_outputs(spn_pkg_unit_t* unit, sp_str_t root) {
  sp_da_for(unit->user_nodes, it) {
    spn_user_node_t* node = &unit->user_nodes[it];
    sp_da_for(node->outputs, ot) {
      spn_path_t path = node->outputs[ot];
      spn_path_rel_t rel = spn_path_within(unit->paths.include, path);
      if (!rel.within) {
        continue;
      }
      sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
      sp_str_t to = sp_fs_join_path(scratch.mem, root, rel.sub);
      sp_fs_create_dir(sp_fs_parent_path(to));
      s32 err = spn_fs_update_file(spn_path_str(&spn.roots, scratch.mem, path), to);
      sp_mem_end_scratch(scratch);
      if (err) {
        spn_event_buffer_push(spn.events, (spn_event_t) {
          .kind = SPN_EVENT_NODE_FAILED,
          .pkg = unit->info->name,
          .node_failed = {
            .path = spn_path_str(&spn.roots, spn.mem, path),
            .message = sp_fmt(spn.mem, "output of node {} could not be published to the package store", sp_fmt_str(node->tag)).value,
          },
        });
        return 1;
      }
    }
  }
  return 0;
}

static spn_err_t dag_tree_exec(spn_dag_t* g, spn_dag_action_t* action, void* user_data, spn_dag_env_t* env, sp_mem_t mem, sp_da(spn_dag_obs_t)* obs) {
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)user_data;

  sp_str_t root = dag_artifact_str(g, spn.mem, action->produces[0]);

  if (spn_pkg_unit_publish_headers(unit, root)) {
    return SPN_ERR_DAG_ACTION;
  }

  if (spn_build_publish_copies(unit, root, obs)) {
    return SPN_ERR_DAG_ACTION;
  }
  if (dag_tree_copy_user_outputs(unit, root)) {
    return SPN_ERR_DAG_ACTION;
  }

  return SPN_OK;
}

static spn_err_t dag_package_exec(spn_dag_t* g, spn_dag_action_t* action, void* user_data, spn_dag_env_t* env, sp_mem_t mem, sp_da(spn_dag_obs_t)* obs) {
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)user_data;

  spn_pkg_unit_create_layout(unit);

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
      spn_event_buffer_push(spn.events, (spn_event_t) {
        .kind = SPN_EVENT_NODE_FAILED,
        .pkg = unit->info->name,
        .node_failed = {
          .path = copy->from,
          .message = sp_fmt(spn.mem, "could not be published to {}", sp_fmt_str(copy->to)).value,
        },
      });
      return SPN_ERR_DAG_ACTION;
    }
  }

  spn_wasm_script_t* script = SP_NULLPTR;
  if (spn_wasm_find_export(unit, sp_str_lit("package"), &script)) {
    return SPN_ERR_DAG_ACTION;
  }
  if (script) {
    spn_event_buffer_push(spn.events, (spn_event_t) {
      .kind = SPN_EVENT_SCRIPT_PACKAGE,
      .pkg = unit->info->name,
    });

    sp_tm_timer_t timer = sp_tm_start_timer();
    if (spn_wasm_script_call_ex(script, unit, sp_str_lit("package"), SPN_ABI_KIND_NONE, SP_NULLPTR, (spn_wasm_obs_t) { .mem = mem, .out = obs })) {
      return SPN_ERR_DAG_ACTION;
    }
    unit->time.package = sp_tm_read_timer(&timer);

    spn_event_buffer_push(spn.events, (spn_event_t) {
      .kind = SPN_EVENT_PACKAGE_OK,
      .pkg = unit->info->name,
      .package_ok = {
        .time = unit->time.package
      }
    });
  }

  spn_dag_artifact_t* stamp = spn_dag_find_artifact(g, action->produces[0]);
  spn_pkg_unit_write_stamp(unit, stamp->materialized);

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
      .kind = SPN_DAG_ACTION_DISCOVERED,
      .identity = spn_build_user_identity(node, &pin),
      .execute = dag_user_exec,
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

  sp_da_for(target->objects, it) {
    spn_compile_unit_t* unit = target->objects[it];
    bool exists = sp_ht_getp(b->ids.objects, unit);
    sp_assert(!exists);

    spn_dag_action_config_t config = {
      .kind = SPN_DAG_ACTION_DISCOVERED,
      .identity = spn_build_compile_identity(unit),
      .execute = compile_object,
      .user_data = unit,
    };

    spn_dag_object_ids_t ids = sp_zero;
    ids.action = spn_dag_add_action(g, config);
    spn_dag_action_add_input(g, ids.action, spn_dag_add_file(g, unit->paths.file));

    ids.object = spn_dag_add_file(g, unit->paths.object);
    spn_try(spn_dag_action_add_output(g, ids.action, ids.object));

    sp_ht_insert(b->ids.objects, unit, ids);
  }

  return SPN_OK;
}

static spn_path_t embed_artifact_path(sp_mem_t mem, spn_target_unit_t* unit, const c8* extension) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);
  sp_str_t name = sp_fmt(s.mem, "{}.embed.{}", sp_fmt_str(unit->info->name), sp_fmt_cstr(extension)).value;
  spn_path_t path = spn_path_join(mem, spn_target_unit_object_dir(s.mem, unit), name);
  sp_mem_end_scratch(s);
  return path;
}

static spn_err_t dag_add_exports(spn_dag_build_t* b, spn_dag_link_ctx_t* link) {
  spn_dag_t* g = b->graph;
  spn_target_unit_t* target = link->target;

  spn_path_t output = spn_target_exports_path(b->mem, target);
  spn_dag_digest_t identity = sp_zero;
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_err_t err = spn_build_exports_identity(s.mem, target, output, dag_declared_paths(s.mem, g, link->objects), &identity);
  sp_mem_end_scratch(s);
  spn_try(err);

  spn_dag_id_t action = spn_dag_add_action(g, (spn_dag_action_config_t) {
    .identity = identity,
    .execute = dag_exports_exec,
    .user_data = link,
  });
  link->exports = spn_dag_add_file(g, output);
  spn_try(spn_dag_action_add_output(g, action, link->exports));

  sp_da_for(link->objects, it) {
    spn_dag_action_add_input(g, action, link->objects[it]);
  }
  sp_da_for(target->link.archives, it) {
    spn_dag_action_add_input(g, action, spn_dag_add_file(g, target->link.archives[it]));
  }
  return SPN_OK;
}

spn_err_t spn_dag_build_add_target(spn_dag_build_t* b, spn_target_unit_t* target) {
  spn_dag_t* g = b->graph;

  switch (target->lib_kind) {
    case SPN_LIB_KIND_SOURCE: {
      return SPN_OK;
    }
    case SPN_LIB_KIND_OBJECT: {
      spn_try(add_object_compilation(b, target));
      return SPN_OK;
    }
    case SPN_LIB_KIND_STATIC:
    case SPN_LIB_KIND_SHARED:
    case SPN_LIB_KIND_NONE: {
      break;
    }
  }

  bool exists = sp_ht_getp(b->ids.targets, target);
  sp_assert(!exists);

  spn_try(add_object_compilation(b, target));

  if (sp_da_empty(target->objects)) {
    return SPN_OK;
  }

  spn_dag_target_ids_t ids = sp_zero;

  if (!sp_da_empty(target->info->embed)) {
    spn_dag_embed_ctx_t* embed = sp_alloc_type(b->mem, spn_dag_embed_ctx_t);
    embed->target = target;

    ids.embed.action = spn_dag_add_action(g, (spn_dag_action_config_t) {
      .kind = SPN_DAG_ACTION_DISCOVERED,
      .identity = hash_embedding(target),
      .execute = generate_embedding,
      .user_data = embed,
    });
    ids.embed.object = spn_dag_add_file(g, embed_artifact_path(b->mem, target, "o"));
    ids.embed.header = spn_dag_add_file(g, embed_artifact_path(b->mem, target, "h"));
    embed->object = ids.embed.object;
    embed->header = ids.embed.header;
    spn_try(spn_dag_action_add_output(g, ids.embed.action, ids.embed.object));
    spn_try(spn_dag_action_add_output(g, ids.embed.action, ids.embed.header));

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
    spn_try(dag_add_exports(b, link));
  }

  spn_path_t output = spn_target_output_path(b->mem, target);
  spn_path_t exports = link->exports.occupied ? dag_artifact_declared(g, link->exports) : (spn_path_t) sp_zero;

  spn_dag_digest_t identity = sp_zero;
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_err_t err = spn_build_link_identity(s.mem, target, output, dag_declared_paths(s.mem, g, link->objects), exports, &identity);
  sp_mem_end_scratch(s);
  spn_try(err);

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
  spn_try(spn_dag_action_add_output(g, ids.action, ids.output));

  sp_ht_insert(b->ids.targets, target, ids);
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
    if (spn_build_copy_to_include(&unit->info->publish.copy[it], SP_NULLPTR)) {
      return true;
    }
  }

  sp_da_for(unit->user_nodes, it) {
    spn_user_node_t* node = &unit->user_nodes[it];
    sp_da_for(node->outputs, ot) {
      if (spn_path_within(unit->paths.include, node->outputs[ot]).within) {
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

  spn_build_source_pin_t pin = spn_build_source_pin(unit);
  spn_dag_id_t action = spn_dag_add_action(g, (spn_dag_action_config_t) {
    .kind = SPN_DAG_ACTION_DISCOVERED,
    .identity = spn_build_tree_identity(unit, &pin),
    .execute = dag_tree_exec,
    .user_data = unit,
  });
  pkg->tree = spn_dag_add_tree(g, unit->paths.include);
  spn_try(spn_dag_action_add_output(g, action, pkg->tree));

  spn_pkg_unit_header_maps_t published = spn_pkg_unit_header_maps(unit);
  sp_for(mt, published.count) {
    sp_om_for(published.maps[mt], it) {
      spn_target_info_t* target = sp_str_om_at(published.maps[mt], it);
      sp_da_for(target->headers, ht) {
        spn_dag_action_add_input(g, action, spn_dag_add_file(g, target->headers[ht]));
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

  spn_build_source_pin_t pin = spn_build_source_pin(unit);
  pkg->action = spn_dag_add_action(g, (spn_dag_action_config_t) {
    .kind = SPN_DAG_ACTION_DISCOVERED,
    .identity = spn_build_package_identity(unit, &pin),
    .execute = dag_package_exec,
    .user_data = unit,
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
        if (!spn_path_within(dep->unit->paths.store, embed->dir.path).within) {
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

static spn_err_t dag_add_unit_targets(spn_dag_build_t* b, sp_da(spn_pkg_unit_t*) units) {
  sp_da_for(units, it) {
    sp_da_for(units[it]->targets, jt) {
      spn_try(spn_dag_build_add_target(b, units[it]->targets[jt]));
    }
  }
  return SPN_OK;
}

static void dag_add_unit_target_edges(spn_dag_build_t* b, sp_da(spn_pkg_unit_t*) units) {
  sp_da_for(units, it) {
    sp_da_for(units[it]->targets, jt) {
      dag_add_target_edges(b, units[it]->targets[jt]);
    }
  }
}

static spn_err_t prepare_graph(spn_dag_build_t* b) {
  spn_session_t* session = b->session;

  sp_om_for(session->units.builds, it) {
    spn_build_unit_t* build = sp_om_at(session->units.builds, it);
    spn_try(dag_add_unit_targets(b, build->packages));
  }

  sp_om_for(session->units.builds, it) {
    spn_build_unit_t* build = sp_om_at(session->units.builds, it);
    sp_da_for(build->packages, jt) {
      if (spn_pkg_unit_is_script_host(build->packages[jt])) {
        continue;
      }
      spn_try(dag_add_package(b, build->packages[jt]));
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
      spn_try(dag_add_edges(b, build->packages[jt]));
    }
  }

  return SPN_OK;
}

/////////
// RUN //
/////////
typedef sp_ht(spn_path_t, u8) dag_staged_t;

static void dag_stage_link(spn_dag_build_t* b, dag_staged_t* staged, spn_path_t from, spn_path_t to) {
  if (spn_path_empty(to) || sp_ht_getp(*staged, to)) {
    return;
  }
  sp_ht_insert(*staged, spn_path_copy(b->mem, to), (u8)true);

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_str_t source = spn_path_str(b->graph->roots, scratch.mem, from);
  sp_str_t target = spn_path_str(b->graph->roots, scratch.mem, to);

  sp_fs_create_dir(sp_fs_parent_path(target));
  if (sp_fs_exists(target)) {
    sp_fs_remove_file(target);
  }
  if (sp_fs_link(source, target, SP_FS_LINK_HARD) != SP_OK) {
    sp_fs_link(source, target, SP_FS_LINK_COPY);
  }
  sp_mem_end_scratch(scratch);

  spn_dag_file_cache_invalidate(&b->files, to);
}

static void dag_stage_dir(spn_dag_build_t* b, dag_staged_t* staged, spn_path_t from, spn_path_t to) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_da(sp_fs_entry_t) entries = sp_zero;
  sp_fs_collect(scratch.mem, spn_path_str(b->graph->roots, scratch.mem, from), &entries);
  sp_da_for(entries, it) {
    sp_str_t name = entries[it].name;
    spn_path_t source = spn_path_join(scratch.mem, from, name);
    spn_path_t target = spn_path_join(scratch.mem, to, name);
    if (entries[it].kind == SP_FS_KIND_DIR) {
      dag_stage_dir(b, staged, source, target);
    }
    else {
      dag_stage_link(b, staged, source, target);
    }
  }
  sp_mem_end_scratch(scratch);
}


static void dag_stage_file(spn_dag_build_t* b, dag_staged_t* staged, spn_dag_id_t artifact, spn_path_t to) {
  dag_stage_link(b, staged, spn_dag_find_artifact(b->graph, artifact)->materialized, to);
}

static void dag_stage_copy(spn_dag_build_t* b, dag_staged_t* staged, spn_dag_id_t id, spn_path_t to) {
  if (spn_path_empty(to) || sp_ht_getp(*staged, to)) {
    return;
  }
  sp_ht_insert(*staged, spn_path_copy(b->mem, to), (u8)true);

  spn_dag_artifact_t* artifact = spn_dag_find_artifact(b->graph, id);

  sp_sys_file_meta_t staged_meta = sp_zero;
  spn_dag_digest_t staged_digest = sp_zero;
  if (spn_dag_digest_valid(artifact->digest) &&
      !spn_dag_file_cache_stat(&b->files, to, &staged_meta) && staged_meta.nlink == 1 &&
      !spn_dag_file_cache_digest(&b->files, to, &staged_digest) &&
      spn_dag_digest_equal(staged_digest, artifact->digest)) {
    return;
  }

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_str_t source = spn_path_str(b->graph->roots, scratch.mem, artifact->materialized);
  sp_str_t target = spn_path_str(b->graph->roots, scratch.mem, to);

  sp_sys_file_meta_t meta = sp_zero;
  sp_mem_slice_t bytes = sp_zero;
  sp_fs_atomic_t af = sp_zero;
  if (!sp_sys_get_path_metadata_s(sp_sys_get_root(0), source, &meta) &&
      !sp_io_read_file_slice(scratch.mem, source, &bytes) &&
      !sp_fs_atomic_open(&af, target)) {
    if (sp_io_write_all(sp_fs_atomic_writer(&af), bytes.data, bytes.len, SP_NULLPTR)) {
      sp_fs_atomic_abort(&af);
    }
    else {
      sp_sys_chmod_s(af.dir, af.temp, &meta);
      sp_fs_atomic_commit(&af, SP_FS_ATOMIC_REPLACE);
    }
  }
  sp_mem_end_scratch(scratch);

  spn_dag_file_cache_invalidate(&b->files, to);
}

static void dag_stage_pkg_store(spn_dag_build_t* b, dag_staged_t* staged, spn_pkg_unit_t* unit, spn_path_t root) {
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

  dag_stage_dir(b, staged, unit->paths.store, root);
}

static void dag_stage(spn_dag_build_t* b) {
  spn_session_t* session = b->session;
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  dag_staged_t staged = SP_NULLPTR;
  sp_ht_init(b->mem, staged);
  sp_ht_set_fns(staged, spn_path_on_hash, spn_path_on_compare);

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

      spn_path_t staged_path = spn_target_unit_staged_path(scratch.mem, target);
      dag_stage_copy(b, &staged, output, staged_path);

      spn_path_t dir = { .root = staged_path.root, .sub = sp_fs_parent_path(staged_path.sub) };
      sp_da(spn_target_unit_t*) libs = spn_target_runtime_libs(scratch.mem, target);
      sp_da_for(libs, lt) {
        spn_target_unit_t* lib = libs[lt];
        spn_dag_target_ids_t* lib_ids = sp_ht_getp(b->ids.targets, lib);
        if (!lib_ids) {
          continue;
        }
        spn_dag_id_t lib_output = lib_ids->output;
        spn_path_t from = spn_target_output_path(scratch.mem, lib);
        dag_stage_file(b, &staged, lib_output, spn_path_join(scratch.mem, dir, sp_fs_get_name(from.sub)));
      }
    }
  }

  sp_da_for(session->plans, it) {
    spn_build_unit_t* build = session->plans[it].build;
    spn_path_t root = spn_path_join(scratch.mem, build->paths.root, sp_str_lit("store"));
    dag_stage_pkg_store(b, &staged, spn_session_find_pkg_unit(session, build, spn_session_root_pkg(session)), root);
    sp_da_for(build->packages, jt) {
      dag_stage_pkg_store(b, &staged, build->packages[jt], root);
    }
  }

  sp_mem_end_scratch(scratch);
}

static spn_err_t dag_result(spn_dag_build_t* b) {
  spn_dag_diag_t* diag = &b->env.diag;

  switch (b->result) {
    case SPN_OK: {
      return SPN_OK;
    }
    case SPN_ERR_DAG_CANCELLED:
    case SPN_ERR_DAG_ACTION: {
      return b->result;
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
      path = spn_path_empty(artifact->path)
        ? artifact->name
        : spn_path_str(b->graph->roots, b->mem, artifact->path);
    }
  }

  return spn_err_emit(b->session->ctx, (spn_err_union_t) {
    .kind = diag->err ? diag->err : b->result,
    .dag = { .path = path },
  });
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
    spn_profile_info_t* profile = &build->profile;

    if (failed) {
      spn_event_buffer_push(session->ctx->events, (spn_event_t) {
        .kind = SPN_EVENT_BUILD_FAILED,
        .pkg = pkg->name,
        .build_failed = {
          .profile = profile->name,
          .time = elapsed,
        },
      });
    }
    else {
      spn_event_buffer_push(session->ctx->events, (spn_event_t) {
        .kind = SPN_EVENT_BUILD_PASSED,
        .pkg = pkg->name,
        .build_passed = {
          .profile = profile->name,
          .time = elapsed,
          .hits = hits,
          .misses = misses,
        },
      });
    }

    spn_event_buffer_push(session->ctx->events, (spn_event_t) {
      .kind = SPN_EVENT_BUILD_SUMMARY,
      .pkg = pkg->name,
      .build_summary = {
        .success = !failed,
        .hits = hits,
        .misses = misses,
        .total = (u32)sp_da_size(b->graph->actions),
        .time = elapsed,
        .profile = profile->name,
        .hashed_files = sp_atomic_u32_load(&b->stats.hashed_files, SP_ATOMIC_SEQ_CST),
        .hashed_bytes = sp_atomic_u64_load(&b->stats.hashed_bytes, SP_ATOMIC_SEQ_CST),
        .stats = sp_atomic_u32_load(&b->stats.stats, SP_ATOMIC_SEQ_CST),
        .obs_rows = sp_atomic_u32_load(&b->stats.obs_rows, SP_ATOMIC_SEQ_CST),
        .cache_reads = sp_atomic_u32_load(&b->stats.cache_reads, SP_ATOMIC_SEQ_CST),
        .cache_writes = sp_atomic_u32_load(&b->stats.cache_writes, SP_ATOMIC_SEQ_CST),
      },
    });
  }
}

spn_dag_build_t* spn_dag_build_new(spn_op_t* op) {
  spn_session_t* session = op->session;
  spn_dag_build_t* b = sp_alloc_type(session->mem, spn_dag_build_t);
  sp_mem_zero(b, sizeof(spn_dag_build_t));
  b->session = session;
  b->mem = spn.mem;
  b->graph = spn_dag_new(spn.mem, &spn.roots);
  sp_ht_init(b->mem, b->ids.packages);
  sp_ht_init(b->mem, b->ids.targets);
  sp_ht_init(b->mem, b->ids.objects);

  spn_path_t root = spn_path_anchor(session->mem, &spn.roots, spn_path_join(session->mem, spn_path_from_root(SPN_PATH_ROOT_CACHE), sp_str_lit("dag")));
  spn_path_t tmp = spn_path_join(session->mem, root, sp_str_lit("tmp"));
  sp_str_t dir = spn_path_str(&spn.roots, session->mem, root);
  sp_fs_create_dir(dir);
  sp_fs_create_dir(spn_path_str(&spn.roots, session->mem, tmp));

  spn_dag_store_init(&b->store, (spn_dag_store_config_t) {
    .kind = SPN_DAG_STORE_FILESYSTEM,
    .mem = spn.mem,
    .roots = &spn.roots,
    .dir = spn_path_join(session->mem, root, sp_str_lit("store")),
  });
  spn_dag_file_cache_init(&b->files, spn.mem, &spn.roots);
  spn_dag_action_cache_init(&b->actions, spn.mem, sp_fs_join_path(session->mem, dir, sp_str_lit("strong")));
  spn_dag_obs_table_init(&b->discovery, spn.mem, sp_fs_join_path(session->mem, dir, sp_str_lit("weak")));
  b->files.stats = &b->stats;
  b->actions.stats = &b->stats;
  b->discovery.stats = &b->stats;
  b->store.stats = &b->stats;
  b->files_path = sp_fs_join_path(session->mem, dir, sp_str_lit("files"));
  spn_dag_file_cache_load(&b->files, b->files_path);

  b->env = (spn_dag_env_t) {
    .files = &b->files,
    .cache = &b->actions,
    .store = &b->store,
    .discovery = &b->discovery,
    .stats = &b->stats,
    .progress = &b->progress,
    .wake = &op->ctx->wake,
    .cancel = &op->cancelled,
    .scratch = tmp,
  };

  return b;
}

spn_err_t spn_dag_build_run(spn_dag_build_t* b, u32 workers) {
  spn_thread_pool_init(&b->pool, spn.mem, (spn_thread_pool_config_t) {
    .workers = sp_min(workers, (u32)sp_da_size(b->graph->actions)),
    .on_worker_exit = spn_wasm_thread_exit,
  });

  b->timer = sp_tm_start_timer();
  b->result = spn_dag_run_executor(b->graph, &b->env, &b->pool.executor);
  spn_thread_pool_deinit(&b->pool);
  spn_dag_file_cache_flush(&b->files, b->files_path);
  return dag_result(b);
}

spn_err_t spn_dag_build_session(spn_op_t* op) {
  spn_session_t* session = op->session;
  spn_project_t* project = session->project;

  spn_dag_build_t* b = spn_dag_build_new(op);
  session->dag.build = b;

  spn_try(prepare_graph(b));

  spn_triple_t target = { session->profile.arch, session->profile.os, session->profile.abi };
  spn_event_buffer_push(spn.events, (spn_event_t) {
    .kind = SPN_EVENT_INIT_BUILD_GRAPH,
    .pkg = session->pkg->name,
    .graph_init = {
      .profile = session->profile.name,
      .target = spn_triple_to_str(session->mem, target),
      .toolchain = session->units.target->toolchain->info->name,
      .version = session->units.target->toolchain->version,
      .force = session->config.force,
    }
  });

  sp_atomic_ptr_store(&session->ctx->progress, &b->progress, SP_ATOMIC_SEQ_CST);
  spn_err_t result = spn_dag_build_run(b, spn_cpu_count());
  sp_atomic_ptr_store(&session->ctx->progress, SP_NULLPTR, SP_ATOMIC_SEQ_CST);
  u64 elapsed = sp_tm_read_timer(&b->timer);

  if (b->result == SPN_ERR_DAG_CANCELLED) {
    return result;
  }

  if (!result) {
    if (!project->lock.some) {
      spn_try(spn_project_update_lock(session->ctx, project, session->resolve));
    }
    dag_stage(b);
    spn_dag_file_cache_flush(&b->files, b->files_path);
  }

  dag_emit_reports(b, elapsed);

  return result;
}
