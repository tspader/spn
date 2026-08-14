#include "dag/dag.h"
#include "dag/types.h"
#include "sp.h"
#include "spn/core.h"
#include "sp/io.h"
#include "sp/atomic_file.h"


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

static bool row_lit(sp_str_t* cursor, c8 c) {
  if (!cursor->len || cursor->data[0] != c) {
    return false;
  }
  cursor->data++;
  cursor->len--;
  return true;
}

static bool row_u64(sp_str_t* cursor, u64* out) {
  u64 value = 0;
  u32 digits = 0;
  while (cursor->len && cursor->data[0] >= '0' && cursor->data[0] <= '9') {
    value = value * 10 + (u64)(cursor->data[0] - '0');
    cursor->data++;
    cursor->len--;
    digits++;
  }
  if (!digits) {
    return false;
  }
  *out = value;
  return true;
}

static bool row_s64(sp_str_t* cursor, s64* out) {
  bool negative = cursor->len && cursor->data[0] == '-';
  if (negative) {
    cursor->data++;
    cursor->len--;
  }
  u64 value = 0;
  if (!row_u64(cursor, &value)) {
    return false;
  }
  *out = negative ? -(s64)value : (s64)value;
  return true;
}

static bool row_bytes(sp_str_t* cursor, u64 len, sp_str_t* out) {
  if (cursor->len < len) {
    return false;
  }
  *out = sp_str(cursor->data, (u32)len);
  cursor->data += len;
  cursor->len -= (u32)len;
  return true;
}

static bool row_str(sp_str_t* cursor, sp_str_t* out) {
  u64 len = 0;
  if (!row_u64(cursor, &len)) {
    return false;
  }
  if (!row_lit(cursor, ':')) {
    return false;
  }
  return row_bytes(cursor, len, out);
}

static bool row_digest(sp_str_t* cursor, spn_dag_digest_t* out) {
  sp_str_t hex = sp_zero;
  if (!row_bytes(cursor, 2 * sizeof(out->bytes), &hex)) {
    return false;
  }
  return spn_dag_digest_parse(hex, out);
}

static bool row_word(sp_str_t* cursor, sp_str_t* out) {
  u32 len = 0;
  while (len < cursor->len && cursor->data[len] != ' ' && cursor->data[len] != '\n') {
    len++;
  }
  if (!len) {
    return false;
  }
  return row_bytes(cursor, len, out);
}

static bool row_line(sp_str_t* cursor, sp_str_t* out) {
  u32 len = 0;
  while (len < cursor->len && cursor->data[len] != '\n') {
    len++;
  }
  return row_bytes(cursor, len, out) && row_lit(cursor, '\n');
}

static bool row_header(sp_str_t* cursor) {
  return row_lit(cursor, '2') && row_lit(cursor, '\n');
}

static spn_err_t write_bytes(sp_io_writer_t* io, const void* data, u64 len) {
  return sp_io_write(io, data, len, SP_NULLPTR) ? SPN_ERR_DAG_STORE_WRITE : SPN_OK;
}

static spn_err_t write_header(sp_io_writer_t* io) {
  return write_bytes(io, "2\n", 2);
}

static spn_err_t write_row_str(sp_io_writer_t* io, sp_str_t str) {
  return sp_fmt_io(io, "{}:{}", sp_fmt_uint(str.len), sp_fmt_str(str)) ? SPN_ERR_DAG_STORE_WRITE : SPN_OK;
}

static sp_str_t entry_path(sp_str_t dir, sp_mem_t mem, spn_dag_digest_t key) {
  sp_str_t name = sp_fmt(mem, "{}.txt", sp_fmt_str(spn_dag_digest_hex(mem, key))).value;
  return sp_fs_join_path(mem, dir, name);
}

static spn_err_t write_output_row(sp_io_writer_t* io, sp_mem_t mem, const spn_dag_action_output_t* output) {
  if (sp_fmt_io(io, "{} ", sp_fmt_str(spn_dag_digest_hex(mem, output->digest)))) {
    return SPN_ERR_DAG_STORE_WRITE;
  }
  spn_try(write_row_str(io, output->name));
  return write_bytes(io, "\n", 1);
}

static bool parse_output_row(sp_str_t* cursor, spn_dag_action_output_t* out) {
  if (!row_digest(cursor, &out->digest)) return false;
  if (!row_lit(cursor, ' ')) return false;
  if (!row_str(cursor, &out->name)) return false;
  return row_lit(cursor, '\n');
}

static spn_err_t write_outputs(sp_io_writer_t* io, sp_mem_t mem, const spn_dag_action_output_t* outputs, u64 count) {
  spn_try(write_header(io));
  sp_for(it, count) {
    spn_try(write_output_row(io, mem, outputs + it));
  }
  return SPN_OK;
}

static bool parse_outputs(sp_str_t content, sp_da(spn_dag_action_output_t)* outputs) {
  sp_str_t cursor = content;
  if (!row_header(&cursor)) {
    return false;
  }
  while (cursor.len) {
    spn_dag_action_output_t output = sp_zero;
    if (!parse_output_row(&cursor, &output)) {
      return false;
    }
    sp_da_push(*outputs, output);
  }
  return true;
}

static spn_err_t write_obs_row(sp_io_writer_t* io, sp_mem_t mem, const spn_dag_roots_t* roots, const spn_dag_obs_t* obs) {
  spn_dag_prefixed_t prefixed = spn_dag_root_collapse(roots, obs->path);
  if (sp_str_contains(prefixed.sub, sp_str_lit("\n"))) {
    return SPN_ERR_DAG_STORE_WRITE;
  }
  if (sp_fmt_io(io, "{} {} {} {} {} {} {} ",
    sp_fmt_str(format_obs_kind(obs->kind)),
    sp_fmt_uint(obs->meta.id.inode),
    sp_fmt_int((s64)obs->meta.mtime.tv_sec),
    sp_fmt_int((s64)obs->meta.mtime.tv_nsec),
    sp_fmt_int(obs->meta.size),
    sp_fmt_str(spn_dag_digest_hex(mem, obs->meta.digest)),
    sp_fmt_uint(prefixed.root)
  )) {
    return SPN_ERR_DAG_STORE_WRITE;
  }
  if (obs->kind == SPN_DAG_OBS_ENUMERATION) {
    spn_try(write_row_str(io, obs->filter));
    spn_try(write_bytes(io, " ", 1));
  }
  spn_try(write_bytes(io, prefixed.sub.data, prefixed.sub.len));
  return write_bytes(io, "\n", 1);
}

static bool parse_obs_row(sp_str_t* cursor, const spn_dag_roots_t* roots, sp_mem_t mem, spn_dag_obs_t* out) {
  sp_str_t kind = sp_zero;
  s64 mtime_s = 0;
  s64 mtime_ns = 0;
  u64 root = 0;
  sp_str_t sub = sp_zero;
  if (!row_word(cursor, &kind) || !parse_obs_kind(kind, &out->kind)) return false;
  if (!row_lit(cursor, ' ')) return false;
  if (!row_u64(cursor, &out->meta.id.inode)) return false;
  if (!row_lit(cursor, ' ')) return false;
  if (!row_s64(cursor, &mtime_s)) return false;
  if (!row_lit(cursor, ' ')) return false;
  if (!row_s64(cursor, &mtime_ns)) return false;
  if (!row_lit(cursor, ' ')) return false;
  if (!row_s64(cursor, &out->meta.size)) return false;
  if (!row_lit(cursor, ' ')) return false;
  if (!row_digest(cursor, &out->meta.digest)) return false;
  if (!row_lit(cursor, ' ')) return false;
  if (!row_u64(cursor, &root) || root >= SPN_DAG_ROOT_COUNT) return false;
  if (!row_lit(cursor, ' ')) return false;
  if (out->kind == SPN_DAG_OBS_ENUMERATION) {
    if (!row_str(cursor, &out->filter)) return false;
    if (!row_lit(cursor, ' ')) return false;
  }
  if (!row_line(cursor, &sub)) return false;

  spn_dag_prefixed_t prefixed = { .root = (spn_dag_root_t)root, .sub = sub };
  if (!spn_dag_root_expand(roots, prefixed, mem, &out->path)) return false;
  if (sp_str_empty(out->path)) return false;

  out->meta.mtime.tv_sec = mtime_s;
  out->meta.mtime.tv_nsec = mtime_ns;
  return true;
}

static spn_err_t write_obs(sp_io_writer_t* io, sp_mem_t mem, const spn_dag_roots_t* roots, const spn_dag_obs_t* obs, u64 count) {
  spn_try(write_header(io));
  sp_for(it, count) {
    spn_try(write_obs_row(io, mem, roots, obs + it));
  }
  return SPN_OK;
}

static bool parse_obs(sp_str_t content, const spn_dag_roots_t* roots, sp_mem_t mem, sp_da(spn_dag_obs_t)* set) {
  sp_str_t cursor = content;
  if (!row_header(&cursor)) {
    return false;
  }
  while (cursor.len) {
    spn_dag_obs_t obs = sp_zero;
    if (!parse_obs_row(&cursor, roots, mem, &obs)) {
      return false;
    }
    sp_da_push(*set, obs);
  }
  return true;
}

static bool load_outputs(sp_str_t dir, spn_dag_digest_t key, sp_mem_t mem, sp_da(spn_dag_action_output_t)* outputs) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_str_t path = entry_path(dir, s.mem, key);
  sp_str_t content = sp_zero;
  bool ok = false;
  if (!sp_io_read_file(mem, path, &content)) {
    ok = parse_outputs(content, outputs);
    if (!ok) {
      sp_fs_remove_file(path);
    }
  }

  sp_mem_end_scratch(s);
  return ok;
}

static void save_outputs(sp_str_t dir, spn_dag_digest_t key, const spn_dag_action_output_t* outputs, u64 count) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_io_dyn_mem_writer_t sink = sp_zero;
  sp_io_dyn_mem_writer_init(s.mem, &sink);
  if (!write_outputs(&sink.base, s.mem, outputs, count)) {
    sp_fs_write_atomic(entry_path(dir, s.mem, key), sp_io_dyn_mem_writer_as_str(&sink));
  }

  sp_mem_end_scratch(s);
}

static bool load_obs(spn_dag_obs_table_t* d, spn_dag_digest_t key, sp_da(spn_dag_obs_t)* set) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_str_t path = entry_path(d->dir, s.mem, key);
  sp_str_t content = sp_zero;
  bool ok = false;
  if (!sp_io_read_file(d->mem, path, &content)) {
    ok = parse_obs(content, d->roots, d->mem, set);
    if (!ok) {
      sp_fs_remove_file(path);
    }
  }

  sp_mem_end_scratch(s);
  return ok;
}

static void save_obs(spn_dag_obs_table_t* d, spn_dag_digest_t key, const spn_dag_obs_t* obs, u64 count) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_io_dyn_mem_writer_t sink = sp_zero;
  sp_io_dyn_mem_writer_init(s.mem, &sink);
  if (!write_obs(&sink.base, s.mem, d->roots, obs, count)) {
    sp_fs_write_atomic(entry_path(d->dir, s.mem, key), sp_io_dyn_mem_writer_as_str(&sink));
  }

  sp_mem_end_scratch(s);
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
  if (!load_outputs(c->dir, key, c->mem, &entry.outputs)) {
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
    save_outputs(c->dir, key, entry.outputs, sp_da_size(entry.outputs));
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

void spn_dag_obs_table_init(spn_dag_obs_table_t* d, sp_mem_t mem, sp_str_t dir, const spn_dag_roots_t* roots) {
  d->arena = sp_mem_arena_new(mem);
  d->mem = sp_mem_arena_as_allocator(d->arena);
  d->dir = sp_str_copy(d->mem, dir);
  d->roots = roots;
  sp_ht_init(d->mem, d->entries);

  if (!sp_str_empty(d->dir)) {
    sp_fs_create_dir(d->dir);
  }
}

spn_dag_pathset_t* spn_dag_obs_table_get(spn_dag_obs_table_t* d, spn_dag_digest_t weak) {
  spn_dag_pathset_t* cached = sp_ht_getp(d->entries, weak);
  if (cached) {
    return cached;
  }

  if (sp_str_empty(d->dir)) {
    return SP_NULLPTR;
  }

  spn_dag_pathset_t set = sp_zero;
  sp_da_init(d->mem, set.obs);
  if (!load_obs(d, weak, &set.obs)) {
    return SP_NULLPTR;
  }

  sp_ht_insert(d->entries, weak, set);
  return sp_ht_getp(d->entries, weak);
}

void spn_dag_obs_table_put(spn_dag_obs_table_t* d, spn_dag_digest_t weak, const spn_dag_obs_t* obs, u32 count) {
  spn_dag_pathset_t set = sp_zero;
  sp_da_init(d->mem, set.obs);
  sp_for(it, count) {
    spn_dag_obs_t copy = obs[it];
    copy.path = sp_str_copy(d->mem, obs[it].path);
    copy.filter = sp_str_copy(d->mem, obs[it].filter);
    sp_da_push(set.obs, copy);
  }
  sp_ht_insert(d->entries, weak, set);
  spn_dag_obs_table_flush(d, weak);
}

void spn_dag_obs_table_flush(spn_dag_obs_table_t* d, spn_dag_digest_t weak) {
  if (sp_str_empty(d->dir)) {
    return;
  }

  spn_dag_pathset_t* set = sp_ht_getp(d->entries, weak);
  if (set) {
    save_obs(d, weak, set->obs, sp_da_size(set->obs));
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

  sp_sys_file_meta_t meta = sp_zero;
  sp_mem_slice_t bytes = sp_zero;
  sp_fs_atomic_t af = sp_zero;
  if (sp_sys_get_path_metadata_s(sp_sys_get_root(0), source, &meta)) {
    err = SPN_ERR_DAG_STORE_READ;
  } else if (sp_io_read_file_slice(s.mem, source, &bytes)) {
    err = SPN_ERR_DAG_STORE_READ;
  } else if (sp_fs_atomic_open_staged(&af, target, staging)) {
    err = SPN_ERR_DAG_STORE_WRITE;
  } else if (sp_io_write_all(sp_fs_atomic_writer(&af), bytes.data, bytes.len, SP_NULLPTR)) {
    sp_fs_atomic_abort(&af);
    err = SPN_ERR_DAG_STORE_WRITE;
  } else {
    sp_sys_chmod_s(af.dir, af.temp, &meta);
    if (sp_fs_atomic_commit(&af, SP_FS_ATOMIC_REPLACE)) {
      err = SPN_ERR_DAG_STORE_WRITE;
    }
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
    err = spn_dag_store_put_file(store, files[it].path, entry.name, &entry.digest);
    if (err) {
      goto done;
    }
    sp_da_push(entries, entry);
  }
  sp_da_sort(entries, tree_entry_order);

  sp_io_dyn_mem_writer_t sink = sp_zero;
  sp_io_dyn_mem_writer_init(s.mem, &sink);
  err = write_outputs(&sink.base, s.mem, entries, sp_da_size(entries));
  if (err) {
    goto done;
  }

  sp_str_t manifest = sp_io_dyn_mem_writer_as_str(&sink);
  err = spn_dag_store_put(store, manifest.data, manifest.len, sp_str_lit("tree"), digest);

done:
  sp_mem_end_scratch(s);
  return err;
}

spn_err_t spn_dag_tree_entries(spn_dag_store_t* store, spn_dag_digest_t digest, sp_mem_t mem, sp_da(spn_dag_action_output_t)* out) {
  sp_mem_slice_t manifest = sp_zero;
  spn_try(spn_dag_store_get(store, digest, sp_str_lit("tree"), mem, &manifest));
  sp_assert(manifest.len <= SP_LIMIT_U32_MAX);

  *out = sp_da_new(mem, spn_dag_action_output_t);
  if (!parse_outputs(sp_str((c8*)manifest.data, (u32)manifest.len), out)) {
    return SPN_ERR_DAG_TREE;
  }
  sp_da_for(*out, it) {
    if (!tree_name_ok((*out)[it].name)) {
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
  err = spn_dag_tree_entries(store, digest, s.mem, &entries);
  if (err) {
    goto done;
  }

  sp_fs_remove_dir(dir);
  sp_fs_create_dir(dir);
  sp_da_for(entries, it) {
    sp_str_t path = sp_fs_join_path(s.mem, dir, entries[it].name);
    err = spn_dag_store_materialize(store, entries[it].digest, entries[it].name, path);
    if (err) {
      goto done;
    }
  }

done:
  sp_mem_end_scratch(s);
  return err;
}
