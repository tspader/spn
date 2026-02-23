#include "app.h"

#include "sp/ht.h"

void spn_session_init(spn_session_t* session, spn_pkg_t* pkg, spn_profile_t* profile, sp_str_t dir) {
  session->pkg = pkg;
  session->profile = profile;
  session->paths.root = sp_str_copy(pkg->paths.root);
  session->paths.build = sp_fs_join_path(session->paths.root, dir);
  session->paths.profile = sp_fs_join_path(session->paths.build, session->profile->name);


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

spn_pkg_unit_t* spn_session_find_root(spn_session_t* s) {
  return spn_session_find_pkg(s, s->pkg->name);
}

void spn_session_set_filter(spn_session_t* session, spn_target_filter_t filter) {
  session->filter = filter;
}

void spn_pkg_unit_add_target(spn_pkg_unit_t* pkg, spn_target_t* target) {
  spn_session_t* session = pkg->ctx.session;

  spn_event_buffer_push_ex(spn.events, pkg->ctx.pkg, &pkg->ctx.logs, (spn_build_event_t) {
    .kind = SPN_EVENT_ADD_TARGET,
    .target_add = {
      .target = target->name,
      .kind = target->kind,
    }
  });

  sp_om_insert(pkg->targets, target->name, SP_ZERO_STRUCT(spn_target_unit_t));
  spn_target_unit_t* unit = sp_om_back(pkg->targets);
  unit->session = pkg->ctx.session;
  unit->pkg = pkg->ctx.pkg;
  unit->info = target;

  spn_bp_config_t config = {
    .source = pkg->ctx.paths.source,
    .store = pkg->ctx.paths.store,
    .work = pkg->ctx.paths.work,
  };

  unit->paths.source = sp_str_copy(config.source);
  unit->paths.work = sp_str_copy(config.work);
  unit->paths.store = sp_str_copy(config.store);
  unit->paths.include = sp_fs_join_path(unit->paths.store, SP_LIT("include"));
  unit->paths.bin = sp_fs_join_path(unit->paths.store, SP_LIT("bin"));
  unit->paths.lib = sp_fs_join_path(unit->paths.store, SP_LIT("lib"));
  unit->paths.vendor = sp_fs_join_path(unit->paths.store, SP_LIT("vendor"));
  unit->paths.generated = sp_fs_join_path(unit->paths.work, SP_LIT("spn"));
  unit->paths.object = sp_fs_join_path(unit->paths.generated, sp_str_lit("object"));
  unit->paths.logs.build = sp_fs_join_path(unit->paths.work, sp_format("{}.build.log", SP_FMT_STR(target->name)));
  unit->paths.logs.test = sp_fs_join_path(unit->paths.work, sp_format("{}.test.log", SP_FMT_STR(target->name)));

  sp_fs_create_dir(unit->paths.work);
  sp_fs_create_dir(unit->paths.generated);
  sp_fs_create_dir(unit->paths.object);
  sp_fs_create_dir(unit->paths.store);
  sp_fs_create_dir(unit->paths.bin);
  sp_fs_create_dir(unit->paths.include);
  sp_fs_create_dir(unit->paths.lib);
  sp_fs_create_dir(unit->paths.vendor);
  sp_fs_create_file(unit->paths.logs.build);

  unit->logs.build = sp_io_writer_from_file(unit->paths.logs.build, SP_IO_WRITE_MODE_APPEND);

  spn_event_buffer_push_ex(spn.events, pkg->ctx.pkg, &pkg->ctx.logs, (spn_build_event_t) {
    .kind = SPN_EVENT_ADD_TARGET,
    .target.add = {
      .name = target->name
    }
  });

}

void spn_init_pkg_unit_for_session(spn_session_t* session, spn_pkg_unit_t* unit, spn_pkg_t* pkg, spn_pkg_kind_t kind, spn_semver_t version) {
  switch (kind) {
    case SPN_PACKAGE_KIND_ROOT:
    case SPN_PACKAGE_KIND_FILE: {
      spn_pkg_unit_init(unit, (spn_pkg_unit_config_t)  {
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
    case SPN_PACKAGE_KIND_INDEX: {
      sp_opt(spn_linkage_t) linkage = SP_ZERO_INITIALIZE();

      spn_dep_options_t* options = sp_ht_getp(pkg->config, pkg->name);
      if (options) {
        spn_dep_option_t* kind = sp_ht_getp(*options, sp_str_lit("kind"));
        if (kind) {
          sp_opt_set(linkage, spn_lib_kind_from_str(kind->str));
        }
      }

      switch (linkage.some) {
        case SP_OPT_SOME: {
          break;
        }
        case SP_OPT_NONE: {
          if (spn_pkg_has_lib_kind(pkg, SPN_LIB_KIND_SOURCE)) { sp_opt_set(linkage, SPN_LIB_KIND_SOURCE); break; }
          if (spn_pkg_has_lib_kind(pkg, SPN_LIB_KIND_STATIC)) { sp_opt_set(linkage, SPN_LIB_KIND_STATIC); break; }
          if (spn_pkg_has_lib_kind(pkg, SPN_LIB_KIND_SHARED)) { sp_opt_set(linkage, SPN_LIB_KIND_SHARED); break; }
          sp_unreachable();
        }
      }

      spn_pkg_metadata_t* metadata = sp_ht_getp(pkg->metadata, version);
      sp_assert(metadata);

      sp_dyn_array(sp_hash_t) hashes = SP_NULLPTR;
      sp_dyn_array_push(hashes, sp_hash_bytes(metadata->commit.data, metadata->commit.len, 0));
      sp_dyn_array_push(hashes, sp_hash_bytes(session->profile->cc.exe.data, session->profile->cc.exe.len, 0));
      sp_dyn_array_push(hashes, session->profile->cc.kind);
      sp_dyn_array_push(hashes, session->profile->libc);
      sp_dyn_array_push(hashes, session->profile->mode);
      sp_dyn_array_push(hashes, linkage.value);
      sp_dyn_array_push(hashes, metadata->version.major);
      sp_dyn_array_push(hashes, metadata->version.minor);
      sp_dyn_array_push(hashes, metadata->version.patch);
      sp_hash_t build_id = sp_hash_combine(hashes, sp_dyn_array_size(hashes));
      sp_str_t build_str = sp_format("{}", SP_FMT_SHORT_HASH(build_id));

      spn_pkg_unit_init(unit, (spn_pkg_unit_config_t)  {
        .build_id = build_id,
        .metadata = *metadata,
        .ctx = {
          .name = pkg->name,
          .package = pkg,
          .session = session,
          .linkage = linkage.value,
          .paths = {
            .work = sp_fs_join_path(pkg->paths.cache.work, build_str),
            .store = sp_fs_join_path(pkg->paths.cache.store, build_str),
            .source = pkg->paths.cache.source
          }
        }
      });
      break;
    }
    case SPN_PACKAGE_KIND_WORKSPACE:
    case SPN_PACKAGE_KIND_NONE: {
      SP_UNREACHABLE_CASE();
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
        if (spn_target_filter_pass(&session->filter, target)) {
          spn_pkg_unit_add_target(unit, target);
        }
      }

      break;
    }
    case SPN_PACKAGE_KIND_WORKSPACE:
    case SPN_PACKAGE_KIND_NONE: {
      sp_unreachable_case();
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
    case SPN_PACKAGE_KIND_WORKSPACE:
    case SPN_PACKAGE_KIND_NONE: {
      sp_unreachable_case();
    }
  }

}

void spn_session_add_pkg_unit(spn_session_t* session, spn_resolved_pkg_t resolved) {
  sp_om_insert(session->units.packages, resolved.pkg->name, SP_ZERO_STRUCT(spn_pkg_unit_t));
  spn_pkg_unit_t* unit = sp_om_back(session->units.packages);
  spn_init_pkg_unit_for_session(session, unit, resolved.pkg, resolved.kind, resolved.version);
}
