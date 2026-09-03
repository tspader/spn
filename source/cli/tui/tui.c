#include "spn/host.h"

#include "io/io.h"
#include "macro/macro.h"
#include "sp/sp_prompt.h"
#include "str/str.h"

#include "tui/tui.h"


sp_prompt_widget_t sp_prompt_progress_widget(sp_prompt_ctx_t* ctx, sp_prompt_progress_t config);

#ifdef SP_WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif

  #ifndef NOMINMAX
    #define NOMINMAX
  #endif

  #include <windows.h>
#endif

#if defined(SP_POSIX)
  #include <fcntl.h>
  #include <unistd.h>
#endif


static void write_name(sp_tty_t* tty, sp_str_t name);
static void flush_writer(spn_tui_line_writer_t* writer);

static sp_color_t name_to_color(sp_str_t name) {
  static const f32 hues [] = {
    30, 40, 50, 60,
    160, 180,
    200, 220, 240,
    250, 280, 310, 340
  };

  sp_color_t hsv = {
    .h = hues[sp_hash_str(name) % sp_carr_len(hues)],
    .s = 40.0f,
    .v = 75.f,
  };
  return sp_color_hsv_to_rgb(hsv);
}

static void write_name_color(sp_tty_t* tty, sp_str_t name) {
  if (sp_str_equal_cstr(name, "package")) {
    sp_tty_sgr(tty, SP_ANSI_FG_WHITE_U8);
    return;
  }

  sp_color_t rgb = name_to_color(name);
  sp_tty_rgb(tty, (u8)(rgb.r * 255.0f), (u8)(rgb.g * 255.0f), (u8)(rgb.b * 255.0f));
}

static void write_name(sp_tty_t* tty, sp_str_t name) {
  write_name_color(tty, name);
  sp_io_write_str(tty->io, sp_str_lit("\u2590 "), SP_NULLPTR);
  sp_io_write_str(tty->io, name, SP_NULLPTR);
  sp_tty_reset(tty);
}

static sp_str_t colored_name(sp_tty_color_t color, sp_mem_t mem, sp_str_t name) {
  sp_io_dyn_mem_writer_t buf = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &buf);
  sp_tty_t tty = { .io = &buf.base, .color = color };
  write_name_color(&tty, name);
  sp_io_write_str(&buf.base, name, SP_NULLPTR);
  sp_tty_reset(&tty);
  return sp_io_dyn_mem_writer_take_str(&buf);
}

static sp_str_t colored_fmt(sp_tty_color_t color, sp_mem_t mem, const c8* fmt, ...) {
  sp_io_dyn_mem_writer_t buf = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &buf);
  sp_tty_t tty = { .io = &buf.base, .color = color };

  va_list args;
  va_start(args, fmt);
  sp_tty_fmt_v(&tty, sp_cstr_as_str(fmt), args);
  va_end(args);

  return sp_io_dyn_mem_writer_take_str(&buf);
}

static void write_manifest_issue(sp_tty_t* w, const spn_err_issue_t* issue) {
  switch (issue->code) {
    case SPN_ERR_CODEGEN_MISSING_KEY:
      sp_tty_fmt(w, "missing required field {.cyan}", SP_FMT_STR(issue->path));
      break;
    case SPN_ERR_CODEGEN_EXPECTED_STR:
      sp_tty_fmt(w, "{.cyan} must be a string", SP_FMT_STR(issue->path));
      break;
    case SPN_ERR_CODEGEN_EXPECTED_INT:
      sp_tty_fmt(w, "{.cyan} must be a non-negative integer", SP_FMT_STR(issue->path));
      break;
    case SPN_ERR_CODEGEN_EXPECTED_BOOL:
      sp_tty_fmt(w, "{.cyan} must be a boolean", SP_FMT_STR(issue->path));
      break;
    case SPN_ERR_CODEGEN_EXPECTED_OBJECT:
      sp_tty_fmt(w, "{.cyan} must be a table", SP_FMT_STR(issue->path));
      break;
    case SPN_ERR_CODEGEN_DUPLICATE_KEY:
      sp_tty_fmt(w, "duplicate {.yellow} at {.cyan}", SP_FMT_STR(issue->detail), SP_FMT_STR(issue->path));
      break;
    case SPN_ERR_CODEGEN_UNKNOWN_KEY:
      if (sp_str_empty(issue->path)) {
        sp_tty_fmt(w, "unknown field {.red}", SP_FMT_STR(issue->detail));
      } else {
        sp_tty_fmt(w, "unknown field {.red} in {.cyan}", SP_FMT_STR(issue->detail), SP_FMT_STR(issue->path));
      }
      break;
    case SPN_ERR_CODEGEN_INVALID:
      sp_tty_fmt(w, "invalid value at {.cyan}", SP_FMT_STR(issue->path));
      break;
    case SPN_ERR_CODEGEN_PARSE:
      if (sp_str_empty(issue->detail)) {
        sp_io_write_str(w->io, sp_str_lit("not valid toml"), SP_NULLPTR);
      } else {
        sp_tty_fmt(w, "not valid toml: {}", SP_FMT_STR(issue->detail));
      }
      break;
    case SPN_ERR_CODEGEN_FILE_MISSING:
      sp_io_write_str(w->io, sp_str_lit("file is missing"), SP_NULLPTR);
      break;
    case SPN_ERR_CODEGEN_ROOT_ONLY:
      sp_tty_fmt(w, "{.cyan} is only allowed in the root manifest", SP_FMT_STR(issue->path));
      break;
    default:
      sp_tty_fmt(w, "invalid field at {.cyan}", SP_FMT_STR(issue->path));
      break;
  }
}

static sp_str_t setter_to_str(spn_err_setter_t setter) {
  switch (setter.kind) {
    case SPN_OPTION_SETTER_NONE: {
      return sp_str_lit("");
    }
    case SPN_OPTION_SETTER_DEFAULT: {
      return sp_str_lit("the default");
    }
    case SPN_OPTION_SETTER_PROFILE: {
      return sp_str_lit("the profile");
    }
    case SPN_OPTION_SETTER_ROOT_MANIFEST: {
      return sp_str_lit("the root manifest");
    }
    case SPN_OPTION_SETTER_UNION: {
      return sp_str_lit("the union of requests");
    }
    case SPN_OPTION_SETTER_CONSUMER: {
      return setter.name;
    }
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

static sp_str_t root_label(spn_path_root_t root) {
  switch (root) {
    case SPN_PATH_ROOT_PROJECT:   return sp_str_lit(".");
    case SPN_PATH_ROOT_STORE:     return sp_str_lit("$SPN_STORE");
    case SPN_PATH_ROOT_BUILD:     return sp_str_lit("$SPN_BUILD");
    case SPN_PATH_ROOT_CHECKOUT:  return sp_str_lit("$SPN_CHECKOUTS");
    case SPN_PATH_ROOT_TOOLCHAIN: return sp_str_lit("$SPN_TOOLCHAIN");
    case SPN_PATH_ROOT_INDEX:     return sp_str_lit("$SPN_INDEX");
    case SPN_PATH_ROOT_RUNTIME:   return sp_str_lit("$SPN_RUNTIME");
    case SPN_PATH_ROOT_CACHE:     return sp_str_lit("$SPN_CACHE");
    case SPN_PATH_ROOT_NONE:
    case SPN_PATH_ROOT_COUNT:     break;
  }
  return sp_str_lit("");
}

static sp_str_t get_contextual_path(spn_ctx_t* ctx, sp_mem_t mem, sp_str_t path) {
  spn_path_t collapsed = spn_ctx_collapse_path(ctx, path);
  if (collapsed.root == SPN_PATH_ROOT_NONE) {
    return collapsed.sub;
  }
  if (sp_str_empty(collapsed.sub)) {
    return root_label(collapsed.root);
  }
  return sp_fmt(mem, "{}/{}", sp_fmt_str(root_label(collapsed.root)), sp_fmt_str(collapsed.sub)).value;
}

static sp_str_t render_event_detail(spn_tui_t* tui, sp_mem_t mem, spn_event_t* event) {
  spn_ctx_t* ctx = tui->ctx;
  sp_io_dyn_mem_writer_t buf = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &buf);
  sp_tty_t w = { .io = &buf.base, .color = tui->journal.color };

  switch (event->kind) {
    case SPN_EVENT_SYNC:
    case SPN_EVENT_SYNC_PATCH: {
      sp_tty_fmt(&w, "{} {.gray}", sp_fmt_str(event->sync.name), sp_fmt_str(event->sync.url));
      break;
    }
    case SPN_EVENT_SYNC_START: {
      sp_tty_fmt(&w, "packages={} index={} file={}",
        sp_fmt_uint(event->sync_start.num_packages),
        sp_fmt_uint(event->sync_start.num_index),
        sp_fmt_uint(event->sync_start.num_file)
      );
      break;
    }
    case SPN_EVENT_SYNC_PACKAGE: {
      sp_tty_fmt(&w, "{} fetched={} source={.gray}",
        sp_fmt_str(event->sync_pkg.name),
        sp_fmt_cstr(event->sync_pkg.fetched ? "true" : "false"),
        sp_fmt_str(event->sync_pkg.source_path)
      );
      break;
    }
    case SPN_EVENT_SYNC_END: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->sync_end.time);
      sp_tty_fmt(&w, "synced {} packages in {.cyan}",
        sp_fmt_uint(event->sync_end.num_synced),
        sp_fmt_cstr(buffer)
      );
      break;
    }
    case SPN_EVENT_PUBLISH: {
      sp_tty_fmt(&w, "{} v{}",
        sp_fmt_str(event->publish.name),
        sp_fmt_str(event->publish.version)
      );
      break;
    }
    case SPN_EVENT_PUBLISH_END: {
      sp_tty_fmt(&w, "{} v{} to {} {.gray}",
        sp_fmt_str(event->publish.name),
        sp_fmt_str(event->publish.version),
        sp_fmt_str(event->publish.index),
        sp_fmt_str(event->publish.url)
      );
      break;
    }
    case SPN_EVENT_COMPILE_START: {
      if (!sp_str_empty(event->compile_start.version)) {
        sp_tty_fmt(&w, "v{}", sp_fmt_str(event->compile_start.version));
      }
      break;
    }
    case SPN_EVENT_SCRIPT_COMPILE: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->script_compile.time);
      sp_tty_fmt(&w, "{.gray} in {.gray}",
        sp_fmt_str(event->script_compile.script_path),
        sp_fmt_cstr(buffer)
      );
      break;
    }
    case SPN_EVENT_SCRIPT_USER_FN: {
      sp_tty_fmt(&w, "{}", sp_fmt_str(event->script_user_fn.tag));
      break;
    }
    case SPN_EVENT_PACKAGE_OK: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->package_ok.time);
      sp_tty_fmt(&w, "in {.gray}", sp_fmt_cstr(buffer));
      break;
    }
    case SPN_EVENT_TARGET_BUILD_PASSED: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->target_passed.time);
      sp_tty_fmt(&w, "{.gray} in {.gray}",
        sp_fmt_str(event->target_passed.source_file),
        sp_fmt_cstr(buffer)
      );
      break;
    }
    case SPN_EVENT_LINK_PASSED: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->link_passed.time);
      sp_tty_fmt(&w, "{.gray} in {.gray}",
        sp_fmt_str(event->link_passed.output_path),
        sp_fmt_cstr(buffer)
      );
      break;
    }
    case SPN_EVENT_EMBED_START: {
      sp_tty_fmt(&w, "{} files", sp_fmt_uint(event->embed_start.num_files));
      break;
    }
    case SPN_EVENT_EMBED_PASSED: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->embed_passed.time);
      sp_tty_fmt(&w, "{.gray} in {.gray}",
        sp_fmt_str(event->embed_passed.object_path),
        sp_fmt_cstr(buffer)
      );
      break;
    }
    case SPN_EVENT_BUILD_SUMMARY: {
      c8 duration [64] = sp_zero;
      c8 bytes [64] = sp_zero;
      sp_fmt_write_duration_buf(duration, sizeof(duration), event->build_summary.time);
      sp_fmt_write_size_buf(bytes, sizeof(bytes), event->build_summary.hashed_bytes);
      sp_tty_fmt(&w, "{} executed, {} cached in {.gray} | hashed {} ({.gray}), stats {}, rows {}, cache {}r/{}w",
        sp_fmt_uint(event->build_summary.misses),
        sp_fmt_uint(event->build_summary.hits),
        sp_fmt_cstr(duration),
        sp_fmt_uint(event->build_summary.hashed_files),
        sp_fmt_cstr(bytes),
        sp_fmt_uint(event->build_summary.stats),
        sp_fmt_uint(event->build_summary.obs_rows),
        sp_fmt_uint(event->build_summary.cache_reads),
        sp_fmt_uint(event->build_summary.cache_writes)
      );
      break;
    }
    case SPN_EVENT_RESOLVE_PACKAGE: {
      sp_tty_fmt(&w, "{} {.gray}",
        sp_fmt_str(event->resolve_pkg.name),
        sp_fmt_str(event->resolve_pkg.version)
      );
      break;
    }
    case SPN_EVENT_RESOLVE_END: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->resolve_end.time);
      sp_tty_fmt(&w, "{} {} in {.gray}",
        sp_fmt_uint(event->resolve_end.num_resolved),
        sp_fmt_cstr(event->resolve_end.num_resolved == 1 ? "package" : "packages"),
        sp_fmt_cstr(buffer)
      );
      break;
    }
    case SPN_EVENT_ADDED: {
      sp_tty_fmt(&w, "{.cyan}=={.green}",
        sp_fmt_str(event->added.name),
        sp_fmt_str(event->added.version)
      );
      break;
    }
    case SPN_EVENT_UPDATE_INCOMPATIBLE: {
      sp_tty_fmt(&w, "{.cyan} {.green} (latest {.yellow} is semver incompatible)",
        sp_fmt_str(event->update.name),
        sp_fmt_str(event->update.version),
        sp_fmt_str(event->update.latest)
      );
      break;
    }
    case SPN_EVENT_TARGET_RUN: {
      sp_tty_fmt(&w, "{.gray}", sp_fmt_str(get_contextual_path(ctx, mem, event->target_run.command)));
      break;
    }
    case SPN_EVENT_TEST_PASSED: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->test_passed.time);
      sp_tty_fmt(&w, "in {.gray}", sp_fmt_cstr(buffer));
      break;
    }
    case SPN_EVENT_TEST_FAILED: {
      sp_tty_fmt(&w, "{.yellow} exited with code {}",
        sp_fmt_str(event->test_failed.name),
        sp_fmt_int(event->test_failed.code)
      );
      break;
    }
    case SPN_EVENT_TEST_SUMMARY: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->test_summary.time);
      if (event->test_summary.failed) {
        sp_tty_fmt(&w, "{} passed, {.red} failed in {.gray}",
          sp_fmt_uint(event->test_summary.passed),
          sp_fmt_uint(event->test_summary.failed),
          sp_fmt_cstr(buffer)
        );
      }
      else {
        sp_tty_fmt(&w, "{} passed, {} failed in {.gray}",
          sp_fmt_uint(event->test_summary.passed),
          sp_fmt_uint(event->test_summary.failed),
          sp_fmt_cstr(buffer)
        );
      }
      break;
    }
    case SPN_EVENT_BUILD_PASSED: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->build_passed.time);
      sp_tty_fmt(&w,
        "profile {.cyan} in {.gray} {.gray}",
        sp_fmt_str(event->build_passed.profile),
        sp_fmt_cstr(buffer),
        sp_fmt_str(sp_fmt(mem, "({} executed, {} cached)",
          sp_fmt_uint(event->build_passed.misses),
          sp_fmt_uint(event->build_passed.hits)).value)
      );
      break;
    }
    case SPN_EVENT_SCRIPT_CRASHED: {
      if (sp_str_empty(event->crashed.error)) {
        sp_io_write_str(w.io, sp_str_lit("build script crashed"), SP_NULLPTR);
      }
      else {
        sp_tty_fmt(&w, "build script crashed: {.red}", sp_fmt_str(event->crashed.error));
      }
      break;
    }
    case SPN_EVENT_SCRIPT_COMPILE_FAILED: {
      sp_tty_fmt(&w, "could not compile {.cyan}", sp_fmt_str(get_contextual_path(ctx, mem, event->compile_failed.script_path)));
      break;
    }
    case SPN_EVENT_TARGET_BUILD_FAILED: {
      sp_tty_fmt(&w, "could not compile {.cyan}", sp_fmt_str(get_contextual_path(ctx, mem, event->target_failed.source_file)));
      break;
    }
    case SPN_EVENT_NODE_FAILED: {
      if (sp_str_empty(event->node_failed.path)) {
        sp_io_write_str(w.io, event->node_failed.message, SP_NULLPTR);
      }
      else {
        sp_tty_fmt(&w, "{.cyan} {}",
          sp_fmt_str(get_contextual_path(ctx, mem, event->node_failed.path)),
          sp_fmt_str(event->node_failed.message)
        );
      }
      break;
    }
    case SPN_EVENT_SYNC_FAILED: {
      sp_tty_fmt(
        &w,
        "{} failed to sync from {.gray}: {}",
        sp_fmt_str(colored_name(w.color, mem, event->sync_failed.name)),
        sp_fmt_str(event->sync_failed.url),
        sp_fmt_str(event->sync_failed.error)
      );
      break;
    }
    case SPN_EVENT_SYNC_STALE: {
      sp_tty_fmt(
        &w,
        "{} could not be fetched from {.gray}; using the cached copy",
        sp_fmt_str(colored_name(w.color, mem, event->sync.name)),
        sp_fmt_str(event->sync.url)
      );
      break;
    }
    case SPN_EVENT_ERR: {
      switch (event->err.kind) {
        case SPN_ERR_PATCH_UNUSED: {
          sp_tty_fmt(
            &w,
            "[patch.{.cyan}] does not name a package in this build",
            sp_fmt_str(event->err.patch.name)
          );
          break;
        }
        case SPN_ERR_PATCH_NOT_GIT: {
          sp_tty_fmt(
            &w,
            "[patch.{.cyan}] names a package whose sources are not fetched from git; only git sources can be patched",
            sp_fmt_str(event->err.patch.name)
          );
          break;
        }
        case SPN_ERR_HEADER_COLLISION: {
          sp_tty_fmt(
            &w,
            "two headers are published to {.cyan}: {.gray} and {.gray}",
            sp_fmt_str(event->err.header_collision.path),
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.header_collision.first)),
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.header_collision.second))
          );
          break;
        }
        case SPN_ERR_OPTION_UNDECLARED: {
          sp_tty_fmt(
            &w,
            "{} does not declare an option named {.yellow} (set by {.cyan})",
            sp_fmt_str(colored_name(w.color, mem, event->err.option.pkg)),
            sp_fmt_str(event->err.option.option),
            sp_fmt_str(setter_to_str(event->err.option.a))
          );
          break;
        }
        case SPN_ERR_OPTION_BAD_VALUE: {
          sp_tty_fmt(
            &w,
            "{.yellow} is not a valid value for {}.{.cyan} (set by {.cyan})",
            sp_fmt_str(event->err.option.value),
            sp_fmt_str(colored_name(w.color, mem, event->err.option.pkg)),
            sp_fmt_str(event->err.option.option),
            sp_fmt_str(setter_to_str(event->err.option.a))
          );
          break;
        }
        case SPN_ERR_OPTION_CONFLICT: {
          sp_tty_fmt(
            &w,
            "option conflict on {}.{.cyan}: {.cyan} and {.cyan} request different values",
            sp_fmt_str(colored_name(w.color, mem, event->err.option.pkg)),
            sp_fmt_str(event->err.option.option),
            sp_fmt_str(setter_to_str(event->err.option.a)),
            sp_fmt_str(setter_to_str(event->err.option.b))
          );
          break;
        }
        case SPN_ERR_OPTION_VETO: {
          sp_tty_fmt(
            &w,
            "{.cyan} requires {}.{.cyan} != {.yellow}, but {} set it",
            sp_fmt_str(setter_to_str(event->err.option.a)),
            sp_fmt_str(colored_name(w.color, mem, event->err.option.pkg)),
            sp_fmt_str(event->err.option.option),
            sp_fmt_str(event->err.option.value),
            sp_fmt_str(setter_to_str(event->err.option.b))
          );
          break;
        }
        case SPN_ERR_OPTION_NO_VALUE: {
          sp_tty_fmt(
            &w,
            "no value for {}.{.cyan}: no default matched and nothing set it",
            sp_fmt_str(colored_name(w.color, mem, event->err.option.pkg)),
            sp_fmt_str(event->err.option.option)
          );
          break;
        }
        case SPN_ERR_OPTION_LATE_GATE: {
          sp_tty_fmt(
            &w,
            "the dependency gate on {}'s edge to {.cyan} never settled",
            sp_fmt_str(colored_name(w.color, mem, event->err.option.pkg)),
            sp_fmt_str(setter_to_str(event->err.option.a))
          );
          break;
        }
        case SPN_ERR_OPTION_UNKNOWN_PKG: {
          sp_tty_fmt(
            &w,
            "the root manifest configures {.yellow}, which is not a package in this build",
            sp_fmt_str(event->err.option.pkg)
          );
          break;
        }
        case SPN_ERR_MANIFEST_PARSE: {
          sp_tty_fmt(
            &w,
            "failed to parse manifest {.cyan}",
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.manifest_parse.path))
          );
          break;
        }
        case SPN_ERR_MANIFEST_ISSUES: {
          if (sp_str_empty(event->err.manifest.name)) {
            sp_tty_fmt(
              &w,
              "invalid manifest ({.gray})",
              sp_fmt_str(get_contextual_path(ctx, mem, event->err.manifest.path))
            );
          }
          else {
            sp_tty_fmt(
              &w,
              "{} has an invalid manifest ({.gray})",
              sp_fmt_str(colored_name(w.color, mem, event->err.manifest.name)),
              sp_fmt_str(get_contextual_path(ctx, mem, event->err.manifest.path))
            );
          }
          break;
        }
        case SPN_ERR_PKG_MISMATCH: {
          sp_tty_fmt(
            &w,
            "the manifest at {.cyan} declares {}, but it was requested as {}",
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.mismatch.path)),
            sp_fmt_str(colored_name(w.color, mem, event->err.mismatch.declared)),
            sp_fmt_str(colored_name(w.color, mem, event->err.mismatch.requested))
          );
          break;
        }
        case SPN_ERR_PKG_UNKNOWN: {
          sp_tty_fmt(
            &w,
            "{} could not be located",
            sp_fmt_str(colored_name(w.color, mem, event->err.unknown.qualified))
          );
          break;
        }
        case SPN_ERR_PKG_NO_MATCH: {
          spn_err_unsatisfiable_t* err = &event->err.unsatisfiable;
          sp_str_t requester = sp_str_empty(err->requester) ?
            sp_str_lit("the project") :
            sp_fmt(mem, "{} {}", sp_fmt_str(colored_name(w.color, mem, err->requester)), sp_fmt_str(spn_semver_to_str(mem, err->requester_version))).value;

          sp_tty_fmt(
            &w,
            "no version of {} satisfies {.yellow}, required by {}",
            sp_fmt_str(colored_name(w.color, mem, err->qualified)),
            sp_fmt_str(err->range),
            sp_fmt_str(requester)
          );
          break;
        }
        case SPN_ERR_PKG_CONFLICT: {
          spn_err_unsatisfiable_t* err = &event->err.unsatisfiable;
          sp_str_t requester = sp_str_empty(err->requester) ?
            sp_str_lit("the project") :
            sp_fmt(mem, "{} {}", sp_fmt_str(colored_name(w.color, mem, err->requester)), sp_fmt_str(spn_semver_to_str(mem, err->requester_version))).value;

          sp_tty_fmt(
            &w,
            "{} is already selected at {.yellow}, but {} requires {.yellow}",
            sp_fmt_str(colored_name(w.color, mem, err->qualified)),
            sp_fmt_str(spn_semver_to_str(mem, err->selected)),
            sp_fmt_str(requester),
            sp_fmt_str(err->range)
          );
          break;
        }
        case SPN_ERR_PKG_CONFLICT_EXACT: {
          spn_err_unsatisfiable_t* err = &event->err.unsatisfiable;
          sp_str_t requester = sp_str_empty(err->requester) ?
            sp_str_lit("the project") :
            sp_fmt(mem, "{} {}", sp_fmt_str(colored_name(w.color, mem, err->requester)), sp_fmt_str(spn_semver_to_str(mem, err->requester_version))).value;

          sp_tty_fmt(
            &w,
            "{} is already selected at {.yellow}, which conflicts with the version required by {}",
            sp_fmt_str(colored_name(w.color, mem, err->qualified)),
            sp_fmt_str(spn_semver_to_str(mem, err->selected)),
            sp_fmt_str(requester)
          );
          break;
        }
        case SPN_ERR_DEP_CYCLE: {
          sp_tty_fmt(
            &w,
            "{} transitively includes itself",
            sp_fmt_str(colored_name(w.color, mem, event->err.circular.id.name))
          );
          break;
        }
        case SPN_ERR_UNIT_CYCLE: {
          sp_tty_fmt(
            &w,
            "{} {.yellow} can't build: its build depends on a tool that links this same instance",
            sp_fmt_str(colored_name(w.color, mem, event->err.unit_cycle.id.name)),
            sp_fmt_str(spn_semver_to_str(mem, event->err.unit_cycle.version))
          );
          break;
        }
        case SPN_ERR_DYNAMIC_DUPLICATE: {
          sp_tty_fmt(
            &w,
            "{} {.yellow} and {.yellow} would both load into one process as shared libraries",
            sp_fmt_str(colored_name(w.color, mem, event->err.dynamic_dup.id.name)),
            sp_fmt_str(spn_semver_to_str(mem, event->err.dynamic_dup.low)),
            sp_fmt_str(spn_semver_to_str(mem, event->err.dynamic_dup.high))
          );
          break;
        }
        case SPN_ERR_RESOLVE_TOO_COMPLEX: {
          sp_tty_fmt(
            &w,
            "resolving {} is too complex; pin a version to reduce the search",
            sp_fmt_str(colored_name(w.color, mem, event->err.too_complex.id.name))
          );
          break;
        }
        case SPN_ERR_PROFILE_INVALID: {
          sp_tty_fmt(
            &w,
            "invalid profile {.cyan}",
            sp_fmt_str(event->err.profile.name)
          );
          break;
        }
        case SPN_ERR_PROFILE_UNDEFINED: {
          sp_tty_fmt(
            &w,
            "profile {.cyan} isn't defined",
            sp_fmt_str(event->err.profile.name)
          );
          break;
        }
        case SPN_ERR_PROFILE_ARCH: {
          sp_tty_fmt(
            &w,
            "target {.yellow} isn't valid; {} doesn't run on {.red}",
            sp_fmt_str(spn_triple_to_str(mem, event->err.profile.target)),
            sp_fmt_str(spn_os_to_str(event->err.profile.target.os)),
            sp_fmt_str(spn_arch_to_str(event->err.profile.target.arch))
          );
          break;
        }
        case SPN_ERR_PROFILE_ABI: {
          sp_tty_fmt(
            &w,
            "target {.yellow} isn't valid; {} has no {.red} abi",
            sp_fmt_str(spn_triple_to_str(mem, event->err.profile.target)),
            sp_fmt_str(spn_os_to_str(event->err.profile.target.os)),
            sp_fmt_str(spn_abi_to_str(event->err.profile.target.abi))
          );
          break;
        }
        case SPN_ERR_PROFILE_LINKAGE: {
          sp_tty_fmt(
            &w,
            "target {.yellow} can't link {.red}; {} has no dynamic loader",
            sp_fmt_str(spn_triple_to_str(mem, event->err.profile.target)),
            sp_fmt_str(sp_str_lit("shared")),
            sp_fmt_str(spn_os_to_str(event->err.profile.target.os))
          );
          break;
        }
        case SPN_ERR_SANITIZER_UNSUPPORTED: {
          if (event->err.sanitizer.supported) {
            sp_tty_fmt(
              &w,
              "toolchain {.cyan} targeting {.yellow} doesn't support {.red}; it supports {.green}",
              sp_fmt_str(event->err.sanitizer.toolchain),
              sp_fmt_str(spn_triple_to_str(mem, event->err.sanitizer.target)),
              sp_fmt_str(spn_sanitizer_set_to_str(mem, event->err.sanitizer.unsupported)),
              sp_fmt_str(spn_sanitizer_set_to_str(mem, event->err.sanitizer.supported))
            );
          } else {
            sp_tty_fmt(
              &w,
              "toolchain {.cyan} targeting {.yellow} doesn't support any sanitizers; drop {.red} or pick another toolchain with {.cyan}",
              sp_fmt_str(event->err.sanitizer.toolchain),
              sp_fmt_str(spn_triple_to_str(mem, event->err.sanitizer.target)),
              sp_fmt_str(spn_sanitizer_set_to_str(mem, event->err.sanitizer.unsupported)),
              sp_fmt_str(sp_str_lit("--toolchain"))
            );
          }
          break;
        }
        case SPN_ERR_SANITIZER_STATIC: {
          sp_tty_fmt(
            &w,
            "{.red} requires a dynamically linked executable; set {.cyan} in the profile",
            sp_fmt_str(spn_sanitizer_set_to_str(mem, event->err.sanitizer.unsupported)),
            sp_fmt_str(sp_str_lit("linkage = \"shared\""))
          );
          break;
        }
        case SPN_ERR_COMPILER_FEATURE_UNSUPPORTED: {
          const c8* feature = "";
          switch (event->err.compiler.feature) {
            case SPN_CC_FEATURE_LINK_EXE: feature = "executable linking"; break;
            case SPN_CC_FEATURE_LINK_SHARED: feature = "shared library linking"; break;
            case SPN_CC_FEATURE_LINK_REACTOR: feature = "reactor module linking"; break;
            case SPN_CC_FEATURE_ARCHIVE: feature = "static archiving"; break;
            case SPN_CC_FEATURE_FRAMEWORKS: feature = "framework linking without a macOS SDK"; break;
            case SPN_CC_FEATURE_LINKER_SCRIPT: feature = "linker scripts"; break;
          }

          sp_tty_fmt(
            &w,
            "toolchain {.cyan} targeting {.yellow} doesn't support {.red}",
            sp_fmt_str(event->err.compiler.toolchain),
            sp_fmt_str(spn_triple_to_str(mem, event->err.compiler.target)),
            sp_fmt_cstr(feature)
          );
          break;
        }
        case SPN_ERR_FS_REMOVE: {
          sp_tty_fmt(
            &w,
            "failed to remove {.cyan}",
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.fs.path))
          );
          break;
        }
        case SPN_ERR_WASM_INIT_FAILED: {
          sp_io_write_str(w.io, sp_str_lit("failed to initialize the wasm runtime"), SP_NULLPTR);
          break;
        }
        case SPN_ERR_CONFIGURE_SOURCE_GLOB: {
          sp_tty_fmt(
            &w,
            "{.cyan} declares a configure source {.yellow}, which matched no files",
            sp_fmt_str(event->err.configure_source.name),
            sp_fmt_str(event->err.configure_source.source)
          );
          break;
        }
        case SPN_ERR_CONFIGURE_SOURCE_MISSING: {
          sp_tty_fmt(
            &w,
            "{.cyan} declares a configure source {.yellow}, which is not a file",
            sp_fmt_str(event->err.configure_source.name),
            sp_fmt_str(event->err.configure_source.source)
          );
          break;
        }
        case SPN_ERR_BUILD_GRAPH: {
          sp_tty_fmt(
            &w,
            "failed to construct the build graph at {.cyan}",
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.build_graph.file))
          );
          break;
        }
        case SPN_ERR_FS_READ: {
          sp_tty_fmt(
            &w,
            "failed to read {.cyan}",
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.fs.path))
          );
          break;
        }
        case SPN_ERR_TOC_MAGIC: {
          sp_tty_fmt(
            &w,
            "{.cyan} is not a static archive",
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.fs.path))
          );
          break;
        }
        case SPN_ERR_TOC_TRUNCATED: {
          sp_tty_fmt(
            &w,
            "the symbol table in {.cyan} is truncated",
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.fs.path))
          );
          break;
        }
        case SPN_ERR_TOC_MISSING: {
          sp_tty_fmt(
            &w,
            "{.cyan} has no symbol table",
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.fs.path))
          );
          break;
        }
        case SPN_ERR_SCAFFOLD_TEMPLATE: {
          sp_tty_fmt(
            &w,
            "failed to render the template for {.cyan}",
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.fs.path))
          );
          break;
        }
        case SPN_ERR_FS_CREATE_DIR: {
          sp_tty_fmt(
            &w,
            "failed to create directory {.cyan}",
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.fs.path))
          );
          break;
        }
        case SPN_ERR_FS_WRITE: {
          sp_tty_fmt(
            &w,
            "failed to write {.cyan}",
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.fs.path))
          );
          break;
        }
        case SPN_ERR_PATH_COMPONENT: {
          sp_tty_fmt(
            &w,
            "path {.yellow} must not contain '.', '..', or empty components",
            sp_fmt_str(event->err.fs.path)
          );
          break;
        }
        case SPN_ERR_INDEX_UNKNOWN: {
          sp_tty_fmt(
            &w,
            "index {.cyan} not found",
            sp_fmt_str(event->err.index.name)
          );
          break;
        }
        case SPN_ERR_INDEX_SYNC: {
          sp_tty_fmt(
            &w,
            "failed to sync index {.cyan} from {.gray}",
            sp_fmt_str(event->err.index.name),
            sp_fmt_str(event->err.index.url)
          );
          break;
        }
        case SPN_ERR_MANIFEST_EDIT: {
          sp_tty_fmt(
            &w,
            "failed to edit {.cyan}",
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.manifest_parse.path))
          );
          break;
        }
        case SPN_ERR_NO_MANIFEST: {
          sp_tty_fmt(
            &w,
            "no manifest found at {.cyan}",
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.no_manifest.path))
          );
          break;
        }
        case SPN_ERR_NOT_GIT_REPO: {
          sp_tty_fmt(
            &w,
            "{.cyan} is not inside a git repository",
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.not_git_repo.path))
          );
          break;
        }
        case SPN_ERR_GIT: {
          sp_tty_fmt(
            &w,
            "git command failed: {.yellow}",
            sp_fmt_str(event->err.git.command)
          );
          break;
        }
        case SPN_ERR_VERSION_EXISTS: {
          sp_tty_fmt(
            &w,
            "version {.yellow} of {.cyan} already exists in the index",
            sp_fmt_str(event->err.version_exists.version),
            sp_fmt_str(event->err.version_exists.name)
          );
          break;
        }
        case SPN_ERR_VERSION_INVALID: {
          sp_tty_fmt(
            &w,
            "invalid version {.red}",
            sp_fmt_str(event->err.version_invalid.requested)
          );
          break;
        }
        case SPN_ERR_INDEX_PINNED: {
          sp_tty_fmt(
            &w,
            "index {.cyan} is pinned to a revision and cannot be published to",
            sp_fmt_str(event->err.index.name)
          );
          break;
        }
        case SPN_ERR_INDEX_PUBLISH_PROTOCOL: {
          sp_tty_fmt(
            &w,
            "index {.cyan} does not support publishing",
            sp_fmt_str(event->err.index.name)
          );
          break;
        }
        case SPN_ERR_INDEX_CORRUPT: {
          sp_tty_fmt(
            &w,
            "index entry for {.cyan} at {.cyan} is corrupt",
            sp_fmt_str(event->err.index_corrupt.name),
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.index_corrupt.path))
          );
          break;
        }
        case SPN_ERR_PUBLISH_PUSH: {
          sp_tty_fmt(
            &w,
            "failed to push to {.gray}",
            sp_fmt_str(event->err.publish.url)
          );
          break;
        }
        case SPN_ERR_PUBLISH_DIRTY: {
          sp_tty_fmt(
            &w,
            "{.cyan} has uncommitted changes; commit them or pass {.yellow}",
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.publish.path)),
            sp_fmt_cstr("--allow-dirty")
          );
          break;
        }
        case SPN_ERR_PUBLISH_UNPUSHED: {
          sp_tty_fmt(
            &w,
            "commit {.yellow} is not on any branch of {.gray}; push it first",
            sp_fmt_str(event->err.publish.rev),
            sp_fmt_str(event->err.publish.url)
          );
          break;
        }
        case SPN_ERR_INDEX_PATH_DEP: {
          sp_tty_fmt(
            &w,
            "{.cyan} depends on {.cyan} by path; index packages must depend on versions",
            sp_fmt_str(event->err.pkg.name),
            sp_fmt_str(event->err.pkg.requested)
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_FETCH: {
          sp_tty_fmt(
            &w,
            "toolchain {} failed to download from {.gray}",
            sp_fmt_str(colored_name(w.color, mem, event->err.artifact.name)),
            sp_fmt_str(event->err.artifact.url)
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_SHA: {
          sp_tty_fmt(
            &w,
            "toolchain {} sha256 mismatch for {.gray}: expected {.yellow}, got {.red}",
            sp_fmt_str(colored_name(w.color, mem, event->err.artifact.name)),
            sp_fmt_str(event->err.artifact.url),
            sp_fmt_str(event->err.artifact.expected),
            sp_fmt_str(event->err.artifact.actual)
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_EXTRACT: {
          sp_tty_fmt(
            &w,
            "toolchain {} failed to extract archive from {.gray}",
            sp_fmt_str(colored_name(w.color, mem, event->err.artifact.name)),
            sp_fmt_str(event->err.artifact.url)
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_UNKNOWN: {
          sp_tty_fmt(
            &w,
            "toolchain {} isn't defined",
            sp_fmt_str(colored_name(w.color, mem, event->err.toolchain.name))
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_TARGET: {
          sp_tty_fmt(
            &w,
            "toolchain {} can't target {.yellow}",
            sp_fmt_str(colored_name(w.color, mem, event->err.toolchain.name)),
            sp_fmt_str(spn_triple_to_str(mem, event->err.toolchain.target))
          );
          break;
        }
        case SPN_ERR_TARGET_ABI: {
          sp_tty_fmt(
            &w,
            "cross target {.yellow} needs an abi; pass --abi or add it to --target",
            sp_fmt_str(spn_triple_to_str(mem, event->err.completion.target))
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_NONE: {
          sp_str_t target = spn_triple_to_str(mem, event->err.toolchain.target);
          sp_tty_fmt(
            &w,
            "no toolchain in the catalog can target {.yellow}",
            sp_fmt_str(target)
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_HOST: {
          sp_str_t host = spn_triple_to_str(mem, event->err.toolchain.host);
          sp_tty_fmt(
            &w,
            "toolchain {} isn't available on host {.yellow}",
            sp_fmt_str(colored_name(w.color, mem, event->err.toolchain.name)),
            sp_fmt_str(host)
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_MISSING: {
          sp_tty_fmt(
            &w,
            "toolchain {} needs {.cyan}, which isn't installed",
            sp_fmt_str(colored_name(w.color, mem, event->err.program.name)),
            sp_fmt_str(event->err.program.program)
          );
          break;
        }
        case SPN_ERR_WASM_READ_FAILED: {
          sp_tty_fmt(
            &w,
            "failed to read build script {.cyan}",
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.wasm.path))
          );
          break;
        }
        case SPN_ERR_WASM_MODULE_LOAD_FAILED: {
          sp_tty_fmt(
            &w,
            "failed to load build script {.cyan}: {.red}",
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.wasm.path)),
            sp_fmt_str(event->err.wasm.error)
          );
          break;
        }
        case SPN_ERR_WASM_MODULE_INSTANCE_FAILED: {
          sp_tty_fmt(
            &w,
            "failed to instantiate build script {.cyan}: {.red}",
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.wasm.path)),
            sp_fmt_str(event->err.wasm.error)
          );
          break;
        }
        case SPN_ERR_WASM_THREAD_ENV_FAILED: {
          sp_tty_fmt(
            &w,
            "failed to init wasm thread env for build script {.cyan}",
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.wasm.path))
          );
          break;
        }
        case SPN_ERR_WASM_CTX_FAILED: {
          sp_tty_fmt(
            &w,
            "failed to create wasm context for build script {.cyan}",
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.wasm.path))
          );
          break;
        }
        case SPN_ERR_WASM_MODULE_CALL_FAILED: {
          sp_tty_fmt(
            &w,
            "build script {.cyan} crashed: {.red}",
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.wasm.path)),
            sp_fmt_str(event->err.wasm.error)
          );
          break;
        }
        case SPN_ERR_WASM_SCRIPT_ERROR: {
          sp_tty_fmt(
            &w,
            "build script {.cyan} returned {.red}",
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.wasm.path)),
            sp_fmt_int(event->err.wasm.rc)
          );
          break;
        }
        case SPN_ERR_WASM_NO_SCRIPT: {
          sp_io_write_str(w.io, sp_str_lit("node has a wasm fn but no build script is loaded"), SP_NULLPTR);
          break;
        }
        case SPN_ERR_WASM_EXPORT_NOT_FOUND: {
          sp_tty_fmt(
            &w,
            "Referenced symbol {.yellow} was not found in {.cyan}",
            sp_fmt_str(event->err.wasm.error),
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.wasm.path))
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_NO_CXX: {
          sp_tty_fmt(
            &w,
            "Toolchain {} has no C++ compiler, but the build contains C++ sources",
            sp_fmt_str(colored_name(w.color, mem, event->err.toolchain.name))
          );
          break;
        }
        case SPN_ERR_TARGET_LINKAGE: {
          sp_tty_fmt(
            &w,
            "{.cyan} doesn't support {.yellow} ({} requested it)",
            sp_fmt_str(event->err.target.pkg),
            sp_fmt_str(event->err.target.requested),
            sp_fmt_str(event->err.target.requester)
          );
          break;
        }
        case SPN_ERR_TARGET_DUPLICATE: {
          sp_tty_fmt(
            &w,
            "{.cyan} declares a target {.yellow}, which collides with another target of the same name",
            sp_fmt_str(event->err.target.pkg),
            sp_fmt_str(event->err.target.name)
          );
          break;
        }
        case SPN_ERR_TARGET_RESERVED: {
          sp_tty_fmt(
            &w,
            "{.cyan} names an executable {.yellow}, which collides with a build output directory (store, work, test)",
            sp_fmt_str(event->err.target.pkg),
            sp_fmt_str(event->err.target.name)
          );
          break;
        }
        case SPN_ERR_TARGET_DEP: {
          sp_tty_fmt(
            &w,
            "failed to find {.cyan} as a package or target",
            sp_fmt_str(event->err.target.name)
          );
          break;
        }
        case SPN_ERR_TARGET_SELECTION: {
          sp_tty_fmt(
            &w,
            "target {.yellow} is not defined for the selected target kinds",
            sp_fmt_str(event->err.target.name)
          );
          break;
        }
        case SPN_ERR_INIT_EXISTS: {
          sp_tty_fmt(
            &w,
            "{.cyan} already exists",
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.fs.path))
          );
          break;
        }
        case SPN_ERR_INIT_NAME: {
          sp_tty_fmt(
            &w,
            "invalid name {.quote}",
            sp_fmt_str(event->err.pkg.name)
          );
          break;
        }
        case SPN_ERR_SCRIPT_MISSING: {
          sp_tty_fmt(
            &w,
            "script {.yellow} has no binary at {.cyan}",
            sp_fmt_str(event->err.script.name),
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.script.path))
          );
          break;
        }
        case SPN_ERR_TEST_MISSING: {
          sp_tty_fmt(
            &w,
            "test {.yellow} has no binary at {.cyan}",
            sp_fmt_str(event->err.script.name),
            sp_fmt_str(get_contextual_path(ctx, mem, event->err.script.path))
          );
          break;
        }
        case SPN_ERR_TEST_FAILED: {
          sp_io_write_str(w.io, sp_str_lit("one or more tests failed"), SP_NULLPTR);
          break;
        }
        case SPN_ERR_SCRIPT_FAILED: {
          sp_tty_fmt(
            &w,
            "script {.yellow} failed with exit code {}",
            sp_fmt_str(event->err.script.name),
            sp_fmt_int(event->err.script.code)
          );
          break;
        }
        case SPN_ERR_DAG_DUPLICATE_OUTPUT: {
          sp_io_write_str(w.io, sp_str_lit("two build actions produce the same output file"), SP_NULLPTR);
          break;
        }
        case SPN_ERR_DAG_OUTPUT_NAME: {
          sp_io_write_str(w.io, sp_str_lit("a build action output has no file name"), SP_NULLPTR);
          break;
        }
        case SPN_ERR_DAG_PINNED_OUTPUT: {
          sp_tty_fmt(&w, "{.cyan} is declared as a build action output, but it is inside an immutable cache directory", sp_fmt_str(get_contextual_path(ctx, mem, event->err.dag.path)));
          break;
        }
        case SPN_ERR_DAG_MISSING_INPUT: {
          sp_tty_fmt(&w, "{.cyan} doesn't exist, but is listed as an input", sp_fmt_str(get_contextual_path(ctx, mem, event->err.dag.path)));
          break;
        }
        case SPN_ERR_DAG_MISSING_OUTPUT: {
          sp_tty_fmt(&w, "{.cyan} was not produced by the action that declares it as an output", sp_fmt_str(get_contextual_path(ctx, mem, event->err.dag.path)));
          break;
        }
        case SPN_ERR_DAG_DEPFILE: {
          sp_tty_fmt(&w, "the compiler did not write a valid dependency file for {.cyan}", sp_fmt_str(get_contextual_path(ctx, mem, event->err.dag.path)));
          break;
        }
        case SPN_ERR_DAG_STORE_READ: {
          sp_tty_fmt(&w, "{.cyan} could not be read from the content store", sp_fmt_str(get_contextual_path(ctx, mem, event->err.dag.path)));
          break;
        }
        case SPN_ERR_DAG_STORE_WRITE: {
          sp_tty_fmt(&w, "{.cyan} could not be written to the content store", sp_fmt_str(get_contextual_path(ctx, mem, event->err.dag.path)));
          break;
        }
        case SPN_ERR_DAG_SCRATCH: {
          sp_io_write_str(w.io, sp_str_lit("failed to create a scratch directory for the build"), SP_NULLPTR);
          break;
        }
        case SPN_ERR_DAG_STALLED: {
          sp_io_write_str(w.io, sp_str_lit("the build graph stalled before completing"), SP_NULLPTR);
          break;
        }
        default: {
          sp_io_write_str(w.io, sp_str_lit("unknown error"), SP_NULLPTR);
          break;
        }
      }
      break;
    }
    case SPN_EVENT_USER_LOG: {
      sp_io_write_str(w.io, event->user_log.message, SP_NULLPTR);
      break;
    }
    case SPN_EVENT_API_CALL: {
      sp_tty_fmt(&w, "{}({})",
        sp_fmt_str(event->api_call.fn),
        sp_fmt_str(event->api_call.args)
      );
      break;
    }
    case SPN_EVENT_INIT_BUILD_GRAPH: {
      if (sp_str_empty(event->graph_init.version)) {
        sp_tty_fmt(&w, "{}, {.yellow}, {.green}",
          sp_fmt_str(event->graph_init.profile),
          sp_fmt_str(event->graph_init.target),
          sp_fmt_str(event->graph_init.toolchain)
        );
      } else {
        sp_tty_fmt(&w, "{}, {.yellow}, {.green} {.gray}",
          sp_fmt_str(event->graph_init.profile),
          sp_fmt_str(event->graph_init.target),
          sp_fmt_str(event->graph_init.toolchain),
          sp_fmt_str(event->graph_init.version)
        );
      }
      break;
    }
    case SPN_EVENT_LINK_START: {
      sp_tty_fmt(&w, "{.cyan}",
        sp_fmt_str(event->link_start.target)
      );
      break;
    }
    case SPN_EVENT_LINK_FAILED: {
      sp_tty_fmt(&w, "could not link {.cyan}", sp_fmt_str(event->link_failed.target));
      break;
    }
    case SPN_EVENT_CONFIGURE_OK: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->configure.time);
      sp_tty_fmt(&w, "in {.gray}", sp_fmt_cstr(buffer));
      break;
    }
    case SPN_EVENT_EMBED_FAILED: {
      sp_tty_fmt(&w, "{}: {}",
        sp_fmt_str(get_contextual_path(ctx, mem, event->embed_failed.path)),
        sp_fmt_str(event->embed_failed.error)
      );
      break;
    }
    case SPN_EVENT_BUILD_FAILED: {
      sp_tty_fmt(&w, "profile {.cyan}", sp_fmt_str(event->build_failed.profile));
      break;
    }
    default: {
      break;
    }
  }

  return sp_io_dyn_mem_writer_take_str(&buf);
}

static sp_str_t get_short_name(sp_str_t qualified) {
  sp_for(it, qualified.len) {
    u32 index = qualified.len - it - 1;
    if (qualified.data[index] == '/') {
      return sp_str_sub(qualified, index + 1, qualified.len - index - 1);
    }
  }
  return qualified;
}

static void write_event(sp_tty_t* tty, sp_str_t verb, sp_fmt_style_t style, sp_str_t pkg, sp_str_t detail) {
  sp_tty_fmt(tty, "{:>12 .bold .$}", sp_fmt_style(style), sp_fmt_str(verb));

  if (sp_str_empty(pkg)) {
    sp_tty_fmt(tty, " {.gray}", sp_fmt_cstr("▐"));
  } else {
    sp_io_write_c8(tty->io, ' ');
    write_name(tty, pkg);
  }
  if (!sp_str_empty(detail)) {
    sp_tty_fmt(tty, " {}", sp_fmt_str(detail));
  }
  sp_io_write_c8(tty->io, '\n');
}

static void write_error(sp_tty_t* tty, sp_str_t detail) {
  sp_tty_fmt(tty, "{.bold .red}", sp_fmt_cstr("error:"));
  if (!sp_str_empty(detail)) {
    sp_tty_fmt(tty, " {}", sp_fmt_str(detail));
  }
  sp_io_write_c8(tty->io, '\n');
}

static sp_str_t event_subject(spn_event_t* event) {
  switch (event->kind) {
    case SPN_EVENT_SYNC_FAILED:               return event->sync_failed.name;
    case SPN_EVENT_ERR: {
      switch (event->err.kind) {
        case SPN_ERR_MANIFEST_ISSUES:  return event->err.manifest.name;
        case SPN_ERR_PKG_UNKNOWN:      return event->err.unknown.qualified;
        case SPN_ERR_PKG_NO_MATCH:     return event->err.unsatisfiable.qualified;
        case SPN_ERR_PKG_CONFLICT:     return event->err.unsatisfiable.qualified;
        case SPN_ERR_PKG_CONFLICT_EXACT: return event->err.unsatisfiable.qualified;
        case SPN_ERR_DEP_CYCLE:        return event->err.circular.id.name;
        case SPN_ERR_UNIT_CYCLE:       return event->err.unit_cycle.id.name;
        case SPN_ERR_DYNAMIC_DUPLICATE: return event->err.dynamic_dup.id.name;
        case SPN_ERR_RESOLVE_TOO_COMPLEX: return event->err.too_complex.id.name;
        case SPN_ERR_PATCH_UNUSED:     return event->err.patch.name;
        case SPN_ERR_PATCH_NOT_GIT:    return event->err.patch.name;
        default:                       break;
      }
      return event->pkg;
    }
    default:                                  return event->pkg;
  }
}

static void render_event_extra(sp_tty_t* w, spn_event_t* event) {
  switch (event->kind) {
    case SPN_EVENT_TARGET_BUILD_FAILED: {
      sp_io_write_str(w->io, event->target_failed.out, SP_NULLPTR);
      sp_io_write_str(w->io, event->target_failed.err, SP_NULLPTR);
      break;
    }
    case SPN_EVENT_LINK_FAILED: {
      sp_io_write_str(w->io, event->link_failed.out, SP_NULLPTR);
      sp_io_write_str(w->io, event->link_failed.err, SP_NULLPTR);
      break;
    }
    case SPN_EVENT_TEST_FAILED: {
      sp_io_write_str(w->io, event->test_failed.out, SP_NULLPTR);
      sp_io_write_str(w->io, event->test_failed.err, SP_NULLPTR);
      break;
    }
    case SPN_EVENT_SCRIPT_COMPILE_FAILED: {
      sp_io_write_str(w->io, event->compile_failed.error, SP_NULLPTR);
      break;
    }
    case SPN_EVENT_ERR: {
      switch (event->err.kind) {
        case SPN_ERR_MANIFEST_ISSUES: {
          sp_da_for(event->err.manifest.issues, it) {
            sp_io_write_str(w->io, sp_str_lit("  - "), SP_NULLPTR);
            write_manifest_issue(w, &event->err.manifest.issues[it]);
            sp_io_write_c8(w->io, '\n');
          }
          break;
        }
        case SPN_ERR_PUBLISH_PUSH: {
          sp_io_write_str(w->io, event->err.publish.output, SP_NULLPTR);
          break;
        }
        case SPN_ERR_TOOLCHAIN_TARGET: {
          sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
          sp_da_for(event->err.toolchain.targets, it) {
            sp_io_write_str(w->io, it ? sp_str_lit(", ") : sp_str_lit("it can target: "), SP_NULLPTR);
            sp_tty_fmt(w, "{.yellow}", sp_fmt_str(spn_triple_to_str(scratch.mem, event->err.toolchain.targets[it])));
          }
          sp_mem_end_scratch(scratch);
          if (!sp_da_empty(event->err.toolchain.targets)) {
            sp_io_write_c8(w->io, '\n');
          }
          sp_da_for(event->err.toolchain.candidates, it) {
            sp_io_write_str(w->io, it ? sp_str_lit(", ") : sp_str_lit("toolchains that can: "), SP_NULLPTR);
            sp_tty_fmt(w, "{.green}", sp_fmt_str(event->err.toolchain.candidates[it]));
          }
          if (!sp_da_empty(event->err.toolchain.candidates)) {
            sp_io_write_c8(w->io, '\n');
          }
          break;
        }
        case SPN_ERR_TOOLCHAIN_HOST:
        case SPN_ERR_TOOLCHAIN_UNKNOWN:
        case SPN_ERR_TOOLCHAIN_NONE: {
          sp_da_for(event->err.toolchain.candidates, it) {
            sp_io_write_str(w->io, it ? sp_str_lit(", ") : sp_str_lit("toolchains that can: "), SP_NULLPTR);
            sp_tty_fmt(w, "{.green}", sp_fmt_str(event->err.toolchain.candidates[it]));
          }
          if (!sp_da_empty(event->err.toolchain.candidates)) {
            sp_io_write_c8(w->io, '\n');
          }
          break;
        }
        case SPN_ERR_TARGET_ABI: {
          sp_da_for(event->err.completion.candidates, it) {
            sp_io_write_str(w->io, it ? sp_str_lit(", ") : sp_str_lit("one of: "), SP_NULLPTR);
            sp_tty_fmt(w, "{.green}", sp_fmt_str(spn_abi_to_str(event->err.completion.candidates[it])));
          }
          if (!sp_da_empty(event->err.completion.candidates)) {
            sp_io_write_c8(w->io, '\n');
          }
          break;
        }
        default: {
          break;
        }
      }
      break;
    }
    default: {
      break;
    }
  }
}

void spn_tui_log_event(spn_tui_t* tui, spn_event_t* event) {
  if (tui->json) {
    spn_event_log_jsonl(tui->out->io, event);
    return;
  }

  const spn_event_info_t* info = &spn_event_info[event->kind];
  if (info->severity == SPN_EVENT_SEVERITY_INFO && info->verbosity > tui->verbosity) {
    if (event->kind == SPN_EVENT_USER_LOG) {
      sp_da_push(tui->buffered_logs, ((spn_tui_buffered_log_t) {
        .pkg = sp_str_copy(tui->mem, event->pkg),
        .message = sp_str_copy(tui->mem, event->user_log.message),
      }));
    }
    return;
  }

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_mem_t mem = scratch.mem;
  sp_tty_t* tty = &tui->journal;
  sp_str_t verb = sp_cstr_as_str(info->verb);

  if (info->severity != SPN_EVENT_SEVERITY_INFO) {
    sp_da_for(tui->buffered_logs, it) {
      spn_tui_buffered_log_t* log = &tui->buffered_logs[it];
      write_event(tty, sp_str_lit(""), sp_fmt_style_green, log->pkg, log->message);
    }
    sp_da_clear(tui->buffered_logs);

    sp_str_t detail = render_event_detail(tui, mem, event);
    if (info->severity == SPN_EVENT_SEVERITY_FATAL) {
      sp_str_t subject = event_subject(event);
      if (!sp_str_empty(subject)) {
        write_event(tty, sp_str_lit("Failed"), sp_fmt_style_red, subject, sp_str_lit(""));
        sp_io_write_c8(tty->io, '\n');
      }
      write_error(tty, detail);
    } else {
      write_event(tty, verb, sp_fmt_style_red, event->pkg, detail);
    }
    render_event_extra(tty, event);
    flush_writer(&tui->writer);
    sp_mem_end_scratch(scratch);
    return;
  }

  switch (event->kind) {
    case SPN_EVENT_SYNC: {
      if (!sp_str_ht_get(tui->seen_url, event->sync.url)) {
        sp_str_ht_insert(tui->seen_url, sp_str_copy(tui->mem, event->sync.url), true);
        tui->num_downloads++;
        write_event(
          tty, verb, sp_fmt_style_green,
          get_short_name(event->sync.name),
          colored_fmt(tty->color, mem, "{.gray}", sp_fmt_str(event->sync.url))
        );
      }
      break;
    }

    case SPN_EVENT_SYNC_END: {
      if (!tui->num_downloads) {
        break;
      }

      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->sync_end.time);
      write_event(
        tty, verb, sp_fmt_style_green,
        sp_str_lit(""),
        colored_fmt(tty->color, mem, "{} {} in {.gray}",
          sp_fmt_uint(tui->num_downloads),
          sp_fmt_cstr(tui->num_downloads == 1 ? "package" : "packages"),
          sp_fmt_cstr(buffer)
        )
      );
      break;
    }

    case SPN_EVENT_RESOLVE_END:
    case SPN_EVENT_BUILD_PASSED:
    case SPN_EVENT_TEST_SUMMARY: {
      write_event(tty, verb, sp_fmt_style_green, sp_str_lit(""), render_event_detail(tui, mem, event));
      break;
    }

    case SPN_EVENT_TEST_PASSED: {
      write_event(tty, verb, sp_fmt_style_green, event->test_passed.name, render_event_detail(tui, mem, event));
      break;
    }

    case SPN_EVENT_TARGET_RUN: {
      sp_str_t name = event->target_run.name;
      if (sp_str_empty(name)) {
        name = event->pkg;
      }
      write_event(tty, verb, sp_fmt_style_green, name, render_event_detail(tui, mem, event));
      break;
    }

    case SPN_EVENT_USER_LOG: {
      sp_str_t name = event->pkg;
      write_event(tty, sp_str_lit(""), sp_fmt_style_green, name, event->user_log.message);
      break;
    }

    default: {
      sp_str_t name = event->pkg;
      write_event(tty, verb, sp_fmt_style_green, name, render_event_detail(tui, mem, event));
      break;
    }
  }

  flush_writer(&tui->writer);
  sp_mem_end_scratch(scratch);
}

void spn_tui_error_v(spn_tui_t* tui, const c8* fmt, va_list args) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_io_dyn_mem_writer_t buf = sp_zero;
  sp_io_dyn_mem_writer_init(scratch.mem, &buf);
  sp_tty_t tty = { .io = &buf.base, .color = tui->journal.color };
  sp_tty_fmt_v(&tty, sp_cstr_as_str(fmt), args);

  write_error(&tui->journal, sp_io_dyn_mem_writer_as_str(&buf));
  flush_writer(&tui->writer);
  sp_mem_end_scratch(scratch);
}

void spn_tui_error(spn_tui_t* tui, const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  spn_tui_error_v(tui, fmt, args);
  va_end(args);
}

void spn_tui_usage(spn_tui_t* tui, sp_cli_err_t err) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_io_dyn_mem_writer_t buf = sp_zero;
  sp_io_dyn_mem_writer_init(scratch.mem, &buf);
  sp_tty_t tty = { .io = &buf.base, .color = tui->journal.color };
  sp_cli_err_print(&tty, err);

  spn_tui_error(tui, "{}", sp_fmt_str(sp_io_dyn_mem_writer_as_str(&buf)));
  sp_mem_end_scratch(scratch);
}

static void write_line(spn_tui_line_writer_t* writer, sp_str_t line) {
  if (writer->prompt) {
    sp_prompt_log_str(writer->prompt, line);
  } else {
    sp_io_write_str(writer->downstream, line, SP_NULLPTR);
    sp_io_write_c8(writer->downstream, '\n');
  }
}

static void complete_line(spn_tui_line_writer_t* writer) {
  sp_str_t line = sp_str_trim_right(sp_str(writer->partial, sp_da_size(writer->partial)));

  if (sp_str_empty(line)) {
    writer->deferred_blanks++;
  } else {
    sp_for(it, writer->deferred_blanks) {
      write_line(writer, sp_str_lit(""));
    }
    writer->deferred_blanks = 0;
    write_line(writer, line);
  }

  sp_da_clear(writer->partial);
}

static sp_err_t on_write(sp_io_writer_t* w, const void* ptr, u64 size, u64* bytes_written) {
  spn_tui_line_writer_t* writer = (spn_tui_line_writer_t*)w;
  const c8* bytes = (const c8*)ptr;

  sp_for(it, size) {
    c8 c = bytes[it];
    if (c == '\n') {
      complete_line(writer);
    } else {
      sp_da_push(writer->partial, c);
    }
  }

  if (bytes_written) {
    *bytes_written = size;
  }
  return SP_OK;
}

static void flush_writer(spn_tui_line_writer_t* writer) {
  if (!sp_da_empty(writer->partial)) {
    complete_line(writer);
  }
  writer->deferred_blanks = 0;
}

static void attach_prompt(spn_tui_t* tui, sp_prompt_ctx_t* ctx) {
  flush_writer(&tui->writer);
  tui->writer.prompt = ctx;
}

static void detach_prompt(spn_tui_t* tui) {
  tui->writer.prompt = SP_NULLPTR;
  flush_writer(&tui->writer);
}

void spn_tui_init(spn_tui_t* tui, spn_tui_desc_t desc) {
  tui->ctx = desc.ctx;
  tui->json = desc.json;
  tui->verbosity = desc.verbosity;
  tui->wake = desc.wake;

  tui->mem = sp_mem_arena_as_allocator(sp_mem_arena_new(sp_mem_os_new()));

  tui->out = sp_tty_std_out();
  tui->err = sp_tty_std_err();
  if (desc.no_color) {
    tui->out->color = SP_TTY_COLOR_NONE;
    tui->err->color = SP_TTY_COLOR_NONE;
  }
  tui->interactive = !desc.json && sp_sys_is_tty(sp_sys_stdout);
#ifdef SP_WIN32
  if (sp_sys_is_tty(sp_sys_stdout) || sp_sys_is_tty(sp_sys_stderr)) {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
  }
#endif

  tui->writer = (spn_tui_line_writer_t) {
    .base.write = on_write,
    .downstream = tui->err->io,
  };
  tui->journal = (sp_tty_t) {
    .io = &tui->writer.base,
    .color = tui->err->color,
  };
  sp_da_init(tui->mem, tui->writer.partial);
  sp_str_ht_init(tui->mem, tui->seen_url);
  sp_da_init(tui->mem, tui->buffered_logs);
  sp_ht_init(tui->mem, tui->thread_ids);
}

static u32 get_short_tid(spn_tui_t* tui, u64 thread_id) {
  static u32 next_thread_id = 0;
  if (!sp_ht_key_exists(tui->thread_ids, thread_id)) {
    sp_ht_insert(tui->thread_ids, thread_id, next_thread_id++);
  }
  return *sp_ht_getp(tui->thread_ids, thread_id);
}

static void emit_event(spn_tui_t* tui, spn_event_t* event) {
  event->thread_id = get_short_tid(tui, event->thread_id);
  spn_tui_log_event(tui, event);
}

void spn_tui_flush(spn_tui_t* tui) {
  if (!tui->ctx) {
    return;
  }

  while (true) {
    spn_event_t* event = spn_ctx_drain(tui->ctx);
    if (!event) {
      break;
    }

    switch (event->kind) {
      case SPN_EVENT_BUILD_PASSED: {
        spn_prompt_stop(tui, SP_PROMPT_STATE_SUBMIT);
        break;
      }
      case SPN_EVENT_BUILD_FAILED: {
        bool cancelled = tui->prompt.op && spn_op_cancelled(tui->prompt.op);
        spn_prompt_stop(tui, cancelled ? SP_PROMPT_STATE_CANCEL : SP_PROMPT_STATE_ERROR);
        break;
      }
      default: {
        break;
      }
    }

    emit_event(tui, event);
  }
}

static void print_line(sp_tty_t* tty, const c8* fmt, va_list args) {
  sp_tty_fmt_v(tty, sp_cstr_as_str(fmt), args);
  sp_io_write_new_line(tty->io);
}

static sp_tty_t* print_tty(spn_tui_t* tui) {
  if (tui->json) {
    return tui->err;
  }
  return tui->out;
}

void spn_print(spn_tui_t* tui, const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  print_line(print_tty(tui), fmt, args);
  va_end(args);
}

void spn_print_err(spn_tui_t* tui, const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  print_line(tui->err, fmt, args);
  va_end(args);
}

static spn_tui_t* prompt_tui;

static void on_prompt_event(sp_prompt_ctx_t* ctx, sp_prompt_event_t event) {
  switch (event.kind) {
    case SP_PROMPT_EVENT_CTRL_C: {
      if (prompt_tui->prompt.op) {
        spn_op_cancel(prompt_tui->prompt.op);
      }
      break;
    }
    case SP_PROMPT_EVENT_ESCAPE: {
      break;
    }
    default: {
      prompt_tui->prompt.widget.on_event(ctx, event);
      break;
    }
  }
}

static void prompt_start(spn_tui_t* tui) {
  if (tui->prompt.started) {
    return;
  }
  tui->prompt.started = true;

  if (!tui->interactive) {
    return;
  }

  tui->prompt.ctx = sp_prompt_begin(tui->mem);
  if (!tui->prompt.ctx) {
    return;
  }
  sp_prompt_use_wake(tui->prompt.ctx, tui->wake);

  prompt_tui = tui;
  tui->prompt.widget = sp_prompt_progress_widget(tui->prompt.ctx, (sp_prompt_progress_t) {
    .prompt = "Building",
    .color = { .rgb = { .r = 99, .g = 160, .b = 136 } },
  });
  sp_prompt_widget_t widget = tui->prompt.widget;
  widget.on_event = on_prompt_event;
  tui->prompt.app = sp_app_new(tui->mem, sp_prompt_app(tui->prompt.ctx, widget));
  tui->prompt.last = sp_zero_s(spn_progress_t);
  tui->prompt.on = true;

  if (sp_sys_is_tty(sp_sys_stderr)) {
    attach_prompt(tui, tui->prompt.ctx);
  }
}

void spn_prompt_stop(spn_tui_t* tui, sp_prompt_state_t state) {
  if (!tui->prompt.on) {
    return;
  }

  sp_prompt_set_state(tui->prompt.ctx, state);
  sp_app_tick(tui->prompt.app);
  sp_prompt_end(tui->prompt.ctx);
  tui->prompt.on = false;
  detach_prompt(tui);
}

static void prompt_pump(spn_tui_t* tui, bool building, spn_progress_t progress) {
  if (spn_op_cancelled(tui->prompt.op)) {
    spn_prompt_stop(tui, SP_PROMPT_STATE_CANCEL);
    return;
  }

  if (!tui->prompt.on) {
    if (!building || !progress.misses) return;
    prompt_start(tui);
    if (!tui->prompt.on) return;
  }

  if (building && (progress.completed != tui->prompt.last.completed || progress.total != tui->prompt.last.total)) {
    tui->prompt.last = progress;

    sp_mem_arena_marker_t s = sp_mem_begin_scratch();
    sp_prompt_send_status_str(tui->prompt.ctx, sp_fmt(s.mem,
      "{}/{} units", sp_fmt_uint(progress.completed), sp_fmt_uint(progress.total)).value);
    sp_mem_end_scratch(s);

    f32 value = progress.total ? (f32)progress.completed / (f32)progress.total : 0.f;
    sp_prompt_send_progress_f32(tui->prompt.ctx, value);
  }

  sp_app_tick(tui->prompt.app);
}

void spn_tui_poll(spn_tui_t* tui, spn_op_t* op) {
  if (tui->handoff.granted) {
    return;
  }

  tui->prompt.op = op;
  spn_tui_flush(tui);
  spn_progress_t progress = sp_zero;
  bool building = spn_ctx_progress(tui->ctx, &progress);
  prompt_pump(tui, building, progress);
}

bool spn_tui_wants_input(spn_tui_t* tui) {
  return tui->prompt.on;
}

void spn_tui_op_done(spn_tui_t* tui, spn_op_t* op) {
  if (tui->handoff.granted) {
    tui->prompt.op = SP_NULLPTR;
    return;
  }

  tui->prompt.op = op;
  spn_tui_flush(tui);

  sp_prompt_state_t state = SP_PROMPT_STATE_SUBMIT;
  if (spn_op_cancelled(op)) {
    state = SP_PROMPT_STATE_CANCEL;
  } else if (spn_op_result(op).err) {
    state = SP_PROMPT_STATE_ERROR;
  }
  spn_prompt_stop(tui, state);
  tui->prompt.op = SP_NULLPTR;
}

void spn_tui_handoff(spn_tui_t* tui) {
  spn_prompt_stop(tui, SP_PROMPT_STATE_SUBMIT);
  spn_tui_flush(tui);
  tui->handoff.granted = true;
}
