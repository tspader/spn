#include "dag/dag.h"
#include "error/types.h"
#include "sp.h"
#include "sp/io.h"
#include "dag_file.gen.h"
#include "dag_action.gen.h"
#include "dag_pathset.gen.h"

static sp_str_t emit_u64(sp_mem_t mem, u64 value) {
  return sp_fmt(mem, "{}", sp_fmt_uint(value)).value;
}

static sp_str_t emit_s64(sp_mem_t mem, s64 value) {
  return sp_fmt(mem, "{}", sp_fmt_int(value)).value;
}

static bool lower_u64(sp_str_t str, u64* out) {
  if (sp_str_empty(str)) {
    return false;
  }
  u64 value = 0;
  sp_for(it, str.len) {
    c8 c = str.data[it];
    if (c < '0' || c > '9') {
      return false;
    }
    value = value * 10 + (u64)(c - '0');
  }
  *out = value;
  return true;
}

static bool lower_s64(sp_str_t str, s64* out) {
  bool negative = !sp_str_empty(str) && str.data[0] == '-';
  u64 magnitude = 0;
  if (!lower_u64(negative ? sp_str_sub(str, 1, str.len - 1) : str, &magnitude)) {
    return false;
  }
  *out = negative ? -(s64)magnitude : (s64)magnitude;
  return true;
}

static bool lower_digest(sp_str_t hex, spn_dag_digest_t* out) {
  if (hex.len != 2 * sizeof(out->bytes)) {
    return false;
  }
  sp_for(it, sizeof(out->bytes)) {
    u8 byte = 0;
    sp_for(n, 2) {
      c8 c = hex.data[2 * it + n];
      u8 nibble = 0;
      if (c >= '0' && c <= '9') {
        nibble = (u8)(c - '0');
      } else if (c >= 'a' && c <= 'f') {
        nibble = (u8)(c - 'a' + 10);
      } else {
        return false;
      }
      byte = (u8)(byte << 4) | nibble;
    }
    out->bytes[it] = byte;
  }
  return true;
}

static sp_str_t emit_obs_kind(spn_dag_obs_kind_t kind) {
  switch (kind) {
    case SPN_DAG_OBS_FILE:   return sp_str_lit("file");
    case SPN_DAG_OBS_ABSENT: return sp_str_lit("absent");
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

static bool lower_obs_kind(sp_str_t str, spn_dag_obs_kind_t* out) {
  if (sp_str_equal(str, sp_str_lit("file"))) {
    *out = SPN_DAG_OBS_FILE;
    return true;
  }
  if (sp_str_equal(str, sp_str_lit("absent"))) {
    *out = SPN_DAG_OBS_ABSENT;
    return true;
  }
  return false;
}

static spn_err_t emit_line(sp_io_writer_t* io, sp_str_t line) {
  spn_try_as(sp_io_write(io, line.data, line.len, SP_NULLPTR), SPN_ERROR);
  spn_try_as(sp_io_write(io, "\n", 1, SP_NULLPTR), SPN_ERROR);
  return SPN_OK;
}

static spn_err_t read_lines(sp_mem_t mem, sp_str_t path, sp_da(sp_str_t)* out) {
  sp_str_t content = sp_zero;
  spn_try_as(sp_io_read_file(mem, path, &content), SPN_ERROR);

  *out = sp_da_new(mem, sp_str_t);
  sp_da(sp_str_t) lines = sp_str_split_c8(mem, content, '\n');
  sp_da_for(lines, it) {
    if (!sp_str_empty(sp_str_trim(lines[it]))) {
      sp_da_push(*out, lines[it]);
    }
  }
  return SPN_OK;
}

spn_err_t spn_dag_file_cache_save(spn_dag_file_cache_t* c, sp_str_t path) {
  sp_fs_remove_file(path);

  sp_io_file_writer_t writer = sp_zero;
  sp_try_as(sp_io_file_writer_from_path(&writer, path), SPN_ERROR);

  spn_err_t err = SPN_OK;
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_ht_for(c->entries, it) {
    spn_dag_file_meta_t* entry = sp_ht_it_getp(c->entries, it);
    spn_cg_dag_file_t cg = {
      .device = emit_u64(s.mem, entry->id.device),
      .inode = emit_u64(s.mem, entry->id.id),
      .mtime_s = emit_s64(s.mem, (s64)entry->mtime.tv_sec),
      .mtime_ns = emit_s64(s.mem, (s64)entry->mtime.tv_nsec),
      .size = emit_s64(s.mem, entry->size),
      .digest = spn_dag_digest_hex(s.mem, entry->digest),
    };
    if ((err = emit_line(&writer.base, spn_dag_file_write_compact(s.mem, &cg)))) {
      break;
    }
  }

  sp_mem_end_scratch(s);
  sp_io_file_writer_close(&writer);
  return err;
}

spn_err_t spn_dag_file_cache_load(spn_dag_file_cache_t* c, sp_str_t path) {
  spn_err_t err = SPN_OK;
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_da(sp_str_t) lines = sp_zero;
  spn_try_goto(read_lines(s.mem, path, &lines), err, done);

  sp_da_for(lines, it) {
    spn_cg_dag_file_t cg = sp_zero;
    spn_dag_file_meta_t entry = sp_zero;
    s64 mtime_s = 0;
    s64 mtime_ns = 0;
    if (!spn_dag_file_read(lines[it], &cg, s.mem)
      || !lower_u64(cg.device, &entry.id.device)
      || !lower_u64(cg.inode, &entry.id.id)
      || !lower_s64(cg.mtime_s, &mtime_s)
      || !lower_s64(cg.mtime_ns, &mtime_ns)
      || !lower_s64(cg.size, &entry.size)
      || !lower_digest(cg.digest, &entry.digest)) {
      sp_ht_clear(c->entries);
      err = SPN_ERROR;
      goto done;
    }
    entry.mtime.tv_sec = mtime_s;
    entry.mtime.tv_nsec = mtime_ns;
    sp_ht_insert(c->entries, entry.id, entry);
  }

done:
  sp_mem_end_scratch(s);
  return err;
}

spn_err_t spn_dag_action_cache_save(spn_dag_action_cache_t* c, sp_str_t path) {
  sp_fs_remove_file(path);

  sp_io_file_writer_t writer = sp_zero;
  sp_try_as(sp_io_file_writer_from_path(&writer, path), SPN_ERROR);

  spn_err_t err = SPN_OK;
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_ht_for(c->entries, it) {
    spn_dag_digest_t* key = sp_ht_it_getkp(c->entries, it);
    spn_dag_action_entry_t* entry = sp_ht_it_getp(c->entries, it);
    spn_cg_dag_action_t cg = {
      .key = spn_dag_digest_hex(s.mem, *key),
      .outputs = sp_da_new(s.mem, spn_cg_dag_output_t),
    };
    sp_da_for(entry->outputs, o) {
      sp_da_push(cg.outputs, ((spn_cg_dag_output_t) {
        .path = entry->outputs[o].path,
        .digest = spn_dag_digest_hex(s.mem, entry->outputs[o].digest),
      }));
    }
    if ((err = emit_line(&writer.base, spn_dag_action_write_compact(s.mem, &cg)))) {
      break;
    }
  }

  sp_mem_end_scratch(s);
  sp_io_file_writer_close(&writer);
  return err;
}

spn_err_t spn_dag_action_cache_load(spn_dag_action_cache_t* c, sp_str_t path) {
  spn_err_t err = SPN_OK;
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_da(sp_str_t) lines = sp_zero;
  spn_try_goto(read_lines(s.mem, path, &lines), err, done);

  sp_da_for(lines, it) {
    spn_cg_dag_action_t cg = sp_zero;
    spn_dag_digest_t key = sp_zero;
    if (!spn_dag_action_read(lines[it], &cg, s.mem) || !lower_digest(cg.key, &key)) {
      sp_ht_clear(c->entries);
      err = SPN_ERROR;
      goto done;
    }

    spn_dag_action_entry_t entry = sp_zero;
    sp_da_init(c->mem, entry.outputs);
    sp_da_for(cg.outputs, o) {
      spn_dag_action_output_t out = { .path = sp_str_copy(c->mem, cg.outputs[o].path) };
      if (!lower_digest(cg.outputs[o].digest, &out.digest)) {
        sp_ht_clear(c->entries);
        err = SPN_ERROR;
        goto done;
      }
      sp_da_push(entry.outputs, out);
    }
    sp_ht_insert(c->entries, key, entry);
  }

done:
  sp_mem_end_scratch(s);
  return err;
}

spn_err_t spn_dag_discovery_save(spn_dag_discovery_t* d, sp_str_t path) {
  sp_fs_remove_file(path);

  sp_io_file_writer_t writer = sp_zero;
  sp_try_as(sp_io_file_writer_from_path(&writer, path), SPN_ERROR);

  spn_err_t err = SPN_OK;
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_ht_for(d->entries, it) {
    spn_dag_digest_t* prelim = sp_ht_it_getkp(d->entries, it);
    spn_dag_pathset_t* set = sp_ht_it_getp(d->entries, it);
    spn_cg_dag_pathset_t cg = {
      .prelim = spn_dag_digest_hex(s.mem, *prelim),
      .obs = sp_da_new(s.mem, spn_cg_dag_obs_t),
    };
    sp_da_for(set->obs, o) {
      sp_da_push(cg.obs, ((spn_cg_dag_obs_t) {
        .kind = emit_obs_kind(set->obs[o].kind),
        .path = set->obs[o].path,
      }));
    }
    if ((err = emit_line(&writer.base, spn_dag_pathset_write_compact(s.mem, &cg)))) {
      break;
    }
  }

  sp_mem_end_scratch(s);
  sp_io_file_writer_close(&writer);
  return err;
}

spn_err_t spn_dag_discovery_load(spn_dag_discovery_t* d, sp_str_t path) {
  spn_err_t err = SPN_OK;
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_da(sp_str_t) lines = sp_zero;
  spn_try_goto(read_lines(s.mem, path, &lines), err, done);

  sp_da_for(lines, it) {
    spn_cg_dag_pathset_t cg = sp_zero;
    spn_dag_digest_t prelim = sp_zero;
    if (!spn_dag_pathset_read(lines[it], &cg, s.mem) || !lower_digest(cg.prelim, &prelim)) {
      sp_ht_clear(d->entries);
      err = SPN_ERROR;
      goto done;
    }

    spn_dag_pathset_t set = sp_zero;
    sp_da_init(d->mem, set.obs);
    sp_da_for(cg.obs, o) {
      spn_dag_obs_t obs = { .path = sp_str_copy(d->mem, cg.obs[o].path) };
      if (!lower_obs_kind(cg.obs[o].kind, &obs.kind)) {
        sp_ht_clear(d->entries);
        err = SPN_ERROR;
        goto done;
      }
      sp_da_push(set.obs, obs);
    }
    sp_ht_insert(d->entries, prelim, set);
  }

done:
  sp_mem_end_scratch(s);
  return err;
}
