#include "sp/macro.h"
#include "index/json.h"

#include "release.gen.h"
#include "semver/compare.h"
#include "semver/convert.h"
#include "sp.h"
#include "sp/str.h"

static spn_err_t spn_index_parse_semver(sp_str_t version, spn_semver_t* out) {
  if (sp_str_empty(version)) {
    return SPN_ERROR;
  }

  spn_semver_t semver = spn_semver_from_str(version);
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  bool round_trips = sp_str_equal(version, spn_semver_to_str(scratch.mem, semver));
  sp_mem_end_scratch(scratch);
  if (!round_trips) {
    return SPN_ERROR;
  }

  *out = semver;
  return SPN_OK;
}

static spn_err_t spn_index_parse_rel(sp_mem_t mem, spn_pkg_name_t id, sp_str_t json, spn_index_rel_t* release) {
  spn_cg_release_t rel = SP_ZERO_INITIALIZE();
  if (!spn_release_read(json, &rel, mem)) {
    return SPN_ERROR;
  }

  if (!sp_str_equal(rel.namespace, id.namespace)) {
    return SPN_ERROR;
  }
  if (!sp_str_equal(rel.name, id.name)) {
    return SPN_ERROR;
  }
  spn_try(spn_index_parse_semver(rel.version, &release->version));

  release->id.namespace = rel.namespace;
  release->id.name = rel.name;
  release->checksum = rel.checksum;
  release->yanked = rel.yanked;
  release->source = (spn_index_rel_source_t) { .url = rel.source.url, .rev = rel.source.rev, .dir = rel.source.dir };
  release->manifest = (spn_index_rel_source_t) { .url = rel.manifest.url, .rev = rel.manifest.rev, .dir = rel.manifest.dir };
  release->paths = (spn_index_rel_paths_t) { .manifest = rel.paths.manifest, .script = rel.paths.script };

  sp_da_init(mem, release->deps);
  sp_da_for(rel.deps, it) {
    sp_da_push(release->deps, ((spn_index_dep_t) {
      .kind = rel.deps[it].kind,
      .private = !sp_opt_is_null(rel.deps[it].private) && sp_opt_get(rel.deps[it].private),
      .id = {
        .namespace = rel.deps[it].namespace,
        .name = rel.deps[it].name,
      },
      .version = rel.deps[it].version,
    }));
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

  return SPN_OK;
}

static s32 sort_release_by_version(const void* a, const void* b) {
  const spn_index_rel_t* lhs = (const spn_index_rel_t*)a;
  const spn_index_rel_t* rhs = (const spn_index_rel_t*)b;
  return spn_semver_cmp(lhs->version, rhs->version);
}

sp_str_t spn_index_rel_to_json(sp_mem_t mem, spn_index_rel_t* rel) {
  spn_cg_release_t release = {
    .namespace = rel->id.namespace,
    .name = rel->id.name,
    .version = spn_semver_to_str(mem, rel->version),
    .yanked = rel->yanked,
    .checksum = rel->checksum,
    .source = { .url = rel->source.url, .rev = rel->source.rev, .dir = rel->source.dir },
    .manifest = { .url = rel->manifest.url, .rev = rel->manifest.rev, .dir = rel->manifest.dir },
    .paths = { .manifest = rel->paths.manifest, .script = rel->paths.script },
    .deps = sp_da_new(mem, spn_cg_release_dep_t),
    .targets = sp_da_new(mem, spn_cg_release_target_t),
  };

  sp_da_for(rel->deps, it) {
    spn_cg_release_dep_t dep = {
      .namespace = rel->deps[it].id.namespace,
      .name = rel->deps[it].id.name,
      .version = rel->deps[it].version,
      .kind = rel->deps[it].kind,
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

    spn_index_rel_t release = SP_ZERO_INITIALIZE();
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
