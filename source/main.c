// where do we put the dependency config? you want the default one that gets pulled from the TOML file,
// but also need a way to add options for debug/rel builds
//
// --debug or --release is just an alias for options.build.optimization
// --shared or --static for options.build.library
//
// spn_build_context_t rn is for one dep, need something for all deps. just a container for dep + final options
//
// [deps.sdl.profiles]
// debug.optimization = 'debug'
//

#define SP_IMPLEMENTATION
#define SP_OS_BACKEND_SDL
#include "sp/sp.h"

#define ARGPARSE_IMPLEMENTATION
#include "argparse/argparse.h"

#include "toml/toml.h"

///////////
// SHELL //
///////////
typedef struct {
  sp_str_t output;
  s32 return_code;
} spn_sh_process_result_t;

typedef struct {
  SDL_Process* process;
  spn_sh_process_result_t result;
} spn_sh_process_context_t;

#define SPN_SH(...) SDL_CreateProcess((const c8* []) { __VA_ARGS__, SP_NULLPTR }, SP_SDL_PIPE_STDIO)

spn_sh_process_result_t spn_sh_read_process(SDL_Process* process);
sp_str_t                spn_sh_git_find_head(sp_str_t repo);


/////////
// CLI //
/////////
typedef struct {
  sp_str_t package;
} spn_cli_add_t;

typedef struct {
} spn_cli_init_t;

typedef enum {
  SPN_FLAG_GNU_INCLUDES,
  SPN_FLAG_GNU_LIBS,
} spn_cli_flag_kind_t;

typedef struct {
  spn_cli_flag_kind_t kind;
} spn_cli_flags_t;

typedef struct {
  u32 num_args;
  const c8** args;

  const c8* project_directory;

  spn_cli_add_t add;
  spn_cli_init_t init;
  spn_cli_flags_t flags;
} spn_cli_t;

void spn_cli_command_add(spn_cli_t* cli);
void spn_cli_command_init(spn_cli_t* cli);
void spn_cli_command_nuke(spn_cli_t* cli);
void spn_cli_command_flags(spn_cli_t* cli);


//////////////////
// DEPENDENCIES //
//////////////////
typedef enum {
  SPN_DEPENDENCY_KIND_BUILTIN,
  SPN_DEPENDENCY_KIND_USER,
} spn_dep_kind_t;

typedef enum {
  SPN_BUILD_KIND_DEBUG,
  SPN_BUILD_KIND_RELEASE,
} spn_dep_build_kind_t;

typedef struct {
  sp_str_t std_out;
  sp_str_t std_err;
  sp_str_t std_in;
  sp_str_t source;
  sp_str_t build;
  sp_str_t store;
  sp_str_t include;
  sp_str_t bin;
  sp_str_t recipe;
} spn_dep_build_paths_t;

typedef struct {
  sp_str_t key;
  toml_table_t* table;
} spn_dep_spec_parse_entry_T;

typedef struct {
  sp_str_t key;
  sp_str_t value;
} spn_dep_option_t;

typedef struct {
  spn_dep_kind_t kind;
  sp_str_t name;
  toml_table_t* toml;
} spn_dep_spec_t;

typedef struct {
  spn_dep_spec_t* dep;
  sp_dyn_array(spn_dep_option_t) options;
} spn_dep_profile_t;

typedef struct {
  spn_dep_profile_t profile;
  spn_dep_build_paths_t paths;
  sp_hash_t hash;
  sp_str_t build_id;
  SDL_PropertiesID shell;
  SDL_Environment* environment;
} spn_dep_context_t;

typedef struct {
  sp_dyn_array(spn_dep_context_t) deps;
} spn_build_context_t;

bool                spn_dependency_is_cloned(spn_dep_spec_t* dependency);
sp_str_t            spn_dependency_source_dir(spn_dep_spec_t* dependency);
sp_str_t            spn_dependency_build_dir(spn_dep_spec_t* dependency);
sp_str_t            spn_dependency_store_dir(spn_dep_spec_t* dependency);
sp_str_t            spn_dependency_recipe_file(spn_dep_spec_t* dependency);
void                spn_dependency_build_async(spn_dep_context_t* context);
void                spn_dependency_clone_async(spn_dep_context_t* context);
sp_str_t            spn_dependency_kind_to_str(spn_dep_kind_t kind);
void                spn_dep_context_prepare(spn_dep_context_t* context);
void                spn_dep_context_add_options(spn_dep_context_t* context, toml_table_t* toml);
spn_dep_context_t   spn_dep_context_from_default_profile(spn_dep_spec_t* dep);
void                spn_dep_context_set_env_var(spn_dep_context_t* context, sp_str_t name, sp_str_t value);
void                spn_build_context_prepare(spn_build_context_t* context);
sp_str_t            spn_build_context_make_flag(spn_build_context_t* context, spn_cli_flag_kind_t flag);
spn_build_context_t spn_build_context_from_default_profile();


/////////
// APP //
/////////
typedef struct {
  toml_table_t* toml;
  bool auto_pull_recipes;
} spn_config_t;

void spn_config_read(spn_config_t* config, sp_str_t path);

typedef struct {
  sp_str_t install;
  sp_str_t   executable;
  sp_str_t config;
  sp_str_t   user_toml;
  sp_str_t cache;
  sp_str_t   store;
  sp_str_t   build;
  sp_str_t   source;
  sp_str_t     bootstrap;
  sp_str_t       recipes;
  sp_str_t project;
  sp_str_t   toml;
} spn_paths_t;

typedef struct {
  sp_str_t name;
  sp_dyn_array(spn_dep_spec_t) dependencies;
  toml_table_t* toml;
} spn_project_t;

typedef struct {
  spn_cli_t cli;
  spn_paths_t paths;
  spn_project_t project;
  spn_config_t config;
  sp_dyn_array(spn_dep_spec_t) builtins;
} spn_app_t;

spn_app_t app;

void            spn_app_init(spn_app_t* app, u32 num_args, const c8** args);
spn_dep_spec_t* spn_project_find_dependency(spn_project_t* project, sp_str_t name);
bool            spn_project_write(spn_project_t* project, sp_str_t path);
bool            spn_project_read(spn_project_t* project, sp_str_t path);
void            spn_project_build(spn_project_t* project);

/////////////////
// TOML WRITER //
/////////////////
typedef struct {
  sp_str_builder_t builder;
  s32 indent_level;
} sp_toml_writer_t;

void     sp_toml_writer_add_header(sp_toml_writer_t* writer, sp_str_t name);
void     sp_toml_writer_add_string(sp_toml_writer_t* writer, sp_str_t key, sp_str_t value);
void     sp_toml_writer_add_s32(sp_toml_writer_t* writer, sp_str_t key, s32 value);
void     sp_toml_writer_add_bool(sp_toml_writer_t* writer, sp_str_t key, bool value);
void     sp_toml_writer_new_line(sp_toml_writer_t* writer);
sp_str_t sp_toml_writer_write(sp_toml_writer_t* writer);

////////////////////
// IMPLEMENTATION //
////////////////////
void sp_toml_writer_new_line(sp_toml_writer_t* writer) {
  sp_str_builder_new_line(&writer->builder);
}

void sp_toml_writer_add_header(sp_toml_writer_t* writer, sp_str_t name) {
  sp_str_builder_append_fmt(&writer->builder, SP_LIT("[{}]"), SP_FMT_STR(name));
  sp_str_builder_new_line(&writer->builder);
}

void sp_toml_writer_add_string(sp_toml_writer_t* writer, sp_str_t key, sp_str_t value) {
  sp_str_builder_append_fmt(&writer->builder, SP_LIT("{} = {}"), SP_FMT_STR(key), SP_FMT_QUOTED_STR(value));
  sp_str_builder_new_line(&writer->builder);
}

void sp_toml_writer_add_s32(sp_toml_writer_t* writer, sp_str_t key, s32 value) {
  sp_str_builder_append_fmt(&writer->builder, SP_LIT("{} = {}"), SP_FMT_STR(key), SP_FMT_S32(value));
  sp_str_builder_new_line(&writer->builder);
}

void sp_toml_writer_add_bool(sp_toml_writer_t* writer, sp_str_t key, bool value) {
  sp_str_builder_append_fmt(&writer->builder, SP_LIT("{} = {}"), SP_FMT_STR(key), value ? SP_FMT_CSTR("true") : SP_FMT_CSTR("false"));
  sp_str_builder_new_line(&writer->builder);
}

sp_str_t sp_toml_writer_write(sp_toml_writer_t* writer) {
  return sp_str_builder_write(&writer->builder);
}

void spn_cli_command_add(spn_cli_t* cli) {
  spn_cli_add_t* add = &cli->add;

  struct argparse argparse;
  argparse_init(
    &argparse,
    (struct argparse_option []) {
      OPT_HELP(),
      OPT_STRING('p', "package", &add->package, SP_NULLPTR, SP_NULLPTR),
      OPT_END()
    },
    (const c8* const []) {
      "spn add [options] package",
      SP_NULLPTR
    },
    SP_NULL
  );
  cli->num_args = argparse_parse(&argparse, cli->num_args, cli->args);

  if (!cli->num_args) {
    argparse_usage(&argparse);
    exit(0);
  }

  add->package = SP_CSTR(cli->args[0]);

  // Read spn.toml
  if (!sp_os_does_path_exist(app.paths.toml)) {
    SP_FATAL("Expected project TOML file at {}, but it did not exist", SP_FMT_STR(app.paths.toml));
  }

  if (!spn_project_read(&app.project, app.paths.toml)) {
    SP_FATAL("Failed to read project TOML file at {}", SP_FMT_STR(app.paths.toml));
  }

  // Locate the dependency from our recipes
  spn_dep_spec_t* dependency = SP_NULLPTR;
  sp_dyn_array_for(app.builtins, index) {
    spn_dep_spec_t* builtin = &app.builtins[index];
    if (sp_str_equal(builtin->name, add->package)) {
      dependency = builtin;
    }
  }

  if (!dependency) {
    SP_FATAL("Could not find {} in available dependencies", SP_FMT_QUOTED_STR(add->package));
  }

  // Add the dependency to spn.toml if needed
  if (spn_project_find_dependency(&app.project, dependency->name)) {
    SP_LOG("{} is already in defined in {}", SP_FMT_STR(dependency->name), SP_FMT_STR(app.paths.toml));
  }
  else {
    sp_dyn_array_push(app.project.dependencies, *dependency);

    if (!spn_project_write(&app.project, app.paths.toml)) {
      SP_FATAL("Failed to write project TOML file");
    }

    SP_LOG("Added {} to {}", SP_FMT_STR(dependency->name), SP_FMT_STR(app.paths.toml));
  }
}

void spn_cli_command_init(spn_cli_t* cli) {
  struct argparse argparse;
  argparse_init(
    &argparse,
    (struct argparse_option []) {
      OPT_HELP(),
      OPT_END()
    },
    (const c8* const []) {
      "spn init",
      SP_NULLPTR
    },
    SP_NULL
  );
  cli->num_args = argparse_parse(&argparse, cli->num_args, cli->args);

  app.project = (spn_project_t){
    .name = sp_os_extract_file_name(app.paths.project)
  };

  if (!spn_project_write(&app.project, app.paths.toml)) {
    SP_FATAL("Failed to write project TOML file");
  }

  SP_LOG("Initialized project {} in spn.toml", SP_FMT_QUOTED_STR(app.project.name));
}

void spn_cli_command_nuke(spn_cli_t* cli) {
  sp_os_remove_directory(app.paths.cache);
  sp_os_remove_file(app.paths.toml);
}

void spn_cli_command_flags(spn_cli_t* cli) {
  struct argparse argparse;
  argparse_init(
    &argparse,
    (struct argparse_option []) {
      OPT_HELP(),
      OPT_END()
    },
    (const c8* const []) {
      "spn flags (include, bin)",
      SP_NULLPTR
    },
    SP_NULL
  );
  cli->num_args = argparse_parse(&argparse, cli->num_args, cli->args);

  if (!cli->num_args) {
    argparse_usage(&argparse);
    SP_EXIT_SUCCESS();
  }

  sp_str_t flag_str = SP_CSTR(cli->args[0]);
  if (sp_str_equal_cstr(flag_str, "include")) {
    cli->flags.kind = SPN_FLAG_GNU_INCLUDES;
  }
  else if (sp_str_equal_cstr(flag_str, "libs")) {
    cli->flags.kind = SPN_FLAG_GNU_LIBS;
  }
  else {
    SP_FATAL("Unknown flag {}; options are [include, libs]", SP_FMT_QUOTED_STR(flag_str));
  }

  spn_build_context_t context = spn_build_context_from_default_profile();
  spn_build_context_prepare(&context);

  sp_str_t flag = spn_build_context_make_flag(&context, cli->flags.kind);
  printf("%s", sp_str_to_cstr(flag));
}

sp_str_t spn_build_context_make_flag(spn_build_context_t* context, spn_cli_flag_kind_t flag) {
  sp_str_builder_t builder = SP_ZERO_INITIALIZE();

  sp_dyn_array_for(context->deps, index) {
    spn_dep_context_t* dep = context->deps + index;
    sp_str_t str;

    switch (flag) {
      case SPN_FLAG_GNU_INCLUDES: { str = sp_fmt_c8("-I{}", SP_FMT_STR(dep->paths.include)); break; }
      case SPN_FLAG_GNU_LIBS:     { str = sp_fmt_c8("-L{}", SP_FMT_STR(dep->paths.include)); break; }
      default:                    { SP_UNREACHABLE_CASE(); }
    }

    sp_str_builder_append(&builder, str);

    if (index != SP_SIZE_TO_INDEX(sp_dyn_array_size(context->deps))) {
      sp_str_builder_append_c8(&builder, ' ');
    }
  }

  return sp_str_builder_write(&builder);
}


spn_build_context_t spn_build_context_from_default_profile() {
  spn_build_context_t context;

  sp_dyn_array_for(app.project.dependencies, index) {
    spn_dep_context_t dep = spn_dep_context_from_default_profile(app.project.dependencies + index);
    sp_dyn_array_push(context.deps, dep);
  }

  return context;
}

spn_dep_context_t spn_dep_context_from_default_profile(spn_dep_spec_t* dep) {
  spn_dep_context_t context = {
    .profile = {
      .dep = dep,
      .options = SP_NULLPTR
    },
  };

  toml_table_t* deps = toml_table_table(app.project.toml, "deps");
  SP_ASSERT_FMT(deps, "Malformed project at {}: Parsing dependency {}, but no [deps] header", SP_FMT_STR(app.paths.toml), SP_FMT_STR(dep->name));

  toml_table_t* toml = toml_table_table(deps, sp_str_to_cstr(dep->name));
  SP_ASSERT_FMT(toml, "Malformed project at {}: Parsing dependency {}, but no header", SP_FMT_STR(app.paths.toml), SP_FMT_STR(dep->name));

  toml_table_t* options = toml_table_table(toml, "options");
  if (!options) return context;

  spn_dep_context_add_options(&context, options);
  return context;
}

void spn_dep_context_add_options(spn_dep_context_t* context, toml_table_t* options) {
  spn_dep_profile_t* config = &context->profile;
  sp_dyn_array(spn_dep_spec_parse_entry_T) entries = SP_NULLPTR;

  sp_dyn_array_push(entries, ((spn_dep_spec_parse_entry_T) {
    .key = SP_LIT(""),
    .table = options
  }));

  while (sp_dyn_array_size(entries)) {
    spn_dep_spec_parse_entry_T entry = *sp_dyn_array_back(entries);
    sp_dyn_array_pop(entries);

    for (u32 index = 0; index < toml_table_len(entry.table); index++) {
      s32 key_len;
      const c8* key_data = toml_table_key(entry.table, index, &key_len);
      sp_str_t key = SP_CSTR(key_data);

      sp_str_t full_key = SP_ZERO_INITIALIZE();
      if (entry.key.len) {
        full_key = sp_str_join(entry.key, key, SP_LIT("."));
      }
      else {
        full_key = sp_str_copy(key);
      }

      toml_table_t* table = toml_table_table(entry.table, key_data);
      toml_array_t* array = toml_table_array(options, key_data);

      if (table) {
        sp_dyn_array_push(entries, ((spn_dep_spec_parse_entry_T) {
          .key = full_key,
          .table = table
        }));
      }
      else if (array) {
        SP_LOG("array: {}", SP_FMT_STR(full_key));
      }
      else {
        spn_dep_option_t option = SP_ZERO_INITIALIZE();
        option.key = sp_str_copy(full_key);
        option.key = sp_str_to_upper(option.key);
        option.key = sp_str_replace(option.key, '.', '_');
        option.value = sp_str_copy_cstr(toml_table_unparsed(entry.table, key_data));
        sp_dyn_array_push(config->options, option);
      }
    }
  }

  sp_str_builder_t builder = SP_ZERO_INITIALIZE();
  sp_str_t kind = spn_dependency_kind_to_str(config->dep->kind);
  sp_str_builder_append_fmt_c8(&builder, "{} ({})", SP_FMT_STR(config->dep->name), SP_FMT_STR(kind));
  sp_dyn_array_for(config->options, index) {
    spn_dep_option_t* option = &config->options[index];

  }


}

bool spn_dependency_is_cloned(spn_dep_spec_t* dependency) {
  sp_str_t source = spn_dependency_source_dir(dependency);
  return sp_os_does_path_exist(source);
}

sp_str_t spn_dependency_source_dir(spn_dep_spec_t* dependency) {
  return sp_os_join_path(app.paths.source, dependency->name);
}

sp_str_t spn_dependency_build_dir(spn_dep_spec_t* dependency) {
  return sp_os_join_path(app.paths.build, dependency->name);
}

sp_str_t spn_dependency_store_dir(spn_dep_spec_t* dependency) {
  return sp_os_join_path(app.paths.store, dependency->name);
}

sp_str_t spn_dependency_recipe_file(spn_dep_spec_t* dependency) {
  return sp_fmt_c8("{}.mk", SP_FMT_STR(dependency->name));
}

sp_str_t spn_dependency_kind_to_str(spn_dep_kind_t kind) {
  switch (kind) {
    case SPN_DEPENDENCY_KIND_BUILTIN: return SP_LIT("builtin");
    case SPN_DEPENDENCY_KIND_USER:    return SP_LIT("user");
    default:                          SP_FATAL("Unknown dependency kind ({})", SP_FMT_U32(kind));
  }
}

spn_sh_process_result_t spn_sh_read_process(SDL_Process* process) {
  spn_sh_process_result_t result;
  sp_size_t len;

  result.output.data = (c8*)SDL_ReadProcess(process, &len, &result.return_code);
  result.output.len = (u32)len;
  return result;
}

sp_str_t spn_sh_git_find_head(sp_str_t repo) {
  SDL_Process* process = SPN_SH("git", "-C", sp_str_to_cstr(repo), "rev-parse", "--abbrev-ref", "HEAD");
  spn_sh_process_result_t result = spn_sh_read_process(process);
  SDL_DestroyProcess(process);

  return sp_str_strip_right(result.output);
}

void spn_build_context_prepare(spn_build_context_t* context) {
  sp_dyn_array_for(context->deps, index) {
    spn_dep_context_t* dep = context->deps + index;
    spn_dep_context_prepare(dep);
  }
}

void spn_dep_context_set_env_var(spn_dep_context_t* context, sp_str_t name, sp_str_t value) {
  if (!SDL_SetEnvironmentVariable(context->environment, sp_str_to_cstr(name), sp_str_to_cstr(value), SP_SDL_OVERWRITE_ENV_VAR)) {
    SP_FATAL("Failed to set {}={} in build context for {}", SP_FMT_STR(name), SP_FMT_STR(value), SP_FMT_STR(context->profile.dep->name));
  }
}

void spn_dep_context_prepare(spn_dep_context_t* context) {
  spn_dep_profile_t* config = &context->profile;

  sp_dyn_array(sp_hash_t) hashes = SP_NULLPTR;
  sp_dyn_array_for(config->options, index) {
    spn_dep_option_t* option = &config->options[index];
    sp_dyn_array_push(hashes, sp_hash_str(option->key));
    sp_dyn_array_push(hashes, sp_hash_str(option->value));
  }

  context->hash = sp_hash_combine(hashes, sp_dyn_array_size(hashes));
  context->build_id = sp_fmt_c8("{}", SP_FMT_HASH(context->hash));

  sp_str_t build = spn_dependency_build_dir(context->profile.dep);
  sp_str_t store = spn_dependency_store_dir(context->profile.dep);
  context->paths.source = spn_dependency_source_dir(context->profile.dep);
  context->paths.build = sp_os_join_path(build, context->build_id);
  context->paths.store = sp_os_join_path(store, context->build_id);
  context->paths.include = sp_os_join_path(context->paths.store, SP_LIT("include"));
  context->paths.bin = sp_os_join_path(context->paths.store, SP_LIT("bin"));
  context->paths.recipe = spn_dependency_recipe_file(context->profile.dep);
  context->paths.std_out = sp_os_join_path(context->paths.build, SP_LIT("build.stdout"));
  context->paths.std_err = sp_os_join_path(context->paths.build, SP_LIT("build.stderr"));
  context->paths.std_in  = sp_os_join_path(context->paths.build, SP_LIT("build.stdin"));

  context->environment = SDL_CreateEnvironment(SP_SDL_INHERIT_ENVIRONMENT);
  spn_dep_context_set_env_var(context, SP_LIT("SPN_DIR_PROJECT"), context->paths.source);
  spn_dep_context_set_env_var(context, SP_LIT("SPN_DIR_BUILD"), context->paths.build);
  spn_dep_context_set_env_var(context, SP_LIT("SPN_DIR_STORE_INCLUDE"), context->paths.include);
  spn_dep_context_set_env_var(context, SP_LIT("SPN_DIR_STORE_BIN"), context->paths.bin);
  sp_dyn_array_for(context->profile.options, index) {
    spn_dep_option_t* option = &context->profile.options[index];
    sp_str_t key = sp_str_concat(SP_LIT("SPN_OPT_"), option->key);
    spn_dep_context_set_env_var(context, key, option->value);
  }

  context->shell = SDL_CreateProperties();
  SDL_SetPointerProperty(context->shell, SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER, context->environment);
  SDL_SetNumberProperty(context->shell, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
  SDL_SetNumberProperty(context->shell, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
  SDL_SetNumberProperty(context->shell, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_APP);
}

void spn_dependency_clone_async(spn_dep_context_t* context) {
  const c8* args [] = {
    "make",
    "--directory", sp_str_to_cstr(app.paths.recipes),
    "--makefile", sp_str_to_cstr(context->paths.recipe),
    "spn-clone",
    SP_NULLPTR
  };

  SDL_PropertiesID shell = SDL_CreateProperties();
  SDL_CopyProperties(context->shell, shell);
  SDL_SetPointerProperty(shell, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void*)args);

  SDL_Process* process = SDL_CreateProcessWithProperties(shell);
  if (!process) {
    const c8* sdl = SDL_GetError();
    SP_FATAL("Failed to create process to run clone target for {}: {}", SP_FMT_STR(context->paths.recipe), SP_FMT_CSTR(sdl));
  }

  spn_sh_process_result_t result = spn_sh_read_process(process);
  if (result.return_code) {
    SP_FATAL("Failed to clone for {}: {}", SP_FMT_STR(context->paths.recipe), SP_FMT_STR(result.output));
  }

  SDL_DestroyProcess(process);


}

void spn_dependency_build_async(spn_dep_context_t* context) {
  sp_os_create_directory(context->paths.build);
  sp_os_create_directory(context->paths.store);
  sp_os_create_directory(context->paths.include);
  sp_os_create_directory(context->paths.bin);

  sp_str_t target = SP_LIT("spn-build");
  const c8* args [] = {
    "make",
    "--directory", sp_str_to_cstr(app.paths.recipes),
    "--makefile", sp_str_to_cstr(context->paths.recipe),
    sp_str_to_cstr(target),
    SP_NULLPTR
  };

  SDL_PropertiesID shell = SDL_CreateProperties();
  SDL_CopyProperties(context->shell, shell);
  SDL_SetPointerProperty(shell, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void*)args);

  SDL_Process* process = SDL_CreateProcessWithProperties(shell);
  if (!process) {
    const c8* sdl = SDL_GetError();
    SP_FATAL("Failed to create process: {}", SP_FMT_CSTR(sdl));
  }

  spn_sh_process_result_t result = spn_sh_read_process(process);
  if (result.return_code) {
    SP_FATAL("Failed to build {}:\n{}", SP_FMT_STR(context->paths.recipe), SP_FMT_STR(result.output));
  }

  SDL_DestroyProcess(process);

  /*
  pid_t pid = fork();
  if (pid == 0) {
      setsid();

      close(STDIN_FILENO);
      close(STDOUT_FILENO);
      close(STDERR_FILENO);

      s32 std_out = open()
    // redirect to /dev/null or a log file
      int fd = open("/dev/null", O_RDWR);
      dup2(fd, STDIN_FILENO);
      dup2(fd, STDOUT_FILENO);
      dup2(fd, STDERR_FILENO);

      // actually build
      char* build_dir = get_build_dir(dep_name);
      char* recipe = get_recipe_path(dep_name);

      execlp("make", "make", "-f", recipe, "spn-build", NULL);
      exit(1);  // if exec fails
    // */
}

void spn_app_init(spn_app_t* app, u32 num_args, const c8** args) {
  // Parse the command line
  spn_cli_t* cli = &app->cli;

  struct argparse_option options [] = {
    OPT_HELP(),
    OPT_STRING('d', "project-dir", &cli->project_directory, "specify the directory containing spn.toml", SP_NULLPTR),
    OPT_END(),
  };

  const c8* const usages [] = {
    "subcommands [options] [cmd]",
    SP_NULLPTR
  };

  struct argparse argparse;
  argparse_init(&argparse, options, usages, ARGPARSE_STOP_AT_NON_OPTION);

  cli->args = args;
  cli->num_args = num_args;
  cli->num_args = argparse_parse(&argparse, cli->num_args, cli->args);

  if (!cli->num_args) {
    argparse_usage(&argparse);
    exit(0);
  }

  // Build a static list of paths
  app->paths.executable = sp_os_get_executable_path();
  app->paths.install = sp_os_parent_path(app->paths.executable);

  // ./spn.toml
  if (app->cli.project_directory) {
    app->paths.project = sp_str_copy_cstr(app->cli.project_directory);
  }
  else {
    c8* working_directory = SDL_GetCurrentDirectory();
    app->paths.project = sp_os_canonicalize_path(SP_CSTR(working_directory));
    SDL_free(working_directory);
  }
  app->paths.toml = sp_os_join_path(app->paths.project, SP_LIT("spn.toml"));

  // ~/.config/spn and ~/.cache/spn
  const c8* xdg_cache = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "XDG_CACHE_HOME");
  const c8* xdg_config = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "XDG_CONFIG_HOME");
  const c8* home = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "HOME");

  if (xdg_config) {
    app->paths.config = sp_os_join_path(SP_CSTR(xdg_config), SP_LIT("spn"));
  }
  else if (home) {
    app->paths.config = sp_os_join_path(SP_CSTR(home), SP_LIT(".config/spn"));
  }
  else {
    SP_FATAL("No $XDG_CONFIG_HOME? No $HOME? Someone fucked up here and surely it was me.");
  }

  app->paths.user_toml = sp_os_join_path(app->paths.config, SP_LIT("spn.toml"));

  if (xdg_cache) {
    app->paths.cache = sp_str_join(SP_CSTR(xdg_cache), SP_LIT("spn"), SP_LIT("/"));
  }
  else if (home) {
    app->paths.cache = sp_str_join(SP_CSTR(home), SP_LIT(".cache/spn"), SP_LIT("/"));
  }
  else {
    SP_FATAL("No $XDG_CACHE_HOME? No $HOME? Someone fucked up here and surely it was me.");
  }

  app->paths.source = sp_os_join_path(app->paths.cache, SP_LIT("source"));
  app->paths.build = sp_os_join_path(app->paths.cache, SP_LIT("build"));
  app->paths.store = sp_os_join_path(app->paths.cache, SP_LIT("store"));
  app->paths.bootstrap = sp_os_join_path(app->paths.source, SP_LIT("spn"));
  app->paths.recipes = sp_os_join_path(app->paths.bootstrap, SP_LIT("asset/recipes"));

  sp_os_create_directory(app->paths.cache);
  sp_os_create_directory(app->paths.source);
  sp_os_create_directory(app->paths.build);

  spn_config_read(&app->config, app->paths.user_toml);

  // Bootstrap spn
  const c8* url = "https://github.com/spaderthomas/spn.git";
  const c8* spn = sp_str_to_cstr(app->paths.bootstrap);
  if (!sp_os_does_path_exist(app->paths.bootstrap)) {
    SP_LOG("Cloning recipe repository from {} to {}", SP_FMT_CSTR(url), SP_FMT_CSTR(spn));

    SDL_Process* process = SPN_SH("git", "clone", url, spn);
    spn_sh_process_result_t result = spn_sh_read_process(process);
    if (result.return_code) {
      SP_FATAL("Failed to clone spn recipe sources from {} to {}", SP_FMT_CSTR(url), SP_FMT_CSTR(spn));
    }
  }
  else {
    spn_sh_process_context_t fetch = SP_ZERO_INITIALIZE();
    fetch.process = SPN_SH("git", "-C", spn, "fetch", "--quiet");
    fetch.result = spn_sh_read_process(fetch.process);

    spn_sh_process_context_t rev_list = SP_ZERO_INITIALIZE();
    rev_list.process = SPN_SH("git", "-C", spn, "rev-list", "HEAD..@{u}", "--count");
    rev_list.result = spn_sh_read_process(rev_list.process);
    rev_list.result.output = sp_str_strip_right(rev_list.result.output);
    if (sp_parse_u32(rev_list.result.output)) {
      SP_LOG("spn has updates to recipes; pull? (y)es, (n)o, (a)lways");

      #if 0
      c8 response = 0;
      while (true) {
        c8 c = (c8)getchar();
        switch (c) {
          case EOF: {}
          default: response = c;
        }
      }
      #endif

      SP_LOG("Cloning recipe repository from {} to {}", SP_FMT_CSTR(url), SP_FMT_CSTR(spn));
      SDL_Process* process = SPN_SH("git", "-C", spn, "pull");
      spn_sh_process_result_t result = spn_sh_read_process(process);
      if (result.return_code) {
        SP_FATAL("Failed to clone spn recipe sources from {} to {}", SP_FMT_CSTR(url), SP_FMT_CSTR(spn));
      }
    }

  }

  // Make a list of all recipes that are available
  sp_os_directory_entry_list_t entries = sp_os_scan_directory(app->paths.recipes);
  for (sp_os_directory_entry_t* entry = entries.data; entry < entries.data + entries.count; entry++) {
    spn_dep_spec_t spec = {
      .kind = SPN_DEPENDENCY_KIND_BUILTIN,
      .name = sp_os_extract_stem(entry->file_name),
    };
    sp_dyn_array_push(app->builtins, spec);
  }

  sp_str_t head = spn_sh_git_find_head(app->paths.bootstrap);

  // Load the project
  if (!sp_os_does_path_exist(app->paths.toml)) {
    SP_FATAL("Expected project TOML file at {}, but it did not exist", SP_FMT_STR(app->paths.toml));
  }

  if (!spn_project_read(&app->project, app->paths.toml)) {
    SP_FATAL("Failed to read project TOML file at {}", SP_FMT_STR(app->paths.toml));
  }
}

void spn_app_run(spn_app_t* app) {
  spn_cli_t* cli = &app->cli;

  if (!cli->num_args || !cli->args) {
    SP_ASSERT(false);
  }
  else if (sp_cstr_equal("add", cli->args[0])) {
    spn_cli_command_add(cli);
  }
  else if (sp_cstr_equal("init", cli->args[0])) {
    spn_cli_command_init(cli);
  }
  else if (sp_cstr_equal("nuke", cli->args[0])) {
    spn_cli_command_nuke(cli);
  }
  else if (sp_cstr_equal("flags", cli->args[0])) {
    spn_cli_command_flags(cli);
  }
  else if (sp_cstr_equal("build", cli->args[0])) {
    spn_project_build(&app->project);
  }
}

#define TOML_READ_BOOL(table, var, key) \
    do { \
      toml_value_t value = toml_table_bool((table), (key)); \
      if (value.ok) { \
        (var) = value.u.b; \
      } \
    } while (0)

void spn_config_read(spn_config_t* config, sp_str_t path) {
  *config = (spn_config_t) {
    .auto_pull_recipes = false,
    .toml = SP_NULLPTR
  };

  size_t file_size;
  c8* file_data = (c8*)SDL_LoadFile(sp_str_to_cstr(path), &file_size);
  if (!file_data) return;

  c8 toml_error [256] = SP_ZERO_INITIALIZE();
  config->toml = toml_parse(file_data, toml_error, sizeof(toml_error));
  if (!config->toml) {
    SP_FATAL("Failed to parse {}: {}", SP_FMT_STR(path), SP_FMT_CSTR(toml_error));
  }

  toml_table_t* options = toml_table_table(config->toml, "options");
  if (options) {
    TOML_READ_BOOL(options, config->auto_pull_recipes, "auto_pull_recipes");
  }
}

spn_dep_spec_t* spn_project_find_dependency(spn_project_t* project, sp_str_t name) {
  SP_ASSERT(project);
  if (!project->dependencies) return false;

  sp_dyn_array_for(project->dependencies, index) {
    spn_dep_spec_t* dependency = &project->dependencies[index];
    if (sp_str_equal(dependency->name, name)) {
      return dependency;
    }
  }

  return SP_NULLPTR;
}

bool spn_project_write(spn_project_t* project, sp_str_t path) {
  sp_toml_writer_t writer = SP_ZERO_INITIALIZE();

  sp_toml_writer_add_header(&writer, sp_str_lit("project"));
  sp_toml_writer_add_string(&writer, sp_str_lit("name"), project->name);

  sp_toml_writer_new_line(&writer);

  if (project->dependencies) {
    sp_dyn_array_for(project->dependencies, i) {
      spn_dep_spec_t* dep = &project->dependencies[i];

      sp_toml_writer_add_header(&writer, sp_fmt_c8("deps.{}", SP_FMT_STR(dep->name)));
      sp_toml_writer_add_string(&writer, SP_LIT("name"), dep->name);
    }
  }

  sp_str_t content = sp_toml_writer_write(&writer);
  return SDL_SaveFile(sp_str_to_cstr(path), content.data, content.len);
}

bool spn_project_read(spn_project_t* project, sp_str_t path) {
  size_t file_size;
  void* file_data = SDL_LoadFile(sp_str_to_cstr(path), &file_size);

  if (!file_data) {
    SP_FATAL("Failed to read project file at {}", SP_FMT_STR(path));
  }

  c8 toml_error [256] = SP_ZERO_INITIALIZE();
  project->toml = toml_parse((c8*)file_data, toml_error, sizeof(toml_error));
  SP_ASSERT_FMT(project->toml, "Failed to read project file at {}: {}", SP_FMT_STR(path), SP_FMT_CSTR(toml_error));

  toml_table_t* project_table = toml_table_table(project->toml, "project");
  SP_ASSERT_FMT(project->toml, "Malformed project file: missing [project]");

  toml_value_t name = toml_table_string(project_table, "name");
  SP_ASSERT_FMT(name.ok, "Malformed project file: missing project.name");
  project->name = sp_str_copy_cstr(name.u.s);

  // Read dependencies
  toml_table_t* deps_table = toml_table_table(project->toml, "deps");
  if (deps_table) {
    for (u32 index = 0; index < toml_table_len(deps_table); index++) {
      s32 len;
      const c8* key = toml_table_key(deps_table, index, &len);

      spn_dep_spec_t dependency = {
        .kind = SPN_DEPENDENCY_KIND_BUILTIN,
        .name = sp_str_copy_cstr_n(key, len),
        .toml = toml_table_table(deps_table, key)
      };

      sp_dyn_array_push(project->dependencies, dependency);
    }
  }

  return true;
}

void spn_project_build(spn_project_t* project) {
  spn_build_context_t context = spn_build_context_from_default_profile();
  spn_build_context_prepare(&context);

  sp_dyn_array_for(context.deps, index) {
    spn_dep_context_t* dep = context.deps + index;
    if (!spn_dependency_is_cloned(dep->profile.dep)) {
      spn_dependency_clone_async(dep);
    }

    spn_dependency_build_async(dep);
  }
}

s32 main(s32 num_args, const c8** args) {
  sp_allocator_malloc_t malloc_allocator = SP_ZERO_INITIALIZE();
  sp_allocator_t allocator = sp_allocator_malloc_init(&malloc_allocator);
  sp_context_push_allocator(&allocator);

  app = SP_ZERO_STRUCT(spn_app_t);
  spn_app_init(&app, num_args, args);
  spn_app_run(&app);

  sp_context_pop();
  return 0;
}
