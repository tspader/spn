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

typedef struct {
  u32 num_args;
  const c8** args;
  spn_cli_add_t add;
  spn_cli_init_t init;
} spn_cli_t;

void spn_cli_init(spn_cli_t* cli, u32 num_args, const c8** args);
void spn_cli_run(spn_cli_t* cli);
void spn_cli_command_add(spn_cli_t* cli);
void spn_cli_command_init(spn_cli_t* cli);

/////////
// GIT //
/////////
void spn_git_clone(sp_str_t url, sp_str_t target);
sp_str_t spn_git_find_head(sp_str_t repo);

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
  sp_str_t   builds;
  sp_str_t   repos;
  sp_str_t     spn;
  sp_str_t       recipes;
  sp_str_t project;
} spn_paths_t;

typedef struct {
  spn_dependency_kind_t kind;
  sp_str_t name;
  sp_str_t url;
} spn_dependency_t;

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
void spn_bootstrap();
bool spn_project_write(spn_project_t* project, sp_str_t path);
bool spn_project_read(spn_project_t* project, sp_str_t path);

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
sp_str_t sp_toml_writer_write(sp_toml_writer_t* writer);


////////////////////
// IMPLEMENTATION //
////////////////////
void sp_toml_writer_add_header(sp_toml_writer_t* writer, sp_str_t name) {
  sp_str_builder_append_fmt(&writer->builder, SP_LIT("[{}]"), SP_FMT_STR(name));
  sp_str_builder_new_line(&writer->builder);
}

void sp_toml_writer_add_string(sp_toml_writer_t* writer, sp_str_t key, sp_str_t value) {
  sp_str_builder_append_fmt(&writer->builder, SP_LIT("{} = {}"), SP_FMT_QUOTED_STR(key), SP_FMT_STR(value));
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
}
void spn_cli_command_add(spn_cli_t* cli) {
  spn_cli_add_t* add = &cli->add;
  struct argparse_option options [] = {
    OPT_HELP(),
    OPT_STRING('p', "package", &add->package, SP_NULLPTR, SP_NULLPTR),
    OPT_END()
  };

  const c8* const usages [] = {
    "spn add [options] [url]",
    SP_NULLPTR
  };

  struct argparse argparse;
  argparse_init(&argparse, options, usages, SP_NULL);
  cli->num_args = argparse_parse(&argparse, cli->num_args, cli->args);

  if (!cli->num_args) {
    argparse_usage(&argparse);
    exit(0);
  }

  // Read existing project if it exists
  sp_str_t toml_path = sp_os_join_path(app.paths.project, SP_LIT("spn.toml"));
  if (!sp_os_does_path_exist(toml_path)) {
    SP_FATAL("Expected project TOML file at {}, but it did not exist", SP_FMT_STR(toml_path));
  }

  if (!spn_project_read(&app.project, toml_path)) {
    SP_FATAL("Failed to read project TOML file at {}", SP_FMT_STR(toml_path));
  }

  // Get the repository identifier/URL
  add->package = SP_CSTR(cli->args[0]);
  sp_str_t url =  SP_ZERO_INITIALIZE();
  sp_str_t name = SP_ZERO_INITIALIZE();

  // Check if it's a URL or simple identifier
  bool is_url = false;
  for (u32 i = 0; i < add->package.len; i++) {
    if (add->package.data[i] == ':') {
      is_url = true;
      break;
    }
  }

  if (is_url) {
    url = add->package;

    sp_str_t file_name = sp_os_extract_file_name(url);
    sp_str_t extension = SP_LIT(".git");
    SP_ASSERT(sp_str_ends_with(file_name, extension));
    name = SP_REVERSE_SUBSTR(file_name, 0, extension.len);
  }
  else {
    name = add->package;

    sp_str_builder_t builder = SP_ZERO_INITIALIZE();
    sp_str_builder_append(&builder, SP_LIT("https://github.com/"));

    // Map common packages to their repos
    if (sp_str_equal(name, SP_LIT("sdl"))) {
      sp_str_builder_append(&builder, SP_LIT("libsdl-org/SDL"));
    } else if (sp_str_equal(name, SP_LIT("imgui"))) {
      sp_str_builder_append(&builder, SP_LIT("ocornut/imgui"));
    } else {
      // Default: assume github.com/{name}/{name}
      sp_str_builder_append(&builder, name);
      sp_str_builder_append_c8(&builder, '/');
      sp_str_builder_append(&builder, name);
    }
    sp_str_builder_append(&builder, SP_LIT(".git"));
    url = sp_str_builder_write(&builder);
  }

  // Check if repo already exists
  sp_str_t repo_path = sp_os_join_path(app.paths.repos, name);

  if (!sp_os_does_path_exist(repo_path)) {
    SP_LOG("Cloning {} to {}", SP_FMT_STR(url), SP_FMT_STR(repo_path));

    // Clone the repository
    const c8* git_args[] = {
      "git",
      "clone",
      sp_str_to_cstr(url),
      sp_str_to_cstr(repo_path),
      SP_NULLPTR
    };

    SDL_Process* process = SDL_CreateProcess(git_args, false);
    if (!process) {
      SP_FATAL("Failed to start git clone");
    }

    s32 exitcode = 0;
    if (!SDL_WaitProcess(process, true, &exitcode) || exitcode != 0) {
      SDL_DestroyProcess(process);
      SP_FATAL("Failed to clone repository");
    }

    SDL_DestroyProcess(process);

    SP_LOG("Successfully cloned {}", SP_FMT_STR(name));
  } else {
    SP_LOG("Repository {} already exists", SP_FMT_STR(name));
  }

  // Add to dependencies if not already present
  if (!app.project.dependencies) {
    app.project.dependencies = sp_dyn_array_new(spn_dependency_t);
  }

  bool already_exists = false;
  sp_dyn_array_for(app.project.dependencies, i) {
    if (sp_str_equal(app.project.dependencies[i].name, name)) {
      already_exists = true;
      break;
    }
  }

  if (!already_exists) {
    spn_dependency_t dep = {
      .name = name,
      .url = url
    };
    sp_dyn_array_push(app.project.dependencies, dep);

    // Save project
    if (!spn_project_write(&app.project, toml_path)) {
      SP_FATAL("Failed to write project TOML file");
    }

    SP_LOG("Added dependency {} to spn.toml", SP_FMT_STR(name));
  } else {
    SP_LOG("Dependency {} already in project", SP_FMT_STR(name));
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

  sp_str_t toml_path = sp_str_lit("spn.toml");
  if (!spn_project_write(&app.project, sp_os_join_path(app.paths.project, SP_LIT("spn.toml")))) {
    SP_FATAL("Failed to write project TOML file");
  }

  SP_LOG("Initialized project {} in spn.toml", SP_FMT_STR(app.project.name));
}

void spn_git_clone(sp_str_t url, sp_str_t target) {
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

sp_str_t spn_git_find_head(sp_str_t repo) {
  SDL_Process* process = SDL_CreateProcess(
    (const c8* []) {
      "git",
      "rev-parse",
      "--abbrev-ref",
      "HEAD",
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

  return sp_str_strip_right(output);
}

void spn_app_init(spn_app_t* app) {
  app->paths.executable = sp_os_get_executable_path();
  app->paths.install = sp_os_parent_path(app->paths.executable);

  c8* working_directory = SDL_GetCurrentDirectory();
  app->paths.project = sp_os_canonicalize_path(SP_CSTR(working_directory));
  SDL_free(working_directory);

  const c8* xdg = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "XDG_CACHE_HOME");
  const c8* home = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "HOME");
  if (xdg) {
    app->paths.cache = sp_str_join(SP_CSTR(xdg), SP_LIT("spn"), SP_LIT("/"));
  }
  else if (home) {
    app->paths.cache = sp_str_join(SP_CSTR(home), SP_LIT(".cache/spn"), SP_LIT("/"));
  }

  app->paths.repos = sp_os_join_path(app->paths.cache, SP_LIT("repos"));
  app->paths.builds = sp_os_join_path(app->paths.cache, SP_LIT("builds"));
  app->paths.spn = sp_os_join_path(app->paths.repos, SP_LIT("spn"));
  app->paths.recipes = sp_os_join_path(app->paths.spn, SP_LIT("asset/recipes"));

  sp_os_create_directory(app->paths.cache);
  sp_os_create_directory(app->paths.repos);
  sp_os_create_directory(app->paths.builds);

  if (!sp_os_does_path_exist(app->paths.spn)) {
    spn_git_clone(SP_LIT("https://github.com/spaderthomas/spn.git"), app->paths.spn);
  }

  sp_str_t head = spn_git_find_head(app->paths.spn);

  sp_os_directory_entry_list_t entries = sp_os_scan_directory(app->paths.recipes);
  for (sp_os_directory_entry_t* entry = entries.data; entry < entries.data + entries.count; entry++) {
    spn_dependency_t builtin = {
      .kind = SPN_DEPENDENCY_KIND_BUILTIN,
      .name = sp_os_path_stem(entry->file_name),
      .url = SP_LIT("")
    };
    sp_dyn_array_push(app->builtins, builtin);
  }

  sp_dyn_array_for(app->builtins, index) {
    SP_LOG("{}: {}", app->builtins[index].name, app->builtins[index].url);
  }
}

bool spn_project_write(spn_project_t* project, sp_str_t path) {
  sp_toml_writer_t writer = SP_ZERO_INITIALIZE();

  sp_toml_writer_add_header(&writer, sp_str_lit("project"));
  sp_toml_writer_add_string(&writer, sp_str_lit("name"), project->name);

  // Write dependencies
  if (project->dependencies) {
    sp_dyn_array_for(project->dependencies, i) {
      spn_dependency_t* dep = &project->dependencies[i];

      sp_str_builder_t header_builder = SP_ZERO_INITIALIZE();
      sp_str_builder_append(&header_builder, SP_LIT("deps."));
      sp_str_builder_append(&header_builder, dep->name);
      sp_str_t header = sp_str_builder_write(&header_builder);

      sp_toml_writer_add_header(&writer, header);
      sp_toml_writer_add_string(&writer, sp_str_lit("url"), dep->url);
    }
  }

  sp_str_t content = sp_toml_writer_write(&writer);
  c8* path_cstr = sp_str_to_cstr(path);

  bool result = SDL_SaveFile(path_cstr, content.data, content.len);

  return result;
}

bool spn_project_read(spn_project_t* project, sp_str_t path) {
  c8* path_cstr = sp_str_to_cstr(path);
  size_t file_size;
  void* file_data = SDL_LoadFile(path_cstr, &file_size);

  if (!file_data) {
    SP_FATAL("Failed to read project file at {}", SP_FMT_STR(path));
  }

  c8 errbuf[256];
  toml_table_t* conf = toml_parse((c8*)file_data, errbuf, sizeof(errbuf));
  SDL_free(file_data);

  if (!conf) {
    SP_LOG("Failed to parse TOML: {}", SP_FMT_CSTR(errbuf));
    return false;
  }

  toml_table_t* project_table = toml_table_table(conf, "project");
  if (project_table) {
    toml_value_t name_val = toml_table_string(project_table, "name");
    if (name_val.ok) {
      project->name = sp_str_copy_cstr(name_val.u.s);
      SDL_free(name_val.u.s);
    }
  }

  // Read dependencies
  toml_table_t* deps_table = toml_table_table(conf, "deps");
  if (deps_table) {
    if (!project->dependencies) {
      project->dependencies = sp_dyn_array_new(spn_dependency_t);
    }

    for (s32 i = 0; ; i++) {
      s32 keylen = 0;
      const c8* key = toml_table_key(deps_table, i, &keylen);
      if (!key) break;

      toml_table_t* dep_table = toml_table_table(deps_table, key);
      if (dep_table) {
        spn_dependency_t dep = SP_ZERO_STRUCT(spn_dependency_t);
        dep.name = sp_str_copy(sp_str(key, keylen));

        toml_value_t url_val = toml_table_string(dep_table, "url");
        if (url_val.ok) {
          dep.url = sp_str_copy_cstr(url_val.u.s);
          SDL_free(url_val.u.s);
        }

        sp_dyn_array_push(project->dependencies, dep);
      }
    }
  }

  toml_free(conf);
  return true;
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
