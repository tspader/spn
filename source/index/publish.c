#include "sp.h"
#include "sp/macro.h"
#include "error/types.h"
#include "codegen/lower.h"
#include "external/git.h"
#include "index/publish.h"
#include "pkg/id.h"
#include "semver/convert.h"
#include "target/mutate.h"
#include "toml/loader.h"

static spn_index_dep_kind_t dep_kind_to_index(spn_dep_kind_t kind) {
  switch (kind) {
    case SPN_DEP_KIND_PACKAGE: return SPN_INDEX_DEP_NORMAL;
    case SPN_DEP_KIND_BUILD:   return SPN_INDEX_DEP_BUILD;
    case SPN_DEP_KIND_TEST:    return SPN_INDEX_DEP_TEST;
  }
  sp_unreachable_return(SPN_INDEX_DEP_NORMAL);
}

spn_err_union_t spn_publish_build(spn_publish_opts_t* opts, spn_index_rel_t* out) {
  sp_str_t manifest_path = sp_fs_join_path(opts->mem, opts->cwd, sp_str_lit("spn.toml"));

  if (!sp_fs_exists(manifest_path)) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_NO_MANIFEST,
      .no_manifest.path = manifest_path,
    };
  }

  spn_pkg_info_t info = SP_ZERO_INITIALIZE();
  spn_toml_loader_t ctx = sp_zero;
  spn_toml_loader_init(&ctx, opts->mem, opts->intern);
  if (spn_codegen_load_pkg(&ctx, manifest_path, &info)) {
    return spn_codegen_err(&ctx);
  }

  sp_str_t repo = SP_ZERO_INITIALIZE();
  if (spn_git_get_root(opts->mem, opts->cwd, &repo)) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_NOT_GIT_REPO,
      .not_git_repo.path = opts->cwd,
    };
  }

  sp_str_t url = opts->url;
  if (sp_str_empty(url)) {
    if (spn_git_get_remote_url(opts->mem, repo, &url)) {
      return (spn_err_union_t) {
        .kind = SPN_ERR_GIT,
        .git.command = sp_str_lit("git remote get-url origin"),
      };
    }
  }

  sp_str_t revision = opts->revision;
  if (sp_str_empty(revision)) {
    if (!opts->allow_dirty && spn_git_is_dirty(repo, opts->cwd)) {
      return (spn_err_union_t) {
        .kind = SPN_ERR_PUBLISH_DIRTY,
        .publish.path = repo,
      };
    }
    if (spn_git_get_commit_full(opts->mem, repo, sp_str_lit("HEAD"), &revision)) {
      return (spn_err_union_t) {
        .kind = SPN_ERR_GIT,
        .git.command = sp_str_lit("git rev-parse HEAD"),
      };
    }
    if (sp_str_empty(opts->url) && !spn_git_rev_on_remote(repo, revision)) {
      return (spn_err_union_t) {
        .kind = SPN_ERR_PUBLISH_UNPUSHED,
        .publish = { .url = url, .rev = revision },
      };
    }
  }

  sp_str_t subdir = sp_str_lit("");
  if (!sp_str_equal(opts->cwd, repo) && sp_str_starts_with(opts->cwd, repo)) {
    subdir = sp_str_suffix(opts->cwd, opts->cwd.len - repo.len - 1);
  }

  spn_index_rel_t release = {
    .id = {
      .namespace = sp_str_empty(info.namespace) ? sp_str_lit("core") : info.namespace,
      .name = info.name,
    },
    .version = info.version,
    .source = { .url = url, .rev = revision, .dir = subdir },
    .paths = {
      .manifest = sp_str_lit("spn.toml"),
      .script = sp_str_lit("spn.c"),
    },
  };

  sp_da_init(opts->mem, release.deps);
  sp_da_init(opts->mem, release.targets);
  release.options = info.options;

  if (!sp_str_empty(info.upstream.url)) {
    release.source = (spn_index_rel_source_t) { .url = info.upstream.url, .rev = info.upstream.commit };
    release.manifest = (spn_index_rel_source_t) { .url = url, .rev = revision, .dir = subdir };
  }

  sp_da_for(info.deps, it) {
    spn_requested_pkg_t* req = &info.deps[it];
    if (req->source != SPN_PKG_SOURCE_INDEX) {
      continue;
    }

    sp_da_push(release.deps, ((spn_index_dep_t) {
      .kind = dep_kind_to_index(req->kind),
      .private = req->private,
      .id = spn_pkg_name_from_qualified(req->qualified),
      .version = spn_semver_range_to_str(opts->mem, req->index.range),
      .when = req->when,
      .options = req->options,
    }));
  }

  sp_str_om_for(info.libs, it) {
    spn_target_info_t* lib = sp_str_om_at(info.libs, it);
    spn_index_target_t target = { .name = lib->name };
    sp_da_init(opts->mem, target.linkages);

    const spn_linkage_t kinds [] = { SPN_LIB_KIND_SOURCE, SPN_LIB_KIND_STATIC, SPN_LIB_KIND_SHARED, SPN_LIB_KIND_OBJECT };
    sp_for(kind, sp_carr_len(kinds)) {
      if (spn_linkage_set_has(lib->linkages, kinds[kind])) {
        sp_da_push(target.linkages, kinds[kind]);
      }
    }

    sp_da_push(release.targets, target);
  }

  *out = release;
  return spn_result(SPN_OK);
}
