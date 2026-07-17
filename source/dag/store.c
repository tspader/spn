#include "dag/dag.h"
#include "dag/types.h"
#include "error/types.h"
#include "sp.h"
#include "sp/io.h"
#include "sp/atomic_file.h"
#include "dag_output.gen.h"
#include "dag_obs.gen.h"

#define try(expr) spn_try(expr)

static sp_str_t format_u64(sp_mem_t mem, u64 value) {
  return sp_fmt(mem, "{}", sp_fmt_uint(value)).value;
}

static sp_str_t format_s64(sp_mem_t mem, s64 value) {
  return sp_fmt(mem, "{}", sp_fmt_int(value)).value;
}

static sp_str_t format_obs_kind(spn_dag_obs_kind_t kind) {
  switch (kind) {
    case SPN_DAG_OBS_FILE:        return sp_str_lit("file");
    case SPN_DAG_OBS_ABSENT:      return sp_str_lit("absent");
    case SPN_DAG_OBS_ENUMERATION: return sp_str_lit("enumeration");
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

static bool parse_obs_kind(sp_str_t str, spn_dag_obs_kind_t* out) {
  if (sp_str_equal(str, sp_str_lit("file"))) {
    *out = SPN_DAG_OBS_FILE;
    return true;
  }
  if (sp_str_equal(str, sp_str_lit("absent"))) {
    *out = SPN_DAG_OBS_ABSENT;
    return true;
  }
  if (sp_str_equal(str, sp_str_lit("enumeration"))) {
    *out = SPN_DAG_OBS_ENUMERATION;
    return true;
  }
  return false;
}

static spn_err_t write_line(sp_io_writer_t* io, sp_str_t line) {
  spn_try_as(sp_io_write(io, line.data, line.len, SP_NULLPTR), SPN_ERR_DAG_STORE_WRITE);
  spn_try_as(sp_io_write(io, "\n", 1, SP_NULLPTR), SPN_ERR_DAG_STORE_WRITE);
  return SPN_OK;
}

static spn_err_t read_lines(sp_mem_t mem, sp_str_t path, sp_da(sp_str_t)* out) {
  sp_str_t content = sp_zero;
  spn_try_as(sp_io_read_file(mem, path, &content), SPN_ERR_DAG_STORE_READ);

  *out = sp_da_new(mem, sp_str_t);
  sp_da(sp_str_t) lines = sp_str_split_c8(mem, content, '\n');
  sp_da_for(lines, it) {
    if (!sp_str_empty(sp_str_trim(lines[it]))) {
      sp_da_push(*out, lines[it]);
    }
  }
  return SPN_OK;
}

static sp_str_t entry_path(sp_str_t dir, sp_mem_t mem, spn_dag_digest_t key) {
  sp_str_t name = sp_fmt(mem, "{}.jsonl", sp_fmt_str(spn_dag_digest_hex(mem, key))).value;
  return sp_fs_join_path(mem, dir, name);
}

typedef bool (*spn_dag_parse_row_fn)(sp_str_t line, sp_mem_t scratch, sp_mem_t mem, void* into);
typedef spn_err_t (*spn_dag_format_row_fn)(sp_io_writer_t* io, sp_mem_t mem, const void* rows, u64 index);

static bool table_load(sp_str_t dir, spn_dag_digest_t key, sp_mem_t mem, spn_dag_parse_row_fn parse, void* into) {
  bool ok = false;
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_str_t path = entry_path(dir, s.mem, key);
  sp_da(sp_str_t) lines = sp_zero;
  if (read_lines(s.mem, path, &lines)) {
    goto done;
  }

  sp_da_for(lines, it) {
    if (!parse(lines[it], s.mem, mem, into)) {
      sp_fs_remove_file(path);
      goto done;
    }
  }
  ok = true;

done:
  sp_mem_end_scratch(s);
  return ok;
}

static void table_write(sp_str_t dir, spn_dag_digest_t key, spn_dag_format_row_fn format, const void* rows, u64 count) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_fs_atomic_t af = sp_zero;
  if (sp_fs_atomic_open(&af, entry_path(dir, s.mem, key))) {
    goto done;
  }

  sp_for(it, count) {
    if (format(sp_fs_atomic_writer(&af), s.mem, rows, it)) {
      sp_fs_atomic_abort(&af);
      goto done;
    }
  }
  sp_fs_atomic_commit(&af, SP_FS_ATOMIC_REPLACE);

done:
  sp_mem_end_scratch(s);
}

static spn_err_t format_output_row(sp_io_writer_t* io, sp_mem_t mem, const void* rows, u64 index) {
  const spn_dag_action_output_t* output = (const spn_dag_action_output_t*)rows + index;
  spn_cg_dag_output_t cg = {
    .name = output->name,
    .digest = spn_dag_digest_hex(mem, output->digest),
  };
  return write_line(io, spn_dag_output_write_compact(mem, &cg));
}

static bool parse_output_row(sp_str_t line, sp_mem_t scratch, sp_mem_t mem, void* into) {
  sp_da(spn_dag_action_output_t)* outputs = (sp_da(spn_dag_action_output_t)*)into;

  spn_cg_dag_output_t cg = sp_zero;
  spn_dag_action_output_t output = sp_zero;
  if (!spn_dag_output_read(line, &cg, scratch) || !spn_dag_digest_parse(cg.digest, &output.digest)) {
    return false;
  }
  output.name = sp_str_copy(mem, cg.name);
  sp_da_push(*outputs, output);
  return true;
}

static spn_err_t format_obs_row(sp_io_writer_t* io, sp_mem_t mem, const void* rows, u64 index) {
  const spn_dag_obs_t* obs = (const spn_dag_obs_t*)rows + index;
  spn_cg_dag_obs_t cg = {
    .kind = format_obs_kind(obs->kind),
    .path = obs->path,
    .filter = obs->filter,
    .device = format_u64(mem, obs->meta.id.device),
    .inode = format_u64(mem, obs->meta.id.inode),
    .mtime_s = format_s64(mem, (s64)obs->meta.mtime.tv_sec),
    .mtime_ns = format_s64(mem, (s64)obs->meta.mtime.tv_nsec),
    .size = format_s64(mem, obs->meta.size),
    .digest = spn_dag_digest_hex(mem, obs->meta.digest),
  };
  return write_line(io, spn_dag_obs_write_compact(mem, &cg));
}

static bool parse_obs_row(sp_str_t line, sp_mem_t scratch, sp_mem_t mem, void* into) {
  sp_da(spn_dag_obs_t)* set = (sp_da(spn_dag_obs_t)*)into;

  spn_cg_dag_obs_t cg = sp_zero;
  if (!spn_dag_obs_read(line, &cg, scratch)) return false;

  spn_dag_obs_t obs = sp_zero;
  s64 mtime_s = 0;
  s64 mtime_ns = 0;
  if (!parse_obs_kind(cg.kind, &obs.kind)) return false;
  if (!sp_parse_u64_ex(cg.device, &obs.meta.id.device)) return false;
  if (!sp_parse_u64_ex(cg.inode, &obs.meta.id.inode)) return false;
  if (!sp_parse_s64_ex(cg.mtime_s, &mtime_s)) return false;
  if (!sp_parse_s64_ex(cg.mtime_ns, &mtime_ns)) return false;
  if (!sp_parse_s64_ex(cg.size, &obs.meta.size)) return false;
  if (!spn_dag_digest_parse(cg.digest, &obs.meta.digest)) return false;

  obs.path = sp_str_copy(mem, cg.path);
  obs.filter = sp_str_copy(mem, cg.filter);
  obs.meta.mtime.tv_sec = mtime_s;
  obs.meta.mtime.tv_nsec = mtime_ns;
  sp_da_push(*set, obs);
  return true;
}

static spn_err_t write_outputs(sp_io_writer_t* io, sp_mem_t mem, const spn_dag_action_output_t* outputs, u64 count) {
  sp_for(it, count) {
    try(format_output_row(io, mem, outputs, it));
  }
  return SPN_OK;
}

void spn_dag_action_cache_init(spn_dag_action_cache_t* c, sp_mem_t mem, sp_str_t dir) {
  c->arena = sp_mem_arena_new(mem);
  c->mem = sp_mem_arena_as_allocator(c->arena);
  c->dir = sp_str_copy(c->mem, dir);
  sp_ht_init(c->mem, c->entries);

  if (!sp_str_empty(c->dir)) {
    sp_fs_create_dir(c->dir);
  }
}

const spn_dag_action_entry_t* spn_dag_action_cache_get(spn_dag_action_cache_t* c, spn_dag_digest_t key) {
  const spn_dag_action_entry_t* cached = sp_ht_getp(c->entries, key);
  if (cached) {
    return cached;
  }

  if (sp_str_empty(c->dir)) {
    return SP_NULLPTR;
  }

  spn_dag_action_entry_t entry = sp_zero;
  sp_da_init(c->mem, entry.outputs);
  if (!table_load(c->dir, key, c->mem, parse_output_row, &entry.outputs)) {
    return SP_NULLPTR;
  }

  sp_ht_insert(c->entries, key, entry);
  return sp_ht_getp(c->entries, key);
}

void spn_dag_action_cache_put(spn_dag_action_cache_t* c, spn_dag_digest_t key, const spn_dag_action_output_t* outputs, u32 count) {
  spn_dag_action_entry_t entry = sp_zero;
  sp_da_init(c->mem, entry.outputs);
  sp_for(it, count) {
    sp_da_push(entry.outputs, ((spn_dag_action_output_t) {
      .name = sp_str_copy(c->mem, outputs[it].name),
      .digest = outputs[it].digest
    }));
  }
  sp_ht_insert(c->entries, key, entry);

  if (!sp_str_empty(c->dir)) {
    table_write(c->dir, key, format_output_row, entry.outputs, sp_da_size(entry.outputs));
  }
}

bool spn_dag_action_cache_remove(spn_dag_action_cache_t* c, spn_dag_digest_t key) {
  bool removed = false;

  if (sp_ht_getp(c->entries, key)) {
    sp_ht_erase(c->entries, key);
    removed = true;
  }

  if (!sp_str_empty(c->dir)) {
    sp_mem_arena_marker_t s = sp_mem_begin_scratch();
    sp_str_t path = entry_path(c->dir, s.mem, key);
    if (sp_fs_exists(path)) {
      sp_fs_remove_file(path);
      removed = true;
    }
    sp_mem_end_scratch(s);
  }

  return removed;
}

void spn_dag_discovery_init(spn_dag_discovery_t* d, sp_mem_t mem, sp_str_t dir) {
  d->arena = sp_mem_arena_new(mem);
  d->mem = sp_mem_arena_as_allocator(d->arena);
  d->dir = sp_str_copy(d->mem, dir);
  sp_ht_init(d->mem, d->entries);

  if (!sp_str_empty(d->dir)) {
    sp_fs_create_dir(d->dir);
  }
}

spn_dag_pathset_t* spn_dag_discovery_get(spn_dag_discovery_t* d, spn_dag_digest_t weak) {
  spn_dag_pathset_t* cached = sp_ht_getp(d->entries, weak);
  if (cached) {
    return cached;
  }

  if (sp_str_empty(d->dir)) {
    return SP_NULLPTR;
  }

  spn_dag_pathset_t set = sp_zero;
  sp_da_init(d->mem, set.obs);
  if (!table_load(d->dir, weak, d->mem, parse_obs_row, &set.obs)) {
    return SP_NULLPTR;
  }

  sp_ht_insert(d->entries, weak, set);
  return sp_ht_getp(d->entries, weak);
}

void spn_dag_discovery_put(spn_dag_discovery_t* d, spn_dag_digest_t weak, const spn_dag_obs_t* obs, u32 count) {
  spn_dag_pathset_t set = sp_zero;
  sp_da_init(d->mem, set.obs);
  sp_for(it, count) {
    spn_dag_obs_t copy = obs[it];
    copy.path = sp_str_copy(d->mem, obs[it].path);
    copy.filter = sp_str_copy(d->mem, obs[it].filter);
    sp_da_push(set.obs, copy);
  }
  sp_ht_insert(d->entries, weak, set);
  spn_dag_discovery_flush(d, weak);
}

void spn_dag_discovery_flush(spn_dag_discovery_t* d, spn_dag_digest_t weak) {
  if (sp_str_empty(d->dir)) {
    return;
  }

  spn_dag_pathset_t* set = sp_ht_getp(d->entries, weak);
  if (set) {
    table_write(d->dir, weak, format_obs_row, set->obs, sp_da_size(set->obs));
  }
}

static sp_str_t get_blob_name(sp_str_t name) {
  sp_str_t base = sp_fs_get_name(name);
  return sp_str_empty(base) ? sp_str_lit("blob") : base;
}

static sp_str_t get_blob_dir(spn_dag_store_t* store, sp_mem_t mem, spn_dag_digest_t digest) {
  return sp_fs_join_path(mem, store->dir, spn_dag_digest_hex(mem, digest));
}

static sp_str_t get_blob_path(spn_dag_store_t* store, sp_mem_t mem, spn_dag_digest_t digest, sp_str_t name) {
  return sp_fs_join_path(mem, get_blob_dir(store, mem, digest), get_blob_name(name));
}

static sp_str_t get_staging_dir(spn_dag_store_t* store, sp_mem_t mem) {
  return sp_fs_join_path(mem, store->dir, sp_str_lit(".staging"));
}

static spn_err_t copy_blob(sp_str_t source, sp_str_t target, sp_str_t staging) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_err_t err = SPN_OK;

  sp_mem_slice_t bytes = sp_zero;
  if (sp_io_read_file_slice(s.mem, source, &bytes)) {
    err = SPN_ERR_DAG_STORE_READ;
  } else if (sp_fs_write_atomic_slice_staged(target, staging, bytes)) {
    err = SPN_ERR_DAG_STORE_WRITE;
  }

  sp_mem_end_scratch(s);
  return err;
}

static spn_err_t link_into_store(sp_str_t source, sp_str_t blob, sp_str_t staging) {
  if (!sp_fs_link(source, blob, SP_FS_LINK_HARD)) {
    return SPN_OK;
  }
  return copy_blob(source, blob, staging);
}

static spn_err_t link_from_store(sp_str_t source, sp_str_t target, sp_str_t staging) {
  sp_fs_create_dir(sp_fs_parent_path(target));
  sp_fs_remove_file(target);

  if (!sp_fs_link(source, target, SP_FS_LINK_HARD)) {
    return SPN_OK;
  }
  return copy_blob(source, target, staging);
}

static bool find_blob(spn_dag_store_t* store, spn_dag_digest_t digest, sp_mem_slice_t* blob) {
  sp_mutex_lock(&store->mutex);
  sp_mem_slice_t* found = sp_ht_getp(store->blobs, digest);
  if (found) {
    *blob = *found;
  }
  sp_mutex_unlock(&store->mutex);
  return found != SP_NULLPTR;
}

void spn_dag_store_init(spn_dag_store_t* store, spn_dag_store_config_t config) {
  store->kind = config.kind;
  store->arena = sp_mem_arena_new(config.mem);
  store->mem = sp_mem_arena_as_allocator(store->arena);
  sp_mutex_init(&store->mutex, SP_MUTEX_PLAIN);

  switch (store->kind) {
    case SPN_DAG_STORE_MEM: {
      sp_ht_init(store->mem, store->blobs);
      break;
    }
    case SPN_DAG_STORE_FILESYSTEM: {
      store->dir = sp_str_copy(store->mem, config.dir);
      sp_fs_create_dir(store->dir);
      break;
    }
  }
}

spn_err_t spn_dag_store_put(spn_dag_store_t* store, const void* data, u64 len, sp_str_t name, spn_dag_digest_t* digest) {
  *digest = spn_dag_digest(data, len);

  switch (store->kind) {
    case SPN_DAG_STORE_MEM: {
      sp_mutex_lock(&store->mutex);
      if (!sp_ht_getp(store->blobs, *digest)) {
        sp_mem_slice_t blob = {
          .data = sp_alloc_n(store->mem, u8, len),
          .len = len
        };
        sp_mem_copy(blob.data, data, len);
        sp_ht_insert(store->blobs, *digest, blob);
      }
      sp_mutex_unlock(&store->mutex);
      return SPN_OK;
    }
    case SPN_DAG_STORE_FILESYSTEM: {
      sp_mem_arena_marker_t s = sp_mem_begin_scratch();
      spn_err_t err = SPN_OK;
      sp_str_t blob = get_blob_path(store, s.mem, *digest, name);
      if (!sp_fs_is_file(blob)) {
        sp_fs_create_dir(get_blob_dir(store, s.mem, *digest));
        if (sp_fs_write_atomic_slice_staged(blob, get_staging_dir(store, s.mem), sp_mem_slice((u8*)data, len))) {
          err = SPN_ERR_DAG_STORE_WRITE;
        }
      }
      sp_mem_end_scratch(s);
      return err;
    }
  }

  SP_UNREACHABLE_RETURN(SPN_ERROR);
}

spn_err_t spn_dag_store_put_file(spn_dag_store_t* store, sp_str_t path, sp_str_t name, spn_dag_digest_t* digest) {
  switch (store->kind) {
    case SPN_DAG_STORE_MEM: {
      sp_mem_arena_marker_t s = sp_mem_begin_scratch();
      sp_mem_slice_t content = sp_zero;
      if (sp_io_read_file_slice(s.mem, path, &content)) {
        sp_mem_end_scratch(s);
        return SPN_ERR_DAG_STORE_READ;
      }
      spn_err_t err = spn_dag_store_put(store, content.data, content.len, name, digest);
      sp_mem_end_scratch(s);
      return err;
    }
    case SPN_DAG_STORE_FILESYSTEM: {
      if (spn_sha256_file_digest(path, digest->bytes)) {
        return SPN_ERR_DAG_STORE_READ;
      }

      sp_mem_arena_marker_t s = sp_mem_begin_scratch();
      spn_err_t err = SPN_OK;
      sp_str_t blob = get_blob_path(store, s.mem, *digest, name);
      if (!sp_fs_is_file(blob)) {
        sp_fs_create_dir(get_blob_dir(store, s.mem, *digest));
        err = link_into_store(path, blob, get_staging_dir(store, s.mem));
      }
      sp_mem_end_scratch(s);
      return err;
    }
  }

  SP_UNREACHABLE_RETURN(SPN_ERROR);
}

sp_str_t spn_dag_store_path(spn_dag_store_t* store, sp_mem_t mem, spn_dag_digest_t digest, sp_str_t name) {
  switch (store->kind) {
    case SPN_DAG_STORE_MEM: {
      return sp_str_lit("");
    }
    case SPN_DAG_STORE_FILESYSTEM: {
      sp_str_t blob = get_blob_path(store, mem, digest, name);
      return sp_fs_is_file(blob) ? blob : sp_str_lit("");
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

bool spn_dag_store_has(spn_dag_store_t* store, spn_dag_digest_t digest, sp_str_t name) {
  switch (store->kind) {
    case SPN_DAG_STORE_MEM: {
      sp_mem_slice_t blob = sp_zero;
      return find_blob(store, digest, &blob);
    }
    case SPN_DAG_STORE_FILESYSTEM: {
      sp_mem_arena_marker_t s = sp_mem_begin_scratch();
      bool exists = sp_fs_is_file(get_blob_path(store, s.mem, digest, name));
      sp_mem_end_scratch(s);
      return exists;
    }
  }

  SP_UNREACHABLE_RETURN(false);
}

spn_err_t spn_dag_store_get(spn_dag_store_t* store, spn_dag_digest_t digest, sp_str_t name, sp_mem_t mem, sp_mem_slice_t* data) {
  switch (store->kind) {
    case SPN_DAG_STORE_MEM: {
      sp_mem_slice_t blob = sp_zero;
      if (!find_blob(store, digest, &blob)) {
        return SPN_ERR_DAG_STORE_MISSING;
      }
      data->data = sp_alloc_n(mem, u8, blob.len);
      data->len = blob.len;
      sp_mem_copy(data->data, blob.data, blob.len);
      return SPN_OK;
    }
    case SPN_DAG_STORE_FILESYSTEM: {
      sp_str_t stored = get_blob_path(store, mem, digest, name);
      if (!sp_fs_is_file(stored)) {
        return SPN_ERR_DAG_STORE_MISSING;
      }
      return sp_io_read_file_slice(mem, stored, data) ? SPN_ERR_DAG_STORE_READ : SPN_OK;
    }
  }

  SP_UNREACHABLE_RETURN(SPN_ERROR);
}

spn_err_t spn_dag_store_materialize(spn_dag_store_t* store, spn_dag_digest_t digest, sp_str_t name, sp_str_t path) {
  switch (store->kind) {
    case SPN_DAG_STORE_MEM: {
      sp_mem_slice_t blob = sp_zero;
      if (!find_blob(store, digest, &blob)) {
        return SPN_ERR_DAG_STORE_MISSING;
      }
      sp_fs_create_dir(sp_fs_parent_path(path));
      return sp_fs_write_atomic_slice(path, blob) ? SPN_ERR_DAG_STORE_WRITE : SPN_OK;
    }
    case SPN_DAG_STORE_FILESYSTEM: {
      sp_mem_arena_marker_t s = sp_mem_begin_scratch();
      sp_str_t stored = get_blob_path(store, s.mem, digest, name);
      spn_err_t err = SPN_ERR_DAG_STORE_MISSING;
      if (sp_fs_is_file(stored)) {
        err = link_from_store(stored, path, sp_str_lit(""));
      }
      sp_mem_end_scratch(s);
      return err;
    }
  }

  SP_UNREACHABLE_RETURN(SPN_ERROR);
}

static s32 tree_entry_order(const void* a, const void* b) {
  return sp_str_compare_alphabetical(((const spn_dag_action_output_t*)a)->name, ((const spn_dag_action_output_t*)b)->name);
}

static bool tree_name_ok(sp_str_t name) {
  if (sp_str_empty(name) || name.data[0] == '/') {
    return false;
  }
  bool ok = true;
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  sp_da(sp_str_t) segments = sp_str_split_c8(s.mem, name, '/');
  sp_da_for(segments, it) {
    if (sp_str_equal(segments[it], sp_str_lit(".."))) {
      ok = false;
      break;
    }
  }
  sp_mem_end_scratch(s);
  return ok;
}

spn_err_t spn_dag_store_put_tree(spn_dag_store_t* store, sp_str_t dir, spn_dag_digest_t* digest) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_err_t err = SPN_OK;

  sp_da(spn_dag_action_output_t) entries = sp_da_new(s.mem, spn_dag_action_output_t);
  sp_da(sp_fs_entry_t) files = sp_fs_collect_recursive(s.mem, dir);
  sp_da_for(files, it) {
    if (files[it].kind == SP_FS_KIND_DIR) {
      continue;
    }
    spn_dag_action_output_t entry = {
      .name = sp_str_strip_left(sp_str_strip_left(files[it].path, dir), sp_str_lit("/"))
    };
    spn_try_goto(spn_dag_store_put_file(store, files[it].path, entry.name, &entry.digest), err, done);
    sp_da_push(entries, entry);
  }
  sp_da_sort(entries, tree_entry_order);

  sp_io_dyn_mem_writer_t sink = sp_zero;
  sp_io_dyn_mem_writer_init(s.mem, &sink);
  spn_try_goto(write_outputs(&sink.base, s.mem, entries, sp_da_size(entries)), err, done);

  sp_str_t manifest = sp_io_dyn_mem_writer_as_str(&sink);
  err = spn_dag_store_put(store, manifest.data, manifest.len, sp_str_lit("tree"), digest);

done:
  sp_mem_end_scratch(s);
  return err;
}

spn_err_t spn_dag_tree_entries(spn_dag_store_t* store, spn_dag_digest_t digest, sp_mem_t mem, sp_da(spn_dag_action_output_t)* out) {
  sp_mem_slice_t manifest = sp_zero;
  try(spn_dag_store_get(store, digest, sp_str_lit("tree"), mem, &manifest));
  sp_assert(manifest.len <= SP_LIMIT_U32_MAX);

  sp_str_t content = sp_str((c8*)manifest.data, (u32)manifest.len);
  *out = sp_da_new(mem, spn_dag_action_output_t);
  sp_da(sp_str_t) lines = sp_str_split_c8(mem, content, '\n');
  sp_da_for(lines, it) {
    if (sp_str_empty(sp_str_trim(lines[it]))) {
      continue;
    }
    if (!parse_output_row(lines[it], mem, mem, out)) {
      return SPN_ERR_DAG_TREE;
    }
    if (!tree_name_ok(sp_da_back(*out)->name)) {
      return SPN_ERR_DAG_TREE;
    }
  }
  return SPN_OK;
}

bool spn_dag_store_has_tree(spn_dag_store_t* store, spn_dag_digest_t digest) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  bool ok = false;

  sp_da(spn_dag_action_output_t) entries = sp_zero;
  if (spn_dag_tree_entries(store, digest, s.mem, &entries)) {
    goto done;
  }
  sp_da_for(entries, it) {
    if (!spn_dag_store_has(store, entries[it].digest, entries[it].name)) {
      goto done;
    }
  }
  ok = true;

done:
  sp_mem_end_scratch(s);
  return ok;
}

spn_err_t spn_dag_store_materialize_tree(spn_dag_store_t* store, spn_dag_digest_t digest, sp_str_t dir) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_err_t err = SPN_OK;

  sp_da(spn_dag_action_output_t) entries = sp_zero;
  spn_try_goto(spn_dag_tree_entries(store, digest, s.mem, &entries), err, done);

  sp_fs_remove_dir(dir);
  sp_fs_create_dir(dir);
  sp_da_for(entries, it) {
    sp_str_t path = sp_fs_join_path(s.mem, dir, entries[it].name);
    spn_try_goto(spn_dag_store_materialize(store, entries[it].digest, entries[it].name, path), err, done);
  }

done:
  sp_mem_end_scratch(s);
  return err;
}
