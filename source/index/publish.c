#include "error/types.h"
#include "external/git.h"
#include "index/index.h"
#include "index/publish.h"
#include "pkg/id.h"
#include "pkg/load.h"
#include "semver/convert.h"

spn_err_union_t spn_publish(spn_publish_opts_t* opts) {
  sp_str_t manifest_path = sp_fs_join_path(spn_mem_todo, opts->cwd, sp_str_lit("spn.toml"));

  if (!sp_fs_exists(manifest_path)) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_NO_MANIFEST,
      .no_manifest.path = manifest_path,
    };
  }

  spn_pkg_info_t info = SP_ZERO_INITIALIZE();
  spn_try_union(spn_pkg_load(&info, manifest_path));

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

  spn_pkg_metadata_t* meta = sp_ht_getp(info.metadata, info.version);
  if (!sp_str_empty(info.url) && meta && !sp_str_empty(meta->commit)) {
    release.source = (spn_index_rel_source_t) { .url = info.url, .rev = meta->commit };
    release.manifest = (spn_index_rel_source_t) { .url = url, .rev = revision, .dir = subdir };
  }

  sp_ht_for_kv(info.deps, it) {
    spn_requested_pkg_t* req = it.val;
    if (req->source != SPN_PKG_SOURCE_INDEX) {
      continue;
    }

    sp_da_push(release.deps, ((spn_index_dep_t) {
      .id = spn_qualified_name_to_pkg_id(req->qualified),
      .version = spn_semver_range_to_str(req->index.range),
    }));
  }

  spn_err_t publish_err = spn_index_publish(opts->index, &release);
  if (publish_err == SPN_ERR_VERSION_EXISTS) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_VERSION_EXISTS,
      .version_exists = {
        .name = release.id.name,
        .version = spn_semver_to_str(release.version),
      },
    };
  }
  if (publish_err) {
    return (spn_err_union_t) { .kind = publish_err };
  }

  return spn_result(SPN_OK);
}
