#include "dag/dag.h"
#include "sha256/sha256.h"
#include "error/types.h"
#include "sp.h"
#include "sp/io.h"

spn_dag_t* spn_dag_new(sp_mem_t mem) {
  sp_mem_arena_t* arena = sp_mem_arena_new(mem);
  sp_mem_t a = sp_mem_arena_as_allocator(arena);
  spn_dag_t* g = sp_mem_allocator_alloc_type(a, spn_dag_t);
  g->arena = arena;
  g->mem = a;
  sp_da_init(a, g->artifacts);
  sp_da_init(a, g->actions);
  return g;
}

spn_dag_artifact_t* spn_dag_find_artifact(spn_dag_t* g, spn_dag_id_t id) {
  sp_assert(id.occupied);
  return g->artifacts + id.index;
}

spn_dag_action_t* spn_dag_find_action(spn_dag_t* g, spn_dag_id_t id) {
  sp_assert(id.occupied);
  return g->actions + id.index;
}

static spn_dag_id_t spn_dag_add_artifact(spn_dag_t* g, spn_dag_artifact_t artifact) {
  artifact.id = (spn_dag_id_t) {
    .index = (u32)sp_da_size(g->artifacts),
    .occupied = true
  };
  sp_da_init(g->mem, artifact.consumers);
  sp_da_push(g->artifacts, artifact);
  return artifact.id;
}

spn_dag_id_t spn_dag_add_value(spn_dag_t* g, const void* data, u64 len) {
  return spn_dag_add_artifact(g, (spn_dag_artifact_t) {
    .kind = SPN_DAG_ARTIFACT_KIND_VALUE,
    .digest = spn_dag_digest(data, len),
  });
}

spn_dag_id_t spn_dag_add_file(spn_dag_t* g, sp_str_t path) {
  return spn_dag_add_artifact(g, (spn_dag_artifact_t) {
    .kind = SPN_DAG_ARTIFACT_KIND_FILE,
    .path = sp_str_copy(g->mem, path),
  });
}

spn_dag_id_t spn_dag_add_action(spn_dag_t* g, spn_dag_action_config_t config) {
  spn_dag_action_t action = {
    .id = {
      .index = (u32)sp_da_size(g->actions),
      .occupied = true
    },
    .identity = config.identity,
    .execute = config.execute,
    .user_data = config.user_data,
  };
  sp_da_init(g->mem, action.consumes);
  sp_da_init(g->mem, action.produces);
  sp_da_push(g->actions, action);
  return action.id;
}

void spn_dag_action_add_input(spn_dag_t* g, spn_dag_id_t action_id, spn_dag_id_t artifact_id) {
  spn_dag_action_t* action = spn_dag_find_action(g, action_id);
  spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, artifact_id);
  sp_da_push(action->consumes, artifact_id);
  sp_da_push(artifact->consumers, action_id);
}

spn_err_t spn_dag_action_add_output(spn_dag_t* g, spn_dag_id_t action_id, spn_dag_id_t artifact_id) {
  spn_dag_action_t* action = spn_dag_find_action(g, action_id);
  spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, artifact_id);
  if (artifact->producer.occupied) {
    return SPN_ERROR;
  }
  artifact->producer = action_id;
  sp_da_push(action->produces, artifact_id);
  return SPN_OK;
}

spn_dag_digest_t spn_dag_action_key(spn_dag_t* g, spn_dag_id_t action_id) {
  spn_dag_action_t* action = spn_dag_find_action(g, action_id);

  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  spn_sha256_update(&ctx, action->identity.bytes, sizeof(action->identity.bytes));
  sp_da_for(action->consumes, it) {
    spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, action->consumes[it]);
    sp_assert(spn_dag_digest_valid(artifact->digest));
    spn_sha256_update(&ctx, artifact->digest.bytes, sizeof(artifact->digest.bytes));
  }

  spn_dag_digest_t key = sp_zero;
  spn_sha256_final(&ctx, key.bytes);
  return key;
}

spn_dag_digest_t spn_dag_digest(const void* data, u64 len) {
  spn_dag_digest_t digest = sp_zero;
  spn_sha256(data, len, digest.bytes);
  return digest;
}

bool spn_dag_digest_equal(spn_dag_digest_t a, spn_dag_digest_t b) {
  return sp_mem_is_equal(a.bytes, b.bytes, sizeof(a.bytes));
}

bool spn_dag_digest_valid(spn_dag_digest_t digest) {
  sp_for(it, sizeof(digest.bytes)) {
    if (digest.bytes[it]) {
      return true;
    }
  }
  return false;
}

sp_str_t spn_dag_digest_hex(sp_mem_t mem, spn_dag_digest_t digest) {
  return spn_sha256_digest_hex(mem, digest.bytes);
}

#define SPN_DAG_FILE_CACHE_VERSION 1

void spn_dag_file_cache_init(spn_dag_file_cache_t* c, sp_mem_t mem) {
  c->arena = sp_mem_arena_new(mem);
  c->mem = sp_mem_arena_as_allocator(c->arena);
  sp_ht_init(c->mem, c->entries);
  sp_str_ht_init(c->mem, c->metadata);
}

spn_err_t spn_dag_get_file_meta(spn_dag_file_cache_t* c, sp_str_t path, sp_sys_file_meta_t* meta) {
  sp_sys_file_meta_t* cached = sp_ht_getp(c->metadata, path);
  if (cached) {
    *meta = *cached;
    return SPN_OK;
  }

  sp_sys_file_meta_t st = sp_zero;
  spn_try_as(sp_sys_get_path_metadata_s(sp_sys_get_root(0), path, &st), SPN_ERROR);

  sp_ht_insert(c->metadata, sp_str_copy(c->mem, path), st);
  *meta = st;
  return SPN_OK;
}

static bool spn_dag_timespec_equal(sp_sys_timespec_t a, sp_sys_timespec_t b) {
  return a.tv_sec == b.tv_sec && a.tv_nsec == b.tv_nsec;
}

static bool is_file_clean(spn_dag_file_meta_t* cached, sp_sys_file_meta_t sys) {
  if (!cached) return false;
  if (!spn_dag_timespec_equal(sys.mtime, cached->mtime)) return false;
  if (cached->size != sys.size) return false;
  return true;
}

spn_err_t spn_dag_get_file_digest(spn_dag_file_cache_t* c, sp_str_t path, spn_dag_digest_t* digest) {
  sp_sys_file_meta_t sys = sp_zero;
  spn_try(spn_dag_get_file_meta(c, path, &sys));

  spn_dag_file_id_t id = {
    .device = sys.device,
    .id = sys.id
  };
  spn_dag_file_meta_t* cached = sp_ht_getp(c->entries, id);

  if (is_file_clean(cached, sys)) {
    *digest = cached->digest;
    return SPN_OK;
  }

  spn_try(spn_sha256_file_digest(path, digest->bytes));

  spn_dag_file_meta_t fresh = {
    .id = id,
    .mtime = sys.mtime,
    .size = sys.size,
    .digest = *digest
  };

  sp_ht_insert(c->entries, id, fresh);

  return SPN_OK;
}

spn_err_t spn_dag_file_cache_save(spn_dag_file_cache_t* c, sp_str_t path) {
  sp_fs_remove_file(path);

  sp_io_file_writer_t writer = sp_zero;
  sp_io_writer_t* io = &writer.base;
  sp_try_as(sp_io_file_writer_from_path(&writer, path), SPN_ERROR);

  sp_err_t err = SP_OK;
  u64 version = SPN_DAG_FILE_CACHE_VERSION;
  u64 count = sp_ht_size(c->entries);
  sp_try_goto(sp_io_write(io, &version, sizeof(version), SP_NULLPTR), err, done);
  sp_try_goto(sp_io_write(io, &count, sizeof(count), SP_NULLPTR), err, done);

  sp_ht_for(c->entries, it) {
    spn_dag_file_meta_t* entry = sp_ht_it_getp(c->entries, it);
    sp_try_goto(sp_io_write(io, entry, sizeof(*entry), SP_NULLPTR), err, done);
  }

done:
  sp_io_file_writer_close(&writer);
  return err ? SPN_ERROR : SPN_OK;
}

spn_err_t spn_dag_file_cache_load(spn_dag_file_cache_t* c, sp_str_t path) {
  sp_err_t err = SP_OK;
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_mem_slice_t data = sp_zero;
  sp_try_goto(sp_io_read_file_slice(s.mem, path, &data), err, done);

  sp_io_reader_t reader = sp_zero;
  sp_io_reader_from_mem(&reader, data.data, data.len);

  struct { u64 version; u64 count; } header = sp_zero;
  if (sp_io_read_all(&reader, &header, sizeof(header), SP_NULLPTR)) {
    goto done;
  }

  if (header.version != SPN_DAG_FILE_CACHE_VERSION) {
    goto done;
  }

  sp_for(it, header.count) {
    spn_dag_file_meta_t entry = sp_zero;
    sp_try_goto(sp_io_read_all(&reader, &entry, sizeof(entry), SP_NULLPTR), err, done);
    sp_ht_insert(c->entries, entry.id, entry);
  }

done:
  sp_mem_end_scratch(s);
  return err ? SPN_ERROR : SPN_OK;
}

void spn_dag_action_cache_init(spn_dag_action_cache_t* c, sp_mem_t mem) {
  c->arena = sp_mem_arena_new(mem);
  c->mem = sp_mem_arena_as_allocator(c->arena);
  sp_ht_init(c->mem, c->entries);
}

const spn_dag_action_entry_t* spn_dag_action_cache_get(spn_dag_action_cache_t* c, spn_dag_digest_t key) {
  return SP_NULLPTR;
}

void spn_dag_action_cache_put(spn_dag_action_cache_t* c, spn_dag_digest_t key, const spn_dag_action_output_t* outputs, u32 count) {
  sp_assert(!sp_ht_getp(c->entries, key));
}

bool spn_dag_action_cache_remove(spn_dag_action_cache_t* c, spn_dag_digest_t key) {
  return false;
}

spn_err_t spn_dag_action_cache_save(spn_dag_action_cache_t* c, sp_str_t path) {
  return SPN_ERROR;
}

spn_err_t spn_dag_action_cache_load(spn_dag_action_cache_t* c, sp_str_t path) {
  return SPN_ERROR;
}

spn_err_t spn_dag_execute(spn_dag_t* g, spn_dag_id_t action, spn_dag_file_cache_t* files, spn_dag_action_cache_t* cache, spn_dag_store_t* store) {
  return SPN_ERROR;
}

static sp_str_t spn_dag_store_path(spn_dag_store_t* store, sp_mem_t mem, spn_dag_digest_t digest) {
  return sp_fs_join_path(mem, store->dir, spn_dag_digest_hex(mem, digest));
}

void spn_dag_store_init(spn_dag_store_t* store, spn_dag_store_config_t config) {
  store->kind = config.kind;
  store->arena = sp_mem_arena_new(config.mem);
  store->mem = sp_mem_arena_as_allocator(store->arena);

  switch (store->kind) {
    case SPN_DAG_STORE_MEM: {
      sp_ht_init(store->mem, store->blobs);
      break;
    }
    case SPN_DAG_STORE_FILESYSTEM: {
      store->dir = sp_str_copy(store->mem, config.dir);
      if (!sp_fs_exists(store->dir)) {
        sp_fs_create_dir(store->dir);
      }
      break;
    }
  }
}

spn_err_t spn_dag_put(spn_dag_store_t* store, const void* data, u64 len, spn_dag_digest_t* digest) {
  *digest = spn_dag_digest(data, len);

  switch (store->kind) {
    case SPN_DAG_STORE_MEM: {
      if (sp_ht_getp(store->blobs, *digest)) {
        return SPN_OK;
      }
      sp_mem_slice_t blob = {
        .data = sp_alloc_n(store->mem, u8, len),
        .len = len
      };
      sp_mem_copy(blob.data, data, len);
      sp_ht_insert(store->blobs, *digest, blob);
      return SPN_OK;
    }
    case SPN_DAG_STORE_FILESYSTEM: {
      sp_mem_arena_marker_t s = sp_mem_begin_scratch();
      sp_str_t path = spn_dag_store_path(store, s.mem, *digest);
      spn_err_t err = SPN_OK;
      if (!sp_fs_exists(path)) {
        if (sp_fs_create_file_slice(path, sp_mem_slice((u8*)data, len))) {
          err = SPN_ERROR;
        }
      }
      sp_mem_end_scratch(s);
      return err;
    }
  }

  SP_UNREACHABLE_RETURN(SPN_ERROR);
}

spn_err_t spn_dag_store_put_file(spn_dag_store_t* store, sp_str_t path, spn_dag_digest_t* digest) {
  switch (store->kind) {
    case SPN_DAG_STORE_MEM: {
      sp_mem_arena_marker_t s = sp_mem_begin_scratch();
      sp_mem_slice_t content = sp_zero;
      if (sp_io_read_file_slice(s.mem, path, &content)) {
        sp_mem_end_scratch(s);
        return SPN_ERROR;
      }
      spn_err_t err = spn_dag_put(store, content.data, content.len, digest);
      sp_mem_end_scratch(s);
      return err;
    }
    case SPN_DAG_STORE_FILESYSTEM: {
      if (spn_sha256_file_digest(path, digest->bytes)) {
        return SPN_ERROR;
      }

      sp_mem_arena_marker_t s = sp_mem_begin_scratch();
      sp_str_t stored = spn_dag_store_path(store, s.mem, *digest);
      spn_err_t err = SPN_OK;
      if (!sp_fs_exists(stored)) {
        if (sp_fs_copy(path, stored)) {
          err = SPN_ERROR;
        }
      }
      sp_mem_end_scratch(s);
      return err;
    }
  }

  SP_UNREACHABLE_RETURN(SPN_ERROR);
}

bool spn_dag_store_has(spn_dag_store_t* store, spn_dag_digest_t digest) {
  switch (store->kind) {
    case SPN_DAG_STORE_MEM: {
      return sp_ht_getp(store->blobs, digest) != SP_NULLPTR;
    }
    case SPN_DAG_STORE_FILESYSTEM: {
      sp_mem_arena_marker_t s = sp_mem_begin_scratch();
      bool exists = sp_fs_exists(spn_dag_store_path(store, s.mem, digest));
      sp_mem_end_scratch(s);
      return exists;
    }
  }

  SP_UNREACHABLE_RETURN(false);
}

spn_err_t spn_dag_store_get(spn_dag_store_t* store, spn_dag_digest_t digest, sp_mem_t mem, sp_mem_slice_t* data) {
  switch (store->kind) {
    case SPN_DAG_STORE_MEM: {
      sp_mem_slice_t* blob = sp_ht_getp(store->blobs, digest);
      if (!blob) {
        return SPN_ERROR;
      }
      data->data = sp_alloc_n(mem, u8, blob->len);
      data->len = blob->len;
      sp_mem_copy(data->data, blob->data, blob->len);
      return SPN_OK;
    }
    case SPN_DAG_STORE_FILESYSTEM: {
      sp_mem_arena_marker_t s = sp_mem_begin_scratch();
      sp_str_t path = spn_dag_store_path(store, s.mem, digest);
      spn_err_t err = sp_io_read_file_slice(mem, path, data) ? SPN_ERROR : SPN_OK;
      sp_mem_end_scratch(s);
      return err;
    }
  }

  SP_UNREACHABLE_RETURN(SPN_ERROR);
}

spn_err_t spn_dag_store_materialize(spn_dag_store_t* store, spn_dag_digest_t digest, sp_str_t path) {
  switch (store->kind) {
    case SPN_DAG_STORE_MEM: {
      sp_mem_slice_t* blob = sp_ht_getp(store->blobs, digest);
      if (!blob) {
        return SPN_ERROR;
      }
      return sp_fs_create_file_slice(path, *blob) ? SPN_ERROR : SPN_OK;
    }
    case SPN_DAG_STORE_FILESYSTEM: {
      sp_mem_arena_marker_t s = sp_mem_begin_scratch();
      sp_str_t stored = spn_dag_store_path(store, s.mem, digest);
      spn_err_t err = SPN_OK;
      if (!sp_fs_exists(stored)) {
        err = SPN_ERROR;
      } else if (sp_fs_copy(stored, path)) {
        err = SPN_ERROR;
      }
      sp_mem_end_scratch(s);
      return err;
    }
  }

  SP_UNREACHABLE_RETURN(SPN_ERROR);
}
