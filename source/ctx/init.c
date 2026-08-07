#include "spn/host.h"

#include "codegen/gen/config.gen.h"
#include "codegen/lower.h"
#include "ctx/ctx.h"
#include "error/error.h"
#include "event/event.h"
#include "event/log.h"
#include "git/cache.h"
#include "index/index.h"
#include "intern/intern.h"
#include "project/project.h"
#include "project/types.h"
#include "session/session.h"
#include "sp/os.h"
#include "sp/sp_glob.h"
#include "spn.embed.h"
#include "toml/loader.h"
#include "toolchain/provision.h"
#include "version.h"

static sp_str_t join_path(spn_ctx_t* ctx, sp_str_t base, const c8* dir) {
  return sp_fs_join_path(ctx->heap, base, sp_cstr_as_str(dir));
}

static sp_str_t env_or(spn_ctx_t* ctx, const c8* env, sp_str_t fallback) {
  sp_str_t path = sp_env_get(ctx->env, sp_cstr_as_str(env));
  return sp_str_empty(path) ? fallback : path;
}

static void extract_runtime(spn_ctx_t* ctx) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  sp_str_t version = sp_zero;
  if (sp_fs_exists(ctx->paths.version)) {
    sp_io_read_file(scratch.mem, ctx->paths.version, &version);
    version = sp_str_trim(version);
  }

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

  if (!sp_str_equal(version, stamp)) {
    sp_fs_remove_dir(ctx->paths.runtime);
    sp_fs_create_dir(ctx->paths.runtime);

    sp_glob_set_t* glob = sp_glob_set_new(scratch.mem);
    sp_glob_set_add(glob, "include/*");
    sp_glob_set_build(glob);

    sp_carr_for(spn_embed_manifest, it) {
      spn_embed_entry_t entry = spn_embed_manifest[it];
      sp_str_t path = sp_cstr_as_str(entry.path);
      if (sp_glob_set_match(glob, path)) {
        path = sp_fs_join_path(scratch.mem, ctx->paths.runtime, path);
        sp_fs_create_dir(sp_fs_parent_path(path));
        sp_io_file_writer_t io = sp_zero;
        sp_io_file_writer_from_path(&io, path);
        sp_io_write(&io.base, entry.data, entry.size, SP_NULLPTR);
        sp_io_file_writer_close(&io);
      }
    }

    {
      sp_io_file_writer_t io = sp_zero;
      sp_io_file_writer_from_path(&io, ctx->paths.version);
      sp_io_write_str(&io.base, stamp, SP_NULLPTR);
      sp_io_file_writer_close(&io);
    }
  }

  sp_mem_end_scratch(scratch);
}

static void init(spn_ctx_t* ctx) {
  *ctx = sp_zero_s(spn_ctx_t);
  ctx->mem = sp_mem_os_new();
  ctx->arena = sp_mem_arena_new(ctx->mem);
  ctx->heap = sp_mem_arena_as_allocator(ctx->arena);
  ctx->intern = sp_intern_new(ctx->mem);
  ctx->env = sp_alloc_type(ctx->heap, sp_env_t);
  *ctx->env = sp_env_capture(ctx->heap);
  ctx->events = spn_event_buffer_new(ctx->mem);
  spn_event_log_init(ctx->heap);
}

spn_ctx_t* spn_ctx_new() {
  sp_assert(!spn.arena);
  init(&spn);
  return &spn;
}

static spn_err_union_t mount(spn_ctx_t* ctx) {
  ctx->paths.cwd = sp_fs_get_cwd(ctx->heap);
  ctx->paths.bin = sp_fs_get_bin_path(ctx->heap);
  ctx->paths.patches = sp_env_get(ctx->env, sp_str_lit("SPN_PATCH_DIR"));
  ctx->paths.config.dir = join_path(ctx, env_or(ctx, "SPN_CONFIG_DIR", sp_fs_get_config_path(ctx->heap)), "spn");
    ctx->paths.config.toml = sp_fs_join_path(ctx->heap, ctx->paths.config.dir, sp_str_lit("spn.toml"));
  ctx->paths.storage = env_or(ctx, "SPN_STORAGE_DIR", join_path(ctx, sp_fs_get_storage_path(ctx->heap), "spn"));
    ctx->paths.caches.dir = join_path(ctx, ctx->paths.storage, "cache");
      ctx->paths.caches.git.dir = join_path(ctx, ctx->paths.caches.dir, "source");
        ctx->paths.caches.git.checkouts = join_path(ctx, ctx->paths.caches.git.dir, "checkouts");
      ctx->paths.caches.store.dir = join_path(ctx, ctx->paths.caches.dir, "store");
      ctx->paths.caches.build.dir = join_path(ctx, ctx->paths.caches.dir, "build");
      ctx->paths.toolchain = env_or(ctx, "SPN_TOOLCHAIN_DIR", join_path(ctx, ctx->paths.caches.dir, "toolchain"));
    ctx->paths.index = join_path(ctx, ctx->paths.storage, "index");
    ctx->paths.log = join_path(ctx, ctx->paths.storage, "log");
    ctx->paths.cache = join_path(ctx, ctx->paths.storage, "cache");
    ctx->paths.runtime = join_path(ctx, ctx->paths.storage, "runtime");
      ctx->paths.include = join_path(ctx, ctx->paths.runtime, "include");
      ctx->paths.version = join_path(ctx, ctx->paths.runtime, "version.stamp");
    ctx->paths.tools.dir = sp_fs_join_path(ctx->heap, ctx->paths.storage, sp_str_lit("tools"));

  sp_fs_create_dir(ctx->paths.log);
  sp_fs_create_dir(ctx->paths.caches.dir);
  sp_fs_create_dir(ctx->paths.caches.git.dir);
  sp_fs_create_dir(ctx->paths.caches.build.dir);
  sp_fs_create_dir(ctx->paths.caches.store.dir);
  sp_fs_create_dir(ctx->paths.index);
  sp_fs_create_dir(ctx->paths.toolchain);
  sp_fs_create_dir(ctx->paths.bin);
  sp_fs_create_dir(ctx->paths.tools.dir);

  extract_runtime(ctx);

  spn_git_cache_init(&ctx->caches.git, ctx->mem, ctx->intern, ctx->paths.caches.git.dir);
  ctx->caches.toolchains = (spn_toolchain_store_t) {
    .mem = ctx->mem,
    .dir = ctx->paths.toolchain,
    .mirror = sp_env_get(ctx->env, sp_str_lit("SPN_MIRROR")),
    .fetch = spn_fetch_curl,
  };

  ctx->config.indexes = sp_da_new(ctx->heap, spn_index_info_t);
  if (sp_fs_exists(ctx->paths.config.toml)) {
    spn_cg_config_t config = sp_zero;
    spn_toml_loader_t loader = sp_zero;
    spn_toml_loader_init(&loader, ctx->mem, ctx->intern);
    spn_err_t loaded = spn_codegen_load_config(&loader, ctx->paths.config.toml, &config);
    if (!loaded) {
      sp_da_for(config.index, it) {
        sp_da_push(ctx->config.indexes, spn_index_lower(&loader, it, SPN_INDEX_KIND_USER, &config.index[it]));
      }
    }
    if (loaded || !sp_da_empty(loader.issues)) {
      return (spn_err_union_t) {
        .kind = SPN_ERR_MANIFEST_ISSUES,
        .manifest = { .path = ctx->paths.config.toml, .issues = loader.issues },
      };
    }
  }

  return spn_result(SPN_OK);
}

static spn_err_union_t load_project(spn_ctx_t* ctx, sp_str_t dir, u32 refresh) {
  sp_assert(ctx->config.indexes);

  if (sp_str_valid(dir)) {
    ctx->paths.project = sp_fs_canonicalize_path(ctx->heap, dir);
  }
  else {
    ctx->paths.project = sp_str_copy(ctx->heap, ctx->paths.cwd);
  }
  spn_try_union(spn_project_load(ctx->heap, ctx->intern, ctx->events, ctx->paths.project, &ctx->project));

  spn_index_assemble(ctx->heap, ctx->project ? &ctx->project->package.indexes : SP_NULLPTR, ctx->config.indexes, &ctx->indexes);

  sp_da_for(ctx->indexes, it) {
    spn_index_info_t* index = &ctx->indexes[it];
    index->location = spn_index_location(index, ctx->heap, ctx->paths.index);
    if (!index->refresh) {
      index->refresh = refresh ? refresh : SPN_INDEX_DEFAULT_REFRESH;
    }
  }

  return spn_result(SPN_OK);
}

static spn_err_union_t open_session(spn_ctx_t* ctx, spn_session_config_t config) {
  spn_try_union(spn_ctx_require_project(ctx));

  ctx->session = sp_alloc_type(ctx->heap, spn_session_t);
  return spn_session_init(ctx->session, ctx, ctx->heap, ctx->project, config);
}

spn_err_t spn_ctx_mount(spn_ctx_t* ctx) {
  return spn_err_emit(ctx, mount(ctx));
}

spn_err_t spn_ctx_load_project(spn_ctx_t* ctx, spn_project_request_t request) {
  return spn_err_emit(ctx, load_project(ctx, request.dir, request.index_refresh_seconds));
}

spn_err_t spn_ctx_open_session(spn_ctx_t* ctx, const spn_session_config_t* config, spn_session_t** session) {
  spn_err_t err = spn_err_emit(ctx, open_session(ctx, *config));
  *session = err ? SP_NULLPTR : ctx->session;
  return err;
}

void spn_ctx_close(spn_ctx_t* ctx, bool ok) {
  spn_err_t result = SPN_OK;
  if (!ok) {
    result = (spn_err_t)sp_atomic_s32_load(&ctx->error, SP_ATOMIC_SEQ_CST);
    if (!result) {
      result = SPN_ERROR;
    }
  }

  spn_event_buffer_push(ctx->events, (spn_build_event_t) {
    .kind = SPN_EVENT_RESULT,
    .result = {
      .ok = result == SPN_OK,
      .err = result,
    },
  });

  if (ctx->session) {
    spn_session_finalize(ctx->session);
    ctx->session = SP_NULLPTR;
  }
}
