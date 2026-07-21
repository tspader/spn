#include "sp.h"
#include "sp/macro.h"
#include "git/cache.h"

#include "external/git.h"
#include "git/key.h"
#include "sp/fs.h"

void spn_git_cache_init(spn_git_cache_t* cache, sp_mem_t mem, sp_intern_t* intern, sp_str_t root) {
  *cache = (spn_git_cache_t) {
    .mem = mem,
    .intern = intern,
    .root = root,
    .db.dir = sp_fs_join_path(mem, root, SP_LIT("db")),
    .checkouts.dir = sp_fs_join_path(mem, root, SP_LIT("checkouts")),
  };

  sp_str_ht_init(mem, cache->db.entries);
  sp_str_om_init(cache->checkouts.entries);
  sp_mutex_init(&cache->mutex, SP_MUTEX_PLAIN);

  sp_fs_create_dir(cache->db.dir);
  sp_fs_create_dir(cache->checkouts.dir);
}

static spn_git_db_t* spn_git_cache_db_entry(spn_git_cache_t* cache, sp_str_t url) {
  sp_mutex_lock(&cache->mutex);

  sp_str_t key = spn_git_db_key(cache->mem, url);
  spn_git_db_t** existing = sp_str_ht_get(cache->db.entries, key);
  spn_git_db_t* db = existing ? *existing : SP_NULLPTR;
  if (!db) {
    db = sp_alloc_type(cache->mem, spn_git_db_t);
    db->url = url;
    db->path = sp_fs_join_path(cache->mem, cache->db.dir, key);
    sp_mutex_init(&db->mutex, SP_MUTEX_PLAIN);
    sp_str_ht_insert(cache->db.entries, key, db);
  }

  sp_mutex_unlock(&cache->mutex);
  return db;
}

spn_err_t spn_git_cache_ensure_db(spn_git_cache_t* cache, sp_str_t url, spn_git_db_t** db) {
  spn_git_db_t* entry = spn_git_cache_db_entry(cache, url);
  *db = entry;

  sp_mutex_lock(&entry->mutex);
  if (!entry->ready) {
    entry->ready = true;

    if (!sp_fs_is_dir(entry->path)) {
      sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
      sp_ps_output_t result = sp_ps_run(scratch.mem, (sp_ps_config_t) {
        .command = SP_LIT("git"),
        .args = {
          SP_LIT("clone"), SP_LIT("--bare"), SP_LIT("--quiet"),
          SP_LIT("-c"), SP_LIT("core.autocrlf=false"),
          url,
          entry->path
        },
        .io.err.mode = SP_PS_IO_MODE_REDIRECT,
      });

      if (result.status.exit_code) {
        entry->err = SPN_ERROR;
        entry->error = sp_str_copy(cache->mem, sp_str_trim_right(result.out));
      }
      sp_mem_end_scratch(scratch);
    }
  }
  sp_mutex_unlock(&entry->mutex);

  return entry->err;
}

spn_err_t spn_git_db_ensure_rev(spn_git_db_t* db, sp_str_t rev) {
  sp_mutex_lock(&db->mutex);
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  sp_ps_config_t cat_file = {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), db->path,
      SP_LIT("cat-file"), SP_LIT("-t"), rev
    },
    .io.err.mode = SP_PS_IO_MODE_NULL,
  };

  sp_ps_output_t result = sp_ps_run(scratch.mem, cat_file);

  if (result.status.exit_code) {
    // Bare clones have no remote.origin.fetch refspec, so a plain fetch never
    // brings in commits made after the clone. Ask for the rev itself, and if
    // the remote won't serve it by hash, mirror all heads and tags.
    result = sp_ps_run(scratch.mem, (sp_ps_config_t) {
      .command = SP_LIT("git"),
      .args = {
        SP_LIT("-C"), db->path,
        SP_LIT("fetch"), SP_LIT("--quiet"), SP_LIT("origin"), rev
      },
      .io.err.mode = SP_PS_IO_MODE_NULL,
    });

    if (result.status.exit_code) {
      result = sp_ps_run(scratch.mem, (sp_ps_config_t) {
        .command = SP_LIT("git"),
        .args = {
          SP_LIT("-C"), db->path,
          SP_LIT("fetch"), SP_LIT("--quiet"), SP_LIT("origin"),
          SP_LIT("+refs/heads/*:refs/heads/*"), SP_LIT("+refs/tags/*:refs/tags/*")
        },
        .io.err.mode = SP_PS_IO_MODE_NULL,
      });
    }

    if (!result.status.exit_code) {
      result = sp_ps_run(scratch.mem, cat_file);
    }
  }

  sp_mem_end_scratch(scratch);
  sp_mutex_unlock(&db->mutex);
  return result.status.exit_code ? SPN_ERROR : SPN_OK;
}

static spn_err_t spn_git_cache_fill_checkout(spn_git_cache_t* cache, spn_git_checkout_t* entry, spn_git_db_t* db, sp_str_t work) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  // Checkouts must be byte-identical to the committed content no matter
  // what the machine's autocrlf is; hashes and golden comparisons depend
  // on it. -c on clone persists into the new repo's config.
  sp_ps_output_t result = sp_ps_run(scratch.mem, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("clone"), SP_LIT("--shared"), SP_LIT("--quiet"),
      SP_LIT("-c"), SP_LIT("core.autocrlf=false"),
      db->path,
      work
    },
    .io.err.mode = SP_PS_IO_MODE_REDIRECT,
  });

  spn_err_t err = SPN_OK;
  if (result.status.exit_code) {
    entry->error = sp_str_copy(cache->mem, sp_str_trim_right(result.out));
    err = SPN_ERROR;
  }
  sp_mem_end_scratch(scratch);
  if (err) {
    return err;
  }

  if (spn_git_checkout(work, entry->id.rev)) {
    entry->error = sp_fmt(cache->mem, "failed to check out {}", sp_fmt_str(entry->id.rev)).value;
    return SPN_ERROR;
  }

  sp_da_for(entry->id.patches.files, it) {
    sp_str_t error = sp_zero;
    if (spn_git_apply(cache->mem, work, entry->id.patches.files[it], &error)) {
      entry->error = sp_fmt(cache->mem, "failed to apply {}: {}",
        sp_fmt_str(entry->id.patches.files[it]), sp_fmt_str(error)).value;
      return SPN_ERROR;
    }
  }

  return SPN_OK;
}

static spn_err_t spn_git_cache_materialize_checkout(spn_git_cache_t* cache, spn_git_checkout_t* entry) {
  spn_git_db_t* db = SP_NULLPTR;
  if (spn_git_cache_ensure_db(cache, entry->id.url, &db)) {
    entry->error = db->error;
    return SPN_ERROR;
  }

  if (spn_git_db_ensure_rev(db, entry->id.rev)) {
    entry->error = sp_fmt(cache->mem, "{} has no rev {}", sp_fmt_str(entry->id.url), sp_fmt_str(entry->id.rev)).value;
    return SPN_ERROR;
  }

  if (!sp_fs_is_dir(entry->path)) {
    entry->fetched = true;

    // Fill a claimed staging dir and rename into place, so a crash never
    // leaves a partial tree that later runs mistake for a finished checkout
    sp_str_t work = sp_zero;
    if (sp_fs_staging_dir(cache->mem, entry->path, sp_str_lit("tmp"), &work)) {
      entry->error = sp_fmt(cache->mem, "failed to stage checkout at {}", sp_fmt_str(entry->path)).value;
      return SPN_ERROR;
    }

    if (spn_git_cache_fill_checkout(cache, entry, db, work)) {
      sp_fs_remove_dir(work);
      return SPN_ERROR;
    }

    sp_sys_fd_t cwd = sp_sys_get_root(0);
    if (sp_sys_rename_s(cwd, work, cwd, entry->path)) {
      sp_fs_remove_dir(work);
      if (!sp_fs_is_dir(entry->path)) {
        entry->error = sp_fmt(cache->mem, "failed to place checkout at {}", sp_fmt_str(entry->path)).value;
        return SPN_ERROR;
      }
    }
  }

  if (!sp_str_empty(entry->id.dir)) {
    sp_str_t subdir = sp_fs_join_path(cache->mem, entry->path, entry->id.dir);
    if (!sp_fs_is_dir(subdir)) {
      entry->error = sp_fmt(cache->mem, "{} does not exist in {}", sp_fmt_str(entry->id.dir), sp_fmt_str(entry->id.url)).value;
      return SPN_ERROR;
    }
    entry->path = subdir;
  }

  return SPN_OK;
}

spn_err_t spn_git_cache_ensure_checkout(spn_git_cache_t* cache, spn_git_checkout_id_t id, spn_git_checkout_t** checkout) {
  sp_mutex_lock(&cache->mutex);
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  sp_str_t key = spn_git_checkout_key(scratch.mem, id);
  spn_git_checkout_t*** existing = sp_str_om_getp(cache->checkouts.entries, key);
  spn_git_checkout_t* entry = existing ? **existing : SP_NULLPTR;
  if (!entry) {
    sp_str_t owned = sp_str_copy(cache->mem, key);
    entry = sp_alloc_type(cache->mem, spn_git_checkout_t);
    entry->id = id;
    entry->path = sp_fs_join_path(cache->mem, cache->checkouts.dir, owned);
    sp_mutex_init(&entry->mutex, SP_MUTEX_PLAIN);
    sp_str_om_insert(cache->checkouts.entries, owned, entry);
  }

  sp_mem_end_scratch(scratch);
  sp_mutex_unlock(&cache->mutex);

  sp_mutex_lock(&entry->mutex);
  if (!entry->ready) {
    entry->ready = true;
    entry->err = spn_git_cache_materialize_checkout(cache, entry);
  }
  sp_mutex_unlock(&entry->mutex);

  *checkout = entry;
  return entry->err;
}

bool spn_git_cache_is_checkout_cached(spn_git_cache_t* cache, spn_git_checkout_id_t id) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_str_t key = spn_git_checkout_key(scratch.mem, id);
  bool cached = sp_fs_is_dir(sp_fs_join_path(scratch.mem, cache->checkouts.dir, key));
  sp_mem_end_scratch(scratch);
  return cached;
}
