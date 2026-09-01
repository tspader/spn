#include "dag/dag.h"
#include "dag/types.h"
#include "paths/paths.h"
#include "sp.h"
#include "spn/core.h"
#include "io/io.h"
#include "atomic_file/atomic_file.h"


static c8 format_obs_kind(spn_dag_obs_kind_t kind) {
  switch (kind) {
    case SPN_DAG_OBS_FILE:        return 'f';
    case SPN_DAG_OBS_ABSENT:      return 'a';
    case SPN_DAG_OBS_ENUMERATION: return 'e';
  }
  SP_UNREACHABLE_RETURN('f');
}

static bool parse_obs_kind(c8 c, spn_dag_obs_kind_t* out) {
  switch (c) {
    case 'f': *out = SPN_DAG_OBS_FILE; return true;
    case 'a': *out = SPN_DAG_OBS_ABSENT; return true;
    case 'e': *out = SPN_DAG_OBS_ENUMERATION; return true;
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

static bool row_line(sp_str_t* cursor, sp_str_t* out) {
  u32 len = 0;
  while (len < cursor->len && cursor->data[len] != '\n') {
    len++;
  }
  return row_bytes(cursor, len, out) && row_lit(cursor, '\n');
}

static bool row_header(sp_str_t* cursor, c8 version) {
  return row_lit(cursor, version) && row_lit(cursor, '\n');
}

static spn_err_t write_bytes(sp_io_writer_t* io, const void* data, u64 len) {
  return sp_io_write(io, data, len, SP_NULLPTR) ? SPN_ERR_DAG_STORE_WRITE : SPN_OK;
}

static spn_err_t write_header(sp_io_writer_t* io, c8 version) {
  c8 header [2] = { version, '\n' };
  return write_bytes(io, header, sizeof(header));
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
  spn_try(write_header(io, '2'));
  sp_for(it, count) {
    spn_try(write_output_row(io, mem, outputs + it));
  }
  return SPN_OK;
}

static bool parse_outputs(sp_str_t content, sp_da(spn_dag_action_output_t)* outputs) {
  sp_str_t cursor = content;
  if (!row_header(&cursor, '2')) {
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

static spn_err_t write_obs_row(sp_io_writer_t* io, const spn_dag_obs_t* obs) {
  if (sp_str_contains(obs->path.sub, sp_str_lit("\n"))) {
    return SPN_ERR_DAG_STORE_WRITE;
  }
  c8 kind [2] = { format_obs_kind(obs->kind), ' ' };
  spn_try(write_bytes(io, kind, sizeof(kind)));
  if (sp_fmt_io(io, "{} ", sp_fmt_uint(obs->path.root))) {
    return SPN_ERR_DAG_STORE_WRITE;
  }
  if (obs->kind == SPN_DAG_OBS_ENUMERATION) {
    spn_try(write_row_str(io, obs->filter));
    spn_try(write_bytes(io, " ", 1));
  }
  spn_try(write_bytes(io, obs->path.sub.data, obs->path.sub.len));
  return write_bytes(io, "\n", 1);
}

static bool parse_obs_row(sp_str_t* cursor, spn_dag_obs_t* out) {
  sp_str_t kind = sp_zero;
  u64 root = 0;
  sp_str_t sub = sp_zero;
  if (!row_bytes(cursor, 1, &kind) || !parse_obs_kind(kind.data[0], &out->kind)) return false;
  if (!row_lit(cursor, ' ')) return false;
  if (!row_u64(cursor, &root) || root >= SPN_PATH_ROOT_COUNT) return false;
  if (!row_lit(cursor, ' ')) return false;
  if (out->kind == SPN_DAG_OBS_ENUMERATION) {
    if (!row_str(cursor, &out->filter)) return false;
    if (!row_lit(cursor, ' ')) return false;
  }
  if (!row_line(cursor, &sub)) return false;
  if (root == SPN_PATH_ROOT_NONE && !sp_fs_is_absolute(sub)) return false;

  out->path = (spn_path_t) { .root = (spn_path_root_t)root, .sub = sub };
  return true;
}

#define obs_version '7'

static spn_err_t write_obs(sp_io_writer_t* io, sp_mem_t mem, const spn_dag_pathset_t* set) {
  spn_try(write_header(io, obs_version));
  if (sp_fmt_io(io, "{}\n", sp_fmt_str(spn_dag_digest_hex(mem, set->pinned)))) {
    return SPN_ERR_DAG_STORE_WRITE;
  }
  sp_da_for(set->obs, it) {
    spn_try(write_obs_row(io, set->obs + it));
  }
  return SPN_OK;
}

static bool parse_obs(sp_str_t content, spn_dag_pathset_t* set) {
  sp_str_t cursor = content;
  if (!row_header(&cursor, obs_version)) {
    return false;
  }
  if (!row_digest(&cursor, &set->pinned)) {
    return false;
  }
  if (!row_lit(&cursor, '\n')) {
    return false;
  }
  while (cursor.len) {
    spn_dag_obs_t obs = sp_zero;
    if (!parse_obs_row(&cursor, &obs)) {
      return false;
    }
    sp_da_push(set->obs, obs);
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

static bool load_obs(spn_dag_obs_table_t* d, spn_dag_digest_t key, spn_dag_pathset_t* set) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_str_t path = entry_path(d->dir, s.mem, key);
  sp_str_t content = sp_zero;
  bool ok = false;
  if (!sp_io_read_file(d->mem, path, &content)) {
    ok = parse_obs(content, set);
    if (!ok) {
      sp_fs_remove_file(path);
    }
  }

  sp_mem_end_scratch(s);
  return ok;
}

static void save_obs(spn_dag_obs_table_t* d, spn_dag_digest_t key, const spn_dag_pathset_t* set) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_io_dyn_mem_writer_t sink = sp_zero;
  sp_io_dyn_mem_writer_init(s.mem, &sink);
  if (!write_obs(&sink.base, s.mem, set)) {
    sp_fs_write_atomic(entry_path(d->dir, s.mem, key), sp_io_dyn_mem_writer_as_str(&sink));
    if (d->stats) {
      sp_atomic_u32_add(&d->stats->cache_writes, 1, SP_ATOMIC_RELAXED);
    }
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

bool spn_dag_action_cache_get(spn_dag_action_cache_t* c, spn_dag_digest_t key, spn_dag_action_entry_t* out) {
  sp_mutex_lock(&c->mutex);
  const spn_dag_action_entry_t* cached = sp_ht_getp(c->entries, key);
  if (cached) {
    *out = *cached;
    sp_mutex_unlock(&c->mutex);
    return true;
  }

  if (sp_str_empty(c->dir)) {
    sp_mutex_unlock(&c->mutex);
    return false;
  }

  spn_dag_action_entry_t entry = sp_zero;
  sp_da_init(c->mem, entry.outputs);
  if (!load_outputs(c->dir, key, c->mem, &entry.outputs)) {
    sp_mutex_unlock(&c->mutex);
    return false;
  }
  if (c->stats) {
    sp_atomic_u32_add(&c->stats->cache_reads, 1, SP_ATOMIC_RELAXED);
  }

  sp_ht_insert(c->entries, key, entry);
  *out = entry;
  sp_mutex_unlock(&c->mutex);
  return true;
}

void spn_dag_action_cache_put(spn_dag_action_cache_t* c, spn_dag_digest_t key, const spn_dag_action_output_t* outputs, u32 count) {
  sp_mutex_lock(&c->mutex);
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
    if (c->stats) {
      sp_atomic_u32_add(&c->stats->cache_writes, 1, SP_ATOMIC_RELAXED);
    }
  }
  sp_mutex_unlock(&c->mutex);
}

bool spn_dag_action_cache_remove(spn_dag_action_cache_t* c, spn_dag_digest_t key) {
  bool removed = false;
  sp_mutex_lock(&c->mutex);

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

  sp_mutex_unlock(&c->mutex);
  return removed;
}

void spn_dag_obs_table_init(spn_dag_obs_table_t* d, sp_mem_t mem, const spn_path_roots_t* roots, sp_str_t dir) {
  d->arena = sp_mem_arena_new(mem);
  d->mem = sp_mem_arena_as_allocator(d->arena);
  d->roots = roots;
  d->dir = sp_str_copy(d->mem, dir);
  sp_ht_init(d->mem, d->entries);

  if (!sp_str_empty(d->dir)) {
    sp_fs_create_dir(d->dir);
  }
}

bool spn_dag_obs_table_get(spn_dag_obs_table_t* d, spn_dag_digest_t weak, spn_dag_pathset_t* set) {
  sp_mutex_lock(&d->mutex);
  spn_dag_pathset_t* cached = sp_ht_getp(d->entries, weak);
  if (cached) {
    *set = *cached;
    sp_mutex_unlock(&d->mutex);
    return true;
  }

  if (sp_str_empty(d->dir)) {
    sp_mutex_unlock(&d->mutex);
    return false;
  }

  spn_dag_pathset_t loaded = sp_zero;
  sp_da_init(d->mem, loaded.obs);
  if (!load_obs(d, weak, &loaded)) {
    sp_mutex_unlock(&d->mutex);
    return false;
  }
  if (d->stats) {
    sp_atomic_u32_add(&d->stats->cache_reads, 1, SP_ATOMIC_RELAXED);
    sp_atomic_u32_add(&d->stats->obs_rows, (u32)sp_da_size(loaded.obs), SP_ATOMIC_RELAXED);
  }

  sp_ht_insert(d->entries, weak, loaded);
  *set = loaded;
  sp_mutex_unlock(&d->mutex);
  return true;
}

spn_dag_pathset_t spn_dag_obs_table_put(spn_dag_obs_table_t* d, spn_dag_digest_t weak, const spn_dag_obs_t* obs, u32 count) {
  sp_mutex_lock(&d->mutex);
  spn_dag_pathset_t set = sp_zero;
  set.pinned = spn_dag_pinned_digest(d->roots->pinned, obs, count);
  sp_da_init(d->mem, set.obs);
  sp_for(it, count) {
    if (d->roots->pinned & spn_path_root_mask(obs[it].path.root)) {
      continue;
    }
    spn_dag_obs_t copy = obs[it];
    copy.path.sub = sp_str_copy(d->mem, obs[it].path.sub);
    copy.filter = sp_str_copy(d->mem, obs[it].filter);
    sp_da_push(set.obs, copy);
  }
  sp_ht_insert(d->entries, weak, set);

  if (!sp_str_empty(d->dir)) {
    save_obs(d, weak, &set);
  }
  sp_mutex_unlock(&d->mutex);
  return set;
}

static spn_err_t write_hint_row(sp_io_writer_t* io, sp_mem_t mem, spn_path_t path, const spn_dag_file_meta_t* meta) {
  if (sp_fmt_io(io, "{} {} {} {} {} {} ",
    sp_fmt_uint(meta->id.inode),
    sp_fmt_int((s64)meta->mtime.tv_sec),
    sp_fmt_int((s64)meta->mtime.tv_nsec),
    sp_fmt_int(meta->size),
    sp_fmt_str(spn_dag_digest_hex(mem, meta->digest)),
    sp_fmt_uint(path.root)
  )) {
    return SPN_ERR_DAG_STORE_WRITE;
  }
  spn_try(write_bytes(io, path.sub.data, path.sub.len));
  return write_bytes(io, "\n", 1);
}

static bool parse_hint_row(sp_str_t* cursor, spn_path_t* path, spn_dag_file_meta_t* meta) {
  s64 mtime_s = 0;
  s64 mtime_ns = 0;
  u64 root = 0;
  sp_str_t sub = sp_zero;
  if (!row_u64(cursor, &meta->id.inode)) return false;
  if (!row_lit(cursor, ' ')) return false;
  if (!row_s64(cursor, &mtime_s)) return false;
  if (!row_lit(cursor, ' ')) return false;
  if (!row_s64(cursor, &mtime_ns)) return false;
  if (!row_lit(cursor, ' ')) return false;
  if (!row_s64(cursor, &meta->size)) return false;
  if (!row_lit(cursor, ' ')) return false;
  if (!row_digest(cursor, &meta->digest)) return false;
  if (!row_lit(cursor, ' ')) return false;
  if (!row_u64(cursor, &root) || root >= SPN_PATH_ROOT_COUNT) return false;
  if (!row_lit(cursor, ' ')) return false;
  if (!row_line(cursor, &sub)) return false;
  if (root == SPN_PATH_ROOT_NONE && !sp_fs_is_absolute(sub)) return false;

  *path = (spn_path_t) { .root = (spn_path_root_t)root, .sub = sub };

  meta->mtime.tv_sec = mtime_s;
  meta->mtime.tv_nsec = mtime_ns;
  return true;
}

void spn_dag_file_cache_load(spn_dag_file_cache_t* c, sp_str_t path) {
  sp_str_t content = sp_zero;
  if (sp_io_read_file(c->mem, path, &content)) {
    return;
  }
  if (c->stats) {
    sp_atomic_u32_add(&c->stats->cache_reads, 1, SP_ATOMIC_RELAXED);
  }

  sp_str_t cursor = content;
  if (!row_header(&cursor, '3')) {
    sp_fs_remove_file(path);
    return;
  }
  while (cursor.len) {
    spn_path_t row_path = sp_zero;
    spn_dag_file_meta_t meta = sp_zero;
    if (!parse_hint_row(&cursor, &row_path, &meta)) {
      sp_fs_remove_file(path);
      return;
    }
    sp_ht_insert(c->hints, row_path, meta);
  }
}

void spn_dag_file_cache_flush(spn_dag_file_cache_t* c, sp_str_t path) {
  if (!c->hints_dirty) {
    return;
  }
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_io_dyn_mem_writer_t sink = sp_zero;
  sp_io_dyn_mem_writer_init(s.mem, &sink);
  spn_err_t err = write_header(&sink.base, '3');
  sp_ht_for_kv(c->hints, it) {
    if (err) {
      break;
    }
    if (sp_str_contains(it.key->sub, sp_str_lit("\n"))) {
      continue;
    }
    err = write_hint_row(&sink.base, s.mem, *it.key, it.val);
  }
  if (!err) {
    sp_fs_write_atomic(path, sp_io_dyn_mem_writer_as_str(&sink));
    c->hints_dirty = false;
    if (c->stats) {
      sp_atomic_u32_add(&c->stats->cache_writes, 1, SP_ATOMIC_RELAXED);
    }
  }

  sp_mem_end_scratch(s);
}

static sp_str_t get_blob_name(sp_str_t name) {
  sp_str_t base = sp_fs_get_name(name);
  return sp_str_empty(base) ? sp_str_lit("blob") : base;
}

static spn_path_t get_blob_dir(spn_dag_store_t* store, sp_mem_t mem, spn_dag_digest_t digest) {
  return spn_path_join(mem, store->dir, spn_dag_digest_hex(mem, digest));
}

static spn_path_t get_blob(spn_dag_store_t* store, sp_mem_t mem, spn_dag_digest_t digest, sp_str_t name) {
  return spn_path_join(mem, get_blob_dir(store, mem, digest), get_blob_name(name));
}

static sp_str_t get_blob_path(spn_dag_store_t* store, sp_mem_t mem, spn_dag_digest_t digest, sp_str_t name) {
  return spn_path_str(store->roots, mem, get_blob(store, mem, digest, name));
}

static sp_str_t get_staging_dir(spn_dag_store_t* store, sp_mem_t mem) {
  return spn_path_str(store->roots, mem, spn_path_join(mem, store->dir, sp_str_lit(".staging")));
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
  store->roots = config.roots;
  store->dir = spn_path_copy(store->mem, config.dir);

  switch (store->kind) {
    case SPN_DAG_STORE_MEM: {
      sp_ht_init(store->mem, store->blobs);
      break;
    }
    case SPN_DAG_STORE_FILESYSTEM: {
      sp_mem_arena_marker_t s = sp_mem_begin_scratch();
      sp_fs_create_dir(spn_path_str(store->roots, s.mem, store->dir));
      sp_mem_end_scratch(s);
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
        sp_fs_create_dir(spn_path_str(store->roots, s.mem, get_blob_dir(store, s.mem, *digest)));
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
      u64 size = 0;
      if (spn_digest_file(SPN_DIGEST_BLAKE3, path, digest->bytes, &size)) {
        return SPN_ERR_DAG_STORE_READ;
      }
      if (store->stats) {
        sp_atomic_u32_add(&store->stats->hashed_files, 1, SP_ATOMIC_RELAXED);
        sp_atomic_u64_add(&store->stats->hashed_bytes, size, SP_ATOMIC_RELAXED);
      }

      sp_mem_arena_marker_t s = sp_mem_begin_scratch();
      spn_err_t err = SPN_OK;
      sp_str_t blob = get_blob_path(store, s.mem, *digest, name);
      if (!sp_fs_is_file(blob)) {
        sp_fs_create_dir(spn_path_str(store->roots, s.mem, get_blob_dir(store, s.mem, *digest)));
        err = link_into_store(path, blob, get_staging_dir(store, s.mem));
      }
      sp_mem_end_scratch(s);
      return err;
    }
  }

  SP_UNREACHABLE_RETURN(SPN_ERROR);
}

spn_path_t spn_dag_store_path(spn_dag_store_t* store, sp_mem_t mem, spn_dag_digest_t digest, sp_str_t name) {
  switch (store->kind) {
    case SPN_DAG_STORE_MEM: {
      return (spn_path_t) sp_zero;
    }
    case SPN_DAG_STORE_FILESYSTEM: {
      sp_mem_arena_marker_t s = sp_mem_begin_scratch();
      spn_path_t blob = get_blob(store, mem, digest, name);
      bool present = sp_fs_is_file(spn_path_str(store->roots, s.mem, blob));
      sp_mem_end_scratch(s);
      return present ? blob : (spn_path_t) sp_zero;
    }
  }

  SP_UNREACHABLE_RETURN((spn_path_t) sp_zero);
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
  sp_da(sp_fs_entry_t) files = sp_zero;
  sp_fs_collect_recursive(s.mem, dir, &files);
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
