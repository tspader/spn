#include "toolchain/probe.h"

#include "ctx/types.h"
#include "error/error.h"
#include "paths/paths.h"

#if defined(SP_WIN32)
  #define SPN_PROBE_PATH_SEP ';'
#else
  #define SPN_PROBE_PATH_SEP ':'
#endif

#define SPN_PROBE_CACHE_HEADER "spn-probe-cache 1"

SP_PRIVATE bool is_path_name(sp_str_t name) {
  const c8* path = "path";
  if (name.len != 4) {
    return false;
  }
  sp_for(it, name.len) {
    c8 c = name.data[it];
    if (c >= 'A' && c <= 'Z') {
      c = (c8)(c + ('a' - 'A'));
    }
    if (c != path[it]) {
      return false;
    }
  }
  return true;
}

sp_str_t spn_probe_env_path(sp_env_t* env) {
  sp_str_t exact = sp_env_get(env, sp_str_lit("PATH"));
  if (!sp_str_empty(exact)) {
    return exact;
  }
  if (!env->vars) {
    return sp_str_lit("");
  }
  sp_ht_for_kv(env->vars, it) {
    if (is_path_name(*it.key)) {
      return *it.val;
    }
  }
  return sp_str_lit("");
}

sp_da(sp_str_t) spn_probe_split_path(sp_mem_t mem, sp_str_t path) {
  sp_da(sp_str_t) dirs = sp_da_new(mem, sp_str_t);
  sp_str_t remaining = path;
  while (remaining.len) {
    s32 sep = sp_str_find_c8(remaining, SPN_PROBE_PATH_SEP);
    sp_str_t entry = sep < 0 ? remaining : sp_str_prefix(remaining, sep);
    if (!sp_str_empty(entry)) {
      sp_da_push(dirs, entry);
    }
    if (sep < 0) {
      break;
    }
    remaining = sp_str_suffix(remaining, remaining.len - sep - 1);
  }
  return dirs;
}

SP_PRIVATE sp_str_t next_field(sp_str_t* line) {
  s32 sep = sp_str_find_c8(*line, ' ');
  if (sep < 0) {
    sp_str_t field = *line;
    *line = sp_str_lit("");
    return field;
  }
  sp_str_t field = sp_str_prefix(*line, sep);
  *line = sp_str_suffix(*line, line->len - sep - 1);
  return field;
}

SP_PRIVATE sp_str_t next_line(sp_str_t* remaining) {
  s32 sep = sp_str_find_c8(*remaining, '\n');
  if (sep < 0) {
    sp_str_t line = *remaining;
    *remaining = sp_str_lit("");
    return line;
  }
  sp_str_t line = sp_str_prefix(*remaining, sep);
  *remaining = sp_str_suffix(*remaining, remaining->len - sep - 1);
  return line;
}

void spn_probe_cache_load(spn_probe_cache_t* cache, sp_str_t file, sp_mem_t mem) {
  *cache = sp_zero_s(spn_probe_cache_t);
  cache->mem = mem;
  cache->file = file;
  sp_str_om_init(cache->entries);

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_str_t contents = sp_zero;
  if (sp_io_read_file(scratch.mem, file, &contents)) {
    sp_mem_end_scratch(scratch);
    return;
  }

  sp_str_t remaining = contents;
  if (!sp_str_equal_cstr(next_line(&remaining), SPN_PROBE_CACHE_HEADER)) {
    sp_mem_end_scratch(scratch);
    return;
  }

  while (remaining.len) {
    sp_str_t line = next_line(&remaining);
    if (sp_str_empty(line)) {
      continue;
    }
    spn_probe_entry_t entry = sp_zero;
    entry.size = sp_parse_u64(next_field(&line));
    entry.mtime.s = sp_parse_u64(next_field(&line));
    entry.mtime.ns = sp_parse_u32(next_field(&line));
    entry.hash = sp_parse_u64(next_field(&line));
    if (sp_str_empty(line)) {
      continue;
    }
    entry.path = sp_str_copy(cache->mem, line);
    sp_str_om_insert(cache->entries, entry.path, entry);
  }
  sp_mem_end_scratch(scratch);
}

spn_err_t spn_probe_cache_flush(spn_probe_cache_t* cache) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_str_t contents = sp_str_lit(SPN_PROBE_CACHE_HEADER "\n");
  sp_om_for(cache->entries, it) {
    spn_probe_entry_t* entry = sp_om_at(cache->entries, it);
    contents = sp_fmt(scratch.mem, "{}{} {} {} {} {}\n",
      sp_fmt_str(contents),
      sp_fmt_uint(entry->size),
      sp_fmt_uint(entry->mtime.s),
      sp_fmt_uint(entry->mtime.ns),
      sp_fmt_uint(entry->hash),
      sp_fmt_str(entry->path)).value;
  }
  spn_err_t result = sp_fs_create_file_str(cache->file, contents) ? SPN_ERROR : SPN_OK;
  sp_mem_end_scratch(scratch);
  return result;
}

SP_PRIVATE sp_str_t probe_file(sp_mem_t mem, sp_str_t candidate) {
  if (sp_fs_is_target_file(candidate)) {
    return candidate;
  }
#if defined(SP_WIN32)
  sp_str_t exe = sp_fmt(mem, "{}.exe", sp_fmt_str(candidate)).value;
  if (sp_fs_is_target_file(exe)) {
    return exe;
  }
#endif
  return sp_str_lit("");
}

SP_PRIVATE bool is_pathless(sp_str_t program) {
  sp_for(it, program.len) {
    if (sp_fs_is_sep(program.data[it])) {
      return false;
    }
  }
  return true;
}

SP_PRIVATE sp_str_t probe_resolve(sp_mem_t mem, sp_str_t program, sp_da(sp_str_t) dirs) {
  if (!is_pathless(program)) {
    return probe_file(mem, program);
  }
  sp_da_for(dirs, it) {
    sp_str_t resolved = probe_file(mem, sp_fs_join_path(mem, dirs[it], program));
    if (!sp_str_empty(resolved)) {
      return resolved;
    }
  }
  return sp_str_lit("");
}

SP_PRIVATE spn_err_t probe_hash(spn_probe_cache_t* cache, sp_str_t path, sp_hash_t* hash) {
  sp_sys_file_meta_t meta = sp_zero;
  if (sp_sys_get_path_metadata_s(sp_sys_get_root(0), path, &meta)) {
    return SPN_ERROR;
  }

  spn_probe_entry_t** cached = sp_str_om_getp(cache->entries, path);
  if (cached
      && (*cached)->size == (u64)meta.size
      && (*cached)->mtime.s == (u64)meta.mtime.tv_sec
      && (*cached)->mtime.ns == (u32)meta.mtime.tv_nsec) {
    *hash = (*cached)->hash;
    return SPN_OK;
  }

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_str_t contents = sp_zero;
  spn_err_t err = sp_io_read_file(scratch.mem, path, &contents) ? SPN_ERROR : SPN_OK;
  sp_hash_t computed = err ? 0 : sp_hash_bytes(contents.data, contents.len, 0);
  sp_mem_end_scratch(scratch);
  if (err) {
    return SPN_ERROR;
  }

  spn_probe_entry_t entry = {
    .size = (u64)meta.size,
    .mtime = { .s = (u64)meta.mtime.tv_sec, .ns = (u32)meta.mtime.tv_nsec },
    .hash = computed,
  };
  if (cached) {
    entry.path = (*cached)->path;
    **cached = entry;
  } else {
    entry.path = sp_str_copy(cache->mem, path);
    sp_str_om_insert(cache->entries, entry.path, entry);
  }

  *hash = computed;
  return SPN_OK;
}

spn_err_t spn_toolchain_probe(spn_cc_toolchain_t* cc, sp_da(sp_str_t) dirs, spn_probe_cache_t* cache, sp_mem_t mem, sp_hash_t* identity) {
  *identity = 0;

  spn_toolchain_launcher_t* launchers [] = { &cc->compiler, &cc->linker, &cc->archiver, &cc->cxx };
  sp_hash_t hashes [sp_carr_len(launchers)] = sp_zero;
  u32 num_hashes = 0;

  sp_carr_for(launchers, it) {
    spn_toolchain_launcher_t* launcher = launchers[it];
    if (spn_arg_empty(launcher->program)) {
      continue;
    }

    sp_str_t program = launcher->program.prefix;
    sp_str_t resolved = probe_resolve(mem, program, dirs);
    sp_hash_t hash = 0;
    if (sp_str_empty(resolved) || probe_hash(cache, resolved, &hash)) {
      if (launcher == &cc->cxx) {
        *launcher = sp_zero_s(spn_toolchain_launcher_t);
        continue;
      }
      return spn_err_emit(&spn, (spn_err_union_t) {
        .kind = SPN_ERR_TOOLCHAIN_MISSING,
        .program = {
          .name = cc->name,
          .program = program,
        },
      });
    }

    launcher->program = spn_arg_lit(resolved);
    hashes[num_hashes++] = hash;
  }

  *identity = sp_hash_combine(hashes, num_hashes);
  return SPN_OK;
}
