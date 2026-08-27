#include "spn/host.h"

#include "codegen/gen/config.gen.h"
#include "codegen/lower.h"
#include "ctx/ctx.h"
#include "error/error.h"
#include "event/event.h"
#include "external/wasm/wasm.h"
#include "git/cache.h"
#include "index/index.h"
#include "intern/intern.h"
#include "log/lazy/lazy.h"
#include "op/op.h"
#include "paths/paths.h"
#include "project/project.h"
#include "project/types.h"
#include "session/session.h"
#include "sp/fs.h"
#include "sp/os.h"
#include "sp/sp_glob.h"
#include "spn.embed.h"
#include "toml/issue.h"
#include "toml/loader.h"
#include "toolchain/catalog.h"
#include "toolchain/probe.h"
#include "toolchain/provision.h"
#include "triple/triple.h"
#include "version.h"

static sp_str_t join_path(spn_ctx_t* ctx, sp_str_t base, const c8* dir) {
  return sp_fs_join_path(ctx->heap, base, sp_cstr_as_str(dir));
}

static sp_str_t env_or(spn_ctx_t* ctx, const c8* env, sp_str_t fallback) {
  sp_str_t path = sp_env_get(ctx->env, sp_cstr_as_str(env));
  return sp_str_empty(path) ? fallback : path;
}

static bool write_file(sp_str_t path, const void* data, u64 size) {
  sp_io_file_writer_t io = sp_zero;
  if (sp_io_file_writer_from_path(&io, path) != SP_OK) {
    return false;
  }
  sp_err_t written = sp_io_write(&io.base, data, size, SP_NULLPTR);
  sp_err_t closed = sp_io_file_writer_close(&io);
  return written == SP_OK && closed == SP_OK;
}

static sp_str_t read_stamp(sp_mem_t mem, spn_ctx_t* ctx) {
  sp_str_t version = sp_zero;
  if (sp_fs_exists(ctx->paths.version)) {
    sp_io_read_file(mem, ctx->paths.version, &version);
    version = sp_str_trim(version);
  }
  return version;
}

static spn_err_t extract(spn_ctx_t* ctx, sp_mem_t mem, sp_str_t stamp) {
  sp_str_t staging = sp_zero;
  if (sp_fs_staging_dir(mem, ctx->paths.runtime, sp_str_lit("tmp"), &staging) != SP_OK) {
    return spn_err_emit(ctx, (spn_err_union_t) { .kind = SPN_ERR_FS_WRITE, .fs = { .path = ctx->paths.runtime } });
  }

  sp_glob_set_t* glob = sp_glob_set_new(mem);
  sp_glob_set_add(glob, "include/*");
  sp_glob_set_build(glob);

  sp_carr_for(spn_embed_manifest, it) {
    spn_embed_entry_t entry = spn_embed_manifest[it];
    sp_str_t rel = sp_cstr_as_str(entry.path);
    if (!sp_glob_set_match(glob, rel)) {
      continue;
    }
    sp_str_t path = sp_fs_join_path(mem, staging, rel);
    sp_fs_create_dir(sp_fs_parent_path(path));
    if (!write_file(path, entry.data, entry.size)) {
      sp_fs_remove_dir(staging);
      return spn_err_emit(ctx, (spn_err_union_t) { .kind = SPN_ERR_FS_WRITE, .fs = { .path = sp_str_copy(ctx->heap, path) } });
    }
  }

  sp_str_t stamp_path = sp_fs_join_path(mem, staging, sp_str_lit("version.stamp"));
  if (!write_file(stamp_path, stamp.data, stamp.len)) {
    sp_fs_remove_dir(staging);
    return spn_err_emit(ctx, (spn_err_union_t) { .kind = SPN_ERR_FS_WRITE, .fs = { .path = sp_str_copy(ctx->heap, stamp_path) } });
  }

  sp_fs_remove_dir(ctx->paths.runtime);
  sp_sys_fd_t root = sp_sys_get_root(0);
  if (sp_sys_rename_s(root, staging, root, ctx->paths.runtime)) {
    sp_fs_remove_dir(staging);
    return spn_err_emit(ctx, (spn_err_union_t) { .kind = SPN_ERR_FS_WRITE, .fs = { .path = ctx->paths.runtime } });
  }

  return SPN_OK;
}

static spn_err_t extract_runtime(spn_ctx_t* ctx) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_err_t result = SPN_OK;

  // @spader Use SHA256 for this
  // The stamp must change whenever the embedded runtime does, not just on
  // release; otherwise dev builds compile scripts against a stale extraction
  sp_hash_t runtime_hash = 0;
  sp_carr_for(spn_embed_manifest, it) {
    spn_embed_entry_t entry = spn_embed_manifest[it];
    sp_hash_t hashes [] = {
      runtime_hash,
      sp_hash_cstr(entry.path),
      sp_hash_bytes(entry.data, entry.size, 0),
    };
    runtime_hash = sp_hash_combine(hashes, sp_carr_len(hashes));
  }
  sp_str_t stamp = sp_fmt(scratch.mem, "{}:{}", sp_fmt_cstr(SPN_VERSION), sp_fmt_uint(runtime_hash)).value;

  if (!sp_str_equal(read_stamp(scratch.mem, ctx), stamp)) {
    sp_fs_lock_t lock = sp_zero;
    sp_str_t lock_path = sp_fs_join_path(scratch.mem, ctx->paths.storage, sp_str_lit("runtime.lock"));
    if (sp_fs_lock_acquire(&lock, lock_path) != SP_OK) {
      result = spn_err_emit(ctx, (spn_err_union_t) { .kind = SPN_ERR_FS_WRITE, .fs = { .path = sp_str_copy(ctx->heap, lock_path) } });
    }
    else {
      if (!sp_str_equal(read_stamp(scratch.mem, ctx), stamp)) {
        result = extract(ctx, scratch.mem, stamp);
      }
      sp_fs_lock_release(&lock);
    }
  }

  sp_mem_end_scratch(scratch);
  return result;
}

static spn_err_t open_ctx(spn_ctx_t* ctx, spn_open_request_t request) {
  sp_assert(!ctx->config.indexes);

  if (sp_str_valid(request.dir)) {
    ctx->paths.project = sp_fs_canonicalize_path(ctx->heap, request.dir);
  }
  else {
    ctx->paths.project = ctx->paths.cwd;
  }
  ctx->paths.project = spn_path_roots_set(&ctx->roots, ctx->heap, SPN_PATH_ROOT_PROJECT, ctx->paths.project);

  // Make sure any per-machine directories we need exist
  sp_str_t dirs [] = {
    ctx->paths.caches.dir,
    ctx->paths.caches.git.dir,
    ctx->paths.caches.build.dir,
    ctx->paths.caches.store.dir,
    ctx->paths.index,
    ctx->paths.toolchain,
  };
  sp_carr_for(dirs, it) {
    if (sp_fs_create_dir(dirs[it])) {
      return spn_err_emit(ctx, (spn_err_union_t) {
        .kind = SPN_ERR_FS_CREATE_DIR,
        .fs = { .path = dirs[it] }
      });
    }
  }

  spn_git_cache_init(&ctx->caches.git, ctx->mem, ctx->intern, ctx->paths.caches.git.dir);

  ctx->caches.toolchains = (spn_toolchain_store_t) {
    .mem = ctx->mem,
    .dir = ctx->paths.toolchain,
    .mirror = sp_env_get(ctx->env, sp_str_lit("SPN_MIRROR")),
    .fetch = spn_fetch_curl,
  };
  spn_probe_cache_load(&ctx->caches.toolchains.probes, join_path(ctx, ctx->paths.toolchain, "probe.cache"), ctx->mem);

  spn_try(extract_runtime(ctx));

  spn_event_buffer_push(ctx->events, (spn_event_t) {
    .kind = SPN_EVENT_OPEN,
    .open = {
      .version = sp_cstr_as_str(SPN_VERSION),
      .project = ctx->paths.project,
    },
  });

  // Load the per-machine config file
  ctx->config.indexes = sp_da_new(ctx->heap, spn_index_info_t);
  if (sp_fs_exists(ctx->paths.config.toml)) {
    spn_cg_config_t config = sp_zero;
    spn_toml_loader_t loader = sp_zero;
    spn_toml_loader_init(&loader, ctx->mem, ctx->intern);
    spn_err_t loaded = spn_codegen_load_config(&loader, ctx->paths.config.toml, &config);
    sp_da(spn_index_info_t) indexes = sp_da_new(ctx->heap, spn_index_info_t);
    if (!loaded) {
      sp_da_for(config.index, it) {
        sp_da_push(indexes, spn_index_lower(&loader, it, SPN_INDEX_KIND_USER, &config.index[it]));
      }
    }
    if (loaded || !sp_da_empty(loader.issues)) {
      return spn_err_emit(ctx, (spn_err_union_t) {
        .kind = SPN_ERR_MANIFEST_ISSUES,
        .manifest = { .path = ctx->paths.config.toml, .issues = spn_codegen_issues_to_err(ctx->mem, loader.issues) },
      });
    }
    ctx->config.indexes = indexes;
  }

  spn_try(spn_project_load(ctx, ctx->paths.project, &ctx->project));
  if (!request.project_optional) {
    spn_try(spn_ctx_require_project(ctx));
  }

  if (ctx->project) {
    sp_str_om_for(ctx->project->package.toolchains, it) {
      spn_toolchain_catalog_add(&ctx->catalog, *sp_str_om_at(ctx->project->package.toolchains, it));
    }
  }

  spn_index_assemble(ctx->heap, ctx->project ? &ctx->project->package.indexes : SP_NULLPTR, ctx->config.indexes, &ctx->indexes);

  sp_da_for(ctx->indexes, it) {
    spn_index_info_t* index = &ctx->indexes[it];
    index->location = spn_index_location(index, ctx->heap, ctx->paths.index);
    if (!index->refresh) {
      index->refresh = request.index_refresh_seconds ? request.index_refresh_seconds : SPN_INDEX_DEFAULT_REFRESH;
    }
  }

  return SPN_OK;
}

spn_ctx_t* spn_ctx_new(spn_wake_fn_t wake, void* wake_data) {
  sp_assert(!spn.arena);
  spn_ctx_t* ctx = &spn;
  *ctx = sp_zero_s(spn_ctx_t);
  ctx->wake.fn = wake;
  ctx->wake.data = wake_data;
  ctx->mem = sp_mem_os_new();
  ctx->arena = sp_mem_arena_new(ctx->mem);
  ctx->heap = sp_mem_arena_as_allocator(ctx->arena);
  ctx->intern = sp_intern_new(ctx->mem);
  ctx->env = sp_alloc_type(ctx->heap, sp_env_t);
  *ctx->env = sp_env_capture(ctx->heap);
  ctx->events = spn_event_buffer_new(ctx->mem);
  ctx->events->wake = &ctx->wake;

  ctx->host = spn_triple_host();

  sp_str_t builtins = sp_str((const c8*)toolchains_json, toolchains_json_size);
  sp_assert(spn_toolchain_catalog_init(&ctx->catalog, builtins, ctx->heap) == SPN_OK);

  ctx->paths.cwd = sp_fs_get_cwd(ctx->heap);
  ctx->paths.patches = sp_env_get(ctx->env, sp_str_lit("SPN_PATCH_DIR"));
  ctx->paths.config.dir = join_path(ctx, env_or(ctx, "SPN_CONFIG_DIR", sp_fs_get_config_path(ctx->heap)), "spn");
    ctx->paths.config.toml = sp_fs_join_path(ctx->heap, ctx->paths.config.dir, sp_str_lit("spn.toml"));
  ctx->paths.storage = spn_path_roots_init(&ctx->roots, ctx->heap, env_or(ctx, "SPN_STORAGE_DIR", join_path(ctx, sp_fs_get_storage_path(ctx->heap), "spn")));
    ctx->paths.caches.dir = join_path(ctx, ctx->paths.storage, "cache");
      ctx->paths.caches.git.dir = join_path(ctx, ctx->paths.caches.dir, "source");
        ctx->paths.caches.git.checkouts = join_path(ctx, ctx->paths.caches.git.dir, "checkouts");
      ctx->paths.caches.store.dir = join_path(ctx, ctx->paths.caches.dir, "store");
      ctx->paths.caches.build.dir = join_path(ctx, ctx->paths.caches.dir, "build");
    ctx->paths.index = join_path(ctx, ctx->paths.storage, "index");
    ctx->paths.runtime = join_path(ctx, ctx->paths.storage, "runtime");
      ctx->paths.version = join_path(ctx, ctx->paths.runtime, "version.stamp");

  spn_path_roots_set(&ctx->roots, ctx->heap, SPN_PATH_ROOT_CACHE, ctx->paths.caches.dir);
  spn_path_roots_set(&ctx->roots, ctx->heap, SPN_PATH_ROOT_STORE, ctx->paths.caches.store.dir);
  spn_path_roots_set(&ctx->roots, ctx->heap, SPN_PATH_ROOT_BUILD, ctx->paths.caches.build.dir);
  spn_path_roots_set(&ctx->roots, ctx->heap, SPN_PATH_ROOT_CHECKOUT, ctx->paths.caches.git.checkouts);
  spn_path_roots_set(&ctx->roots, ctx->heap, SPN_PATH_ROOT_INDEX, ctx->paths.index);
  spn_path_roots_set(&ctx->roots, ctx->heap, SPN_PATH_ROOT_RUNTIME, ctx->paths.runtime);
  ctx->paths.toolchain = spn_path_roots_set(&ctx->roots, ctx->heap, SPN_PATH_ROOT_TOOLCHAIN, env_or(ctx, "SPN_TOOLCHAIN_DIR", join_path(ctx, ctx->paths.caches.dir, "toolchain")));

  spn_op_thread_start(ctx);
  return ctx;
}

static spn_err_t open_session(spn_ctx_t* ctx, spn_session_config_t config) {
  spn_try(spn_ctx_require_project(ctx));

  ctx->session = sp_alloc_type(ctx->heap, spn_session_t);
  return spn_session_init(ctx->session, ctx, ctx->heap, ctx->project, config);
}

spn_err_t spn_ctx_open(spn_ctx_t* ctx, spn_open_request_t request) {
  return open_ctx(ctx, request);
}

spn_err_t spn_ctx_open_session(spn_ctx_t* ctx, const spn_session_config_t* config, spn_session_t** session) {
  spn_err_t err = open_session(ctx, *config);
  *session = err ? SP_NULLPTR : ctx->session;
  return err;
}

void spn_ctx_close(spn_ctx_t* ctx, bool ok) {
  spn_op_thread_stop(ctx);

  spn_err_t result = SPN_OK;
  if (!ok) {
    result = (spn_err_t)sp_atomic_s32_load(&ctx->error, SP_ATOMIC_SEQ_CST);
    if (!result) {
      result = SPN_ERROR;
    }
  }

  spn_event_buffer_push(ctx->events, (spn_event_t) {
    .kind = SPN_EVENT_RESULT,
    .result = {
      .ok = result == SPN_OK,
      .err = result,
    },
  });

  if (ctx->session) {
    sp_om_for(ctx->session->units.packages, it) {
      spn_pkg_unit_t* unit = sp_om_at(ctx->session->units.packages, it);
      spn_wasm_script_close(&unit->wasm.configure);
      spn_wasm_script_close(&unit->wasm.build);
    }
  }
  ctx->session = SP_NULLPTR;

  spn_lazy_log_close(&ctx->events->log);
}
