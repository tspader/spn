#include "lock.h"

#include "ctx.h"
#include "stoml.h"

spn_lock_file_t spn_build_lock_file(void) {
  spn_lock_file_t lock = SP_ZERO_INITIALIZE();
  spn_lock_file_init(&lock);

  spn_resolver_t* resolver = spn_ctx_resolver();
  spn_pkg_t* root_pkg = spn_ctx_root_package();

  sp_ht_for_kv(resolver->resolved, it) {
    spn_resolved_pkg_t* resolved = it.val;
    spn_pkg_t* pkg = resolved->pkg;
    spn_pkg_metadata_t* metadata = sp_ht_getp(pkg->metadata, resolved->version);

    spn_pkg_req_t* direct_req = sp_ht_getp(root_pkg->deps, resolved->pkg->name);
    bool is_explicit = direct_req != SP_NULLPTR;

    spn_lock_entry_t entry = {
      .name = pkg->name,
      .version = metadata->version,
      .commit = metadata->commit,
      .import_kind = is_explicit ?
        SPN_DEP_IMPORT_KIND_EXPLICIT :
        SPN_DEP_IMPORT_KIND_TRANSITIVE,
      .visibility = is_explicit ?
        direct_req->visibility :
        SPN_VISIBILITY_PUBLIC,
      .kind = resolved->kind,
    };

    sp_da_for(pkg->system_deps, dep_it) {
      sp_ht_insert(lock.system_deps, sp_str_copy(pkg->system_deps[dep_it]), true);
    }

    sp_ht_for_kv(pkg->deps, dep_it) {
      sp_da_push(entry.deps, sp_str_copy(dep_it.val->name));
    }

    sp_ht_insert(lock.entries, entry.name, entry);
  }

  sp_ht_for_kv(lock.entries, it) {
    sp_da_for(it.val->deps, dep_it) {
      spn_lock_entry_t* dep = sp_ht_getp(lock.entries, it.val->deps[dep_it]);
      if (dep) {
        sp_da_push(dep->dependents, sp_str_copy(it.val->name));
      }
    }
  }

  return lock;
}

void spn_lock_file_init(spn_lock_file_t* lock) {
  sp_ht_set_fns(lock->entries, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(lock->system_deps, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
}

spn_lock_file_t spn_lock_file_load(sp_str_t path) {
  spn_lock_file_t lock = SP_ZERO_INITIALIZE();
  spn_lock_file_init(&lock);

  SP_ASSERT(sp_fs_exists(path));

  toml_table_t* root = spn_toml_parse(path);
  SP_ASSERT(root);

  toml_table_t* pkg_table = toml_table_table(root, "package");
  if (pkg_table) {
    sp_da(sp_str_t) sys_deps = spn_toml_arr_to_str_arr(toml_table_array(pkg_table, "system_deps"));
    sp_da_for(sys_deps, it) {
      sp_ht_insert(lock.system_deps, sys_deps[it], true);
    }
  }

  toml_array_t* deps = toml_table_array(root, "dep");
  if (!deps) {
    return lock;
  }

  spn_toml_arr_for(deps, it) {
    toml_table_t* pkg = toml_array_table(deps, it);
    SP_ASSERT(pkg);

    spn_lock_entry_t entry = {
      .name = spn_toml_str(pkg, "name"),
      .version = spn_semver_from_str(spn_toml_str(pkg, "version")),
      .commit = spn_toml_str(pkg, "commit"),
      .kind = spn_package_kind_from_str(spn_toml_str(pkg, "kind")),
      .visibility = spn_visibility_from_str(spn_toml_str(pkg, "visibility")),
      .deps = spn_toml_arr_to_str_arr(toml_table_array(pkg, "deps")),
    };
    sp_ht_insert(lock.entries, entry.name, entry);
  }

  sp_ht_for_kv(lock.entries, it) {
    sp_da_for(it.val->deps, dep_it) {
      spn_lock_entry_t* dep = sp_ht_getp(lock.entries, it.val->deps[dep_it]);
      if (dep) {
        sp_da_push(dep->dependents, it.val->name);
      }
    }
  }

  return lock;
}
