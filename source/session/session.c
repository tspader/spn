#include "ctx/types.h"
#include "semver/types.h"
#include "unit/types.h"

#include "enum/enum.h"
#include "filter/filter.h"
#include "pkg/pkg.h"
#include "session/session.h"
#include "sp/hash.h"

static spn_linkage_t resolve_linkage(spn_session_t* session, spn_pkg_info_t* pkg) {
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

typedef struct {
  sp_hash_t commit;
  spn_semver_t version;
  spn_build_mode_t mode;
  spn_linkage_t linkage;
  spn_c_standard_t standard;
  spn_arch_t arch;
  spn_os_t os;
  spn_abi_t abi;
  struct {
    sp_hash_t name;
    sp_hash_t cc;
    sp_hash_t ld;
    sp_hash_t ar;
    sp_hash_t url;
    spn_semver_t version;
  } toolchain;
} fingerprint_input_t;

typedef struct {
  sp_hash_t hash;
  sp_str_t str;
} fingerprint_t;


fingerprint_t fingerprint_package(spn_session_t* session, spn_pkg_info_t* pkg) {
  spn_pkg_metadata_t* metadata = sp_ht_getp(pkg->metadata, pkg->version);
  sp_assert(metadata);

  fingerprint_input_t fingerprint = SP_ZERO_INITIALIZE();
  sp_mem_zero(&fingerprint, sizeof(fingerprint));
  fingerprint.commit = sp_hash_str(metadata->commit);
  fingerprint.version = metadata->version;

  bool compiled = sp_om_size(pkg->libs) > 0 || sp_om_size(pkg->exes) > 0;
  if (compiled) {
    fingerprint.mode = session->profile.mode;
    fingerprint.linkage = resolve_linkage(session, pkg);
    fingerprint.standard = session->profile.standard;
    fingerprint.arch = session->profile.arch;
    fingerprint.os = session->profile.os;
    fingerprint.abi = session->profile.abi;

    if (session->toolchain.source == SPN_TOOLCHAIN_INDEX) {
      fingerprint.toolchain.name = sp_hash_str(session->toolchain.pkg->qualified);
      fingerprint.toolchain.version = session->toolchain.pkg->version;
    }
    fingerprint.toolchain.cc = sp_hash_str(session->toolchain.info.compiler.program);
    fingerprint.toolchain.ld = sp_hash_str(session->toolchain.info.linker.program);
    fingerprint.toolchain.ar = sp_hash_str(session->toolchain.info.archiver.program);
    fingerprint.toolchain.url = sp_hash_str(session->toolchain.info.url);
  }

  fingerprint_t id = SP_ZERO_INITIALIZE();
  id.hash = sp_hash_bytes(&fingerprint, sizeof(fingerprint), 0);
  id.str = sp_format("{}", SP_FMT_HASH(id.hash));
  return id;
}

spn_pkg_unit_t* spn_session_find_pkg(spn_session_t* session, sp_str_t name) {
  sp_mutex_lock(&session->mutex);
  spn_pkg_unit_t* pkg = sp_om_get(session->units.packages, name);
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

spn_target_unit_t* spn_session_add_target(spn_session_t* session, spn_pkg_unit_t* pkg, spn_target_info_t* info) {
  sp_om_insert(session->units.targets, info->name, SP_ZERO_STRUCT(spn_target_unit_t));
  spn_target_unit_t* target = sp_om_back(session->units.targets);
  target->session = session;
  target->parent = pkg;
  target->pkg = pkg->pkg;
  target->info = info;

  sp_str_ht_insert(pkg->targets, info->name, target);

  spn_bp_config_t config = {
    .source = pkg->paths.source,
    .store = pkg->paths.store,
    .work = pkg->paths.work,
  };

  target->paths.source = sp_str_copy(config.source);
  target->paths.work = sp_str_copy(config.work);
  target->paths.store = sp_str_copy(config.store);
  target->paths.include = sp_fs_join_path(target->paths.store, SP_LIT("include"));
  target->paths.bin = sp_fs_join_path(target->paths.store, SP_LIT("bin"));
  target->paths.lib = sp_fs_join_path(target->paths.store, SP_LIT("lib"));
  target->paths.vendor = sp_fs_join_path(target->paths.store, SP_LIT("vendor"));
  target->paths.generated = sp_fs_join_path(target->paths.work, SP_LIT("spn"));
  target->paths.object = sp_fs_join_path(target->paths.generated, sp_str_lit("object"));
  target->paths.logs.build = sp_fs_join_path(target->paths.work, sp_format("{}.build.log", SP_FMT_STR(info->name)));
  target->paths.logs.test = sp_fs_join_path(target->paths.work, sp_format("{}.test.log", SP_FMT_STR(info->name)));
  target->paths.logs.jsonl = sp_fs_join_path(target->paths.work, sp_format("{}.build.jsonl", SP_FMT_STR(info->name)));

  sp_fs_create_dir(target->paths.work);
  sp_fs_create_dir(target->paths.generated);
  sp_fs_create_dir(target->paths.object);
  sp_fs_create_dir(target->paths.store);
  sp_fs_create_dir(target->paths.bin);
  sp_fs_create_dir(target->paths.include);
  sp_fs_create_dir(target->paths.lib);
  sp_fs_create_dir(target->paths.vendor);
  sp_fs_create_file(target->paths.logs.build);
  sp_fs_create_file(target->paths.logs.jsonl);

  target->logs.build = sp_io_writer_from_file(target->paths.logs.build, SP_IO_WRITE_MODE_APPEND);
  target->logs.jsonl = sp_io_writer_from_file(target->paths.logs.jsonl, SP_IO_WRITE_MODE_APPEND);
  return target;
}

spn_target_unit_t* spn_session_add_exe(spn_session_t* session, spn_pkg_unit_t* pkg, spn_target_info_t* info) {
  spn_target_unit_t* target = spn_session_add_target(session, pkg, info);
  sp_str_ht_insert(pkg->exes, info->name, target);
  return target;
}

spn_target_unit_t* spn_session_add_lib(spn_session_t* session, spn_pkg_unit_t* pkg, spn_target_info_t* info) {
  spn_target_unit_t* target = spn_session_add_target(session, pkg, info);
  sp_str_ht_insert(pkg->libs, info->name, target);
  return target;
}

spn_target_unit_t* spn_session_add_script(spn_session_t* session, spn_pkg_unit_t* pkg, spn_target_info_t* info) {
  spn_target_unit_t* target = spn_session_add_target(session, pkg, info);
  sp_str_ht_insert(pkg->scripts, info->name, target);
  return target;
}

spn_target_unit_t* spn_session_add_test(spn_session_t* session, spn_pkg_unit_t* pkg, spn_target_info_t* info) {
  spn_target_unit_t* target = spn_session_add_target(session, pkg, info);
  sp_str_ht_insert(pkg->tests, info->name, target);
  return target;
}

spn_pkg_unit_t* spn_session_add_pkg(spn_session_t* session, spn_loaded_pkg_t* loaded) {
  spn_pkg_info_t* pkg = loaded->pkg;

  sp_om_insert(session->units.packages, loaded->pkg->qualified, SP_ZERO_STRUCT(spn_pkg_unit_t));
  spn_pkg_unit_t* unit = sp_om_back(session->units.packages);
  unit->pkg = pkg;
  unit->session = session;
  unit->paths.manifest = loaded->paths.manifest;
  unit->paths.script = loaded->paths.script;
  unit->paths.source = loaded->paths.source;

  switch (loaded->kind) {
    case SPN_PACKAGE_KIND_ROOT:
    case SPN_PACKAGE_KIND_FILE: {
      sp_str_t work = sp_fs_join_path(session->paths.profile, sp_str_lit("work"));
      unit->paths.work = sp_fs_join_path(work, pkg->name);
      unit->paths.store = sp_fs_join_path(session->paths.profile, sp_str_lit("store"));
      unit->paths.source = loaded->paths.source;
      break;
    }
    case SPN_PACKAGE_KIND_INDEX: {
      spn_pkg_metadata_t* metadata = sp_ht_getp(pkg->metadata, pkg->version);
      sp_assert(metadata);

      fingerprint_t id = fingerprint_package(session, pkg);
      unit->paths.work = sp_fs_join_path(sp_fs_join_path(spn.paths.build, pkg->qualified), id.str);
      unit->paths.store = sp_fs_join_path(sp_fs_join_path(spn.paths.store, pkg->qualified), id.str);
      unit->paths.source = loaded->paths.source;
      break;
    }
  }

  unit->paths.include = sp_fs_join_path(unit->paths.store, SP_LIT("include"));
  unit->paths.bin = sp_fs_join_path(unit->paths.store, SP_LIT("bin"));
  unit->paths.lib = sp_fs_join_path(unit->paths.store, SP_LIT("lib"));
  unit->paths.vendor = sp_fs_join_path(unit->paths.store, SP_LIT("vendor"));

  unit->paths.generated = sp_fs_join_path(unit->paths.work, SP_LIT("spn"));

  unit->logs.build = sp_format("{}.build.log", SP_FMT_STR(unit->pkg->name));
  unit->logs.test = sp_format("{}.test.log", SP_FMT_STR(unit->pkg->name));
  unit->logs.jsonl = sp_format("{}.jsonl", SP_FMT_STR(unit->pkg->name));

  unit->paths.logs.build = sp_fs_join_path(unit->paths.work, unit->logs.build);
  unit->paths.logs.test = sp_fs_join_path(unit->paths.work, unit->logs.test);
  unit->paths.logs.jsonl = sp_fs_join_path(unit->paths.work, unit->logs.jsonl);

  sp_fs_create_dir(unit->paths.work);
  sp_fs_create_dir(unit->paths.generated);
  sp_fs_create_dir(unit->paths.store);
  sp_fs_create_dir(unit->paths.bin);
  sp_fs_create_dir(unit->paths.include);
  sp_fs_create_dir(unit->paths.lib);
  sp_fs_create_dir(unit->paths.vendor);
  sp_fs_create_file(unit->paths.logs.build);
  sp_fs_create_file(unit->paths.logs.jsonl);

  unit->logs.io.build = sp_io_writer_from_file(unit->paths.logs.build, SP_IO_WRITE_MODE_APPEND);
  unit->logs.io.jsonl = sp_io_writer_from_file(unit->paths.logs.jsonl, SP_IO_WRITE_MODE_APPEND);

  unit->paths.stamp.dir = sp_fs_join_path(unit->paths.generated, SP_LIT("stamp"));
  unit->paths.stamp.main = sp_fs_join_path(unit->paths.stamp.dir, SP_LIT("main.stamp"));
  unit->paths.stamp.exit = sp_fs_join_path(unit->paths.stamp.dir, SP_LIT("user.stamp"));
  unit->paths.stamp.configure = sp_fs_join_path(unit->paths.stamp.dir, SP_LIT("configure.stamp"));
  unit->paths.stamp.build = sp_fs_join_path(unit->paths.stamp.dir, SP_LIT("build.stamp"));
  unit->paths.stamp.package = sp_fs_join_path(unit->paths.stamp.dir, SP_LIT("package.stamp"));

  sp_fs_create_dir(unit->paths.stamp.dir);

  sp_om_for(pkg->exes, it) {
    spn_target_info_t* info = sp_om_at(pkg->exes, it);
    if (spn_target_filter_pass(&session->filter, info)) {
      spn_session_add_exe(session, unit, info);
    }
  }

  sp_om_for(pkg->libs, it) {
    spn_target_info_t* info = sp_om_at(pkg->libs, it);
    if (spn_target_filter_pass(&session->filter, info)) {
      spn_session_add_lib(session, unit, info);
    }
  }

  if (loaded->kind == SPN_PACKAGE_KIND_ROOT) {
    sp_om_for(pkg->scripts, it) {
      spn_target_info_t* info = sp_om_at(pkg->scripts, it);
      if (spn_target_filter_pass(&session->filter, info)) {
        spn_session_add_script(session, unit, info);
      }
    }

    sp_om_for(pkg->tests, it) {
      spn_target_info_t* info = sp_om_at(pkg->tests, it);
      if (spn_target_filter_pass(&session->filter, info)) {
        spn_session_add_test(session, unit, info);
      }
    }
  }

  return unit;
}
