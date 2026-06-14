#include "app/app.h"
#include "ctx/ctx.h"
#include "enum/enum.h"
#include "event/event.h"
#include "gen.h"
#include "session/session.h"
#include "external/cc.h"

// sp_da(sp_str_t) spn_gen_build_entry(spn_build_ctx_t* build, spn_gen_entry_t kind, spn_cc_driver_t driver) {
//   sp_da(sp_str_t) entries = SP_NULLPTR;
//
//   switch (kind) {
//     case SPN_GEN_INCLUDE: {
//       sp_da_push(entries, spn_ctx_build_include_dir(build));
//       break;
//     }
//     case SPN_GEN_RPATH: {
//       switch (spn_ctx_build_linkage(build)) {
//         case SPN_LIB_KIND_SHARED: {
//           sp_da_push(entries, spn_ctx_build_lib_dir(build));
//           break;
//         }
//         case SPN_LIB_KIND_NONE:
//         case SPN_LIB_KIND_STATIC:
//         case SPN_LIB_KIND_SOURCE: {
//           return entries;
//         }
//       }
//       break;
//     }
//     case SPN_GEN_LIB_INCLUDE: {
//       switch (spn_ctx_build_linkage(build)) {
//         case SPN_LIB_KIND_SHARED: {
//           sp_da_push(entries, spn_ctx_build_lib_dir(build));
//           break;
//         }
//         case SPN_LIB_KIND_NONE:
//         case SPN_LIB_KIND_STATIC:
//         case SPN_LIB_KIND_SOURCE: {
//           return entries;
//         }
//       }
//       break;
//     }
//     case SPN_GEN_LIBS: {
//       entries = spn_ctx_build_lib_entries(build);
//       break;
//     }
//     case SPN_GEN_SYSTEM_LIBS: {
//       break;
//     }
//     case SPN_GEN_NONE:
//     case SPN_GEN_DEFINE:
//     case SPN_GEN_ALL: {
//       SP_UNREACHABLE_CASE();
//     }
//   }
//
//   spn_gen_format_context_t context = {
//     .driver = driver,
//     .kind = kind
//   };
//   entries = sp_str_map(entries, sp_da_size(entries), &context, spn_gen_format_entry_kernel);
//
//   return entries;
// }

// sp_str_t spn_gen_build_entries_for_all(sp_da(spn_build_ctx_t*) builds, spn_gen_entry_t kind, spn_cc_driver_t driver) {
//   sp_da(sp_str_t) entries = SP_NULLPTR;
//
//   sp_da_for(builds, it) {
//     spn_build_ctx_t* build = builds[it];
//     sp_da(sp_str_t) dep_entries = spn_gen_build_entry(build, kind, driver);
//     sp_str_t dep_flags = sp_str_join_n(spn_allocator, dep_entries, sp_da_size(dep_entries), sp_str_lit(" "));
//     if (!sp_str_empty(dep_flags)) {
//       sp_da_push(entries, dep_flags);
//     }
//   }
//
//   return sp_str_join_n(spn_allocator, entries, sp_da_size(entries), sp_str_lit(" "));
// }

spn_task_result_t spn_task_generate(spn_app_t* app) {
  // spn_cli_generate_t* command = &spn.cli.generate;
  //
  // sp_str_om_for(app->session.units.packages, it) {
  //   spn_pkg_unit_t* unit = sp_str_om_at(app->session.units.packages, it);
  //   sp_da_push(builds, &unit->ctx);
  // }
  //
  // spn_generator_t gen = {
  //   .kind = spn_gen_kind_from_str(command->generator),
  //   .compiler = spn_cc_kind_from_str(command->compiler)
  // };
  // gen.include = spn_gen_build_entries_for_all(builds, SPN_GEN_INCLUDE, gen.compiler);
  // gen.lib_include = spn_gen_build_entries_for_all(builds, SPN_GEN_LIB_INCLUDE, gen.compiler);
  // gen.libs = spn_gen_build_entries_for_all(builds, SPN_GEN_LIBS, gen.compiler);
  // gen.rpath = spn_gen_build_entries_for_all(builds, SPN_GEN_RPATH, gen.compiler);
  //
  // spn_gen_format_context_t fmt = {
  //   .kind = SPN_GEN_SYSTEM_LIBS,
  //   .driver = gen.compiler
  // };
  // sp_dyn_array(sp_str_t) entries = sp_str_map(app->resolver->system_deps, sp_dyn_array_size(app->resolver->system_deps), &fmt, spn_gen_format_entry_kernel);
  // gen.system_libs = sp_str_join_n(spn_allocator, entries, sp_dyn_array_size(entries), sp_str_lit(" "));
  //
  // switch (gen.kind) {
  //   case SPN_GEN_KIND_RAW: {
  //     gen.file_name = SP_LIT("spn.txt");
  //     gen.output = sp_format(
  //       "{} {} {} {} {}",
  //       SP_FMT_STR(gen.include),
  //       SP_FMT_STR(gen.lib_include),
  //       SP_FMT_STR(gen.libs),
  //       SP_FMT_STR(gen.system_libs),
  //       SP_FMT_STR(gen.rpath)
  //     );
  //     break;
  //   }
  //   case SPN_GEN_KIND_SHELL: {
  //     gen.file_name = SP_LIT("spn.sh");
  //     const c8* template =
  //       "export SPN_INCLUDES=\"{}\"\n"
  //       "export SPN_LIB_INCLUDES=\"{}\"\n"
  //       "export SPN_LIBS=\"{}\"\n"
  //       "export SPN_SYSTEM_LIBS=\"{}\"\n"
  //       "export SPN_RPATH=\"{}\"\n"
  //       "export SPN_FLAGS=\"$SPN_INCLUDES $SPN_LIB_INCLUDES $SPN_LIBS $SPN_SYSTEM_LIBS $SPN_RPATH\"\n";
  //     gen.output = sp_format(template,
  //       SP_FMT_STR(gen.include),
  //       SP_FMT_STR(gen.lib_include),
  //       SP_FMT_STR(gen.libs),
  //       SP_FMT_STR(gen.system_libs),
  //       SP_FMT_STR(gen.rpath)
  //     );
  //     break;
  //   }
  //
  //   case SPN_GEN_KIND_MAKE: {
  //     gen.file_name = SP_LIT("spn.mk");
  //     const c8* template =
  //       "SPN_INCLUDES := {}\n"
  //       "SPN_LIB_INCLUDES := {}\n"
  //       "SPN_LIBS := {}\n"
  //       "SPN_SYSTEM_LIBS := {}\n"
  //       "SPN_RPATH := {}\n"
  //       "SPN_FLAGS := $(SPN_INCLUDES) $(SPN_LIB_INCLUDES) $(SPN_LIBS) $(SPN_SYSTEM_LIBS) $(SPN_RPATH)\n";
  //     gen.output = sp_format(template,
  //       SP_FMT_STR(gen.include),
  //       SP_FMT_STR(gen.lib_include),
  //       SP_FMT_STR(gen.libs),
  //       SP_FMT_STR(gen.system_libs),
  //       SP_FMT_STR(gen.rpath)
  //     );
  //     break;
  //   }
  //
  //   case SPN_GEN_KIND_CMAKE: {
  //     gen.file_name = SP_LIT("spn.cmake");
  //     const c8* template =
  //       "set(SPN_INCLUDES \"{}\")\n"
  //       "set(SPN_LIB_INCLUDES \"{}\")\n"
  //       "set(SPN_LIBS \"{}\")\n"
  //       "set(SPN_SYSTEM_LIBS \"{}\")\n"
  //       "set(SPN_RPATH \"{}\")\n"
  //       "set(SPN_FLAGS \"$";
  //     sp_str_t template_end = sp_str_lit(
  //       "{SPN_INCLUDES} $"
  //       "{SPN_LIB_INCLUDES} $"
  //       "{SPN_LIBS} $"
  //       "{SPN_SYSTEM_LIBS} $"
  //       "{SPN_RPATH}\")\n");
  //     sp_str_t formatted = sp_format(template,
  //       SP_FMT_STR(gen.include),
  //       SP_FMT_STR(gen.lib_include),
  //       SP_FMT_STR(gen.libs),
  //       SP_FMT_STR(gen.system_libs),
  //       SP_FMT_STR(gen.rpath)
  //     );
  //     gen.output = sp_str_concat(spn_allocator, formatted, template_end);
  //     break;
  //   }
  //
  //   case SPN_GEN_KIND_PKGCONFIG: {
  //     gen.file_name = SP_LIT("spn.pc");
  //     const c8* template =
  //       "Name: {}\n"
  //       "Description: spn-managed dependencies for {}\n"
  //       "Version: {}.{}.{}\n"
  //       "Cflags: {} {}\n"
  //       "Libs: {} {} {}\n";
  //     gen.output = sp_format(template,
  //       SP_FMT_STR(app->package.name),
  //       SP_FMT_STR(app->package.name),
  //       SP_FMT_U32(app->package.version.major),
  //       SP_FMT_U32(app->package.version.minor),
  //       SP_FMT_U32(app->package.version.patch),
  //       SP_FMT_STR(gen.include),
  //       SP_FMT_STR(gen.lib_include),
  //       SP_FMT_STR(gen.libs),
  //       SP_FMT_STR(gen.system_libs),
  //       SP_FMT_STR(gen.rpath)
  //     );
  //     break;
  //   }
  //
  //   default: {
  //     SP_UNREACHABLE();
  //   }
  // }
  //
  // if (sp_str_valid(command->path)) {
  //   sp_str_t destination = sp_fs_normalize_path(spn_allocator, command->path);
  //   if (!sp_str_starts_with(destination, sp_str_lit("/"))) {
  //     destination = sp_fs_join_path(spn_allocator, spn.paths.cwd, destination);
  //   }
  //   sp_fs_create_dir(destination);
  //
  //   sp_str_t file_path = sp_fs_join_path(spn_allocator, destination, gen.file_name);
  //   sp_io_writer_t file = sp_io_writer_from_file(file_path, SP_IO_WRITE_MODE_OVERWRITE);
  //   if (sp_io_write_str(&file, gen.output, SP_NULLPTR) != gen.output.len) {
  //     SP_FATAL("Failed to write {}", SP_FMT_STR(file_path));
  //   }
  //   sp_io_writer_close(&file);
  //
  //   spn_event_buffer_push_ctx(spn.events, &spn_session_find_root(&app->session)->ctx, (spn_build_event_t) {
  //     .kind = SPN_EVENT_GENERATE,
  //     .generate.path = file_path
  //   });
  // }
  // else {
  //   // Write directly to stdout without treating as format string
  //   sp_io_write_str(&spn.logger.out, gen.output, SP_NULLPTR);
  // }
  //
  return SPN_TASK_DONE;
}
