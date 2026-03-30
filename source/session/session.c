#include "session/session.h"

#include "app/app.h"
#include "ctx/types.h"
#include "filter/filter.h"
#include "event/event.h"
#include "external/cc.h"
#include "pkg/pkg.h"
#include "sp/hash.h"
#include "sp/ht.h"
#include "sp/macro.h"
#include "log/log.h"
#include "spn.h"
#include "target/mutate.h"
#include "unit/package.h"

static spn_linkage_t spn_session_resolve_dep_linkage(spn_session_t* session, spn_pkg_t* pkg) {
  sp_opt(spn_linkage_t) requested = SP_ZERO_INITIALIZE();

  spn_dep_options_t* options = sp_ht_getp(session->pkg->config, pkg->name);
  if (options) {
    spn_dep_option_t* kind = sp_ht_getp(*options, sp_str_lit("kind"));
    if (kind && kind->kind == SPN_DEP_OPTION_KIND_STR) {
      sp_opt_set(requested, spn_lib_kind_from_str(kind->str));
    }
  }

  if (requested.some) {
    spn_linkage_t value = requested.value;
    if (!spn_pkg_has_lib_kind(pkg, value)) {
      SP_FATAL(
        "{:fg brightcyan} does not support {:fg brightyellow}; requested from {:fg brightcyan}",
        SP_FMT_STR(pkg->name),
        SP_FMT_STR(spn_pkg_linkage_to_str(value)),
        SP_FMT_STR(session->pkg->name)
      );
    }

    return value;
  }

  if (spn_pkg_has_lib_kind(pkg, SPN_LIB_KIND_SOURCE)) {
    return SPN_LIB_KIND_SOURCE;
  }

  if (spn_pkg_has_lib_kind(pkg, SPN_LIB_KIND_STATIC)) {
    return SPN_LIB_KIND_STATIC;
  }

  if (spn_pkg_has_lib_kind(pkg, SPN_LIB_KIND_SHARED)) {
    return SPN_LIB_KIND_SHARED;
  }

  return SPN_LIB_KIND_SOURCE; // @spader: For toolchain, which doesn't have a lib entry
  // SP_FATAL("{:fg brightcyan} has no consumable lib kinds", SP_FMT_STR(pkg->name));
  // SP_UNREACHABLE_RETURN(SPN_LIB_KIND_SHARED);
}

void spn_session_init(spn_session_t* session, spn_pkg_t* pkg, spn_profile_t* profile, sp_str_t dir) {
  session->pkg = pkg;
  session->profile = profile;
  session->paths.root = sp_str_copy(pkg->paths.root);
  session->paths.build = sp_fs_join_path(session->paths.root, dir);
  session->paths.profile = sp_fs_join_path(session->paths.build, session->profile->name);
  session->env = sp_env_capture();

  sp_mutex_init(&session->mutex, SP_MUTEX_PLAIN);
}

spn_pkg_unit_t* spn_session_find_pkg(spn_session_t* session, sp_str_t name) {
  sp_mutex_lock(&session->mutex);
  spn_pkg_unit_t* pkg = sp_str_equal(name, session->pkg->name) ?
    &session->units.root :
    sp_om_get(session->units.packages, name);
  sp_mutex_unlock(&session->mutex);

  return pkg;
}

spn_pkg_unit_t* spn_session_find_pkg_or_assert(spn_session_t* s, sp_str_t name) {
  spn_pkg_unit_t* unit = spn_session_find_pkg(s, name);
  SP_ASSERT_FMT(unit, "{:fg brightyellow} is not in this project", SP_FMT_STR(name));
  return unit;
}

spn_pkg_unit_t* spn_session_find_root(spn_session_t* s) {
  return spn_session_find_pkg(s, s->pkg->name);
}

void spn_session_set_filter(spn_session_t* session, spn_target_filter_t filter) {
  session->filter = filter;
}

void spn_init_pkg_unit_for_session(spn_session_t* session, spn_pkg_unit_t* unit, spn_pkg_t* pkg, spn_pkg_kind_t kind) {
  switch (kind) {
    case SPN_PACKAGE_KIND_ROOT: {
      spn_pkg_unit_init(unit, (spn_pkg_unit_config_t) {
        .ctx = {
          .name = pkg->name,
          .package = pkg,
          .session = session,
          .paths = {
            .store = sp_fs_join_path(session->paths.profile, sp_str_lit("store")),
            .work = sp_fs_join_path(session->paths.profile, sp_format("work/{}", SP_FMT_STR(pkg->name))),
            .source = sp_str_copy(pkg->paths.root),
          }
        }
      });
      break;
    }
    case SPN_PACKAGE_KIND_FILE: {
      spn_linkage_t linkage = spn_session_resolve_dep_linkage(session, pkg);

      spn_pkg_unit_init(unit, (spn_pkg_unit_config_t) {
        .ctx = {
          .name = pkg->name,
          .package = pkg,
          .session = session,
          .linkage = linkage,
          .paths = {
            .store = sp_fs_join_path(session->paths.profile, sp_str_lit("store")),
            .work = sp_fs_join_path(session->paths.profile, sp_format("work/{}", SP_FMT_STR(pkg->name))),
            .source = sp_str_copy(pkg->paths.root),
          }
        }
      });
      break;
    }
    case SPN_PACKAGE_KIND_INDEX: {
      spn_linkage_t linkage = spn_session_resolve_dep_linkage(session, pkg);

      spn_pkg_metadata_t* metadata = sp_ht_getp(pkg->metadata, pkg->version);
      sp_assert(metadata);

      sp_da(sp_hash_t) hashes = SP_NULLPTR;
      sp_da_push(hashes, sp_hash_str(metadata->commit));
      // sp_da_push(hashes, sp_hash_str(session->toolchain.info->name));
      // sp_da_push(hashes, sp_hash_str(session->toolchain.info->compiler.program));
      // sp_da_push(hashes, sp_hash_str(session->toolchain.info->linker.program));
      // sp_da_push(hashes, sp_hash_str(session->toolchain.info->archiver.program));
      // sp_da_push(hashes, sp_hash_str(session->toolchain.info->sysroot));
      // sp_da_push(hashes, session->toolchain.info->driver);
      // sp_da_push(hashes, session->toolchain.info->abi);
      sp_da_push(hashes, session->profile->mode);
      sp_da_push(hashes, linkage);
      sp_da_push(hashes, metadata->version.major);
      sp_da_push(hashes, metadata->version.minor);
      sp_da_push(hashes, metadata->version.patch);
      sp_hash_t build_id = sp_hash_combine(hashes, sp_dyn_array_size(hashes));
      sp_str_t build_str = sp_format("{}", SP_FMT_SHORT_HASH(build_id));

      spn_pkg_unit_init(unit, (spn_pkg_unit_config_t) {
        .build_id = build_id,
        .metadata = *metadata,
        .ctx = {
          .name = pkg->name,
          .package = pkg,
          .session = session,
          .linkage = linkage,
          .paths = {
            .work = sp_fs_join_path(pkg->paths.cache.work, build_str),
            .store = sp_fs_join_path(pkg->paths.cache.store, build_str),
            .source = pkg->paths.cache.source
          }
        }
      });
      break;
    }
  }

  switch (kind) {
    case SPN_PACKAGE_KIND_ROOT:
    case SPN_PACKAGE_KIND_FILE:
    case SPN_PACKAGE_KIND_INDEX: {
      sp_om_for(pkg->exes, it) {
        spn_target_t* target = sp_om_at(pkg->exes, it);
        if (spn_target_filter_pass(&session->filter, target)) {
          spn_pkg_unit_add_target(unit, target);
        }
      }

      sp_om_for(pkg->libs, it) {
        spn_target_t* target = sp_om_at(pkg->libs, it);
        if (kind != SPN_PACKAGE_KIND_ROOT) {
          spn_linkage_t linkage = unit->ctx.linkage;
          if (!spn_linkage_set_has(target->linkages, linkage)) {
            continue;
          }

          if (linkage == SPN_LIB_KIND_SOURCE) {
            continue;
          }

          target->kind = spn_pkg_linkage_to_target_kind(linkage);
        }

        if (spn_target_filter_pass(&session->filter, target)) {
          spn_pkg_unit_add_target(unit, target);
        }
      }

      break;
    }
  }

  switch (kind) {
    case SPN_PACKAGE_KIND_ROOT: {
      sp_om_for(pkg->scripts, it) {
        spn_target_t* target = sp_om_at(pkg->scripts, it);
        if (spn_target_filter_pass(&session->filter, target)) {
          spn_pkg_unit_add_target(unit, target);
        }
      }

      break;
    }
    case SPN_PACKAGE_KIND_FILE:
    case SPN_PACKAGE_KIND_INDEX: {
      break;
    }
  }

  switch (kind) {
    case SPN_PACKAGE_KIND_ROOT:
    case SPN_PACKAGE_KIND_FILE: {
      sp_om_for(pkg->tests, it) {
        spn_target_t* target = sp_om_at(pkg->tests, it);
        if (spn_target_filter_pass(&session->filter, target)) {
          spn_pkg_unit_add_target(unit, target);
        }
      }

      break;
    }
    case SPN_PACKAGE_KIND_INDEX: {
      break;
    }
  }
}

spn_err_t spn_session_compile_pkg(spn_session_t* session, spn_pkg_unit_t* unit) {
  spn_pkg_t* pkg = unit->ctx.pkg;

  if (!sp_fs_exists(pkg->paths.script)) {
    return SPN_OK;
  }

  sp_tm_timer_t timer = sp_tm_start_timer();
  spn_tcc_err_ctx_t error_context = {
    .arena = unit->ctx.arena,
    .error = sp_str_lit("")
  };

  spn_tcc_t* tcc = tcc_new();
  sp_try_goto(spn_tcc_prepare_script(tcc, &error_context), fail);

  spn_cc_t cc = SP_ZERO_INITIALIZE();
  spn_cc_set_profile(&cc, session->profile);
  spn_cc_target_t* target = spn_cc_add_target(&cc, SPN_TARGET_JIT, pkg->name);
  sp_ht_for_kv(pkg->deps, it) {
    switch (it.val->visibility) {
      case SPN_VISIBILITY_BUILD: {
        spn_cc_target_add_dep(target, spn_session_find_pkg(session, *it.key));
        break;
      }
      case SPN_VISIBILITY_SCRIPT:
      case SPN_VISIBILITY_TEST:
      case SPN_VISIBILITY_PUBLIC: {
        break;
      }
    }
  }

  spn_cc_target_to_tcc(&cc, target, tcc);
  sp_try_goto(spn_tcc_add_file(tcc, pkg->paths.script), fail);
  sp_try_goto(tcc_relocate(tcc), fail);

  unit->tcc = tcc;
  unit->on_configure = tcc_get_symbol(tcc, "configure");
  unit->on_package = tcc_get_symbol(tcc, "package");
  sp_assert_fmt(!tcc_get_symbol(tcc, "build"), "{} still has build()", SP_FMT_STR(unit->ctx.name));

  unit->time.compile = sp_tm_read_timer(&timer);

  spn_event_buffer_push_ctx(spn.events, &unit->ctx, (spn_build_event_t) {
    .kind = SPN_EVENT_BUILD_SCRIPT_COMPILE,
    .script_compile = {
      .script_path = pkg->paths.script,
      .time = unit->time.compile,
      .has_configure = unit->on_configure != SP_NULLPTR,
      .has_package = unit->on_package != SP_NULLPTR,
    }
  });

  return SPN_OK;

fail:
  spn_event_buffer_push_ctx(spn.events, &unit->ctx, (spn_build_event_t) {
    .kind = SPN_EVENT_BUILD_SCRIPT_COMPILE_FAILED,
    .compile_failed = {
      .script_path = pkg->paths.script,
      .error = error_context.error,
    }
  });
  return SPN_ERROR;
}
