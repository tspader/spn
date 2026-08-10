#include "sp/macro.h"
#include "index/json.h"

#include "release.gen.h"
#include "semver/compare.h"
#include "semver/convert.h"
#include "semver/parser.h"
#include "sp.h"
#include "sp/str.h"

static spn_err_t spn_index_parse_rel(sp_mem_t mem, spn_pkg_name_t id, sp_str_t json, spn_index_release_t* release) {
  spn_cg_release_t rel = sp_zero;
  if (!spn_release_read(json, &rel, mem)) {
    return SPN_ERROR;
  }

  if (!sp_str_equal(rel.namespace, id.namespace)) {
    return SPN_ERROR;
  }
  if (!sp_str_equal(rel.name, id.name)) {
    return SPN_ERROR;
  }
  spn_try(spn_semver_parse(rel.version, &release->version));

  release->id.namespace = rel.namespace;
  release->id.name = rel.name;
  release->yanked = rel.yanked;
  if (!sp_str_empty(rel.source.url)) {
    release->source = (spn_pkg_root_t) {
      .kind = SPN_PKG_ROOT_GIT,
      .git = { .url = rel.source.url, .rev = rel.source.rev, .dir = rel.source.dir },
    };
  }
  if (!sp_str_empty(rel.manifest.url)) {
    release->manifest = (spn_pkg_root_t) {
      .kind = SPN_PKG_ROOT_GIT,
      .git = { .url = rel.manifest.url, .rev = rel.manifest.rev, .dir = rel.manifest.dir },
    };
  }
  release->paths = (spn_index_rel_paths_t) { .manifest = rel.paths.manifest, .script = rel.paths.script };

  sp_da_init(mem, release->deps);
  sp_da_for(rel.deps, it) {
    spn_index_dep_t dep = {
      .kind = rel.deps[it].kind,
      .private = !sp_opt_is_null(rel.deps[it].private) && sp_opt_get(rel.deps[it].private),
      .id = {
        .namespace = rel.deps[it].namespace,
        .name = rel.deps[it].name,
      },
      .when = rel.deps[it].when,
      .options = rel.deps[it].options,
    };
    spn_try(spn_semver_parse_range(rel.deps[it].version, &dep.range));
    sp_da_push(release->deps, dep);
  }

  sp_da_init(mem, release->targets);
  sp_da_for(rel.targets, it) {
    spn_index_target_t target = { .name = rel.targets[it].name };
    sp_da_init(mem, target.linkages);
    sp_da_for(rel.targets[it].linkages, n) {
      if (rel.targets[it].linkages[n] != SPN_LIB_KIND_NONE) {
        sp_da_push(target.linkages, rel.targets[it].linkages[n]);
      }
    }
    sp_da_push(release->targets, target);
  }

  sp_da_for(rel.options, it) {
    spn_cg_release_options_entry_t* entry = &rel.options[it];
    spn_option_info_t option = {
      .name = entry->key,
      .type = entry->value.type,
      .additive = !sp_opt_is_null(entry->value.additive) && sp_opt_get(entry->value.additive),
      .values = entry->value.values ? entry->value.values : sp_da_new(mem, sp_str_t),
      .defaults = entry->value.defaults ? entry->value.defaults : sp_da_new(mem, spn_option_default_t),
    };
    sp_str_om_insert(release->options, option.name, option);
  }

  return SPN_OK;
}

static s32 sort_release_by_version(const void* a, const void* b) {
  const spn_index_release_t* lhs = (const spn_index_release_t*)a;
  const spn_index_release_t* rhs = (const spn_index_release_t*)b;
  return spn_semver_cmp(lhs->version, rhs->version);
}

sp_str_t spn_index_release_to_json(sp_mem_t mem, spn_index_release_t* rel) {
  spn_cg_release_t release = {
    .namespace = rel->id.namespace,
    .name = rel->id.name,
    .version = spn_semver_to_str(mem, rel->version),
    .yanked = rel->yanked,
    .paths = { .manifest = rel->paths.manifest, .script = rel->paths.script },
    .deps = sp_da_new(mem, spn_cg_release_dep_t),
    .targets = sp_da_new(mem, spn_cg_release_target_t),
    .options = sp_da_new(mem, spn_cg_release_options_entry_t),
  };

  if (rel->source.kind == SPN_PKG_ROOT_GIT) {
    release.source = (spn_cg_release_source_t) { .url = rel->source.git.url, .rev = rel->source.git.rev, .dir = rel->source.git.dir };
  }
  if (rel->manifest.kind == SPN_PKG_ROOT_GIT) {
    release.manifest = (spn_cg_release_source_t) { .url = rel->manifest.git.url, .rev = rel->manifest.git.rev, .dir = rel->manifest.git.dir };
  }

  sp_da_for(rel->deps, it) {
    spn_cg_release_dep_t dep = {
      .namespace = rel->deps[it].id.namespace,
      .name = rel->deps[it].id.name,
      .version = spn_semver_range_to_str(mem, rel->deps[it].range),
      .kind = rel->deps[it].kind,
      .when = rel->deps[it].when,
      .options = rel->deps[it].options,
    };
    if (rel->deps[it].private) {
      sp_opt_set(dep.private, true);
    }
    sp_da_push(release.deps, dep);
  }

  sp_da_for(rel->targets, it) {
    sp_da_push(release.targets, ((spn_cg_release_target_t) {
      .name = rel->targets[it].name,
      .linkages = rel->targets[it].linkages,
    }));
  }

  sp_str_om_for(rel->options, it) {
    spn_option_info_t* option = sp_str_om_at(rel->options, it);
    spn_cg_release_options_entry_t entry = {
      .key = option->name,
      .value = {
        .type = option->type,
        .values = option->values,
        .defaults = option->defaults,
      },
    };
    if (option->additive) {
      sp_opt_set(entry.value.additive, true);
    }
    sp_da_push(release.options, entry);
  }

  return spn_release_write_compact(mem, &release);
}

spn_err_t spn_index_parse_pkg(sp_mem_t mem, spn_pkg_name_t id, sp_str_t blob, spn_index_pkg_t* pkg) {
  pkg->id = id;
  sp_da_init(mem, pkg->releases);
  sp_str_for_line(blob, it) {
    sp_str_t line = sp_str_trim(it.line);
    if (sp_str_empty(line)) {
      continue;
    }

    spn_index_release_t release = sp_zero;
    spn_try(spn_index_parse_rel(mem, id, line, &release));

    sp_da_for(pkg->releases, n) {
      if (spn_semver_eq(pkg->releases[n].version, release.version)) {
        return SPN_ERROR;
      }
    }

    sp_da_push(pkg->releases, release);
  }

  if (sp_da_empty(pkg->releases)) {
    return SPN_ERROR;
  }

  sp_da_sort(pkg->releases, sort_release_by_version);
  return SPN_OK;
}
