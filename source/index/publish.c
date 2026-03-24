#include "err.h"
#include "external/git.h"
#include "index/index.h"
#include "index/publish.h"
#include "pkg/load.h"
#include "semver/convert.h"

spn_err_union_t spn_publish(spn_publish_opts_t* opts) {
  sp_str_t manifest_path = sp_fs_join_path(opts->cwd, sp_str_lit("spn.toml"));

  if (!sp_fs_exists(manifest_path)) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_NO_MANIFEST,
      .no_manifest.path = manifest_path,
    };
  }

  spn_pkg_t pkg = SP_ZERO_INITIALIZE();
  spn_try_union(spn_pkg_load(&pkg, manifest_path));

  sp_str_t repo = SP_ZERO_INITIALIZE();
  if (spn_git_get_root(opts->cwd, &repo)) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_NOT_GIT_REPO,
      .not_git_repo.path = opts->cwd,
    };
  }

  sp_str_t url = opts->url;
  if (sp_str_empty(url)) {
    if (spn_git_get_remote_url(repo, &url)) {
      return (spn_err_union_t) {
        .kind = SPN_ERR_GIT,
        .git.command = sp_str_lit("git remote get-url origin"),
      };
    }
  }

  sp_str_t revision = opts->revision;
  if (sp_str_empty(revision)) {
    if (spn_git_get_commit(repo, sp_str_lit("HEAD"), &revision)) {
      return (spn_err_union_t) {
        .kind = SPN_ERR_GIT,
        .git.command = sp_str_lit("git rev-parse HEAD"),
      };
    }
  }

  sp_str_t subdir = sp_str_lit("");
  if (!sp_str_equal(opts->cwd, repo) && sp_str_starts_with(opts->cwd, repo)) {
    subdir = sp_str_suffix(opts->cwd, opts->cwd.len - repo.len - 1);
  }

  spn_index_rel_t rel = {
    .id = {
      .namespace = sp_str_empty(pkg.namespace) ? sp_str_lit("core") : pkg.namespace,
      .name = pkg.name,
    },
    .version = pkg.version,
    .source = { .url = url, .rev = revision, .dir = subdir },
    .paths = {
      .manifest = sp_str_lit("spn.toml"),
      .script = sp_str_lit("spn.c"),
    },
  };

  spn_pkg_metadata_t* meta = sp_ht_getp(pkg.metadata, pkg.version);
  if (!sp_str_empty(pkg.url) && meta && !sp_str_empty(meta->commit)) {
    rel.source = (spn_index_rel_source_t) { .url = pkg.url, .rev = meta->commit };
    rel.manifest = (spn_index_rel_source_t) { .url = url, .rev = revision, .dir = subdir };
  }

  sp_ht_for_kv(pkg.deps, it) {
    spn_pkg_req_t* req = it.val;
    if (req->kind != SPN_PACKAGE_KIND_INDEX) {
      continue;
    }

    spn_index_dep_t dep = {
      .id = req->id,
      .version = spn_semver_range_to_str(req->range),
    };
    sp_da_push(rel.deps, dep);
  }

  spn_err_t publish_err = spn_index_publish(opts->index, &rel);
  if (publish_err == SPN_ERR_VERSION_EXISTS) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_VERSION_EXISTS,
      .version_exists = {
        .name = rel.id.name,
        .version = spn_semver_to_str(rel.version),
      },
    };
  }
  if (publish_err) {
    return (spn_err_union_t) { .kind = publish_err };
  }

  return spn_result(SPN_OK);
}
