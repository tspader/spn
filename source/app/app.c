#include "app/app.h"

#include "ctx/types.h"
#include "event/event.h"
#include "external/cc.h"
#include "external/tom.h"
#include "lock/lock.h"
#include "pkg/load.h"
#include "pkg/mutate.h"
#include "pkg/pkg.h"
#include "target/target.h"
#include "semver/convert.h"
#include "sp.h"
#include "sp/ht.h"
#include "sp/str.h"

sp_str_t spn_pkg_req_to_str(spn_pkg_req_t dep) {
  switch (dep.kind) {
    case SPN_PACKAGE_KIND_FILE: {
      return dep.file;
    }
    case SPN_PACKAGE_KIND_INDEX: {
      return spn_semver_range_to_str(dep.range);
    }
    case SPN_PACKAGE_KIND_ROOT: {
      SP_BROKEN();
      break;
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}


void spn_app_update_lock_file(spn_app_t* app) {
  spn_lock_file_t lock = spn_build_lock_file(app->resolver, &app->package);

  sp_da_for(app->package.system_deps, i) {
    sp_ht_insert(lock.system_deps, sp_str_copy(app->package.system_deps[i]), true);
  }

  sp_str_t output = spn_lock_file_to_str(&lock);
  sp_io_writer_t file = sp_io_writer_from_file(app->paths.lock, SP_IO_WRITE_MODE_OVERWRITE);
  sp_io_write_str(&file, output);
  sp_io_writer_close(&file);
}

void spn_app_write_manifest(spn_pkg_t* pkg, sp_str_t path) {
}

spn_app_t spn_app_init_and_write(sp_str_t path, sp_str_t name, spn_app_init_mode_t mode) {
  sp_str_t paths[] = {
    sp_fs_join_path(path, sp_str_lit("spn.toml")),
    sp_fs_join_path(path, sp_str_lit("spn.c")),
  };
  sp_carr_for(paths, it) {
    if (sp_fs_exists(paths[it])) {
      SP_FATAL("{:fg brightcyan} already exists; bailing", SP_FMT_STR(paths[it]));
    }
  }

  spn_app_t app = SP_ZERO_INITIALIZE();

  switch (mode) {
    case SPN_APP_INIT_NORMAL: {
      app.package = spn_pkg_from_default(path, name);

      sp_str_t main = sp_fs_join_path(path, sp_str_lit("main.c"));
      sp_io_writer_t io = sp_io_writer_from_file(main, SP_IO_WRITE_MODE_OVERWRITE);

      sp_str_t content = sp_str_lit(
        "#define SP_IMPLEMENTATION\n"
        "#include \"sp.h\"\n"
        "\n"
        "s32 main(s32 num_args, const c8** args) {\n"
        "  SP_LOG(\"hello, {:fg brightcyan}\", SP_FMT_CSTR(\"world\"));\n"
        "  SP_EXIT_SUCCESS();\n"
        "}\n"
      );

      if (sp_io_write_str(&io, content) != content.len) {
        SP_FATAL("Failed to write {:fg brightyellow}", SP_FMT_STR(main));
      }

      sp_io_writer_close(&io);

      spn_app_write_manifest(&app.package, app.package.paths.manifest);

      break;
    }
    case SPN_APP_INIT_BARE: {
      app.package = spn_pkg_new(name);
      spn_pkg_set_manifest(&app.package, sp_fs_join_path(path, SP_LIT("spn.toml")));
      spn_app_write_manifest(&app.package, app.package.paths.manifest);

      break;
    }
  }

  return app;
}

spn_err_t spn_app_load(spn_app_t* app, sp_str_t manifest_path) {
  if (sp_fs_exists(manifest_path)) {
    sp_try(spn_pkg_from_manifest(&app->package, manifest_path));
  }

  app->paths.dir = app->package.paths.root;
  app->paths.lock = sp_fs_join_path(app->paths.dir, SP_LIT("spn.lock"));

  if (sp_fs_exists(app->paths.lock)) {
    sp_opt_set(app->lock, spn_lock_file_load(app->paths.lock, spn.events));
  }

  if (sp_om_empty(app->package.profiles)) {
    spn_profile_t profiles[] = {
      {
        .name = sp_str_lit("debug"),
        .linkage = SPN_LIB_KIND_SHARED,
        .libc = SPN_LIBC_GNU,
        .standard = SPN_C11,
        .mode = SPN_DEP_BUILD_MODE_DEBUG,
        .kind = SPN_PROFILE_BUILTIN,
        .cc = {
          .kind = SPN_CC_GCC,
          .exe = sp_str_lit("gcc")
        },
      },
      {
        .name = sp_str_lit("release"),
        .linkage = SPN_LIB_KIND_SHARED,
        .libc = SPN_LIBC_GNU,
        .standard = SPN_C11,
        .mode = SPN_DEP_BUILD_MODE_RELEASE,
        .kind = SPN_PROFILE_BUILTIN,
        .cc = {
          .kind = SPN_CC_GCC,
          .exe = sp_str_lit("gcc")
        },
      }
    };
    sp_carr_for(profiles, it) {
      spn_pkg_add_profile_ex(&app->package, profiles[it]);
    }
  }

  return SPN_OK;
}
