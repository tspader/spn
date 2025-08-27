#ifndef SPN_H
#define SPN_H

///////////
// SHELL //
///////////
#include "sp.h"
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
  SPN_FLAG_GNU_INCLUDE,
  SPN_FLAG_GNU_LIB_INCLUDE,
  SPN_FLAG_ALL,
} spn_cli_flag_kind_t;

typedef struct {
  spn_cli_flag_kind_t kind;
  const c8* package;
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
void spn_cli_command_clean(spn_cli_t* cli);
void spn_cli_command_flags(spn_cli_t* cli);

//////////////////
// DEPENDENCIES //
//////////////////
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

typedef sp_str_t spn_dep_id_t;

typedef struct {
  spn_dep_id_t id;
  sp_dyn_array(spn_dep_option_t) options;
  spn_dep_build_paths_t paths;
  sp_hash_t hash;
  sp_str_t build_id;
  SDL_PropertiesID shell;
  SDL_Environment* environment;
  bool is_cloned;
  bool is_built;
} spn_dep_context_t;

typedef struct {
  sp_dyn_array(spn_dep_context_t) deps;
} spn_build_context_t;

sp_str_t            spn_dependency_source_dir(sp_str_t name);
sp_str_t            spn_dependency_build_dir(sp_str_t name);
sp_str_t            spn_dependency_store_dir(sp_str_t name);
sp_str_t            spn_dependency_recipe_file(sp_str_t name);
spn_dep_context_t*  spn_dep_find(sp_str_t name);
sp_str_t            spn_dep_option_env_name(spn_dep_option_t* option);
void                spn_dep_context_build_async(spn_dep_context_t* context);
void                spn_dep_context_clone_async(spn_dep_context_t* context);
void                spn_dep_context_prepare(spn_dep_context_t* context);
void                spn_dep_context_add_options(spn_dep_context_t* context, toml_table_t* toml);
spn_dep_context_t   spn_dep_context_from_default_profile(sp_str_t name);
void                spn_dep_context_set_env_var(spn_dep_context_t* context, sp_str_t name, sp_str_t value);
bool                spn_dep_context_is_cloned(spn_dep_context_t* context);
bool                spn_dep_context_is_built(spn_dep_context_t* context);
void                spn_build_context_prepare(spn_build_context_t* context);
sp_str_t            spn_build_context_make_flag(spn_build_context_t* context, spn_cli_flag_kind_t flag);
spn_build_context_t spn_build_context_from_default_profile();

/////////
// APP //
/////////
typedef struct {
  toml_table_t* toml;
  bool auto_pull_recipes;
  sp_str_t cache_override;
  sp_dyn_array(sp_str_t) additional_recipe_dirs;
  bool builtin_recipes_enabled;
} spn_config_t;

void spn_config_read(spn_config_t* config, sp_str_t path);
void spn_config_read_from_string(spn_config_t* config, sp_str_t toml_content);

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
  sp_dyn_array(spn_dep_id_t) dependencies;
  toml_table_t* toml;
} spn_project_t;

typedef struct {
  spn_cli_t cli;
  spn_paths_t paths;
  spn_project_t project;
  spn_config_t config;
  sp_dyn_array(spn_dep_id_t) builtins;
} spn_app_t;

extern spn_app_t app;

void            spn_app_init(spn_app_t* app, u32 num_args, const c8** args);
void            spn_app_run(spn_app_t* app);
spn_dep_context_t* spn_project_find_dependency(spn_project_t* project, sp_str_t name);
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
void     sp_toml_writer_add_string_array(sp_toml_writer_t* writer, sp_str_t key, sp_dyn_array(sp_str_t) values);
void     sp_toml_writer_new_line(sp_toml_writer_t* writer);
sp_str_t sp_toml_writer_write(sp_toml_writer_t* writer);

#ifdef SPN_IMPLEMENTATION

spn_app_t app;

#define TOML_READ_BOOL(table, var, key) \
    do { \
      toml_value_t value = toml_table_bool((table), (key)); \
      if (value.ok) { \
        (var) = value.u.b; \
      } \
    } while (0)

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

void sp_toml_writer_add_string_array(sp_toml_writer_t* writer, sp_str_t key, sp_dyn_array(sp_str_t) values) {
  sp_str_builder_append_fmt(&writer->builder, SP_LIT("{} = ["), SP_FMT_STR(key));
  sp_dyn_array_for(values, i) {
    sp_str_builder_append_fmt(&writer->builder, SP_LIT("{}"), SP_FMT_QUOTED_STR(values[i]));
    if (i != SP_SIZE_TO_INDEX(sp_dyn_array_size(values))) {
      sp_str_builder_append(&writer->builder, SP_LIT(", "));
    }
  }
  sp_str_builder_append(&writer->builder, SP_LIT("]"));
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

  spn_dep_context_t* dependency = spn_dep_find(add->package);
  if (!dependency) {
    SP_FATAL("Could not find {} in available dependencies", SP_FMT_QUOTED_STR(add->package));
  }

  if (spn_project_find_dependency(&app.project, add->package)) {
    SP_LOG("{} is already in defined in {}", SP_FMT_STR(add->package), SP_FMT_STR(app.paths.toml));
    SP_EXIT_SUCCESS();
  }

  spn_dep_id_t id = sp_str_copy(add->package);
  sp_dyn_array_push(app.project.dependencies, id);

  if (!spn_project_write(&app.project, app.paths.toml)) {
    SP_FATAL("Failed to write project TOML file");
  }

  SP_LOG("Added {} to {}", SP_FMT_STR(add->package), SP_FMT_STR(app.paths.toml));
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

  // Check if project file already exists
  if (sp_os_does_path_exist(app.paths.toml)) {
    SP_LOG("Project already initialized at {}", SP_FMT_COLOR(SP_ANSI_FORE_CYAN), SP_FMT_STR(app.paths.toml));
    return;
  }

  app.project = (spn_project_t){
    .name = sp_os_extract_file_name(app.paths.project)
  };

  if (!spn_project_write(&app.project, app.paths.toml)) {
    SP_FATAL("Failed to write project TOML file");
  }

  SP_LOG("Initialized project {} in spn.toml", SP_FMT_COLOR(SP_ANSI_FORE_CYAN), SP_FMT_QUOTED_STR(app.project.name));
}

void spn_cli_command_nuke(spn_cli_t* cli) {
  sp_os_remove_directory(app.paths.cache);
  sp_os_remove_file(app.paths.toml);
}

void spn_cli_command_clean(spn_cli_t* cli) {
  SP_LOG("Cleaning build directories at {:color cyan}", SP_FMT_STR(app.paths.build));
  sp_os_remove_directory(app.paths.build);
  SP_LOG("Cleaning store directories at {:color cyan}", SP_FMT_STR(app.paths.store));
  sp_os_remove_directory(app.paths.store);
}

void spn_cli_command_flags(spn_cli_t* cli) {
  spn_cli_flags_t* flags = &cli->flags;

  struct argparse argparse;
  argparse_init(
    &argparse,
    (struct argparse_option []) {
      OPT_HELP(),
      OPT_STRING('p', "package", &flags->package, "only show flags for specified package", SP_NULLPTR),
      OPT_END()
    },
    (const c8* const []) {
      "spn flags (include, libs, all)",
      SP_NULLPTR
    },
    SP_NULL
  );
  cli->num_args = argparse_parse(&argparse, cli->num_args, cli->args);

  if (!cli->num_args) {
    argparse_usage(&argparse);
    SP_EXIT_SUCCESS();
  }

  spn_build_context_t context = spn_build_context_from_default_profile();
  spn_build_context_prepare(&context);

  // Filter to only specified package if --package flag is set
  if (flags->package) {
    sp_str_t package_name = SP_CSTR(flags->package);
    spn_build_context_t filtered = SP_ZERO_INITIALIZE();
    sp_dyn_array_for(context.deps, i) {
      if (sp_str_equal(context.deps[i].id, package_name)) {
        sp_dyn_array_push(filtered.deps, context.deps[i]);
        break;
      }
    }
    context = filtered;
  }

  sp_str_builder_t builder = SP_ZERO_INITIALIZE();

  if (sp_cstr_equal(cli->args[0], "include")) {
    sp_str_builder_append(&builder, spn_build_context_make_flag(&context, SPN_FLAG_GNU_INCLUDE));
  }
  else if (sp_cstr_equal(cli->args[0], "libs")) {
    sp_str_builder_append(&builder, spn_build_context_make_flag(&context, SPN_FLAG_GNU_LIB_INCLUDE));
  }
  else if (sp_cstr_equal(cli->args[0], "all")) {
    sp_str_builder_append(&builder, spn_build_context_make_flag(&context, SPN_FLAG_GNU_LIB_INCLUDE));
    if (sp_dyn_array_size(context.deps) > 0) {
      sp_str_builder_append_c8(&builder, ' ');
    }
    sp_str_builder_append(&builder, spn_build_context_make_flag(&context, SPN_FLAG_GNU_INCLUDE));
  }
  else {
    sp_str_t requested_flag = SP_CSTR(cli->args[0]);
    SP_FATAL("Unknown flag {}; options are [include, libs, all]",  SP_FMT_COLOR(SP_ANSI_FORE_YELLOW), SP_FMT_QUOTED_STR(requested_flag));
  }

  printf("%s", sp_str_builder_write_cstr(&builder));
}

sp_str_t spn_build_context_make_flag(spn_build_context_t* context, spn_cli_flag_kind_t flag) {
  sp_str_builder_t builder = SP_ZERO_INITIALIZE();

  sp_dyn_array_for(context->deps, index) {
    spn_dep_context_t* dep = context->deps + index;
    sp_str_t str;

    switch (flag) {
      case SPN_FLAG_GNU_INCLUDE:     { str = sp_format("-I{}", SP_FMT_STR(dep->paths.include)); break; }
      case SPN_FLAG_GNU_LIB_INCLUDE: { str = sp_format("-L{}", SP_FMT_STR(dep->paths.bin)); break; }
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
  spn_build_context_t context = SP_ZERO_INITIALIZE();

  sp_dyn_array_for(app.project.dependencies, index) {
    spn_dep_id_t id = app.project.dependencies[index];
    spn_dep_context_t dep = spn_dep_context_from_default_profile(id);
    sp_dyn_array_push(context.deps, dep);
  }

  return context;
}

spn_dep_context_t spn_dep_context_from_default_profile(sp_str_t id) {
  spn_dep_context_t context = {
    .id = sp_str_copy(id),
    .options = SP_NULLPTR
  };

  // Check if there's a [deps.id] section with configuration
  toml_table_t* deps = toml_table_table(app.project.toml, "deps");
  if (deps) {
    toml_table_t* toml = toml_table_table(deps, sp_str_to_cstr(id));
    if (toml) {
      toml_table_t* options = toml_table_table(toml, "options");
      if (options) {
        spn_dep_context_add_options(&context, options);
      }
    }
  }

  return context;
}

void spn_dep_context_add_options(spn_dep_context_t* context, toml_table_t* options) {
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
      }
      else {
        spn_dep_option_t option = SP_ZERO_INITIALIZE();
        option.key = sp_str_copy(full_key);
        option.key = sp_str_to_upper(option.key);
        option.key = sp_str_replace_c8(option.key, '.', '_');
        option.value = sp_str_from_cstr(toml_table_unparsed(entry.table, key_data));
        sp_dyn_array_push(context->options, option);
      }
    }
  }

  sp_str_builder_t builder = SP_ZERO_INITIALIZE();
  sp_str_builder_append_fmt_c8(&builder, "{}", SP_FMT_STR(context->id));
  sp_dyn_array_for(context->options, index) {
    spn_dep_option_t* option = &context->options[index];
  }
}

sp_str_t spn_dependency_source_dir(sp_str_t name) {
  return sp_os_join_path(app.paths.source, name);
}

sp_str_t spn_dependency_build_dir(sp_str_t name) {
  return sp_os_join_path(app.paths.build, name);
}

sp_str_t spn_dependency_store_dir(sp_str_t name) {
  return sp_os_join_path(app.paths.store, name);
}

sp_str_t spn_dependency_recipe_file(sp_str_t name) {
  return sp_format("{}.mk", SP_FMT_STR(name));
}

spn_dep_context_t* spn_dep_find(sp_str_t name) {
  sp_dyn_array_for(app.builtins, index) {
    if (sp_str_equal(app.builtins[index], name)) {
      return (spn_dep_context_t*)1; // Just return non-null to indicate found
    }
  }
  return SP_NULLPTR;
}

sp_str_t spn_dep_option_env_name(spn_dep_option_t* option) {
  return SP_LIT("");
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
  return sp_str_trim_right(result.output);
}

void spn_build_context_prepare(spn_build_context_t* context) {
  sp_dyn_array_for(context->deps, index) {
    spn_dep_context_t* dep = context->deps + index;
    spn_dep_context_prepare(dep);
  }
}

void spn_dep_context_set_env_var(spn_dep_context_t* context, sp_str_t name, sp_str_t value) {
  if (!SDL_SetEnvironmentVariable(context->environment, sp_str_to_cstr(name), sp_str_to_cstr(value), SP_SDL_OVERWRITE_ENV_VAR)) {
    SP_FATAL("Failed to set {}={} in build context for {}", SP_FMT_STR(name), SP_FMT_STR(value), SP_FMT_STR(context->id));
  }
}

bool spn_dep_context_is_cloned(spn_dep_context_t* context) {
  sp_str_t source = spn_dependency_source_dir(context->id);
  return sp_os_does_path_exist(source);
}

bool spn_dep_context_is_built(spn_dep_context_t* context) {
  return sp_os_does_path_exist(context->paths.build);
}

void spn_dep_context_prepare(spn_dep_context_t* context) {
  sp_dyn_array(sp_hash_t) hashes = SP_NULLPTR;
  // Include the dependency ID in the hash
  sp_dyn_array_push(hashes, sp_hash_str(context->id));
  sp_dyn_array_for(context->options, index) {
    spn_dep_option_t* option = &context->options[index];
    sp_dyn_array_push(hashes, sp_hash_str(option->key));
    sp_dyn_array_push(hashes, sp_hash_str(option->value));
  }

  context->hash = sp_hash_combine(hashes, sp_dyn_array_size(hashes));
  context->build_id = sp_format("{}", SP_FMT_SHORT_HASH(context->hash));

  sp_str_t build = spn_dependency_build_dir(context->id);
  sp_str_t store = spn_dependency_store_dir(context->id);
  context->paths.source = spn_dependency_source_dir(context->id);
  context->paths.build = sp_os_join_path(build, context->build_id);
  context->paths.store = sp_os_join_path(store, context->build_id);
  context->paths.include = sp_os_join_path(context->paths.store, SP_LIT("include"));
  context->paths.bin = sp_os_join_path(context->paths.store, SP_LIT("bin"));
  context->paths.recipe = sp_os_join_path(app.paths.recipes, spn_dependency_recipe_file(context->id));
  context->paths.std_out = sp_os_join_path(context->paths.build, SP_LIT("build.stdout"));
  context->paths.std_err = sp_os_join_path(context->paths.build, SP_LIT("build.stderr"));
  context->paths.std_in  = sp_os_join_path(context->paths.build, SP_LIT("build.stdin"));

  context->environment = SDL_CreateEnvironment(SP_SDL_INHERIT_ENVIRONMENT);
  spn_dep_context_set_env_var(context, SP_LIT("SPN_DIR_PROJECT"), context->paths.source);
  spn_dep_context_set_env_var(context, SP_LIT("SPN_DIR_BUILD"), context->paths.build);
  spn_dep_context_set_env_var(context, SP_LIT("SPN_DIR_STORE_INCLUDE"), context->paths.include);
  spn_dep_context_set_env_var(context, SP_LIT("SPN_DIR_STORE_BIN"), context->paths.bin);
  sp_dyn_array_for(context->options, index) {
    spn_dep_option_t* option = &context->options[index];
    sp_str_t key = sp_str_concat(SP_LIT("SPN_OPT_"), option->key);
    spn_dep_context_set_env_var(context, key, option->value);
  }

  context->shell = SDL_CreateProperties();
  SDL_SetPointerProperty(context->shell, SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER, context->environment);
  SDL_SetNumberProperty(context->shell, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
  SDL_SetNumberProperty(context->shell, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_INHERITED);
  SDL_SetNumberProperty(context->shell, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_INHERITED);

  context->is_cloned = spn_dep_context_is_cloned(context);
  context->is_cloned = spn_dep_context_is_built(context);

  sp_os_create_directory(context->paths.build);
  sp_os_create_directory(context->paths.store);
  sp_os_create_directory(context->paths.include);
  sp_os_create_directory(context->paths.bin);
}

void spn_dep_context_clone_async(spn_dep_context_t* context) {
  const c8* args [] = {
    "make",
    "-C", sp_str_to_cstr(context->paths.build),
    "--makefile", sp_str_to_cstr(context->paths.recipe),
    "spn-clone",
    SP_NULLPTR
  };

  SDL_PropertiesID shell = SDL_CreateProperties();
  SDL_CopyProperties(context->shell, shell);
  SDL_SetPointerProperty(shell, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void*)args);

  SP_LOG("Running {:color yellow} for {:color cyan}", SP_FMT_CSTR("spn-clone"), SP_FMT_STR(context->paths.recipe));

  SDL_Process* process = SDL_CreateProcessWithProperties(shell);
  if (!process) {
    const c8* sdl = SDL_GetError();
    SP_FATAL("Failed to create process to run clone target for {:color yellow}: {:color red}", SP_FMT_STR(context->paths.recipe), SP_FMT_CSTR(sdl));
  }

  // Wait for process to complete (stdio is inherited so we can't read output)
  s32 return_code;
  SDL_WaitProcess(process, true, &return_code);
  if (return_code) {
    SP_FATAL("Failed to clone for {:color cyan}: exited with code {:color red}", SP_FMT_STR(context->paths.recipe), SP_FMT_S32(return_code));
  }

  SP_LOG("OK!");
  SDL_DestroyProcess(process);
}

void spn_dep_context_build_async(spn_dep_context_t* context) {
  sp_str_t target = SP_LIT("spn-build");
  const c8* args [] = {
    "make",
    "-C", sp_str_to_cstr(context->paths.build),
    "--makefile", sp_str_to_cstr(context->paths.recipe),
    sp_str_to_cstr(target),
    SP_NULLPTR
  };

  SDL_PropertiesID shell = SDL_CreateProperties();
  SDL_CopyProperties(context->shell, shell);
  SDL_SetPointerProperty(shell, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void*)args);

  SP_LOG("Building {:color cyan} from {}", SP_FMT_STR(context->id), SP_FMT_STR(context->paths.recipe));
  SDL_Process* process = SDL_CreateProcessWithProperties(shell);
  if (!process) {
    const c8* sdl = SDL_GetError();
    SP_FATAL("Failed to create process: {:color red}", SP_FMT_CSTR(sdl));
  }

  // Wait for process to complete (stdio is inherited so we can't read output)
  s32 return_code;
  SDL_WaitProcess(process, true, &return_code);
  if (return_code) {
    SP_FATAL("Building {:color cyan} returned with code {:color red}", SP_FMT_STR(context->paths.recipe), SP_FMT_S32(return_code));
  }

  SP_LOG("{:color green}", SP_FMT_CSTR("OK!"));

  SDL_DestroyProcess(process);
}

void spn_app_init(spn_app_t* app, u32 num_args, const c8** args) {
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

  app->paths.executable = sp_os_get_executable_path();
  app->paths.install = sp_os_parent_path(app->paths.executable);

  if (app->cli.project_directory) {
    app->paths.project = sp_str_from_cstr(app->cli.project_directory);
  }
  else {
    c8* working_directory = SDL_GetCurrentDirectory();
    app->paths.project = sp_os_canonicalize_path(SP_CSTR(working_directory));
    SDL_free(working_directory);
  }
  app->paths.toml = sp_os_join_path(app->paths.project, SP_LIT("spn.toml"));

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
    rev_list.result.output = sp_str_trim_right(rev_list.result.output);
    if (sp_parse_u32(rev_list.result.output)) {
      SP_LOG("spn has updates to recipes; pull? (y)es, (n)o, (a)lways");
      SP_LOG("Cloning recipe repository from {} to {}", SP_FMT_CSTR(url), SP_FMT_CSTR(spn));
      SDL_Process* process = SPN_SH("git", "-C", spn, "pull");
      spn_sh_process_result_t result = spn_sh_read_process(process);
      if (result.return_code) {
        SP_FATAL("Failed to clone spn recipe sources from {} to {}", SP_FMT_CSTR(url), SP_FMT_CSTR(spn));
      }
    }
  }

  sp_os_directory_entry_list_t entries = sp_os_scan_directory(app->paths.recipes);
  for (sp_os_directory_entry_t* entry = entries.data; entry < entries.data + entries.count; entry++) {
    spn_dep_id_t id = sp_os_extract_stem(entry->file_name);
    sp_dyn_array_push(app->builtins, id);
  }

  sp_str_t head = spn_sh_git_find_head(app->paths.bootstrap);

  // Check if this is a command that doesn't need a project file
  bool needs_project = true;
  if (cli->num_args > 0) {
    if (sp_cstr_equal(cli->args[0], "init") ||
        sp_cstr_equal(cli->args[0], "nuke") ||
        sp_cstr_equal(cli->args[0], "clean")) {
      needs_project = false;
    }
  }

  if (needs_project) {
    if (!sp_os_does_path_exist(app->paths.toml)) {
      SP_FATAL("Expected project TOML file at {}, but it did not exist", SP_FMT_STR(app->paths.toml));
    }

    if (!spn_project_read(&app->project, app->paths.toml)) {
      SP_FATAL("Failed to read project TOML file at {}", SP_FMT_STR(app->paths.toml));
    }
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
  else if (sp_cstr_equal("clean", cli->args[0])) {
    spn_cli_command_clean(cli);
  }
  else if (sp_cstr_equal("flags", cli->args[0])) {
    spn_cli_command_flags(cli);
  }
  else if (sp_cstr_equal("build", cli->args[0])) {
    spn_project_build(&app->project);
  }
}

void spn_config_read(spn_config_t* config, sp_str_t path) {
  *config = (spn_config_t) {
    .auto_pull_recipes = false,
    .toml = SP_NULLPTR,
    .cache_override = SP_ZERO_INITIALIZE(),
    .additional_recipe_dirs = SP_NULLPTR,
    .builtin_recipes_enabled = true
  };

  size_t file_size;
  c8* file_data = (c8*)SDL_LoadFile(sp_str_to_cstr(path), &file_size);
  if (!file_data) return;

  c8* toml_error = (c8*)sp_alloc(256);
  config->toml = toml_parse(file_data, toml_error, 256);
  if (!config->toml) {
    SP_FATAL("Failed to parse {}: {}", SP_FMT_STR(path), SP_FMT_CSTR(toml_error));
  }

  toml_table_t* options = toml_table_table(config->toml, "options");
  if (options) {
    TOML_READ_BOOL(options, config->auto_pull_recipes, "auto_pull_recipes");
    TOML_READ_BOOL(options, config->builtin_recipes_enabled, "builtin_recipes_enabled");

    toml_value_t cache_override = toml_table_string(options, "cache_override");
    if (cache_override.ok) {
      config->cache_override = sp_str_from_cstr(cache_override.u.s);
    }

    toml_array_t* recipe_dirs = toml_table_array(options, "additional_recipe_dirs");
    if (recipe_dirs) {
      for (u32 i = 0; i < toml_array_len(recipe_dirs); i++) {
        toml_value_t dir = toml_array_string(recipe_dirs, i);
        if (dir.ok) {
          sp_dyn_array_push(config->additional_recipe_dirs, sp_str_from_cstr(dir.u.s));
        }
      }
    }
  }
}

void spn_config_read_from_string(spn_config_t* config, sp_str_t toml_content) {
  *config = (spn_config_t) {
    .auto_pull_recipes = false,
    .toml = SP_NULLPTR,
    .cache_override = SP_ZERO_INITIALIZE(),
    .additional_recipe_dirs = SP_NULLPTR,
    .builtin_recipes_enabled = true
  };

  c8* toml_error = (c8*)sp_alloc(256);
  config->toml = toml_parse(sp_str_to_cstr(toml_content), toml_error, 256);
  if (!config->toml) {
    SP_FATAL("Failed to parse TOML: {}", SP_FMT_CSTR(toml_error));
  }

  toml_table_t* options = toml_table_table(config->toml, "options");
  if (options) {
    TOML_READ_BOOL(options, config->auto_pull_recipes, "auto_pull_recipes");
    TOML_READ_BOOL(options, config->builtin_recipes_enabled, "builtin_recipes_enabled");

    toml_value_t cache_override = toml_table_string(options, "cache_override");
    if (cache_override.ok) {
      config->cache_override = sp_str_from_cstr(cache_override.u.s);
    }

    toml_array_t* recipe_dirs = toml_table_array(options, "additional_recipe_dirs");
    if (recipe_dirs) {
      for (u32 i = 0; i < toml_array_len(recipe_dirs); i++) {
        toml_value_t dir = toml_array_string(recipe_dirs, i);
        if (dir.ok) {
          sp_dyn_array_push(config->additional_recipe_dirs, sp_str_from_cstr(dir.u.s));
        }
      }
    }
  }
}

spn_dep_context_t* spn_project_find_dependency(spn_project_t* project, sp_str_t name) {
  SP_ASSERT(project);
  if (!project->dependencies) return SP_NULLPTR;

  sp_dyn_array_for(project->dependencies, index) {
    if (sp_str_equal(project->dependencies[index], name)) {
      return (spn_dep_context_t*)1; // Just return non-null to indicate found
    }
  }

  return SP_NULLPTR;
}

bool spn_project_write(spn_project_t* project, sp_str_t path) {
  sp_toml_writer_t writer = SP_ZERO_INITIALIZE();

  sp_toml_writer_add_header(&writer, sp_str_lit("project"));
  sp_toml_writer_add_string(&writer, sp_str_lit("name"), project->name);
  if (project->dependencies) {
    sp_toml_writer_add_string_array(&writer, sp_str_lit("deps"), project->dependencies);
  }

  sp_toml_writer_new_line(&writer);

  // Add configuration sections for deps that have options
  if (project->dependencies) {
    toml_table_t* deps_table = project->toml ? toml_table_table(project->toml, "deps") : SP_NULLPTR;
    sp_dyn_array_for(project->dependencies, i) {
      spn_dep_id_t id = project->dependencies[i];
      // Check if this dep has configuration in the original toml
      if (deps_table) {
        toml_table_t* dep_config = toml_table_table(deps_table, sp_str_to_cstr(id));
        if (dep_config) {
          sp_toml_writer_add_header(&writer, sp_format("deps.{}", SP_FMT_STR(id)));
          // TODO: Write the actual configuration options here
        }
      }
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

  c8* toml_error = (c8*)sp_alloc(256);
  project->toml = toml_parse(file_data, toml_error, 256);
  SP_ASSERT_FMT(project->toml, "Failed to read project file at {}: {}", SP_FMT_STR(path), SP_FMT_CSTR(toml_error));

  toml_table_t* project_table = toml_table_table(project->toml, "project");
  SP_ASSERT_FMT(project->toml, "Malformed project file: missing [project]");

  toml_value_t name = toml_table_string(project_table, "name");
  SP_ASSERT_FMT(name.ok, "Malformed project file: missing project.name");
  project->name = sp_str_from_cstr(name.u.s);

  // First read the deps array from [project]
  toml_array_t* deps_array = toml_table_array(project_table, "deps");
  if (deps_array) {
    for (u32 i = 0; i < toml_array_len(deps_array); i++) {
      toml_value_t dep = toml_array_string(deps_array, i);
      if (dep.ok) {
        spn_dep_id_t id = sp_str_from_cstr(dep.u.s);
        sp_dyn_array_push(project->dependencies, id);
      }
    }
  }

  // Also support legacy format where deps are keys under [deps] table
  toml_table_t* deps_table = toml_table_table(project->toml, "deps");
  if (deps_table && !deps_array) {
    for (u32 index = 0; index < toml_table_len(deps_table); index++) {
      s32 len;
      const c8* key = toml_table_key(deps_table, index, &len);

      spn_dep_id_t id = sp_str_from_cstr_sized(key, len);
      // Only add if not already in the list (from deps array)
      bool found = false;
      sp_dyn_array_for(project->dependencies, j) {
        if (sp_str_equal(project->dependencies[j], id)) {
          found = true;
          break;
        }
      }
      if (!found) {
        sp_dyn_array_push(project->dependencies, id);
      }
    }
  }

  return true;
}

void spn_project_build(spn_project_t* project) {
  spn_build_context_t context = spn_build_context_from_default_profile();
  spn_build_context_prepare(&context);

  u32 width = 0;
  sp_dyn_array_for(context.deps, index) {
    spn_dep_context_t* dep = context.deps + index;
    width = SP_MAX(width, dep->id.len);
  }

  sp_dyn_array_for(context.deps, index) {
    spn_dep_context_t* dep = context.deps + index;
    sp_str_builder_t builder = SP_ZERO_INITIALIZE();

    sp_str_t status;
    if (!dep->is_cloned && !dep->is_built) {
      status = SP_LIT("uninitialized");
    }
    else if (dep->is_cloned && !dep->is_built) {
      status = SP_LIT("cloned");
    }
    else if (dep->is_cloned && dep->is_built) {
      status = SP_LIT("built");
    }

    sp_str_t name = sp_str_pad(dep->id, width);
    sp_str_builder_append_fmt_c8(&builder, "> {:color cyan} [{}]", SP_FMT_STR(name), SP_FMT_STR(status));
    sp_str_builder_new_line(&builder);

    sp_str_builder_indent(&builder);
    sp_dyn_array_for(dep->options, p) {
      spn_dep_option_t* option = dep->options + p;
      sp_str_builder_append_fmt_c8(&builder, "{:color brightblack} = {}", SP_FMT_STR(option->key), SP_FMT_STR(option->value));
      sp_str_builder_new_line(&builder);
    }
    sp_str_t report = sp_str_builder_write(&builder);
    sp_log(report);

    if (!dep->is_cloned) {
      spn_dep_context_clone_async(dep);
    }

    spn_dep_context_build_async(dep);
  }

}

#endif // SPN_IMPLEMENTATION

#endif // SPN_H
