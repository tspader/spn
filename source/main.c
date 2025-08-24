#define SP_IMPLEMENTATION
#define SP_OS_BACKEND_SDL
#include "sp/sp.h"

#define ARGPARSE_IMPLEMENTATION
#include "argparse/argparse.h"

#include "toml/toml.h"


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
  SPN_FLAGS_GNU_LIBS,
} spn_cli_flag_kind_t;

typedef struct {
  spn_cli_flag_kind_t kind;
} spn_cli_flags_t;

typedef struct {
  u32 num_args;
  const c8** args;
  spn_cli_add_t add;
  spn_cli_init_t init;
  spn_cli_flags_t flags;
} spn_cli_t;

void spn_cli_init(spn_cli_t* cli, u32 num_args, const c8** args);
void spn_cli_run(spn_cli_t* cli);
void spn_cli_command_add(spn_cli_t* cli);
void spn_cli_command_init(spn_cli_t* cli);
void spn_cli_command_nuke(spn_cli_t* cli);
void spn_cli_command_flags(spn_cli_t* cli);


/////////
// SPN //
/////////
typedef enum {
  SPN_DEPENDENCY_KIND_BUILTIN,
  SPN_DEPENDENCY_KIND_USER,
} spn_dependency_kind_t;

typedef struct {
  sp_str_t install;
  sp_str_t   executable;
  sp_str_t cache;
  sp_str_t   work;
  sp_str_t   build;
  sp_str_t   repo;
  sp_str_t     spn;
  sp_str_t       recipes;
  sp_str_t project;
  sp_str_t   toml;
} spn_paths_t;

typedef struct {
  sp_str_t key;
  sp_str_t value;
} spn_dependency_option_t;

typedef struct {
  spn_dependency_kind_t kind;
  sp_str_t name;
  spn_dependency_option_t* options;
} spn_dependency_t;

void     spn_dependency_parse_options(spn_dependency_t* dependency, toml_table_t* options);
void     spn_dependency_clone(spn_dependency_t* dependency);
bool     spn_dependency_is_cloned(spn_dependency_t* dependency);
sp_str_t spn_dependency_source_dir(spn_dependency_t* dependency);
sp_str_t spn_dependency_build_dir(spn_dependency_t* dependency);
sp_str_t spn_dependency_install_dir(spn_dependency_t* dependency);
sp_str_t spn_dependency_install_include_dir(spn_dependency_t* dependency);
sp_str_t spn_dependency_install_bin_dir(spn_dependency_t* dependency);
sp_str_t spn_dependency_recipe_file(spn_dependency_t* dependency);

typedef struct {
  sp_str_t name;
  sp_dyn_array(spn_dependency_t) dependencies;
} spn_project_t;

typedef struct {
  spn_cli_t cli;
  spn_paths_t paths;
  spn_project_t project;
  sp_dyn_array(spn_dependency_t) builtins;
} spn_app_t;

spn_app_t app;

void spn_app_init(spn_app_t* app);
spn_dependency_t* spn_project_find_dependency(spn_project_t* project, sp_str_t name);
bool spn_project_write(spn_project_t* project, sp_str_t path);
bool spn_project_read(spn_project_t* project, sp_str_t path);
void spn_project_build(spn_project_t* project);
sp_str_t spn_build_flag(spn_cli_flag_kind_t flag);

///////////
// SHELL //
///////////
typedef struct {
  sp_str_t output;
  s32 return_code;
} spn_sh_process_result_t;

#define SPN_SH(...) SDL_CreateProcess((const c8* []) { __VA_ARGS__, SP_NULLPTR }, SP_SDL_PIPE_STDIO)

spn_sh_process_result_t spn_sh_read_process(SDL_Process* process);
void                    spn_sh_git_clone(sp_str_t url, sp_str_t target);
sp_str_t                spn_sh_git_find_head(sp_str_t repo);
spn_sh_process_result_t spn_dependency_make(spn_dependency_t* dependency, sp_str_t target);

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

void spn_cli_init(spn_cli_t* cli, u32 num_args, const c8** args) {
  struct argparse_option options [] = {
    OPT_HELP(),
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
}

void spn_cli_run(spn_cli_t* cli) {
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

  // Read existing project if it exists
  if (!sp_os_does_path_exist(app.paths.toml)) {
    SP_FATAL("Expected project TOML file at {}, but it did not exist", SP_FMT_STR(app.paths.toml));
  }

  if (!spn_project_read(&app.project, app.paths.toml)) {
    SP_FATAL("Failed to read project TOML file at {}", SP_FMT_STR(app.paths.toml));
  }

  add->package = SP_CSTR(cli->args[0]);

  if (spn_project_find_dependency(&app.project, add->package)) {
    SP_LOG("{} is already in your project", SP_FMT_STR(add->package));
    SP_EXIT_SUCCESS();
  }

  spn_dependency_t* dependency = SP_NULLPTR;
  sp_dyn_array_for(app.builtins, index) {
    spn_dependency_t* builtin = &app.builtins[index];
    if (sp_str_equal(builtin->name, add->package)) {
      dependency = builtin;
    }
  }

  if (!dependency) {
    SP_FATAL("Could not find {} in available dependencies", SP_FMT_QUOTED_STR(add->package));
  }

  if (!spn_dependency_is_cloned(dependency)) {
    spn_dependency_clone(dependency);
  }


  spn_dependency_t dep = {
    .kind = SPN_DEPENDENCY_KIND_BUILTIN,
    .name = add->package
  };
  sp_dyn_array_push(app.project.dependencies, dep);

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
    cli->flags.kind = SPN_FLAGS_GNU_LIBS;
  }
  else {
    SP_FATAL("Unknown flag {}; options are [include, libs]", SP_FMT_QUOTED_STR(flag_str));
  }

  if (!sp_os_does_path_exist(app.paths.toml)) {
    SP_FATAL("Expected project TOML file at {}, but it did not exist", SP_FMT_STR(app.paths.toml));
  }

  if (!spn_project_read(&app.project, app.paths.toml)) {
    SP_FATAL("Failed to read project TOML file at {}", SP_FMT_STR(app.paths.toml));
  }

  sp_str_t flag = spn_build_flag(cli->flags.kind);
  printf("%s", sp_str_to_cstr(flag));
}

typedef struct {
  sp_str_t key;
  toml_table_t* table;
} sp_dependency_parse_entry_t;

void spn_dependency_parse_options(spn_dependency_t* dependency, toml_table_t* config) {
  if (!config) return;

  toml_table_t* options = toml_table_table(config, "options");
  if (!options) return;

  sp_dyn_array(sp_dependency_parse_entry_t) entries = SP_NULLPTR;

  sp_dyn_array_push(entries, ((sp_dependency_parse_entry_t) {
    .key = SP_LIT(""),
    .table = options
  }));

  while (sp_dyn_array_size(entries)) {
    sp_dependency_parse_entry_t* entry = sp_dyn_array_back(entries);
    sp_dyn_array_pop(entries);

    for (u32 index = 0; index < toml_table_len(entry->table); index++) {
      s32 key_len;
      const c8* key_data = toml_table_key(entry->table, index, &key_len);
      sp_str_t key = SP_CSTR(key_data);

      sp_str_t full_key = SP_ZERO_INITIALIZE();
      if (entry->key.len) {
        full_key = sp_str_join(entry->key, key, SP_LIT("."));
      }
      else {
        full_key = sp_str_copy(key);
      }

      toml_table_t* table = toml_table_table(entry->table, key_data);
      toml_array_t* array = toml_table_array(options, key_data);

      if (table) {
        sp_dyn_array_push(entries, ((sp_dependency_parse_entry_t) {
          .key = full_key,
          .table = table
        }));
      }
      else if (array) {
        SP_LOG("array: {}", SP_FMT_STR(full_key));
      }
      else {
        spn_dependency_option_t option = SP_ZERO_INITIALIZE();
        option.key = sp_str_copy(full_key);
        option.key = sp_str_to_upper(option.key);
        option.key = sp_str_replace(option.key, '.', '_');
        option.value = sp_str_copy_cstr(toml_table_unparsed(entry->table, key_data));
        sp_dyn_array_push(dependency->options, option);
      }
    }
  }
}

bool spn_dependency_is_cloned(spn_dependency_t* dependency) {
  sp_str_t source = spn_dependency_source_dir(dependency);
  return sp_os_does_path_exist(source);
}

void spn_dependency_clone(spn_dependency_t* dependency) {
  spn_dependency_make(dependency, SP_LIT("spn-clone"));
}

sp_str_t spn_dependency_source_dir(spn_dependency_t* dependency) {
  return sp_os_join_path(app.paths.repo, dependency->name);
}

sp_str_t spn_dependency_build_dir(spn_dependency_t* dependency) {
  return sp_os_join_path(app.paths.build, dependency->name);
}

sp_str_t spn_dependency_install_dir(spn_dependency_t* dependency) {
  return sp_os_join_path(app.paths.install, dependency->name);
}

sp_str_t spn_dependency_install_include_dir(spn_dependency_t* dependency) {
  return sp_os_join_path(spn_dependency_install_dir(dependency), SP_LIT("include"));
}

sp_str_t spn_dependency_install_bin_dir(spn_dependency_t* dependency) {
  return sp_os_join_path(spn_dependency_install_dir(dependency), SP_LIT("bin"));
}

sp_str_t spn_dependency_recipe_file(spn_dependency_t* dependency) {
  return sp_fmt(SP_LIT("{}.mk"), SP_FMT_STR(dependency->name));
}


spn_sh_process_result_t spn_sh_read_process(SDL_Process* process) {
  spn_sh_process_result_t result;
  sp_size_t len;

  result.output.data = (c8*)SDL_ReadProcess(process, &len, &result.return_code);
  result.output.len = (u32)len;
  return result;
}

void spn_sh_git_clone(sp_str_t url, sp_str_t target) {
  SDL_Process* process = SDL_CreateProcess(
    (const c8* []) {
      "git",
      "clone",
      sp_str_to_cstr(url),
      sp_str_to_cstr(target),
      SP_NULLPTR
    },
    SP_SDL_PIPE_STDIO
  );

  sp_str_t output;
  sp_size_t len;
  s32 return_code = 0;
  output.data = (c8*)SDL_ReadProcess(process, &len, &return_code);
  output.len = (u32)len;

  SDL_DestroyProcess(process);
}

sp_str_t spn_sh_git_find_head(sp_str_t repo) {
  SDL_Process* process = SPN_SH("git", "-C", sp_str_to_cstr(repo), "rev-parse", "--abbrev-ref", "HEAD");
  spn_sh_process_result_t result = spn_sh_read_process(process);
  SDL_DestroyProcess(process);

  return sp_str_strip_right(result.output);
}

spn_sh_process_result_t spn_dependency_make(spn_dependency_t* dependency, sp_str_t target) {
  sp_str_t source = spn_dependency_source_dir(dependency);
  sp_str_t build = spn_dependency_build_dir(dependency);
  sp_str_t install = spn_dependency_install_dir(dependency);
  sp_str_t include = spn_dependency_install_include_dir(dependency);
  sp_str_t bin = spn_dependency_install_bin_dir(dependency);
  sp_str_t recipe = spn_dependency_recipe_file(dependency);

  const c8* args [] = {
    "make",
    "--directory", sp_str_to_cstr(app.paths.recipes),
    "--makefile", sp_str_to_cstr(recipe),
    sp_str_to_cstr(target),
    SP_NULLPTR
  };

  SDL_Environment* environment = SDL_CreateEnvironment(SP_SDL_INHERIT_ENVIRONMENT);
  SDL_SetEnvironmentVariable(environment, "SPN_DIR_PROJECT", sp_str_to_cstr(source), true);
  SDL_SetEnvironmentVariable(environment, "SPN_DIR_BUILD", sp_str_to_cstr(build), true);
  SDL_SetEnvironmentVariable(environment, "SPN_DIR_INSTALL_INCLUDE", sp_str_to_cstr(include), true);
  SDL_SetEnvironmentVariable(environment, "SPN_DIR_INSTALL_BIN", sp_str_to_cstr(bin), true);

  SDL_PropertiesID id = SDL_CreateProperties();
  SDL_SetPointerProperty(id, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void*)args);
  SDL_SetPointerProperty(id, SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER, environment);
  SDL_SetNumberProperty(id, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
  SDL_SetNumberProperty(id, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
  SDL_SetNumberProperty(id, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_NULL);

  SDL_Process* process = SDL_CreateProcessWithProperties(id);
  if (!process) {
    const c8* sdl = SDL_GetError();
    SP_FATAL("Failed to create process: {}", SP_FMT_CSTR(sdl));
  }

  spn_sh_process_result_t result = spn_sh_read_process(process);
  if (result.return_code) {
    SP_FATAL("Target {} for {} failed!", SP_FMT_STR(target), SP_FMT_STR(recipe));
  }

  SDL_DestroyProcess(process);

  return result;
}

void spn_app_init(spn_app_t* app) {
  app->paths.executable = sp_os_get_executable_path();
  app->paths.install = sp_os_parent_path(app->paths.executable);

  c8* working_directory = SDL_GetCurrentDirectory();
  app->paths.project = sp_os_canonicalize_path(SP_CSTR(working_directory));
  app->paths.toml = sp_os_join_path(app->paths.project, SP_LIT("spn.toml"));
  SDL_free(working_directory);

  const c8* xdg = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "XDG_CACHE_HOME");
  const c8* home = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "HOME");
  if (xdg) {
    app->paths.cache = sp_str_join(SP_CSTR(xdg), SP_LIT("spn"), SP_LIT("/"));
  }
  else if (home) {
    app->paths.cache = sp_str_join(SP_CSTR(home), SP_LIT(".cache/spn"), SP_LIT("/"));
  }

  app->paths.repo = sp_os_join_path(app->paths.cache, SP_LIT("repo"));
  app->paths.build = sp_os_join_path(app->paths.cache, SP_LIT("build"));
  app->paths.work = sp_os_join_path(app->paths.cache, SP_LIT("work"));
  app->paths.spn = sp_os_join_path(app->paths.repo, SP_LIT("spn"));
  app->paths.recipes = sp_os_join_path(app->paths.spn, SP_LIT("asset/recipes"));

  sp_os_create_directory(app->paths.cache);
  sp_os_create_directory(app->paths.repo);
  sp_os_create_directory(app->paths.build);

  if (!sp_os_does_path_exist(app->paths.spn)) {
    spn_sh_git_clone(SP_LIT("https://github.com/spaderthomas/spn.git"), app->paths.spn);
  }

  sp_str_t head = spn_sh_git_find_head(app->paths.spn);

  sp_os_directory_entry_list_t entries = sp_os_scan_directory(app->paths.recipes);
  for (sp_os_directory_entry_t* entry = entries.data; entry < entries.data + entries.count; entry++) {
    spn_dependency_t builtin = {
      .kind = SPN_DEPENDENCY_KIND_BUILTIN,
      .name = sp_os_extract_stem(entry->file_name),
    };
    sp_dyn_array_push(app->builtins, builtin);
  }
}

spn_dependency_t* spn_project_find_dependency(spn_project_t* project, sp_str_t name) {
  SP_ASSERT(project);
  if (!project->dependencies) return false;

  sp_dyn_array_for(project->dependencies, index) {
    spn_dependency_t* dependency = &project->dependencies[index];
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
      spn_dependency_t* dep = &project->dependencies[i];

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

  c8 errbuf[256];
  toml_table_t* conf = toml_parse((c8*)file_data, errbuf, sizeof(errbuf));
  SP_ASSERT_FMT(conf, "Failed to read project file at {}: {}", SP_FMT_STR(path), SP_FMT_CSTR(errbuf));

  toml_table_t* project_table = toml_table_table(conf, "project");
  SP_ASSERT_FMT(conf, "Malformed project file: missing [project]");

  toml_value_t name = toml_table_string(project_table, "name");
  SP_ASSERT_FMT(name.ok, "Malformed project file: missing project.name");
  project->name = sp_str_copy_cstr(name.u.s);

  // Read dependencies
  toml_table_t* deps_table = toml_table_table(conf, "deps");
  if (deps_table) {
    for (u32 index = 0; index < toml_table_len(deps_table); index++) {
      s32 len;
      const c8* key = toml_table_key(deps_table, index, &len);

      toml_table_t* dep_table = toml_table_table(deps_table, key);
      SP_ASSERT(dep_table);

      spn_dependency_t dependency = {
        .kind = SPN_DEPENDENCY_KIND_BUILTIN,
        .name = sp_str_copy_cstr_n(key, len),
        .options = SP_NULLPTR
      };

      spn_dependency_parse_options(&dependency, dep_table);

      sp_dyn_array_push(project->dependencies, dependency);
    }
  }

  sp_dyn_array_for(project->dependencies, i) {
    spn_dependency_t* dep = &project->dependencies[i];
    sp_dyn_array_for(dep->options, j) {
      spn_dependency_option_t* option = dep->options + j;
      SP_LOG("{} -> {}", SP_FMT_STR(option->key), SP_FMT_STR(option->value));
    }
  }
  toml_free(conf);
  return true;
}

void spn_project_build(spn_project_t* project) {
  sp_dyn_array_for(project->dependencies, index) {
    spn_dependency_t* dependency = &project->dependencies[index];
    spn_dependency_make(dependency, SP_LIT("spn-build"));
  }
}

sp_str_t spn_build_flag(spn_cli_flag_kind_t flag) {
  sp_str_builder_t builder = SP_ZERO_INITIALIZE();

  switch (flag) {
    case SPN_FLAG_GNU_INCLUDES: {
      sp_dyn_array_for(app.project.dependencies, index) {
        spn_dependency_t* dependency = &app.project.dependencies[index];
        sp_str_t include = spn_dependency_install_include_dir(dependency);
        sp_str_builder_append_fmt(&builder, SP_LIT("-I{}"), SP_FMT_STR(include));
      }
      break;
    }
    case SPN_FLAGS_GNU_LIBS: {
      break;
    }
    default: {
      SP_FATAL("spn_build_flags(): unknown flag ({})", SP_FMT_U32(flag));
    }
  }

  return sp_str_builder_write(&builder);
}

s32 main(s32 num_args, const char** args) {
  sp_allocator_malloc_t malloc_allocator = SP_ZERO_INITIALIZE();
  sp_allocator_t allocator = sp_allocator_malloc_init(&malloc_allocator);
  sp_context_push_allocator(&allocator);

  app = SP_ZERO_STRUCT(spn_app_t);
  spn_app_init(&app);

  spn_cli_init(&app.cli, num_args, args);
  spn_cli_run(&app.cli);

  sp_context_pop();
  return 0;
}
