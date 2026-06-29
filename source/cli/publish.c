#include "ctx/types.h"

#include "cli/cli.h"
#include "index/publish.h"
#include "log/log.h"

sp_cli_result_t spn_cli_publish(sp_cli_t* cli) {
  spn_cli_publish_t* cmd = &spn.cli.publish;

  sp_str_t index_name = sp_str_empty(cmd->index) ? sp_str_lit("core") : cmd->index;

  spn_index_info_t* index = SP_NULLPTR;
  sp_da_for(spn.indexes, it) {
    if (sp_str_equal(spn.indexes[it].name, index_name)) {
      index = &spn.indexes[it];
      break;
    }
  }

  if (!index) {
    spn_log_error("index {.cyan} not found", SP_FMT_STR(index_name));
    return SP_CLI_OK;
  }

  spn_publish_opts_t opts = {
    .mem = spn.mem,
    .intern = spn.intern,
    .cwd = spn.paths.cwd,
    .index = index,
    .url = cmd->source_url,
    .revision = cmd->source_rev,
  };

  spn_err_union_t result = spn_publish(&opts);

  if (result.kind) {
    // SPN_ERR_* kinds are not members of spn_err_t, so switching on the enum
    // type trips -Wswitch on every case
    switch ((s32) result.kind) {
      case SPN_ERR_NO_MANIFEST: {
        spn_log_error("no manifest found at {.cyan}", SP_FMT_STR(result.no_manifest.path));
        break;
      }
      case SPN_ERR_MANIFEST_PARSE: {
        spn_log_error("failed to parse {.cyan}", SP_FMT_STR(result.manifest_parse.path));
        break;
      }
      case SPN_ERR_MANIFEST_FIELD: {
        spn_log_error("invalid field {.yellow} in manifest: expected {.green}, got {.red}",
          SP_FMT_STR(result.manifest_field.path),
          SP_FMT_STR(result.manifest_field.expected),
          SP_FMT_STR(result.manifest_field.actual)
        );
        break;
      }
      case SPN_ERR_NOT_GIT_REPO: {
        spn_log_error("{.cyan} is not inside a git repository", SP_FMT_STR(result.not_git_repo.path));
        break;
      }
      case SPN_ERR_GIT: {
        spn_log_error("git command failed: {.yellow}", SP_FMT_STR(result.git.command));
        break;
      }
      case SPN_ERR_VERSION_EXISTS: {
        spn_log_error("version {.yellow} of {.cyan} already exists in the index",
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

  return SP_CLI_OK;
}
