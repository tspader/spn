#include "sp.h"
#include "sp/macro.h"
#include "error/types.h"
#include "codegen/lower.h"
#include "external/git.h"
#include "index/release.h"
#include "index/publish.h"
#include "pkg/load.h"

spn_err_union_t spn_publish_build(spn_publish_opts_t* opts, spn_index_release_t* out) {
  sp_str_t manifest_path = sp_fs_join_path(opts->mem, opts->cwd, sp_str_lit("spn.toml"));

  spn_pkg_info_t info = sp_zero;
  spn_try_union(spn_pkg_load(opts->mem, opts->intern, manifest_path, SPN_MANIFEST_DEP, sp_str_lit(""), &info));

  sp_str_t repo = sp_zero;
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

  spn_index_release_t release = sp_zero;
  spn_try_union(spn_index_release_from_pkg(opts->mem, &info, &release));

  spn_pkg_tree_t published = {
    .kind = SPN_PKG_TREE_GIT,
    .git = { .url = url, .rev = revision, .dir = subdir },
  };
  if (release.source.kind == SPN_PKG_TREE_NONE) {
    release.source = published;
  } else {
    release.manifest = published;
  }

  *out = release;
  return spn_result(SPN_OK);
}
