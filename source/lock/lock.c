#include "enum/enum.h"
#include "err.h"
#include "event/event.h"
#include "external/tom.h"
#include "lock/lock.h"
#include "semver/convert.h"
#include "sp/ht.h"
#include "version.h"

static void spn_lock_build_dependents(spn_lock_file_t* lock) {
  sp_ht_for_kv(lock->entries, it) {
    sp_da_for(it.val->deps, dep_it) {
      spn_lock_entry_t* dep = sp_ht_getp(lock->entries, it.val->deps[dep_it]);
      if (dep) {
        sp_da_push(dep->dependents, it.val->name);
      }
    }
  }
}

spn_lock_file_t spn_build_lock_file(spn_resolver_t* resolver, spn_pkg_t* root) {
  spn_lock_file_t lock = SP_ZERO_INITIALIZE();
  spn_lock_file_init(&lock);
  return lock;
}

void spn_lock_file_init(spn_lock_file_t* lock) {
  sp_ht_set_fns(lock->entries, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(lock->system_deps, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
}

spn_lock_file_t spn_lock_file_parse(sp_str_t toml, spn_event_buffer_t* events) {
  spn_lock_file_t lock = SP_ZERO_INITIALIZE();
  spn_lock_file_init(&lock);

  c8 parse_err[1024] = {0};
  toml_table_t* root = toml_parse(sp_str_to_cstr(toml), parse_err, SP_CARR_LEN(parse_err));
  if (!root) {
    if (events) {
      spn_event_buffer_push_ex(events, SP_NULLPTR, SP_NULLPTR, (spn_build_event_t) {
        .kind = SPN_EVENT_ERR,
        .err = {
          .kind = SPN_ERR_MANIFEST_PARSE,
        },
      });
    }

    return lock;
  }

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
      .source = {
        .url = spn_toml_str_opt(pkg, "source_url", ""),
        .rev = spn_toml_str_opt(pkg, "source_rev", ""),
        .dir = spn_toml_str_opt(pkg, "source_dir", ""),
      },
      .manifest = {
        .url = spn_toml_str_opt(pkg, "manifest_url", ""),
        .rev = spn_toml_str_opt(pkg, "manifest_rev", ""),
        .dir = spn_toml_str_opt(pkg, "manifest_dir", ""),
      },
      .paths = {
        .manifest = spn_toml_str_opt(pkg, "manifest_file", "spn.toml"),
        .script = spn_toml_str_opt(pkg, "script_file", "spn.c"),
      },
    };
    sp_ht_insert(lock.entries, entry.name, entry);
  }

  spn_lock_build_dependents(&lock);

  return lock;
}

spn_lock_file_t spn_lock_file_load(sp_str_t path, spn_event_buffer_t* events) {
  SP_ASSERT(sp_fs_exists(path));
  sp_str_t contents = sp_io_read_file(path);
  return spn_lock_file_parse(contents, events);
}

sp_str_t spn_lock_file_to_str(spn_lock_file_t* lock) {
  sp_da(sp_str_t) keys = SP_NULLPTR;
  sp_ht_collect_keys(lock->entries, keys);
  sp_dyn_array_sort(keys, sp_str_sort_kernel_alphabetical);

  spn_toml_writer_t toml = spn_toml_writer_new();

  spn_toml_begin_table_cstr(&toml, "spn");
  spn_toml_append_str_cstr(&toml, "version", sp_str_lit(SPN_VERSION));
  spn_toml_append_str_cstr(&toml, "commit", sp_str_lit(SPN_COMMIT));
  spn_toml_end_table(&toml);

  if (sp_ht_size(lock->system_deps)) {
    spn_toml_begin_table_cstr(&toml, "package");
    sp_da(sp_str_t) sys_deps = SP_NULLPTR;
    sp_ht_collect_keys(lock->system_deps, sys_deps);
    sp_dyn_array_sort(sys_deps, sp_str_sort_kernel_alphabetical);
    spn_toml_append_str_array_cstr(&toml, "system_deps", sys_deps);
    spn_toml_end_table(&toml);
  }

  spn_toml_begin_array_cstr(&toml, "dep");
  sp_dyn_array_for(keys, it) {
    spn_lock_entry_t* entry = sp_ht_getp(lock->entries, keys[it]);

    spn_toml_append_array_table(&toml);
    spn_toml_append_str_cstr(&toml, "name", entry->name);
    spn_toml_append_str_cstr(&toml, "version", spn_semver_to_str(entry->version));
    spn_toml_append_str_cstr(&toml, "commit", entry->commit);
    spn_toml_append_str_cstr(&toml, "kind", spn_package_kind_to_str(entry->kind));
    spn_toml_append_str_cstr(&toml, "visibility", spn_visibility_to_str(entry->visibility));

    if (!sp_str_empty(entry->source.url)) {
      spn_toml_append_str_cstr(&toml, "source_url", entry->source.url);
      spn_toml_append_str_cstr(&toml, "source_rev", entry->source.rev);
      if (!sp_str_empty(entry->source.dir)) {
        spn_toml_append_str_cstr(&toml, "source_dir", entry->source.dir);
      }
    }

    if (!sp_str_empty(entry->manifest.url)) {
      spn_toml_append_str_cstr(&toml, "manifest_url", entry->manifest.url);
      spn_toml_append_str_cstr(&toml, "manifest_rev", entry->manifest.rev);
      if (!sp_str_empty(entry->manifest.dir)) {
        spn_toml_append_str_cstr(&toml, "manifest_dir", entry->manifest.dir);
      }
    }

    if (!sp_str_empty(entry->paths.manifest)) {
      spn_toml_append_str_cstr(&toml, "manifest_file", entry->paths.manifest);
    }
    if (!sp_str_empty(entry->paths.script)) {
      spn_toml_append_str_cstr(&toml, "script_file", entry->paths.script);
    }

    if (sp_dyn_array_size(entry->deps)) {
      spn_toml_append_str_array_cstr(&toml, "deps", entry->deps);
    }
  }
  spn_toml_end_array(&toml);

  return spn_toml_writer_write(&toml);
}
