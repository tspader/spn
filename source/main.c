#define SP_IMPLEMENTATION
#define SP_OS_BACKEND_SDL
#include "sp/sp.h"

#define ARGPARSE_IMPLEMENTATION
#include "argparse/argparse.h"

#include "toml/toml.h"

/*
 * pull in argparse and make:
 *   spn add sdl
 *   spn init
 *
 * [deps.sdl]
 * url = "https://github.com/libsdl-org/SDL"
 * branch = "main"
 * commit = "abc123" // or this
 *
 * default to main, check it out in ~/.cache/spn/repos/SDL
 * recipes
*/



/////////
// CLI //
/////////
typedef struct {
  const c8* package;
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

typedef struct {
  sp_str_t executable;
  sp_str_t install;
  sp_str_t cache;
  sp_str_t project;
} spn_paths_t;

typedef struct {
  sp_str_t name;
} spn_project_t;

typedef struct {
  spn_cli_t cli;
  spn_paths_t paths;
  spn_project_t project;
} spn_app_t;

spn_app_t app;

void spn_app_init(spn_app_t* app);
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

void sp_toml_writer_add_header(sp_toml_writer_t* writer, sp_str_t name) {
  sp_str_builder_append_fmt(&writer->builder, SP_LIT("[{}]"), SP_FMT_STR(name));
  sp_str_builder_new_line(&writer->builder);
}

void sp_toml_writer_add_string(sp_toml_writer_t* writer, sp_str_t key, sp_str_t value) {
  sp_str_builder_append_fmt(&writer->builder, SP_LIT("{} = {}"), SP_FMT_STR(key), SP_FMT_STR(value));
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

void spn_app_init(spn_app_t* app) {
  app->paths.executable = sp_os_get_executable_path();
  app->paths.install = sp_os_parent_path(app->paths.executable);

  const c8* xdg = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "XDG_CACHE_HOME");
  const c8* home = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "HOME");
  if (xdg) {
    app->paths.cache = sp_str_join(SP_CSTR(xdg), SP_LIT("spn"), SP_LIT("/"));
  }
  else if (home) {
    app->paths.cache = sp_str_join(SP_CSTR(home), SP_LIT(".cache/spn"), SP_LIT("/"));
  }

  c8* working_directory = SDL_GetCurrentDirectory();
  app->paths.project = sp_os_canonicalize_path(SP_CSTR(working_directory));

  SDL_free(working_directory);

  sp_os_create_directory(app->paths.cache);
}

bool spn_project_write(spn_project_t* project, sp_str_t path) {
  sp_toml_writer_t writer = SP_ZERO_INITIALIZE();
  sp_toml_writer_init(&writer);

  sp_toml_writer_add_header(&writer, sp_str_lit("project"));
  sp_toml_writer_add_string(&writer, sp_str_lit("name"), project->name);

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
    return false;
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
