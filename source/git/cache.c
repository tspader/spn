#include "git/cache.h"

#include "external/git.h"
#include "git/key.h"

void spn_git_cache_init(spn_git_cache_t* cache, sp_mem_t mem, sp_intern_t* intern, sp_str_t root) {
  *cache = (spn_git_cache_t) {
    .mem = mem,
    .intern = intern,
    .root = root,
    .db.dir = sp_fs_join_path(mem, root, SP_LIT("db")),
    .checkouts.dir = sp_fs_join_path(mem, root, SP_LIT("checkouts")),
  };

  sp_fs_create_dir(cache->db.dir);
  sp_fs_create_dir(cache->checkouts.dir);
}

spn_err_t spn_git_cache_ensure_db(spn_git_cache_t* cache, sp_str_t url, spn_git_db_t** out) {
  *out = SP_NULLPTR;

  sp_str_t key = spn_git_db_key(url);

  spn_git_db_t* existing = sp_str_ht_get(cache->db.entries, key);
  if (existing) {
    *out = existing;
    return SPN_OK;
  }

  sp_str_t path = sp_fs_join_path(cache->mem, cache->db.dir, key);

  if (!sp_fs_is_dir(path)) {
    sp_ps_output_t result = sp_ps_run(cache->mem, (sp_ps_config_t) {
      .command = SP_LIT("git"),
      .args = {
        SP_LIT("clone"), SP_LIT("--bare"), SP_LIT("--quiet"),
        url,
        path
      },
    });

    if (result.status.exit_code) return SPN_ERROR;
  }

  sp_str_ht_insert(cache->db.entries, key, ((spn_git_db_t) {
    .url = url,
    .path = path,
  }));

  *out = sp_str_ht_get(cache->db.entries, key);
  return SPN_OK;
}

spn_err_t spn_git_db_ensure_rev(spn_git_db_t* db, sp_str_t rev) {
  sp_ps_output_t result = sp_ps_run(spn_mem_todo, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), db->path,
      SP_LIT("cat-file"), SP_LIT("-t"), rev
    },
    .io.err.mode = SP_PS_IO_MODE_NULL,
  });

  if (!result.status.exit_code) return SPN_OK;

  result = sp_ps_run(spn_mem_todo, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), db->path,
      SP_LIT("fetch"), SP_LIT("--quiet"), SP_LIT("origin")
    },
    .io.err.mode = SP_PS_IO_MODE_NULL,
  });

  if (result.status.exit_code) return SPN_ERROR;

  result = sp_ps_run(spn_mem_todo, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), db->path,
      SP_LIT("cat-file"), SP_LIT("-t"), rev
    },
    .io.err.mode = SP_PS_IO_MODE_NULL,
  });

  if (result.status.exit_code) return SPN_ERROR;
  return SPN_OK;
}

spn_err_t spn_git_cache_ensure_checkout(spn_git_cache_t* cache, spn_git_checkout_id_t id, spn_git_checkout_t** out) {
  *out = SP_NULLPTR;

  sp_str_t key = spn_git_checkout_key(id.url, id.rev, id.dir);

  spn_git_checkout_t** existing = sp_str_om_getp(cache->checkouts.entries, key);
  if (existing) {
    *out = *existing;
    return SPN_OK;
  }

  spn_git_db_t* db = SP_NULLPTR;
  spn_try(spn_git_cache_ensure_db(cache, id.url, &db));
  spn_try(spn_git_db_ensure_rev(db, id.rev));

  sp_str_t path = sp_fs_join_path(cache->mem, cache->checkouts.dir, key);

  if (!sp_fs_is_dir(path)) {
    sp_ps_output_t result = sp_ps_run(cache->mem, (sp_ps_config_t) {
      .command = SP_LIT("git"),
      .args = {
        SP_LIT("clone"), SP_LIT("--shared"), SP_LIT("--quiet"),
        db->path,
        path
      },
    });
    if (result.status.exit_code) return SPN_ERROR;

    spn_try(spn_git_checkout(path, id.rev));

    if (!sp_str_empty(id.dir)) {
      sp_str_t subdir_path = sp_fs_join_path(cache->mem, path, id.dir);
      if (!sp_fs_is_dir(subdir_path)) return SPN_ERROR;
    }
  }

  sp_str_t checkout_root = path;
  if (!sp_str_empty(id.dir)) {
    checkout_root = sp_fs_join_path(cache->mem, path, id.dir);
  }

  sp_str_om_insert(cache->checkouts.entries, key, ((spn_git_checkout_t) {
    .id = id,
    .path = checkout_root,
  }));

  *out = sp_str_om_get(cache->checkouts.entries, key);
  return SPN_OK;
}
