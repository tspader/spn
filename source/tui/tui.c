#include "spn/host.h"

#include "sp/color.h"
#include "sp/io.h"
#include "sp/macro.h"
#include "sp/prompt.h"
#include "sp/str.h"

#include "ctx/types.h"

#include "enum/enum.h"
#include "event/event.h"
#include "event/log.h"
#include "semver/convert.h"
#include "toml/issue.h"
#include "toolchain/select.h"
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

spn_tui_t tui;

static sp_str_t name_to_color(sp_mem_t mem, sp_str_t str);
static sp_str_t decorate_name(sp_mem_t mem, sp_str_t name, u32 padded_len, c8 pad);
static void     flush_writer(spn_tui_line_writer_t* writer);

spn_tui_mode_t spn_output_mode_from_str(sp_str_t str) {
  if (sp_str_equal_cstr(str, "interactive")) {
    return SPN_OUTPUT_MODE_INTERACTIVE;
  } else if (sp_str_equal_cstr(str, "noninteractive")) {
    return SPN_OUTPUT_MODE_NONINTERACTIVE;
  } else if (sp_str_equal_cstr(str, "quiet")) {
    return SPN_OUTPUT_MODE_QUIET;
  } else if (sp_str_equal_cstr(str, "none")) {
    return SPN_OUTPUT_MODE_NONE;
  } else if (sp_str_equal_cstr(str, "json")) {
    return SPN_OUTPUT_MODE_JSON;
  }

  sp_fatal("Unknown output mode {.yellow}; options are [interactive, noninteractive, quiet, none, json]", sp_fmt_str(str));
  sp_unreachable_return(SPN_OUTPUT_MODE_NONE);
}

static sp_str_t decorate_name(sp_mem_t mem, sp_str_t name, u32 padded_len, c8 pad) {
  sp_io_dyn_mem_writer_t w = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &w);
  sp_io_write_str(&w.base, name_to_color(mem, name), SP_NULLPTR);
  sp_io_write_str(&w.base, sp_str_lit("\u2590 "), SP_NULLPTR);
  sp_io_write_str(&w.base, name, SP_NULLPTR);
  sp_io_write_str(&w.base, sp_str_lit(SP_ANSI_RESET), SP_NULLPTR);

  if (padded_len > name.len) {
    sp_io_write_str(&w.base, sp_str_repeat(mem, pad, padded_len - name.len), SP_NULLPTR);
  }

  return sp_io_dyn_mem_writer_take_str(&w);
}

static sp_str_t name_to_color(sp_mem_t mem, sp_str_t str) {
  if (sp_str_equal_cstr(str, "package")) {
    return sp_str_lit(SP_ANSI_FG_WHITE);
  }

  static sp_ht(u32, sp_hash_t) buckets = SP_NULLPTR;
  if (!buckets) {
    sp_ht_init(sp_mem_os_new(), buckets);
  }

  sp_hash_t hash = sp_hash_str(str);
  u32 lo = (u32)hash;

  static const f32 bucket_hues[] = {
    30, 40, 50, 60,
    160, 180,
    200, 220, 240,
    250, 280, 310, 340
  };

  u32 original_bucket = lo % sp_carr_len(bucket_hues);
  u32 bucket = original_bucket;
  while (sp_ht_key_exists(buckets, bucket)) {
    sp_hash_t* claimed = sp_ht_getp(buckets, bucket);
    if (*claimed == hash) {
      break;
    }
    bucket = (bucket + 1) % sp_carr_len(bucket_hues);
    if (bucket == original_bucket) {
      break;
    }
  }
  sp_ht_insert(buckets, bucket, hash);

  sp_color_t hsv = {
    .h = bucket_hues[bucket],
    .s = 40.0f,
    .v = 75.f,
  };
  sp_color_t rgb = sp_color_hsv_to_rgb(hsv);
  u8 r = (u8)(rgb.r * 255.0f);
  u8 g = (u8)(rgb.g * 255.0f);
  u8 b = (u8)(rgb.b * 255.0f);

  return sp_color_to_tui_rgb_f(mem, r, g, b);
}

static sp_str_t get_colored_name(sp_mem_t mem, sp_str_t name) {
  return sp_fmt(mem, "{}{}" SP_ANSI_RESET, sp_fmt_str(name_to_color(mem, name)), sp_fmt_str(name)).value;
}

static sp_str_t get_contextual_path(sp_mem_t mem, sp_str_t path) {
  sp_str_t cache = spn_ctx_cache_dir(&spn);
  if (!sp_str_empty(cache) && sp_str_starts_with(path, cache)) {
    sp_str_t rel = sp_str_strip_left(path, cache);
    rel = sp_str_strip_left(rel, sp_str_lit("/"));
    return sp_fmt(mem, "$SPN_CACHE/{}", sp_fmt_str(rel)).value;
  }

  sp_str_t project = spn_ctx_project_dir(&spn);
  if (!sp_str_empty(project) && sp_str_starts_with(path, project)) {
    sp_str_t rel = sp_str_strip_left(path, project);
    rel = sp_str_strip_left(rel, sp_str_lit("/"));
    rel = sp_str_strip_left(rel, sp_str_lit("./"));
    return sp_fmt(mem, "./{}", sp_fmt_str(rel)).value;
  }

  return path;
}

sp_str_t spn_tui_render_event_detail(sp_mem_t mem, spn_build_event_t* event) {
  sp_io_dyn_mem_writer_t w = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &w);

  switch (event->kind) {
    case SPN_EVENT_SYNC:
    case SPN_EVENT_SYNC_PATCH: {
      sp_fmt_io(&w.base, "{} {.gray}", sp_fmt_str(event->sync.name), sp_fmt_str(event->sync.url));
      break;
    }
    case SPN_EVENT_SYNC_START: {
      sp_fmt_io(&w.base, "packages={} index={} file={}",
        sp_fmt_uint(event->sync_start.num_packages),
        sp_fmt_uint(event->sync_start.num_index),
        sp_fmt_uint(event->sync_start.num_file)
      );
      break;
    }
    case SPN_EVENT_SYNC_PACKAGE: {
      sp_fmt_io(&w.base, "{} fetched={} source={.gray}",
        sp_fmt_str(event->sync_pkg.name),
        sp_fmt_cstr(event->sync_pkg.fetched ? "true" : "false"),
        sp_fmt_str(event->sync_pkg.source_path)
      );
      break;
    }
    case SPN_EVENT_SYNC_END: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->sync_end.time);
      sp_fmt_io(&w.base, "synced {} packages in {.cyan}",
        sp_fmt_uint(event->sync_end.num_synced),
        sp_fmt_cstr(buffer)
      );
      break;
    }
    case SPN_EVENT_PUBLISH: {
      sp_fmt_io(&w.base, "{} v{}",
        sp_fmt_str(event->publish.name),
        sp_fmt_str(event->publish.version)
      );
      break;
    }
    case SPN_EVENT_PUBLISH_END: {
      sp_fmt_io(&w.base, "{} v{} to {} {.gray}",
        sp_fmt_str(event->publish.name),
        sp_fmt_str(event->publish.version),
        sp_fmt_str(event->publish.index),
        sp_fmt_str(event->publish.url)
      );
      break;
    }
    case SPN_EVENT_COMPILE_START: {
      if (event->pkg) {
        sp_fmt_io(&w.base, "v{}", sp_fmt_str(spn_semver_to_str(mem, event->pkg->version)));
      }
      break;
    }
    case SPN_EVENT_BUILD_SCRIPT_COMPILE: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->script_compile.time);
      sp_fmt_io(&w.base, "{.gray} in {.gray}",
        sp_fmt_str(event->script_compile.script_path),
        sp_fmt_cstr(buffer)
      );
      break;
    }
    case SPN_EVENT_BUILD_SCRIPT_USER_FN: {
      sp_fmt_io(&w.base, "{}", sp_fmt_str(event->node.info->tag));
      break;
    }
    case SPN_EVENT_BUILD_SCRIPT_PACKAGE_OK: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->package_ok.time);
      sp_fmt_io(&w.base, "in {.gray}", sp_fmt_cstr(buffer));
      break;
    }
    case SPN_EVENT_TARGET_BUILD_PASSED: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->target.passed.time);
      sp_fmt_io(&w.base, "{.gray} in {.gray}",
        sp_fmt_str(event->target.passed.source_file),
        sp_fmt_cstr(buffer)
      );
      break;
    }
    case SPN_EVENT_LINK_PASSED: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->target.link_passed.time);
      sp_fmt_io(&w.base, "{.gray} in {.gray}",
        sp_fmt_str(event->target.link_passed.output_path),
        sp_fmt_cstr(buffer)
      );
      break;
    }
    case SPN_EVENT_EMBED_START: {
      sp_fmt_io(&w.base, "{} files", sp_fmt_uint(event->embed_start.num_files));
      break;
    }
    case SPN_EVENT_EMBED_PASSED: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->embed_passed.time);
      sp_fmt_io(&w.base, "{.gray} in {.gray}",
        sp_fmt_str(event->embed_passed.object_path),
        sp_fmt_cstr(buffer)
      );
      break;
    }
    case SPN_EVENT_BUILD_SUMMARY: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->build_summary.time);
      sp_fmt_io(&w.base, "{} executed, {} cached in {.gray}",
        sp_fmt_uint(event->build_summary.misses),
        sp_fmt_uint(event->build_summary.hits),
        sp_fmt_cstr(buffer)
      );
      break;
    }
    case SPN_EVENT_RESOLVE_PACKAGE: {
      sp_fmt_io(&w.base, "{} {.gray}",
        sp_fmt_str(event->resolve_pkg.name),
        sp_fmt_str(event->resolve_pkg.version)
      );
      break;
    }
    case SPN_EVENT_RESOLVE_END: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->resolve_end.time);
      sp_fmt_io(&w.base, "Resolved {} packages in {.gray}",
        sp_fmt_uint(event->resolve_end.num_resolved),
        sp_fmt_cstr(buffer)
      );
      break;
    }
    case SPN_EVENT_ADDED: {
      sp_fmt_io(&w.base, "{.cyan}=={.green}",
        sp_fmt_str(event->added.name),
        sp_fmt_str(event->added.version)
      );
      break;
    }
    case SPN_EVENT_UPDATE_INCOMPATIBLE: {
      sp_fmt_io(&w.base, "{.cyan} {.green} (latest {.yellow} is semver incompatible)",
        sp_fmt_str(event->update.name),
        sp_fmt_str(event->update.version),
        sp_fmt_str(event->update.latest)
      );
      break;
    }
    case SPN_EVENT_TARGET_RUN: {
      sp_fmt_io(&w.base, "{.gray}", sp_fmt_str(get_contextual_path(mem, event->run.command)));
      break;
    }
    case SPN_EVENT_TEST_PASSED: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->test_passed.time);
      sp_fmt_io(&w.base, "in {.gray}", sp_fmt_cstr(buffer));
      break;
    }
    case SPN_EVENT_TEST_FAILED: {
      sp_fmt_io(&w.base, "{.yellow} failed with exit code {}",
        sp_fmt_str(event->test_failed.name),
        sp_fmt_int(event->test_failed.code)
      );
      break;
    }
    case SPN_EVENT_TEST_SUMMARY: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->test_summary.time);
      if (event->test_summary.failed) {
        sp_fmt_io(&w.base, "{} passed, {.red} failed in {.gray}",
          sp_fmt_uint(event->test_summary.passed),
          sp_fmt_uint(event->test_summary.failed),
          sp_fmt_cstr(buffer)
        );
      }
      else {
        sp_fmt_io(&w.base, "{} passed, {} failed in {.gray}",
          sp_fmt_uint(event->test_summary.passed),
          sp_fmt_uint(event->test_summary.failed),
          sp_fmt_cstr(buffer)
        );
      }
      break;
    }
    case SPN_EVENT_BUILD_PASSED: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->build.passed.time);
      sp_fmt_io(&w.base,
        "Compiled for profile {.cyan} in {.gray} {.gray}",
        sp_fmt_str(event->build.passed.profile->name),
        sp_fmt_cstr(buffer),
        sp_fmt_str(sp_fmt(mem, "({} executed, {} cached)",
          sp_fmt_uint(event->build.passed.misses),
          sp_fmt_uint(event->build.passed.hits)).value)
      );
      break;
    }
    case SPN_EVENT_BUILD_SCRIPT_CRASHED: {
      if (sp_str_empty(event->crashed.error)) {
        sp_io_write_str(&w.base, sp_str_lit("build script crashed"), SP_NULLPTR);
      }
      else {
        sp_fmt_io(&w.base, "build script crashed: {.red}", sp_fmt_str(event->crashed.error));
      }
      break;
    }
    case SPN_EVENT_BUILD_SCRIPT_COMPILE_FAILED: {
      sp_fmt_io(&w.base, "{.cyan} failed to compile", sp_fmt_str(get_contextual_path(mem, event->compile_failed.script_path)));
      break;
    }
    case SPN_EVENT_TARGET_BUILD_FAILED: {
      sp_fmt_io(&w.base, "{.cyan} failed to compile", sp_fmt_str(get_contextual_path(mem, event->target.failed.source_file)));
      break;
    }
    case SPN_EVENT_NODE_FAILED: {
      if (sp_str_empty(event->node_failed.path)) {
        sp_io_write_str(&w.base, event->node_failed.message, SP_NULLPTR);
      }
      else {
        sp_fmt_io(&w.base, "{.cyan} {}",
          sp_fmt_str(get_contextual_path(mem, event->node_failed.path)),
          sp_fmt_str(event->node_failed.message)
        );
      }
      break;
    }
    case SPN_EVENT_ERR_OPTION: {
      switch (event->option.err) {
        case SPN_OPTION_ERR_UNDECLARED: {
          sp_fmt_io(
            &w.base,
            "{} does not declare an option named {.yellow} (set by {.cyan})",
            sp_fmt_str(get_colored_name(mem, event->option.pkg)),
            sp_fmt_str(event->option.option),
            sp_fmt_str(spn_option_setter_to_str(event->option.a))
          );
          break;
        }
        case SPN_OPTION_ERR_BAD_VALUE: {
          sp_fmt_io(
            &w.base,
            "{.yellow} is not a valid value for {}.{.cyan} (set by {.cyan})",
            sp_fmt_str(event->option.value),
            sp_fmt_str(get_colored_name(mem, event->option.pkg)),
            sp_fmt_str(event->option.option),
            sp_fmt_str(spn_option_setter_to_str(event->option.a))
          );
          break;
        }
        case SPN_OPTION_ERR_CONFLICT: {
          sp_fmt_io(
            &w.base,
            "option conflict on {}.{.cyan}: {.cyan} and {.cyan} request different values",
            sp_fmt_str(get_colored_name(mem, event->option.pkg)),
            sp_fmt_str(event->option.option),
            sp_fmt_str(spn_option_setter_to_str(event->option.a)),
            sp_fmt_str(spn_option_setter_to_str(event->option.b))
          );
          break;
        }
        case SPN_OPTION_ERR_VETO: {
          sp_fmt_io(
            &w.base,
            "{.cyan} requires {}.{.cyan} != {.yellow}, but {} set it",
            sp_fmt_str(spn_option_setter_to_str(event->option.a)),
            sp_fmt_str(get_colored_name(mem, event->option.pkg)),
            sp_fmt_str(event->option.option),
            sp_fmt_str(event->option.value),
            sp_fmt_str(spn_option_setter_to_str(event->option.b))
          );
          break;
        }
        case SPN_OPTION_ERR_NO_VALUE: {
          sp_fmt_io(
            &w.base,
            "no value for {}.{.cyan}: no default matched and nothing set it",
            sp_fmt_str(get_colored_name(mem, event->option.pkg)),
            sp_fmt_str(event->option.option)
          );
          break;
        }
        case SPN_OPTION_ERR_LATE_GATE: {
          sp_fmt_io(
            &w.base,
            "the dependency gate on {}'s edge to {.cyan} never settled",
            sp_fmt_str(get_colored_name(mem, event->option.pkg)),
            sp_fmt_str(spn_option_setter_to_str(event->option.a))
          );
          break;
        }
        case SPN_OPTION_ERR_UNKNOWN_PKG: {
          sp_fmt_io(
            &w.base,
            "the root manifest configures {.yellow}, which is not a package in this build",
            sp_fmt_str(event->option.pkg)
          );
          break;
        }
      }
      break;
    }
    case SPN_EVENT_SYNC_FAILED: {
      sp_fmt_io(
        &w.base,
        "{} failed to sync from {.gray}: {}",
        sp_fmt_str(get_colored_name(mem, event->sync_failed.name)),
        sp_fmt_str(event->sync_failed.url),
        sp_fmt_str(event->sync_failed.error)
      );
      break;
    }
    case SPN_EVENT_SYNC_STALE: {
      sp_fmt_io(
        &w.base,
        "{} could not be fetched from {.gray}; using the cached copy",
        sp_fmt_str(get_colored_name(mem, event->sync.name)),
        sp_fmt_str(event->sync.url)
      );
      break;
    }
    case SPN_EVENT_ERR_PATCH: {
      switch (event->patch_err.kind) {
        case SPN_PATCH_ERR_UNUSED: {
          sp_fmt_io(
            &w.base,
            "[patch.{.cyan}] does not name a package in this build",
            sp_fmt_str(event->patch_err.name)
          );
          break;
        }
        case SPN_PATCH_ERR_NOT_GIT: {
          sp_fmt_io(
            &w.base,
            "[patch.{.cyan}] names a package whose sources are not fetched from git; only git sources can be patched",
            sp_fmt_str(event->patch_err.name)
          );
          break;
        }
      }
      break;
    }
    case SPN_EVENT_ERR: {
      switch (event->err.kind) {
        case SPN_ERR_MANIFEST_PARSE: {
          sp_fmt_io(
            &w.base,
            "failed to parse manifest {.cyan}",
            sp_fmt_str(get_contextual_path(mem, event->err.manifest_parse.path))
          );
          break;
        }
        case SPN_ERR_MANIFEST_ISSUES: {
          if (sp_str_empty(event->err.manifest.name)) {
            sp_fmt_io(
              &w.base,
              "invalid manifest ({.gray})",
              sp_fmt_str(get_contextual_path(mem, event->err.manifest.path))
            );
          }
          else {
            sp_fmt_io(
              &w.base,
              "{} has an invalid manifest ({.gray})",
              sp_fmt_str(get_colored_name(mem, event->err.manifest.name)),
              sp_fmt_str(get_contextual_path(mem, event->err.manifest.path))
            );
          }
          break;
        }
        case SPN_ERR_PKG_MISMATCH: {
          sp_fmt_io(
            &w.base,
            "the manifest at {.cyan} declares {}, but it was requested as {}",
            sp_fmt_str(get_contextual_path(mem, event->err.mismatch.path)),
            sp_fmt_str(get_colored_name(mem, event->err.mismatch.declared)),
            sp_fmt_str(get_colored_name(mem, event->err.mismatch.requested))
          );
          break;
        }
        case SPN_ERR_PKG_UNKNOWN: {
          sp_fmt_io(
            &w.base,
            "{} could not be located",
            sp_fmt_str(get_colored_name(mem, event->err.unknown.request.qualified))
          );
          break;
        }
        case SPN_ERR_PKG_NO_MATCH: {
          spn_err_unsatisfiable_t* err = &event->err.unsatisfiable;
          sp_str_t requester = sp_str_empty(err->requester) ?
            sp_str_lit("the project") :
            sp_fmt(mem, "{} {}", sp_fmt_str(get_colored_name(mem, err->requester)), sp_fmt_str(spn_semver_to_str(mem, err->requester_version))).value;

          if (err->conflict && err->request.source == SPN_PKG_SOURCE_INDEX) {
            sp_fmt_io(
              &w.base,
              "{} is already selected at {.yellow}, but {} requires {.yellow}",
              sp_fmt_str(get_colored_name(mem, err->request.qualified)),
              sp_fmt_str(spn_semver_to_str(mem, err->selected)),
              sp_fmt_str(requester),
              sp_fmt_str(spn_semver_range_to_str(mem, err->request.index.range))
            );
          }
          else if (err->conflict) {
            sp_fmt_io(
              &w.base,
              "{} is already selected at {.yellow}, which conflicts with the version required by {}",
              sp_fmt_str(get_colored_name(mem, err->request.qualified)),
              sp_fmt_str(spn_semver_to_str(mem, err->selected)),
              sp_fmt_str(requester)
            );
          }
          else {
            sp_fmt_io(
              &w.base,
              "no version of {} satisfies {.yellow}, required by {}",
              sp_fmt_str(get_colored_name(mem, err->request.qualified)),
              sp_fmt_str(spn_semver_range_to_str(mem, err->request.index.range)),
              sp_fmt_str(requester)
            );
          }
          break;
        }
        case SPN_ERR_DEP_CYCLE: {
          sp_fmt_io(
            &w.base,
            "{} transitively includes itself",
            sp_fmt_str(get_colored_name(mem, event->err.circular.id.name))
          );
          break;
        }
        case SPN_ERR_UNIT_CYCLE: {
          sp_fmt_io(
            &w.base,
            "{} {.yellow} can't build: its build depends on a tool that links this same instance",
            sp_fmt_str(get_colored_name(mem, event->err.unit_cycle.id.name)),
            sp_fmt_str(spn_semver_to_str(mem, event->err.unit_cycle.version))
          );
          break;
        }
        case SPN_ERR_DYNAMIC_DUPLICATE: {
          sp_fmt_io(
            &w.base,
            "{} {.yellow} and {.yellow} would both load into one process as shared libraries",
            sp_fmt_str(get_colored_name(mem, event->err.dynamic_dup.id.name)),
            sp_fmt_str(spn_semver_to_str(mem, event->err.dynamic_dup.low)),
            sp_fmt_str(spn_semver_to_str(mem, event->err.dynamic_dup.high))
          );
          break;
        }
        case SPN_ERR_RESOLVE_TOO_COMPLEX: {
          sp_fmt_io(
            &w.base,
            "resolving {} is too complex; pin a version to reduce the search",
            sp_fmt_str(get_colored_name(mem, event->err.too_complex.id.name))
          );
          break;
        }
        case SPN_ERR_PROFILE_INVALID: {
          sp_fmt_io(
            &w.base,
            "invalid profile {.cyan}",
            sp_fmt_str(event->err.profile.name)
          );
          break;
        }
        case SPN_ERR_PROFILE_UNDEFINED: {
          sp_fmt_io(
            &w.base,
            "profile {.cyan} isn't defined",
            sp_fmt_str(event->err.profile.name)
          );
          break;
        }
        case SPN_ERR_SANITIZER_UNSUPPORTED: {
          sp_fmt_io(
            &w.base,
            "toolchain {.cyan} targeting {.yellow} doesn't support {.red}",
            sp_fmt_str(event->err.sanitizer.toolchain),
            sp_fmt_str(spn_triple_to_str(mem, event->err.sanitizer.target)),
            sp_fmt_str(spn_sanitizer_set_to_str(mem, event->err.sanitizer.unsupported))
          );
          break;
        }
        case SPN_ERR_SANITIZER_STATIC: {
          sp_fmt_io(
            &w.base,
            "{.red} requires a dynamically linked executable; set {.cyan} in the profile",
            sp_fmt_str(spn_sanitizer_set_to_str(mem, event->err.sanitizer.unsupported)),
            sp_fmt_str(sp_str_lit("linkage = \"shared\""))
          );
          break;
        }
        case SPN_ERR_COMPILER_FEATURE_UNSUPPORTED: {
          const c8* feature = "";
          switch (event->err.compiler.feature) {
            case SPN_CC_FEATURE_COMPILE: feature = "direct compilation"; break;
            case SPN_CC_FEATURE_LINK_EXE: feature = "executable linking"; break;
            case SPN_CC_FEATURE_LINK_SHARED: feature = "shared library linking"; break;
            case SPN_CC_FEATURE_LINK_REACTOR: feature = "reactor module linking"; break;
            case SPN_CC_FEATURE_ARCHIVE: feature = "static archiving"; break;
            case SPN_CC_FEATURE_FRAMEWORKS: feature = "framework linking without a macOS SDK"; break;
          }

          sp_fmt_io(
            &w.base,
            "toolchain {.cyan} targeting {.yellow} doesn't support {.red}",
            sp_fmt_str(event->err.compiler.toolchain),
            sp_fmt_str(spn_triple_to_str(mem, event->err.compiler.target)),
            sp_fmt_cstr(feature)
          );
          break;
        }
        case SPN_ERR_FS_REMOVE: {
          sp_fmt_io(
            &w.base,
            "failed to remove {.cyan}",
            sp_fmt_str(get_contextual_path(mem, event->err.fs.path))
          );
          break;
        }
        case SPN_ERR_WASM_INIT_FAILED: {
          sp_io_write_str(&w.base, sp_str_lit("failed to initialize the wasm runtime"), SP_NULLPTR);
          break;
        }
        case SPN_ERR_CONFIGURE_SOURCE: {
          if (sp_fs_is_glob(event->err.configure_source.source)) {
            sp_fmt_io(
              &w.base,
              "{.cyan} declares a configure source {.yellow}, which matched no files",
              sp_fmt_str(event->err.configure_source.name),
              sp_fmt_str(event->err.configure_source.source)
            );
          }
          else {
            sp_fmt_io(
              &w.base,
              "{.cyan} declares a configure source {.yellow}, which is not a file",
              sp_fmt_str(event->err.configure_source.name),
              sp_fmt_str(event->err.configure_source.source)
            );
          }
          break;
        }
        case SPN_ERR_BUILD_GRAPH: {
          sp_fmt_io(
            &w.base,
            "failed to construct the build graph at {.cyan}",
            sp_fmt_str(get_contextual_path(mem, event->err.build_graph.file))
          );
          break;
        }
        case SPN_ERR_FS_READ: {
          sp_fmt_io(
            &w.base,
            "failed to read {.cyan}",
            sp_fmt_str(get_contextual_path(mem, event->err.fs.path))
          );
          break;
        }
        case SPN_ERR_FS_CREATE_DIR: {
          sp_fmt_io(
            &w.base,
            "failed to create directory {.cyan}",
            sp_fmt_str(get_contextual_path(mem, event->err.fs.path))
          );
          break;
        }
        case SPN_ERR_FS_WRITE: {
          sp_fmt_io(
            &w.base,
            "failed to write {.cyan}",
            sp_fmt_str(get_contextual_path(mem, event->err.fs.path))
          );
          break;
        }
        case SPN_ERR_INDEX_UNKNOWN: {
          sp_fmt_io(
            &w.base,
            "index {.cyan} not found",
            sp_fmt_str(event->err.index.name)
          );
          break;
        }
        case SPN_ERR_INDEX_SYNC: {
          sp_fmt_io(
            &w.base,
            "failed to sync index {.cyan} from {.gray}",
            sp_fmt_str(event->err.index.name),
            sp_fmt_str(event->err.index.url)
          );
          break;
        }
        case SPN_ERR_MANIFEST_EDIT: {
          sp_fmt_io(
            &w.base,
            "failed to edit {.cyan}",
            sp_fmt_str(get_contextual_path(mem, event->err.manifest_parse.path))
          );
          break;
        }
        case SPN_ERR_NO_MANIFEST: {
          sp_fmt_io(
            &w.base,
            "no manifest found at {.cyan}",
            sp_fmt_str(get_contextual_path(mem, event->err.no_manifest.path))
          );
          break;
        }
        case SPN_ERR_NOT_GIT_REPO: {
          sp_fmt_io(
            &w.base,
            "{.cyan} is not inside a git repository",
            sp_fmt_str(get_contextual_path(mem, event->err.not_git_repo.path))
          );
          break;
        }
        case SPN_ERR_GIT: {
          sp_fmt_io(
            &w.base,
            "git command failed: {.yellow}",
            sp_fmt_str(event->err.git.command)
          );
          break;
        }
        case SPN_ERR_VERSION_EXISTS: {
          sp_fmt_io(
            &w.base,
            "version {.yellow} of {.cyan} already exists in the index",
            sp_fmt_str(event->err.version_exists.version),
            sp_fmt_str(event->err.version_exists.name)
          );
          break;
        }
        case SPN_ERR_VERSION_INVALID: {
          sp_fmt_io(
            &w.base,
            "invalid version {.red}",
            sp_fmt_str(event->err.version_invalid.requested)
          );
          break;
        }
        case SPN_ERR_INDEX_PINNED: {
          sp_fmt_io(
            &w.base,
            "index {.cyan} is pinned to a revision and cannot be published to",
            sp_fmt_str(event->err.index.name)
          );
          break;
        }
        case SPN_ERR_INDEX_PUBLISH_PROTOCOL: {
          sp_fmt_io(
            &w.base,
            "index {.cyan} does not support publishing",
            sp_fmt_str(event->err.index.name)
          );
          break;
        }
        case SPN_ERR_INDEX_CORRUPT: {
          sp_fmt_io(
            &w.base,
            "index entry for {.cyan} at {.cyan} is corrupt",
            sp_fmt_str(event->err.index_corrupt.name),
            sp_fmt_str(get_contextual_path(mem, event->err.index_corrupt.path))
          );
          break;
        }
        case SPN_ERR_PUBLISH_PUSH: {
          sp_fmt_io(
            &w.base,
            "failed to push to {.gray}\n{}",
            sp_fmt_str(event->err.publish.url),
            sp_fmt_str(event->err.publish.output)
          );
          break;
        }
        case SPN_ERR_PUBLISH_DIRTY: {
          sp_fmt_io(
            &w.base,
            "{.cyan} has uncommitted changes; commit them or pass {.yellow}",
            sp_fmt_str(get_contextual_path(mem, event->err.publish.path)),
            sp_fmt_cstr("--allow-dirty")
          );
          break;
        }
        case SPN_ERR_PUBLISH_UNPUSHED: {
          sp_fmt_io(
            &w.base,
            "commit {.yellow} is not on any branch of {.gray}; push it first",
            sp_fmt_str(event->err.publish.rev),
            sp_fmt_str(event->err.publish.url)
          );
          break;
        }
        case SPN_ERR_INDEX_PATH_DEP: {
          sp_fmt_io(
            &w.base,
            "{.cyan} depends on {.cyan} by path; index packages must depend on versions",
            sp_fmt_str(event->err.pkg.name),
            sp_fmt_str(event->err.pkg.requested)
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_FETCH: {
          sp_fmt_io(
            &w.base,
            "toolchain {} failed to download from {.gray}",
            sp_fmt_str(get_colored_name(mem, event->err.artifact.name)),
            sp_fmt_str(event->err.artifact.url)
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_NO_SHA: {
          sp_fmt_io(
            &w.base,
            "toolchain {} has no sha256 for {.gray}",
            sp_fmt_str(get_colored_name(mem, event->err.artifact.name)),
            sp_fmt_str(event->err.artifact.url)
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_SHA: {
          sp_fmt_io(
            &w.base,
            "toolchain {} sha256 mismatch for {.gray}: expected {.yellow}, got {.red}",
            sp_fmt_str(get_colored_name(mem, event->err.artifact.name)),
            sp_fmt_str(event->err.artifact.url),
            sp_fmt_str(event->err.artifact.expected),
            sp_fmt_str(event->err.artifact.actual)
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_EXTRACT: {
          sp_fmt_io(
            &w.base,
            "toolchain {} failed to extract archive from {.gray}",
            sp_fmt_str(get_colored_name(mem, event->err.artifact.name)),
            sp_fmt_str(event->err.artifact.url)
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_UNKNOWN: {
          sp_fmt_io(
            &w.base,
            "toolchain {} isn't defined",
            sp_fmt_str(get_colored_name(mem, event->err.toolchain.name))
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_TARGET: {
          sp_str_t target = spn_triple_to_str(mem, event->err.toolchain.target);
          switch (event->err.toolchain.role) {
            case SPN_TOOLCHAIN_ROLE_BUILD: {
              sp_fmt_io(
                &w.base,
                "toolchain {} can't target {.yellow}",
                sp_fmt_str(get_colored_name(mem, event->err.toolchain.name)),
                sp_fmt_str(target)
              );
              break;
            }
            case SPN_TOOLCHAIN_ROLE_SCRIPT: {
              sp_fmt_io(
                &w.base,
                "build scripts compile to {.yellow}, but toolchain {} can't target it",
                sp_fmt_str(target),
                sp_fmt_str(get_colored_name(mem, event->err.toolchain.name))
              );
              break;
            }
          }
          break;
        }
        case SPN_ERR_TOOLCHAIN_NONE: {
          sp_str_t target = spn_triple_to_str(mem, event->err.toolchain.target);
          sp_fmt_io(
            &w.base,
            "no toolchain in the catalog can target {.yellow}",
            sp_fmt_str(target)
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_HOST: {
          sp_str_t host = spn_triple_to_str(mem, event->err.toolchain.host);
          sp_fmt_io(
            &w.base,
            "toolchain {} isn't distributed for host {.yellow}",
            sp_fmt_str(get_colored_name(mem, event->err.toolchain.name)),
            sp_fmt_str(host)
          );
          break;
        }
        case SPN_ERR_WASM_READ_FAILED: {
          sp_fmt_io(
            &w.base,
            "failed to read build script {.cyan}",
            sp_fmt_str(get_contextual_path(mem, event->err.wasm.path))
          );
          break;
        }
        case SPN_ERR_WASM_MODULE_LOAD_FAILED: {
          sp_fmt_io(
            &w.base,
            "failed to load build script {.cyan}: {.red}",
            sp_fmt_str(get_contextual_path(mem, event->err.wasm.path)),
            sp_fmt_str(event->err.wasm.error)
          );
          break;
        }
        case SPN_ERR_WASM_MODULE_INSTANCE_FAILED: {
          sp_fmt_io(
            &w.base,
            "failed to instantiate build script {.cyan}: {.red}",
            sp_fmt_str(get_contextual_path(mem, event->err.wasm.path)),
            sp_fmt_str(event->err.wasm.error)
          );
          break;
        }
        case SPN_ERR_WASM_THREAD_ENV_FAILED: {
          sp_fmt_io(
            &w.base,
            "failed to init wasm thread env for build script {.cyan}",
            sp_fmt_str(get_contextual_path(mem, event->err.wasm.path))
          );
          break;
        }
        case SPN_ERR_WASM_CTX_FAILED: {
          sp_fmt_io(
            &w.base,
            "failed to create wasm context for build script {.cyan}",
            sp_fmt_str(get_contextual_path(mem, event->err.wasm.path))
          );
          break;
        }
        case SPN_ERR_WASM_MODULE_CALL_FAILED: {
          sp_fmt_io(
            &w.base,
            "build script {.cyan} crashed: {.red}",
            sp_fmt_str(get_contextual_path(mem, event->err.wasm.path)),
            sp_fmt_str(event->err.wasm.error)
          );
          break;
        }
        case SPN_ERR_WASM_SCRIPT_ERROR: {
          sp_fmt_io(
            &w.base,
            "build script {.cyan} returned {.red}",
            sp_fmt_str(get_contextual_path(mem, event->err.wasm.path)),
            sp_fmt_int(event->err.wasm.rc)
          );
          break;
        }
        case SPN_ERR_WASM_NO_SCRIPT: {
          sp_io_write_str(&w.base, sp_str_lit("node has a wasm fn but no build script is loaded"), SP_NULLPTR);
          break;
        }
        case SPN_ERR_WASM_EXPORT_NOT_FOUND: {
          sp_fmt_io(
            &w.base,
            "Referenced symbol {.yellow} was not found in {.cyan}",
            sp_fmt_str(event->err.wasm.error),
            sp_fmt_str(get_contextual_path(mem, event->err.wasm.path))
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_NO_CXX: {
          sp_fmt_io(
            &w.base,
            "Toolchain {} has no C++ compiler, but the build contains C++ sources",
            sp_fmt_str(get_colored_name(mem, event->err.toolchain.name))
          );
          break;
        }
        case SPN_ERR_TARGET_LINKAGE: {
          sp_fmt_io(
            &w.base,
            "{.cyan} doesn't support {.yellow} ({} requested it)",
            sp_fmt_str(event->err.target.pkg),
            sp_fmt_str(event->err.target.requested),
            sp_fmt_str(event->err.target.requester)
          );
          break;
        }
        case SPN_ERR_TARGET_DUPLICATE: {
          sp_fmt_io(
            &w.base,
            "{.cyan} declares a target {.yellow}, which collides with another target of the same name",
            sp_fmt_str(event->err.target.pkg),
            sp_fmt_str(event->err.target.name)
          );
          break;
        }
        case SPN_ERR_TARGET_RESERVED: {
          sp_fmt_io(
            &w.base,
            "{.cyan} names an executable {.yellow}, which collides with a build output directory (store, work, test)",
            sp_fmt_str(event->err.target.pkg),
            sp_fmt_str(event->err.target.name)
          );
          break;
        }
        case SPN_ERR_TARGET_DEP: {
          sp_fmt_io(
            &w.base,
            "failed to find {.cyan} as a package or target",
            sp_fmt_str(event->err.target.name)
          );
          break;
        }
        case SPN_ERR_TARGET_SELECTION: {
          sp_fmt_io(
            &w.base,
            "target {.yellow} is not defined for the selected target kinds",
            sp_fmt_str(event->err.target.name)
          );
          break;
        }
        case SPN_ERR_INIT_EXISTS: {
          sp_fmt_io(
            &w.base,
            "{.cyan} already exists",
            sp_fmt_str(get_contextual_path(mem, event->err.fs.path))
          );
          break;
        }
        case SPN_ERR_INIT_NAME: {
          sp_fmt_io(
            &w.base,
            "invalid name {.quote}",
            sp_fmt_str(event->err.pkg.name)
          );
          break;
        }
        case SPN_ERR_SCRIPT_MISSING: {
          sp_fmt_io(
            &w.base,
            "script {.yellow} has no binary at {.cyan}",
            sp_fmt_str(event->err.script.name),
            sp_fmt_str(get_contextual_path(mem, event->err.script.path))
          );
          break;
        }
        case SPN_ERR_TEST_MISSING: {
          sp_fmt_io(
            &w.base,
            "test {.yellow} has no binary at {.cyan}",
            sp_fmt_str(event->err.script.name),
            sp_fmt_str(get_contextual_path(mem, event->err.script.path))
          );
          break;
        }
        case SPN_ERR_TEST_FAILED: {
          sp_io_write_str(&w.base, sp_str_lit("one or more tests failed"), SP_NULLPTR);
          break;
        }
        case SPN_ERR_SCRIPT_FAILED: {
          sp_fmt_io(
            &w.base,
            "script {.yellow} failed with exit code {}",
            sp_fmt_str(event->err.script.name),
            sp_fmt_int(event->err.script.code)
          );
          break;
        }
        case SPN_ERR_DAG_DUPLICATE_OUTPUT: {
          sp_io_write_str(&w.base, sp_str_lit("two build actions produce the same output file"), SP_NULLPTR);
          break;
        }
        case SPN_ERR_DAG_OUTPUT_NAME: {
          sp_io_write_str(&w.base, sp_str_lit("a build action output has no file name"), SP_NULLPTR);
          break;
        }
        case SPN_ERR_DAG_MISSING_INPUT: {
          sp_fmt_io(&w.base, "{.cyan} doesn't exist, but is listed as an input", sp_fmt_str(get_contextual_path(mem, event->err.dag.path)));
          break;
        }
        case SPN_ERR_DAG_MISSING_OUTPUT: {
          sp_fmt_io(&w.base, "{.cyan} was not produced by the action that declares it as an output", sp_fmt_str(get_contextual_path(mem, event->err.dag.path)));
          break;
        }
        case SPN_ERR_DAG_STORE_READ: {
          sp_fmt_io(&w.base, "{.cyan} could not be read from the content store", sp_fmt_str(get_contextual_path(mem, event->err.dag.path)));
          break;
        }
        case SPN_ERR_DAG_STORE_WRITE: {
          sp_fmt_io(&w.base, "{.cyan} could not be written to the content store", sp_fmt_str(get_contextual_path(mem, event->err.dag.path)));
          break;
        }
        case SPN_ERR_DAG_SCRATCH: {
          sp_io_write_str(&w.base, sp_str_lit("failed to create a scratch directory for the build"), SP_NULLPTR);
          break;
        }
        case SPN_ERR_DAG_STALLED: {
          sp_io_write_str(&w.base, sp_str_lit("the build graph stalled before completing"), SP_NULLPTR);
          break;
        }
        default: {
          sp_io_write_str(&w.base, sp_str_lit("unknown error"), SP_NULLPTR);
          break;
        }
      }
      break;
    }
    case SPN_EVENT_USER_LOG: {
      sp_io_write_str(&w.base, event->user_log.message, SP_NULLPTR);
      break;
    }
    case SPN_EVENT_API_CALL: {
      sp_fmt_io(&w.base, "{}({})",
        sp_fmt_str(event->api_call.fn),
        sp_fmt_str(event->api_call.args)
      );
      break;
    }
    case SPN_EVENT_INIT_BUILD_GRAPH: {
      sp_fmt_io(&w.base, "{}, {.yellow}, {.green}",
        sp_fmt_str(event->graph_init.profile),
        sp_fmt_str(event->graph_init.target),
        sp_fmt_str(event->graph_init.toolchain)
      );
      break;
    }
    case SPN_EVENT_PREPARE_BUILD_GRAPH_FAILED: {
      switch (event->err.build_graph.kind) {
        case SPN_BUILD_GRAPH_ERR_MISSING_INPUT: {
          sp_fmt_io(
            &w.base,
            "Missing build graph input {.cyan}",
            sp_fmt_str(get_contextual_path(mem, event->err.build_graph.file))
          );
          break;
        }
        case SPN_BUILD_GRAPH_ERR_DUPLICATE_OUTPUT: {
          sp_fmt_io(
            &w.base,
            "Two graph nodes output the same file {.cyan}",
            sp_fmt_str(get_contextual_path(mem, event->err.build_graph.file))
          );
          break;
        }
        case SPN_BUILD_GRAPH_ERR_UNKNOWN: {
          sp_io_write_str(&w.base, sp_str_lit("Failed to prepare build graph"), SP_NULLPTR);
          break;
        }
      }
      break;
    }
    case SPN_EVENT_LINK_START: {
      sp_fmt_io(&w.base, "Linking target {.cyan}",
        sp_fmt_str(event->target.name)
      );
      break;
    }
    case SPN_EVENT_LINK_FAILED: {
      sp_fmt_io(&w.base, "Failed to link target {.cyan}", sp_fmt_str(event->target.name));
      break;
    }
    case SPN_EVENT_BUILD_SCRIPT_CONFIGURE_OK: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->configure.time);
      sp_fmt_io(&w.base, "in {.gray}", sp_fmt_cstr(buffer));
      break;
    }
    case SPN_EVENT_EMBED_FAILED: {
      sp_fmt_io(&w.base, "{}: {}",
        sp_fmt_str(get_contextual_path(mem, event->embed_failed.path)),
        sp_fmt_str(event->embed_failed.error)
      );
      break;
    }
    case SPN_EVENT_BUILD_FAILED: {
      sp_fmt_io(&w.base, "profile {.cyan} failed with {} {}",
        sp_fmt_str(event->build_failed.profile),
        sp_fmt_uint(event->build_failed.num_errors),
        sp_fmt_cstr(event->build_failed.num_errors == 1 ? "error" : "errors")
      );
      if (!sp_str_empty(event->build_failed.first_error)) {
        sp_fmt_io(&w.base, " ({})", sp_fmt_str(event->build_failed.first_error));
      }
      break;
    }
    default: {
      break;
    }
  }

  return sp_io_dyn_mem_writer_take_str(&w);
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

static void write_event(sp_io_writer_t* w, sp_mem_t mem, sp_str_t verb, bool error, sp_str_t pkg, sp_str_t detail) {
  sp_fmt_io(w, "{:>12 .bold .$}", sp_fmt_style(error ? sp_fmt_style_red : sp_fmt_style_green), sp_fmt_str(verb));

  if (sp_str_empty(pkg)) {
    sp_fmt_io(w, " {.gray}", sp_fmt_cstr("▐"));
  } else {
    sp_fmt_io(w, " {}", sp_fmt_str(decorate_name(mem, pkg, 0, ' ')));
  }
  if (!sp_str_empty(detail)) {
    sp_fmt_io(w, " {}", sp_fmt_str(detail));
  }
  sp_io_write_c8(w, '\n');
}

static void write_error(sp_io_writer_t* w, sp_mem_t mem, sp_str_t verb, sp_str_t detail) {
  sp_str_t label = sp_fmt(mem, "{}:", sp_fmt_str(verb)).value;
  sp_fmt_io(w, "{.bold .red}", sp_fmt_str(label));
  if (!sp_str_empty(detail)) {
    sp_fmt_io(w, " {}", sp_fmt_str(detail));
  }
  sp_io_write_c8(w, '\n');
}

static sp_str_t event_subject(spn_build_event_t* event) {
  switch (event->kind) {
    case SPN_EVENT_SYNC_FAILED:               return event->sync_failed.name;
    case SPN_EVENT_ERR_PATCH:                 return event->patch_err.name;
    case SPN_EVENT_ERR: {
      switch (event->err.kind) {
        case SPN_ERR_MANIFEST_ISSUES:  return event->err.manifest.name;
        case SPN_ERR_PKG_UNKNOWN:      return event->err.unknown.request.qualified;
        case SPN_ERR_PKG_NO_MATCH:     return event->err.unsatisfiable.request.qualified;
        case SPN_ERR_DEP_CYCLE:        return event->err.circular.id.name;
        case SPN_ERR_UNIT_CYCLE:       return event->err.unit_cycle.id.name;
        case SPN_ERR_DYNAMIC_DUPLICATE: return event->err.dynamic_dup.id.name;
        case SPN_ERR_RESOLVE_TOO_COMPLEX: return event->err.too_complex.id.name;
        default:                       break;
      }
      return event->pkg ? event->pkg->name : sp_str_lit("");
    }
    default:                                  return event->pkg ? event->pkg->name : sp_str_lit("");
  }
}

static void render_event_extra(sp_io_writer_t* w, spn_build_event_t* event) {
  switch (event->kind) {
    case SPN_EVENT_TARGET_BUILD_FAILED: {
      sp_io_write_str(w, event->target.failed.out, SP_NULLPTR);
      sp_io_write_str(w, event->target.failed.err, SP_NULLPTR);
      break;
    }
    case SPN_EVENT_LINK_FAILED: {
      sp_io_write_str(w, event->target.link_failed.err, SP_NULLPTR);
      sp_io_write_str(w, event->target.link_failed.out, SP_NULLPTR);
      break;
    }
    case SPN_EVENT_TEST_FAILED: {
      sp_io_write_str(w, event->test_failed.out, SP_NULLPTR);
      sp_io_write_str(w, event->test_failed.err, SP_NULLPTR);
      break;
    }
    case SPN_EVENT_BUILD_SCRIPT_COMPILE_FAILED: {
      sp_io_write_str(w, event->compile_failed.error, SP_NULLPTR);
      break;
    }
    case SPN_EVENT_PREPARE_BUILD_GRAPH_FAILED: {
      if (event->err.build_graph.kind == SPN_BUILD_GRAPH_ERR_DUPLICATE_OUTPUT) {
        if (sp_str_valid(event->err.build_graph.command_a)) {
          sp_io_write_str(w, event->err.build_graph.command_a, SP_NULLPTR);
          sp_io_write_c8(w, '\n');
        }
        if (sp_str_valid(event->err.build_graph.command_b)) {
          sp_io_write_str(w, event->err.build_graph.command_b, SP_NULLPTR);
          sp_io_write_c8(w, '\n');
        }
      }
      break;
    }
    case SPN_EVENT_ERR: {
      switch (event->err.kind) {
        case SPN_ERR_MANIFEST_ISSUES: {
          sp_da_for(event->err.manifest.issues, it) {
            sp_io_write_str(w, sp_str_lit("  - "), SP_NULLPTR);
            spn_codegen_issue_write(w, &event->err.manifest.issues[it]);
            sp_io_write_c8(w, '\n');
          }
          break;
        }
        case SPN_ERR_TOOLCHAIN_TARGET: {
          bool first = true;
          sp_om_for(event->err.toolchain.catalog->entries, it) {
            spn_toolchain_info_t* toolchain = sp_om_at(event->err.toolchain.catalog->entries, it);
            if (!spn_toolchain_supports(toolchain, event->err.toolchain.target, event->err.toolchain.host)) {
              continue;
            }
            sp_io_write_str(w, first ? sp_str_lit("toolchains that can: ") : sp_str_lit(", "), SP_NULLPTR);
            sp_fmt_io(w, "{.green}", sp_fmt_str(toolchain->name));
            first = false;
          }
          if (!first) {
            sp_io_write_c8(w, '\n');
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

void spn_tui_log_event(spn_tui_t* tui, spn_build_event_t* event) {
  if (tui->mode == SPN_OUTPUT_MODE_JSON) {
    spn_event_log_jsonl(&tui->logger.out.base, event);
    return;
  }

  const spn_event_info_t* display = &spn_event_info[event->kind];
  if (display->verbosity > tui->logger.verbosity) {
    if (event->kind == SPN_EVENT_USER_LOG) {
      sp_da_push(tui->buffered_logs, ((spn_tui_buffered_log_t) {
        .pkg = event->pkg ? sp_str_copy(tui->mem, event->pkg->name) : sp_str_lit(""),
        .message = sp_str_copy(tui->mem, event->user_log.message),
      }));
    }
    return;
  }

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_mem_t mem = scratch.mem;
  sp_io_writer_t* io = &tui->writer.base;
  sp_str_t verb = sp_cstr_as_str(display->display);

  if (display->error) {
    sp_da_for(tui->buffered_logs, it) {
      spn_tui_buffered_log_t* log = &tui->buffered_logs[it];
      write_event(io, mem, sp_str_lit(""), false, log->pkg, log->message);
    }
    sp_da_clear(tui->buffered_logs);

    sp_str_t detail = spn_tui_render_event_detail(mem, event);
    if (display->terminal) {
      write_event(io, mem, sp_str_lit("Failed"), true, event_subject(event), sp_str_lit(""));
      sp_io_write_c8(io, '\n');
      write_error(io, mem, verb, detail);
    } else {
      sp_str_t name = event->pkg ? event->pkg->name : sp_str_lit("");
      write_event(io, mem, verb, true, name, detail);
    }
    render_event_extra(io, event);
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
          io, mem, verb, false,
          get_short_name(event->sync.name),
          sp_fmt(mem, "{.gray}", sp_fmt_str(event->sync.url)).value
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
        io, mem, verb, false,
        sp_str_lit(""),
        sp_fmt(mem, "{} {} in {.gray}",
          sp_fmt_uint(tui->num_downloads),
          sp_fmt_cstr(tui->num_downloads == 1 ? "package" : "packages"),
          sp_fmt_cstr(buffer)
        ).value
      );
      break;
    }

    case SPN_EVENT_RESOLVE_END:
    case SPN_EVENT_BUILD_PASSED:
    case SPN_EVENT_TEST_SUMMARY: {
      write_event(io, mem, verb, false, sp_str_lit(""), spn_tui_render_event_detail(mem, event));
      break;
    }

    case SPN_EVENT_TEST_PASSED: {
      write_event(io, mem, verb, false, event->test_passed.name, spn_tui_render_event_detail(mem, event));
      break;
    }

    case SPN_EVENT_TARGET_RUN: {
      sp_str_t name = event->run.name;
      if (sp_str_empty(name) && event->pkg) {
        name = event->pkg->name;
      }
      write_event(io, mem, verb, false, name, spn_tui_render_event_detail(mem, event));
      break;
    }

    case SPN_EVENT_USER_LOG: {
      sp_str_t name = event->pkg ? event->pkg->name : sp_str_lit("");
      write_event(io, mem, sp_str_lit(""), false, name, event->user_log.message);
      break;
    }

    default: {
      sp_str_t name = event->pkg ? event->pkg->name : sp_str_lit("");
      write_event(io, mem, verb, false, name, spn_tui_render_event_detail(mem, event));
      break;
    }
  }

  flush_writer(&tui->writer);
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

void spn_tui_init(spn_tui_t* tui) {
  tui->mem = sp_mem_arena_as_allocator(sp_mem_arena_new(sp_mem_os_new()));

  sp_io_stream_writer_from_fd(&tui->logger.out, sp_sys_stdout, SP_IO_CLOSE_MODE_NONE);
  sp_io_stream_writer_from_fd(&tui->logger.err, sp_sys_stderr, SP_IO_CLOSE_MODE_NONE);
  if (sp_sys_is_tty(sp_sys_stdout)) {
    sp_sys_tty_use_vt(sp_sys_stdout);
  }
  if (sp_sys_is_tty(sp_sys_stderr)) {
    sp_sys_tty_use_vt(sp_sys_stderr);
  }
#ifdef SP_WIN32
  if (sp_sys_is_tty(sp_sys_stdout) || sp_sys_is_tty(sp_sys_stderr)) {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
  }
#endif

  tui->writer = (spn_tui_line_writer_t) {
    .base.write = on_write,
    .downstream = &tui->logger.err.base,
  };
  sp_da_init(tui->mem, tui->writer.partial);
  sp_str_ht_init(tui->mem, tui->seen_url);
  sp_da_init(tui->mem, tui->buffered_logs);
  sp_ht_init(tui->mem, tui->thread_ids);
}

void spn_tui_open(spn_tui_t* tui, spn_tui_mode_t mode, spn_verbosity_t verbosity, sp_sys_fd_t wake_read, sp_sys_fd_t wake_write) {
  tui->mode = mode;
  tui->logger.verbosity = verbosity;
  tui->wake.read = wake_read;
  tui->wake.write = wake_write;
}

static u32 get_short_tid(spn_tui_t* tui, u64 thread_id) {
  static u32 next_thread_id = 0;
  if (!sp_ht_key_exists(tui->thread_ids, thread_id)) {
    sp_ht_insert(tui->thread_ids, thread_id, next_thread_id++);
  }
  return *sp_ht_getp(tui->thread_ids, thread_id);
}

static void emit_event(spn_tui_t* tui, spn_build_event_t* event) {
  event->thread_id = get_short_tid(tui, event->thread_id);
  spn_tui_log_event(tui, event);
}

void spn_tui_flush(spn_tui_t* tui) {
  if (!spn.events) {
    return;
  }

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  sp_da(spn_build_event_t) events = spn_event_buffer_drain(s.mem, spn.events);

  sp_da_for(events, it) {
    spn_build_event_t* event = &events[it];

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

  sp_mem_end_scratch(s);
}

void spn_tui_run_banner(spn_tui_t* tui, sp_str_t name, sp_str_t command) {
  spn_build_event_t event = {
    .kind = SPN_EVENT_TARGET_RUN,
    .run = {
      .name = name,
      .command = command,
    },
  };
  emit_event(tui, &event);
}

static void print_line(sp_io_writer_t* io, const c8* fmt, va_list args) {
  sp_fmt_io_v(io, sp_cstr_as_str(fmt), args);
  sp_io_write_new_line(io);
}

void spn_print(const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  print_line(&tui.logger.out.base, fmt, args);
  va_end(args);
}

void spn_print_err(const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  print_line(&tui.logger.err.base, fmt, args);
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

  if (tui->mode != SPN_OUTPUT_MODE_INTERACTIVE) {
    return;
  }
  if (!sp_sys_is_tty(sp_sys_stdout)) {
    return;
  }

  tui->prompt.ctx = sp_prompt_begin(tui->mem);
  if (!tui->prompt.ctx) {
    return;
  }
  sp_prompt_use_wake(tui->prompt.ctx, tui->wake.read, tui->wake.write);

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
  attach_prompt(tui, tui->prompt.ctx);
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
  spn_progress_t progress = sp_zero;
  bool building = spn_ctx_progress(&spn, &progress);
  spn_tui_flush(tui);
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
