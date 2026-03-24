#include "ctx/types.h"

#include "cli/cli.h"
#include "index/publish.h"
#include "log/log.h"

sp_app_result_t spn_cli_publish(spn_cli_t* cli) {
  spn_cli_publish_t* cmd = &cli->publish;

  sp_str_t index_name = sp_str_empty(cmd->index) ? sp_str_lit("core") : cmd->index;

  spn_index_t* index = SP_NULLPTR;
  sp_da_for(spn.indexes, it) {
    if (sp_str_equal(spn.indexes[it].name, index_name)) {
      index = &spn.indexes[it];
      break;
    }
  }

  if (!index) {
    spn_log_error("index {:fg brightcyan} not found", SP_FMT_STR(index_name));
    return SP_APP_QUIT;
  }

  spn_publish_opts_t opts = {
    .cwd = spn.paths.cwd,
    .index = index,
    .url = cmd->source_url,
    .revision = cmd->source_rev,
  };

  spn_err_union_t result = spn_publish(&opts);

  if (result.kind) {
    switch (result.kind) {
      case SPN_ERR_NO_MANIFEST: {
        spn_log_error("no manifest found at {:fg brightcyan}", SP_FMT_STR(result.no_manifest.path));
        break;
      }
      case SPN_ERR_MANIFEST_PARSE: {
        spn_log_error("failed to parse {:fg brightcyan}", SP_FMT_STR(result.manifest_parse.path));
        break;
      }
      case SPN_ERR_MANIFEST_FIELD: {
        spn_log_error("invalid field {:fg brightyellow} in manifest: expected {:fg brightgreen}, got {:fg brightred}",
          SP_FMT_STR(result.manifest_field.path),
          SP_FMT_STR(result.manifest_field.expected),
          SP_FMT_STR(result.manifest_field.actual)
        );
        break;
      }
      case SPN_ERR_NOT_GIT_REPO: {
        spn_log_error("{:fg brightcyan} is not inside a git repository", SP_FMT_STR(result.not_git_repo.path));
        break;
      }
      case SPN_ERR_GIT: {
        spn_log_error("git command failed: {:fg brightyellow}", SP_FMT_STR(result.git.command));
        break;
      }
      case SPN_ERR_VERSION_EXISTS: {
        spn_log_error("version {:fg brightyellow} of {:fg brightcyan} already exists in the index",
          SP_FMT_STR(result.version_exists.version),
          SP_FMT_STR(result.version_exists.name)
        );
        break;
      }
      default: {
        spn_log_error("publish failed");
        break;
      }
    }
  } else {
    SP_LOG("published successfully");
  }

  return SP_APP_QUIT;
}
