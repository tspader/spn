#include "sp.h"
#include "sp/macro.h"
#include "ctx/types.h"
#include "error/types.h"
#include "codegen/lower.h"
#include "error/error.h"
#include "external/git.h"
#include "index/release.h"
#include "index/publish.h"
#include "pkg/load.h"
#include "toml/issue.h"

spn_err_t spn_publish_build(spn_publish_opts_t* opts, spn_index_release_t* built) {
  sp_str_t manifest_path = sp_fs_join_path(opts->mem, opts->cwd, sp_str_lit("spn.toml"));

  spn_pkg_info_t info = sp_zero;
  spn_codegen_issues_t issues = sp_zero;
  spn_err_t loaded = spn_pkg_load(opts->mem, opts->intern, manifest_path, SPN_MANIFEST_DEP, &info, &issues);
  if (loaded == SPN_ERR_NO_MANIFEST) {
    return spn_err_emit(&spn, (spn_err_union_t) {
      .kind = SPN_ERR_NO_MANIFEST,
      .no_manifest = { .path = manifest_path },
    });
  }
  if (loaded) {
    return spn_err_emit(&spn, (spn_err_union_t) {
      .kind = SPN_ERR_MANIFEST_ISSUES,
      .manifest = { .path = manifest_path, .issues = spn_codegen_issues_to_err(spn.mem, issues) },
    });
  }

  sp_str_t repo = sp_zero;
  if (spn_git_get_root(opts->mem, opts->cwd, &repo)) {
    return spn_err_emit(&spn, (spn_err_union_t) {
      .kind = SPN_ERR_NOT_GIT_REPO,
      .not_git_repo.path = opts->cwd,
    });
  }

  sp_str_t url = opts->url;
  if (sp_str_empty(url)) {
    if (spn_git_get_remote_url(opts->mem, repo, &url)) {
      return spn_err_emit(&spn, (spn_err_union_t) {
        .kind = SPN_ERR_GIT,
        .git.command = sp_str_lit("git remote get-url origin"),
      });
    }
  }

  sp_str_t revision = opts->revision;
  if (sp_str_empty(revision)) {
    if (!opts->allow_dirty && spn_git_is_dirty(repo, opts->cwd)) {
      return spn_err_emit(&spn, (spn_err_union_t) {
        .kind = SPN_ERR_PUBLISH_DIRTY,
        .publish.path = repo,
      });
    }
    if (spn_git_get_commit_full(opts->mem, repo, sp_str_lit("HEAD"), &revision)) {
      return spn_err_emit(&spn, (spn_err_union_t) {
        .kind = SPN_ERR_GIT,
        .git.command = sp_str_lit("git rev-parse HEAD"),
      });
    }
    if (sp_str_empty(opts->url) && !spn_git_rev_on_remote(repo, revision)) {
      return spn_err_emit(&spn, (spn_err_union_t) {
        .kind = SPN_ERR_PUBLISH_UNPUSHED,
        .publish = { .url = url, .rev = revision },
      });
    }
  }

  sp_str_t subdir = sp_str_lit("");
  if (!sp_str_equal(opts->cwd, repo) && sp_str_starts_with(opts->cwd, repo)) {
    subdir = sp_str_suffix(opts->cwd, opts->cwd.len - repo.len - 1);
  }

  spn_index_release_t release = sp_zero;
  sp_str_t dep = sp_zero;
  if (spn_index_release_from_pkg(opts->mem, &info, &release, &dep)) {
    return spn_err_emit(&spn, (spn_err_union_t) {
      .kind = SPN_ERR_INDEX_PATH_DEP,
      .pkg = { .name = info.qualified, .requested = dep },
    });
  }

  spn_pkg_root_t published = {
    .kind = SPN_PKG_ROOT_GIT,
    .git = { .url = url, .rev = revision, .dir = subdir },
  };
  if (release.source.kind == SPN_PKG_ROOT_NONE) {
    release.source = published;
  } else {
    release.manifest = published;
  }

  *built = release;
  return SPN_OK;
}
