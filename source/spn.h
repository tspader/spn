#ifndef SPN_H
#define SPN_H

#define SP_IMPLEMENTATION
#define SP_OS_BACKEND_SDL
#include "sp.h"

#define ARGPARSE_IMPLEMENTATION
#include "argparse.h"

#define TOML_IMPLEMENTATION
#include "toml/toml.h"

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>


///////////
// SHELL //
///////////
#define SPN_SH(...) SDL_CreateProcess((const c8* []) { __VA_ARGS__, SP_NULLPTR }, SP_SDL_PIPE_STDIO)

typedef struct {
  sp_str_t output;
  s32 return_code;
} spn_sh_process_result_t;

typedef struct {
  SDL_Process* process;
  spn_sh_process_result_t result;
} spn_sh_process_context_t;

typedef struct {
  sp_str_t makefile;
  sp_str_t target;
  sp_str_t working_directory;
  SDL_PropertiesID shell;
  sp_dyn_array(const c8*) args;
  SDL_Process* process;
  spn_sh_process_result_t result;
} spn_sh_make_context_t;

void spn_sh_make(spn_sh_make_context_t* make);
sp_str_t spn_sh_build_make_error(spn_sh_make_context_t* make);


/////////
// TUI //
/////////
#define SPN_TUI_NUM_OPTIONS 3
#define SP_TUI_PRINT(command) printf("%s", command)

typedef struct spn_build_context_t spn_build_context_t;

typedef enum {
  SPN_TUI_STATE_INTERACTIVE,
  SPN_TUI_STATE_NONINTERACTIVE
} spn_tui_state_t;

typedef struct {
  spn_tui_state_t state;
  bool* terminal_reported;
  u32 num_deps;
  u32 width;

  struct {
    struct termios ios;
    bool modified;
  } terminal;
} spn_tui_t;

void spn_tui_update_noninteractive(spn_tui_t* tui);
void spn_tui_update_interactive(spn_tui_t* tui);
void spn_tui_update(spn_tui_t* tui);
void spn_tui_read(spn_tui_t* tui);
void spn_tui_init(spn_tui_t* tui, spn_tui_state_t);
void spn_tui_cleanup(spn_tui_t* tui);

spn_sh_process_result_t spn_sh_read_process(SDL_Process* process);
sp_str_t                spn_git_fetch(sp_str_t repo);
u32                     spn_git_num_updates(sp_str_t repo, sp_str_t from, sp_str_t to);
void                    spn_git_checkout(sp_str_t repo, sp_str_t commit);

#define SPN_GIT_ORIGIN_HEAD SP_LIT("origin/HEAD")
#define SPN_GIT_HEAD SP_LIT("HEAD")
#define SPN_GIT_UPSTREAM SP_LIT("@{u}")

/////////
// CLI //
/////////
typedef struct {
  sp_str_t package;
} spn_cli_add_t;

typedef struct {
} spn_cli_init_t;

typedef struct {
} spn_cli_list_t;

typedef enum {
  SPN_FLAG_INCLUDE,
  SPN_FLAG_LIB_INCLUDE,
  SPN_FLAG_LIBS,
  SPN_FLAG_COUNT
} spn_cli_flag_kind_t;

typedef struct {
  spn_cli_flag_kind_t kind;
  const c8* package;
} spn_cli_flags_t;

typedef struct {
  u32 num_args;
  const c8** args;
  const c8* project_directory;
  bool no_interactive;
  bool lock;
  spn_cli_add_t add;
  spn_cli_init_t init;
  spn_cli_list_t list;
  spn_cli_flags_t flags;
} spn_cli_t;

void spn_cli_command_add(spn_cli_t* cli);
void spn_cli_command_init(spn_cli_t* cli);
void spn_cli_command_list(spn_cli_t* cli);
void spn_cli_command_nuke(spn_cli_t* cli);
void spn_cli_command_clean(spn_cli_t* cli);
void spn_cli_command_flags(spn_cli_t* cli);
void spn_cli_command_build(spn_cli_t* cli);

//////////////////
// DEPENDENCIES //
//////////////////
typedef enum {
  SPN_BUILD_KIND_DEBUG,
  SPN_BUILD_KIND_RELEASE,
} spn_dep_build_kind_t;

typedef enum {
  SPN_DEP_REPO_STATE_UNINITIALIZED,
  SPN_DEP_REPO_STATE_STALE,
  SPN_DEP_REPO_STATE_CLEAN,
} spn_dep_repo_state_t;

typedef enum {
  SPN_DEP_BUILD_STATE_IDLE,
  SPN_DEP_BUILD_STATE_CLONING,
  SPN_DEP_BUILD_STATE_FETCHING,
  SPN_DEP_BUILD_STATE_AWAITING_CONFIRMATION,
  SPN_DEP_BUILD_STATE_PULLING,
  SPN_DEP_BUILD_STATE_BUILDING,
  SPN_DEP_BUILD_STATE_DONE,
  SPN_DEP_BUILD_STATE_CANCELED,
  SPN_DEP_BUILD_STATE_FAILED
} spn_dep_build_state_t;

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
} spn_dep_parse_entry_t;

typedef struct {
  sp_str_t key;
  sp_str_t value;
} spn_dep_option_t;

typedef struct {
  sp_str_t name;
  sp_str_t path;
  sp_str_t url;
  sp_dyn_array(sp_str_t) libs;
} spn_dep_info_t;

typedef sp_str_t spn_dep_id_t;

typedef struct {
  sp_str_t commit;
  u32 num_commits;
} spn_dep_update_info_t;

typedef struct {
  spn_dep_id_t id;
  spn_dep_info_t* info;
  sp_dyn_array(spn_dep_option_t) options;
  spn_dep_build_kind_t kind;
  spn_dep_build_paths_t paths;
  sp_hash_t hash;
  sp_str_t build_id;
  SDL_PropertiesID shell;
  SDL_Environment* environment;

  SDL_IOStream* out;
  SDL_IOStream* err;

  sp_thread_t thread;
  sp_mutex_t mutex;
  spn_dep_build_state_t state;
  spn_dep_repo_state_t repo_state;
  spn_dep_update_info_t update;
  c8 confirm_keys[3];
  c8 user_choice;
  sp_dyn_array(sp_str_t) output;
  sp_str_t build_error;

} spn_dep_context_t;

typedef struct {
  sp_str_t name;
  sp_str_t url;
  sp_str_t commit;
  sp_str_t build_id;
} spn_lock_entry_t;

typedef sp_dyn_array(spn_lock_entry_t) spn_lock_file_t;

struct spn_build_context_t {
  sp_dyn_array(spn_dep_context_t) deps;
};

spn_dep_info_t*        spn_dep_find(sp_str_t name);
bool                   spn_dep_state_is_terminal(spn_dep_context_t* dep);
s32                    spn_dep_sort_kernel_alphabetical(const void* a, const void* b);
sp_str_t               spn_dep_read_url(sp_str_t file_path);
sp_dyn_array(sp_str_t) spn_dep_read_libs(sp_str_t file_path);
sp_str_t               spn_dep_option_env_name(spn_dep_option_t* option);
sp_str_t               spn_dep_build_state_to_str(spn_dep_build_state_t state);
spn_lock_entry_t*      spn_dep_context_get_lock_entry(spn_dep_context_t* dep);
void                   spn_dep_context_build(spn_dep_context_t* context);
void                   spn_dep_context_clone(spn_dep_context_t* context);
void                   spn_dep_context_prepare(spn_dep_context_t* context);
void                   spn_dep_context_add_options(spn_dep_context_t* context, toml_table_t* toml);
spn_dep_context_t      spn_dep_context_from_default_profile(sp_str_t name);
void                   spn_dep_context_set_env_var(spn_dep_context_t* context, sp_str_t name, sp_str_t value);
void                   spn_dep_context_set_build_state(spn_dep_context_t* dep, spn_dep_build_state_t state);
void                   spn_dep_context_set_build_error(spn_dep_context_t* dep, sp_str_t error);
sp_str_t               spn_dep_context_make_flag(spn_dep_context_t* dep, spn_cli_flag_kind_t flag);
spn_build_context_t    spn_build_context_from_default_profile();
void                   spn_build_context_prepare(spn_build_context_t* context);

/////////
// APP //
/////////
typedef struct {
  toml_table_t* toml;
  bool auto_pull_recipes;
  bool auto_pull_deps;
  sp_str_t cache_override;
  sp_str_t spn_dir;  // Override path to spn repo for development
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
  sp_str_t   lock;
} spn_paths_t;

typedef struct {
  sp_str_t build;
  sp_str_t clone;
  sp_str_t libs;
  sp_str_t url;
} spn_targets_t;

typedef struct {
  sp_str_t name;
  sp_dyn_array(spn_dep_id_t) dependencies;
  toml_table_t* toml;
} spn_project_t;

typedef struct {
  spn_cli_t cli;
  spn_paths_t paths;
  spn_targets_t targets;
  spn_project_t project;
  spn_config_t config;
  spn_build_context_t build;
  spn_tui_t tui;
  sp_dyn_array(spn_dep_info_t) deps;
  sp_dyn_array(spn_lock_entry_t) lock;
  SDL_AtomicInt control;
} spn_app_t;

extern spn_app_t app;

void               spn_app_init(spn_app_t* app, u32 num_args, const c8** args);
void               spn_app_run(spn_app_t* app);
spn_dep_context_t* spn_project_find_dependency(spn_project_t* project, sp_str_t name);
bool               spn_project_write(spn_project_t* project, sp_str_t path);
sp_str_t           spn_git_get_remote_url(sp_str_t repo_path);
sp_str_t           spn_git_get_commit(sp_str_t repo_path);
bool               spn_lock_file_write(spn_lock_file_t* lock, sp_str_t path);
bool               spn_lock_file_read(spn_lock_file_t* lock, sp_str_t path);
void               spn_lock_file_from_deps(spn_lock_file_t* lock, spn_build_context_t* context);
bool               spn_project_read(spn_project_t* project, sp_str_t path);

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


////////////////////
// IMPLEMENTATION //
////////////////////
#ifdef SPN_IMPLEMENTATION

spn_app_t app;

// SHELL
void spn_sh_make(spn_sh_make_context_t* make) {
  sp_dyn_array_push(make->args, "make");
  sp_dyn_array_push(make->args, "--quiet");

  sp_dyn_array_push(make->args, "--include-dir");
  sp_dyn_array_push(make->args, sp_str_to_cstr(app.paths.recipes));

  sp_dyn_array_push(make->args, "--makefile");
  sp_dyn_array_push(make->args, sp_str_to_cstr(make->makefile));

  if (sp_str_valid(make->working_directory)) {
    sp_dyn_array_push(make->args, "--directory");
    sp_dyn_array_push(make->args, sp_str_to_cstr(make->working_directory));
  }

  sp_dyn_array_push(make->args, sp_str_to_cstr(make->target));
  sp_dyn_array_push(make->args, SP_NULLPTR);

  if (make->shell) {
    SDL_SetPointerProperty(make->shell, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void*)make->args);
    make->process = SDL_CreateProcessWithProperties(make->shell);
  }
  else {
    make->process = SDL_CreateProcess(make->args, SP_SDL_PIPE_STDIO);
  }
}

sp_str_t spn_sh_build_make_error(spn_sh_make_context_t* make) {
   return sp_format(
    "{:fg brightblack} returned {:color brightred}",
    SP_FMT_STR(sp_str_join_cstr_n(make->args, sp_dyn_array_size(make->args), SP_LIT(" "))),
    SP_FMT_S32(make->result.return_code)
  );
}

// TUI
void sp_tui_print(sp_str_t str) {
  printf("%.*s", str.len, str.data);
}

void sp_tui_up(u32 n) {
  sp_str_t command = sp_format("\033[{}A", SP_FMT_U32(n));
  sp_tui_print(command);
}

void sp_tui_down(u32 n) {
  sp_str_t command = sp_format("\033[{}B", SP_FMT_U32(n));
  sp_tui_print(command);
}

void sp_tui_clear_line() {
  SP_TUI_PRINT("\033[K");
}

void sp_tui_show_cursor() {
  SP_TUI_PRINT("\033[?25h");
}

void sp_tui_hide_cursor() {
  SP_TUI_PRINT("\033[?25l");
}

void sp_tui_home() {
  SP_TUI_PRINT("\033[0G");
}

void sp_tui_flush() {
  fflush(stdout);
}

// TOML
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
  sp_str_builder_append_fmt(&writer->builder, "[{}]", SP_FMT_STR(name));
  sp_str_builder_new_line(&writer->builder);
}

void sp_toml_writer_add_string(sp_toml_writer_t* writer, sp_str_t key, sp_str_t value) {
  sp_str_builder_append_fmt(&writer->builder, "{} = {}", SP_FMT_STR(key), SP_FMT_QUOTED_STR(value));
  sp_str_builder_new_line(&writer->builder);
}

void sp_toml_writer_add_s32(sp_toml_writer_t* writer, sp_str_t key, s32 value) {
  sp_str_builder_append_fmt(&writer->builder, "{} = {}", SP_FMT_STR(key), SP_FMT_S32(value));
  sp_str_builder_new_line(&writer->builder);
}

void sp_toml_writer_add_bool(sp_toml_writer_t* writer, sp_str_t key, bool value) {
  sp_str_builder_append_fmt(&writer->builder, "{} = {}", SP_FMT_STR(key), value ? SP_FMT_CSTR("true") : SP_FMT_CSTR("false"));
  sp_str_builder_new_line(&writer->builder);
}

void sp_toml_writer_add_string_array(sp_toml_writer_t* writer, sp_str_t key, sp_dyn_array(sp_str_t) values) {
  sp_str_builder_append_fmt(&writer->builder, "{} = [", SP_FMT_STR(key));
  sp_dyn_array_for(values, i) {
    sp_str_builder_append_fmt(&writer->builder, "{}", SP_FMT_QUOTED_STR(values[i]));
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

// CLI
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
      "spn add <package>",
      "Add a package dependency to your project",
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

  spn_dep_info_t* dep = spn_dep_find(add->package);
  if (!dep) {
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
      "Initialize a new spn project in the current directory",
      SP_NULLPTR
    },
    SP_NULL
  );
  cli->num_args = argparse_parse(&argparse, cli->num_args, cli->args);

  // Check if project file already exists
  if (sp_os_does_path_exist(app.paths.toml)) {
    SP_LOG("Project already initialized at {}", SP_FMT_COLOR(SP_ANSI_FG_CYAN), SP_FMT_STR(app.paths.toml));
    return;
  }

  app.project = (spn_project_t){
    .name = sp_os_extract_file_name(app.paths.project)
  };

  if (!spn_project_write(&app.project, app.paths.toml)) {
    SP_FATAL("Failed to write project TOML file");
  }

  SP_LOG("Initialized project {:color cyan} in spn.toml", SP_FMT_QUOTED_STR(app.project.name));
}

void spn_cli_command_list(spn_cli_t* cli) {
  struct argparse argparse;
  argparse_init(
    &argparse,
    (struct argparse_option []) {
      OPT_HELP(),
      OPT_END()
    },
    (const c8* const []) {
      "spn list",
      "List all available recipe packages",
      SP_NULLPTR
    },
    SP_NULL
  );
  cli->num_args = argparse_parse(&argparse, cli->num_args, cli->args);

  u32 max_name_len = 0;
  sp_dyn_array_for(app.deps, i) {
    spn_dep_info_t* dep = app.deps + i;
    max_name_len = SP_MAX(max_name_len, dep->name.len);
  }

  sp_str_builder_t builder = SP_ZERO_INITIALIZE();
  sp_dyn_array_for(app.deps, i) {
    spn_dep_info_t* dep = app.deps + i;
    sp_str_builder_append_fmt(
      &builder,
      "{:fg cyan} {}",
      SP_FMT_STR(sp_str_pad(dep->name, max_name_len)),
      SP_FMT_STR(dep->url)
    );
    sp_str_builder_new_line(&builder);
  }

  sp_log(sp_str_builder_write(&builder));
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
      OPT_STRING('p', "package", &flags->package, "show flags only for a specific package", SP_NULLPTR),
      OPT_END()
    },
    (const c8* const []) {
      "spn flags <include|libs>",
      "Output compiler flags for dependencies (include paths, library paths)",
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

  // Determine which flag type we're generating
  spn_cli_flag_kind_t flag;
  bool is_all = false;
  if (sp_cstr_equal(cli->args[0], "include")) {
    flag = SPN_FLAG_INCLUDE;
  }
  else if (sp_cstr_equal(cli->args[0], "lib-include")) {
    flag = SPN_FLAG_LIB_INCLUDE;
  }
  else if (sp_cstr_equal(cli->args[0], "libs")) {
    flag = SPN_FLAG_LIBS;
  }
  else {
    sp_str_t requested_flag = SP_CSTR(cli->args[0]);
    SP_FATAL("Unknown flag {}; options are [include, lib-include, libs]",  SP_FMT_COLOR(SP_ANSI_FG_YELLOW), SP_FMT_QUOTED_STR(requested_flag));
  }

  sp_str_builder_t builder = SP_ZERO_INITIALIZE();
  sp_dyn_array_for(context.deps, i) {
    spn_dep_context_t* dep = context.deps + i;

    if (flags->package) {
      if (sp_str_equal_cstr(dep->id, flags->package)) {
        sp_str_builder_append(&builder, spn_dep_context_make_flag(dep, flag));
        printf("%s", sp_str_builder_write_cstr(&builder));
        return;
      }
    }
    else {
      sp_str_builder_append(&builder, spn_dep_context_make_flag(dep, flag));
      sp_str_builder_append_c8(&builder, ' ');
    }
  }

  if (flags->package) {
    SP_FATAL("Package {:color cyan} not found", SP_FMT_CSTR(flags->package));
  }

  if (builder.buffer.data) {
    printf("%s", sp_str_builder_write_cstr(&builder));
  }

  return;
}

sp_str_t spn_dep_context_make_flag(spn_dep_context_t* dep, spn_cli_flag_kind_t flag) {
  switch (flag) {
    case SPN_FLAG_INCLUDE:      return sp_format("-I{}", SP_FMT_STR(dep->paths.include));
    case SPN_FLAG_LIB_INCLUDE:  return sp_format("-L{}", SP_FMT_STR(dep->paths.bin));
    case SPN_FLAG_LIBS: {
      sp_str_builder_t builder = SP_ZERO_INITIALIZE();
      sp_dyn_array_for(dep->info->libs, index) {
        sp_str_builder_append_fmt(&builder, "-l{} ", SP_FMT_STR(dep->info->libs[index]));
      }

      return sp_str_builder_write(&builder);
    }
    default:                    { SP_UNREACHABLE_CASE(); }
  }
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
    .info = spn_dep_find(id),
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
  sp_dyn_array(spn_dep_parse_entry_t) entries = SP_NULLPTR;

  sp_dyn_array_push(entries, ((spn_dep_parse_entry_t) {
    .key = SP_LIT(""),
    .table = options
  }));

  while (sp_dyn_array_size(entries)) {
    spn_dep_parse_entry_t entry = *sp_dyn_array_back(entries);
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
        sp_dyn_array_push(entries, ((spn_dep_parse_entry_t) {
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
  sp_str_builder_append_fmt(&builder, "{}", SP_FMT_STR(context->id));
  sp_dyn_array_for(context->options, index) {
    spn_dep_option_t* option = &context->options[index];
  }
}

sp_str_t spn_dep_build_state_to_str(spn_dep_build_state_t state) {
  switch (state) {
    case SPN_DEP_BUILD_STATE_IDLE: return SP_LIT("idle");
    case SPN_DEP_BUILD_STATE_CLONING: return SP_LIT("cloning");
    case SPN_DEP_BUILD_STATE_FETCHING: return SP_LIT("fetching");
    case SPN_DEP_BUILD_STATE_AWAITING_CONFIRMATION: return SP_LIT("confirming");
    case SPN_DEP_BUILD_STATE_PULLING: return SP_LIT("pulling");
    case SPN_DEP_BUILD_STATE_BUILDING: return SP_LIT("building");
    case SPN_DEP_BUILD_STATE_DONE: return SP_LIT("done");
    case SPN_DEP_BUILD_STATE_CANCELED: return SP_LIT("canceled");
    case SPN_DEP_BUILD_STATE_FAILED: return SP_LIT("failed");
    default: SP_UNREACHABLE_RETURN(SP_LIT(""));
  }
}

bool spn_dep_state_is_terminal(spn_dep_context_t* dep) {
  switch (dep->state) {
    case SPN_DEP_BUILD_STATE_FAILED:
    case SPN_DEP_BUILD_STATE_DONE:
    case SPN_DEP_BUILD_STATE_CANCELED: return true;
    default: return false;
  }
}

sp_str_t spn_dep_read_url(sp_str_t makefile) {
  spn_sh_make_context_t make = {
    .makefile = makefile,
    .target = app.targets.url,
  };
  spn_sh_make(&make);

  if (!make.process) {
    SP_FATAL("Failed to start process reading URL for {:color cyan}", SP_FMT_STR(makefile));
  }

  make.result = spn_sh_read_process(make.process);
  if (make.result.return_code) {
    SP_FATAL("{}", SP_FMT_STR(spn_sh_build_make_error(&make)));
  }

  return sp_str_trim(make.result.output);
}

sp_dyn_array(sp_str_t) spn_dep_read_libs(sp_str_t makefile) {
  spn_sh_make_context_t make = {
    .makefile = makefile,
    .target = app.targets.libs,
  };
  spn_sh_make(&make);

  if (!make.process) {
    SP_FATAL("Failed to start process reading URL for {:color cyan}", SP_FMT_STR(makefile));
  }

  make.result = spn_sh_read_process(make.process);
  if (make.result.return_code) {
    SP_FATAL("{}", SP_FMT_STR(spn_sh_build_make_error(&make)));
  }

  return sp_str_split_c8(sp_str_trim(make.result.output), ' ');

}

spn_dep_info_t* spn_dep_find(sp_str_t name) {
  sp_dyn_array_for(app.deps, index) {
    spn_dep_info_t* dep = app.deps + index;
    if (sp_str_equal(dep->name, name)) {
      return dep;
    }
  }

  return SP_NULLPTR;
}

s32 spn_dep_sort_kernel_alphabetical(const void* a, const void* b) {
  spn_dep_info_t* da = (spn_dep_info_t*)a;
  spn_dep_info_t* db = (spn_dep_info_t*)b;
  return sp_str_sort_kernel_alphabetical(&da->name, &db->name);
}

sp_str_t spn_dep_option_env_name(spn_dep_option_t* option) {
  return SP_LIT("");
}

spn_sh_process_result_t spn_sh_read_process(SDL_Process* process) {
  spn_sh_process_result_t result = SP_ZERO_INITIALIZE();
  sp_size_t len = 0;

  // Try SDL_ReadProcess first - this works when stdio is piped to app
  void* output_data = SDL_ReadProcess(process, &len, &result.return_code);

  if (output_data) {
    result.output.data = (c8*)output_data;
    result.output.len = (u32)len;
  } else if (result.return_code == -1) {
    // SDL_ReadProcess returns -1 when IO is redirected
    // Use SDL_WaitProcess instead to get the actual exit code
    SDL_WaitProcess(process, true, &result.return_code);
    result.output = SP_LIT("");
  } else {
    result.output = SP_LIT("");
  }

  return result;
}

sp_str_t spn_git_fetch(sp_str_t repo_path) {
  SDL_Process* process = SPN_SH("git", "-C", sp_str_to_cstr(repo_path), "fetch", "--quiet");
  spn_sh_process_result_t result = spn_sh_read_process(process);
  SDL_DestroyProcess(process);
  return result.output;
}

u32 spn_git_num_updates(sp_str_t repo_path, sp_str_t from, sp_str_t to) {
  sp_str_t specifier = sp_format("{}..{}", SP_FMT_STR(from), SP_FMT_STR(to));
  SDL_Process* rev = SPN_SH("git", "-C", sp_str_to_cstr(repo_path), "rev-list", sp_str_to_cstr(specifier), "--count");
  spn_sh_process_result_t rev_result = spn_sh_read_process(rev);
  SDL_DestroyProcess(rev);

  sp_str_t trimmed = sp_str_trim_right(rev_result.output);
  return sp_parse_u32(trimmed);
}

sp_str_t spn_git_get_remote_url(sp_str_t repo_path) {
  SDL_Process* process = SPN_SH("git", "-C", sp_str_to_cstr(repo_path), "remote", "get-url", "origin");
  spn_sh_process_result_t result = spn_sh_read_process(process);
  SDL_DestroyProcess(process);
  if (result.return_code != 0) {
    return SP_LIT("");
  }
  return sp_str_trim_right(result.output);
}

sp_str_t spn_git_get_commit(sp_str_t repo_path) {
  SDL_Process* process = SPN_SH("git", "-C", sp_str_to_cstr(repo_path), "rev-parse", sp_str_to_cstr(SPN_GIT_HEAD));
  spn_sh_process_result_t result = spn_sh_read_process(process);
  SDL_DestroyProcess(process);
  if (result.return_code != 0) {
    return SP_LIT("");
  }
  return sp_str_trim_right(result.output);
}

void spn_git_checkout(sp_str_t repo, sp_str_t commit) {
  SDL_Process* process = SPN_SH("git", "-C", sp_str_to_cstr(repo), "checkout", "--quiet", sp_str_to_cstr(commit));
  spn_sh_process_result_t result = spn_sh_read_process(process);
  SDL_DestroyProcess(process);
}

void spn_tui_update_noninteractive(spn_tui_t* tui) {
  sp_dyn_array_for(app.build.deps, i) {
    spn_dep_context_t* dep = &app.build.deps[i];

    if (!tui->terminal_reported[i] && spn_dep_state_is_terminal(dep)) {
      tui->terminal_reported[i] = true;

      switch (dep->state) {
        case SPN_DEP_BUILD_STATE_DONE:
          SP_LOG("{} {:color green}", SP_FMT_STR(dep->id), SP_FMT_CSTR("done"));
          break;
        case SPN_DEP_BUILD_STATE_FAILED:
          SP_LOG("{} {:color red}: {}", SP_FMT_STR(dep->id), SP_FMT_CSTR("failed"), SP_FMT_STR(dep->build_error));
          break;
        case SPN_DEP_BUILD_STATE_CANCELED:
          SP_LOG("{} {:color yellow}", SP_FMT_STR(dep->id), SP_FMT_CSTR("canceled"));
          break;
        default:
          break;
      }
    }
  }
}

void spn_tui_update_interactive(spn_tui_t* tui) {
  sp_tui_up(sp_dyn_array_size(app.build.deps));

  sp_dyn_array_for(app.build.deps, index) {
    spn_dep_context_t* dep = app.build.deps + index;
    sp_str_builder_t builder = SP_ZERO_INITIALIZE();

    sp_mutex_lock(&dep->mutex);
    sp_str_t name = sp_str_pad(dep->id, tui->width);
    sp_str_t state = sp_str_pad(spn_dep_build_state_to_str(dep->state), 10);
    sp_str_t status;

    switch (dep->state) {
      case SPN_DEP_BUILD_STATE_CANCELED:
      case SPN_DEP_BUILD_STATE_FAILED: {
        sp_str_t error = dep->build_error.len ? dep->build_error : SP_LIT("No error reported");

        status = sp_format(
          "{} {:color brightred} {}",
          SP_FMT_STR(name),
          SP_FMT_STR(state),
          SP_FMT_STR(error)
        );
        break;
      }
      case SPN_DEP_BUILD_STATE_DONE: {
        status = sp_format(
          "{} {:color brightgreen}",
          SP_FMT_STR(name),
          SP_FMT_STR(state)
        );
        break;
      }
      case SPN_DEP_BUILD_STATE_AWAITING_CONFIRMATION: {
        status = sp_format(
          "{} {:color brightcyan} {} new commits; {:color brightyellow} to pull, {:color brightyellow} to cancel, {:color brightyellow} to pull + stop asking",
          SP_FMT_STR(name),
          SP_FMT_STR(state),
          SP_FMT_U32(dep->update.num_commits),
          SP_FMT_C8(dep->confirm_keys[0]),
          SP_FMT_C8(dep->confirm_keys[1]),
          SP_FMT_C8(dep->confirm_keys[2])
        );
        break;
      }
      default: {
        status = sp_format(
          "{} {:color brightcyan}",
          SP_FMT_STR(name),
          SP_FMT_STR(state)
        );
        break;
      }
    }
    sp_mutex_unlock(&dep->mutex);

    sp_tui_home();
    sp_tui_clear_line();
    sp_tui_print(status);
    sp_tui_down(1);
  }

  sp_tui_flush();
}

void spn_tui_cleanup(spn_tui_t* tui) {
  switch (tui->state) {
    case SPN_TUI_STATE_INTERACTIVE: {
      tcsetattr(STDIN_FILENO, TCSANOW, &tui->terminal.ios);
      sp_tui_show_cursor();
      sp_tui_home();
      sp_tui_flush();
      break;
    }
    default: {
      break;
    }
  }
}


void spn_tui_read(spn_tui_t* tui) {
  if (tui->state == SPN_TUI_STATE_NONINTERACTIVE) return;

  c8 input_char;
  if (read(STDIN_FILENO, &input_char, 1) == 1) {
    // Distribute input to waiting threads
    sp_dyn_array_for(app.build.deps, i) {
      spn_dep_context_t* dep = app.build.deps + i;
      if (dep->state != SPN_DEP_BUILD_STATE_AWAITING_CONFIRMATION) continue;

      for (u32 j = 0; j < SPN_TUI_NUM_OPTIONS; j++) {
        if (input_char == dep->confirm_keys[j]) {
          sp_mutex_lock(&dep->mutex);
          dep->user_choice = input_char;
          sp_mutex_unlock(&dep->mutex);
        }
      }
    }
  }
}

void spn_tui_init(spn_tui_t* tui, spn_tui_state_t state) {
  tui->state = state;
  tui->num_deps = sp_dyn_array_size(app.build.deps);
  tui->width = 0;
  sp_dyn_array_for(app.build.deps, i) {
    spn_dep_context_t* dep = &app.build.deps[i];
    tui->width = SP_MAX(tui->width, dep->id.len);
  }

  switch (tui->state) {
    case SPN_TUI_STATE_INTERACTIVE: {
      sp_dyn_array_for(app.build.deps, index) {
        sp_tui_print(SP_LIT("\n"));
      }
      sp_tui_hide_cursor();
      sp_tui_flush();

      // Set terminal to non-blocking mode
      tcgetattr(STDIN_FILENO, &tui->terminal.ios);
      struct termios ios = tui->terminal.ios;
      ios.c_lflag &= ~(ICANON | ECHO);
      tcsetattr(STDIN_FILENO, TCSANOW, &ios);
      fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
      tui->terminal.modified = true;

      break;
    }
    case SPN_TUI_STATE_NONINTERACTIVE: {
      tui->terminal_reported = (bool*)sp_alloc(tui->num_deps * sizeof(bool));
      SDL_memset(tui->terminal_reported, 0, tui->num_deps * sizeof(bool));

      // Print initial state for each dependency
      sp_dyn_array_for(app.build.deps, i) {
        spn_dep_context_t* dep = &app.build.deps[i];
        sp_str_t name = sp_str_pad(dep->id, tui->width);
        sp_str_t state_str = spn_dep_build_state_to_str(dep->state);
        SP_LOG("{} {:color cyan}", SP_FMT_STR(name), SP_FMT_STR(state_str));
      }
      break;
    }
  }
}

void spn_tui_update(spn_tui_t* tui) {
  switch (tui->state) {
    case SPN_TUI_STATE_INTERACTIVE:
      spn_tui_update_interactive(tui);
      break;
    case SPN_TUI_STATE_NONINTERACTIVE:
      spn_tui_update_noninteractive(tui);
      break;
  }
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

void spn_dep_context_prepare(spn_dep_context_t* context) {
  context->kind = SPN_BUILD_KIND_DEBUG;

  sp_dyn_array(sp_hash_t) hashes = SP_NULLPTR;

  sp_dyn_array_push(hashes, sp_hash_str(context->id));
  sp_dyn_array_push(hashes, sp_hash_bytes(&context->kind, sizeof(context->kind), 0));

  sp_dyn_array_for(context->options, index) {
    spn_dep_option_t* option = &context->options[index];
    sp_dyn_array_push(hashes, sp_hash_str(option->key));
    sp_dyn_array_push(hashes, sp_hash_str(option->value));
  }

  context->hash = sp_hash_combine(hashes, sp_dyn_array_size(hashes));
  context->build_id = sp_format("{}", SP_FMT_SHORT_HASH(context->hash));

  sp_str_t build = sp_os_join_path(app.paths.build, context->id);
  sp_str_t store = sp_os_join_path(app.paths.store, context->id);
  context->paths.source = sp_os_join_path(app.paths.source, context->id);
  context->paths.build = sp_os_join_path(build, context->build_id);
  context->paths.store = sp_os_join_path(store, context->build_id);
  context->paths.include = sp_os_join_path(context->paths.store, SP_LIT("include"));
  context->paths.bin = sp_os_join_path(context->paths.store, SP_LIT("bin"));
  context->paths.recipe = sp_os_join_path(app.paths.recipes, sp_format("{}.mk", SP_FMT_STR(context->id)));
  context->paths.std_out = sp_os_join_path(context->paths.build, SP_LIT("build.stdout"));
  context->paths.std_err = sp_os_join_path(context->paths.build, SP_LIT("build.stderr"));
  context->paths.std_in  = sp_os_join_path(context->paths.build, SP_LIT("build.stdin"));

  sp_os_create_directory(context->paths.build);
  sp_os_create_directory(context->paths.store);
  sp_os_create_directory(context->paths.include);
  sp_os_create_directory(context->paths.bin);
  context->out = SDL_IOFromFile(sp_str_to_cstr(context->paths.std_out), "w");
  context->err = SDL_IOFromFile(sp_str_to_cstr(context->paths.std_err), "w");

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

  SDL_SetPointerProperty(context->shell, SDL_PROP_PROCESS_CREATE_STDOUT_POINTER, context->out);
  SDL_SetNumberProperty(context->shell, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_REDIRECT);

  SDL_SetPointerProperty(context->shell, SDL_PROP_PROCESS_CREATE_STDERR_POINTER, context->err);
  SDL_SetNumberProperty(context->shell, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_REDIRECT);

  context->state = SPN_DEP_BUILD_STATE_IDLE;
  sp_mutex_init(&context->mutex, SP_MUTEX_PLAIN);

}

s32 spn_dep_context_build_async(void* user_data) {
  spn_dep_context_t* dep = (spn_dep_context_t*)user_data;

  if (sp_os_does_path_exist(dep->paths.source)) {
    // If the repository exists, we just nee
    spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_FETCHING);
    spn_git_fetch(dep->paths.source);

    spn_lock_entry_t* lock = spn_dep_context_get_lock_entry(dep);
    sp_str_t commit = lock ? lock->commit : SPN_GIT_ORIGIN_HEAD;
    spn_git_checkout(dep->paths.source, commit);
    dep->update.num_commits = spn_git_num_updates(dep->paths.source, commit, SPN_GIT_ORIGIN_HEAD);

    bool is_stale = dep->update.num_commits > 0;
    bool is_locked = app.cli.lock;

    if (is_stale) {
      if (is_locked) {
        dep->repo_state = SPN_DEP_REPO_STATE_CLEAN;
      }
      else {
        dep->repo_state = SPN_DEP_REPO_STATE_STALE;
      }
    }
    else {
      dep->repo_state = SPN_DEP_REPO_STATE_CLEAN;
    }
  }
  else {
    dep->repo_state = SPN_DEP_REPO_STATE_UNINITIALIZED;
  }

  switch (dep->repo_state) {
    case SPN_DEP_REPO_STATE_UNINITIALIZED: {
      spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_CLONING);

      spn_sh_make_context_t make = {
        .makefile = dep->paths.recipe,
        .target = app.targets.clone,
        .shell = dep->shell
      };
      spn_sh_make(&make);

      if (!make.process) {
        spn_dep_context_set_build_error(dep, sp_format(
          "Failed to spawn process for {:color cyan} {}",
          dep->info->name,
          SP_FMT_CSTR(SDL_GetError())
        ));

        return 1;
      }

      SDL_WaitProcess(make.process, true, &make.result.return_code);
      if (make.result.return_code > 0) {
        spn_dep_context_set_build_error(dep, spn_sh_build_make_error(&make));
        return make.result.return_code;
      }

      spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_CLONING);

      SDL_DestroyProcess(make.process);

      break;
    }
    case SPN_DEP_REPO_STATE_STALE: {
      if (!app.config.auto_pull_deps) {
        c8 key_offset = SDL_GetAtomicInt(&app.control);
        SDL_AddAtomicInt(&app.control, 3);
        dep->confirm_keys[0] = 'a' + key_offset;
        dep->confirm_keys[1] = 'a' + key_offset + 1;
        dep->confirm_keys[2] = 'a' + key_offset + 2;

        spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_AWAITING_CONFIRMATION);
        dep->user_choice = 0;

        while (true) {
          sp_mutex_lock(&dep->mutex);
          c8 choice = dep->user_choice;
          sp_mutex_unlock(&dep->mutex);

          if (choice == dep->confirm_keys[0]) {
            break;
          }
          else if (choice == dep->confirm_keys[2]) {
            app.config.auto_pull_deps = true;
            break;
          }
          else if (choice == dep->confirm_keys[1]) {
            spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_FAILED);
            return 1;
          }
          else {
            SDL_Delay(5);
          }
        }
      }

      spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_PULLING);
      spn_git_checkout(dep->paths.source, SPN_GIT_ORIGIN_HEAD);
    }
    case SPN_DEP_REPO_STATE_CLEAN: {
      break;
    }
  }

  spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_BUILDING);

  spn_sh_make_context_t make = {
    .makefile = dep->paths.recipe,
    .target = app.targets.build,
    .working_directory = dep->paths.build,
    .shell = dep->shell
  };
  spn_sh_make(&make);

  if (!make.process) {
    spn_dep_context_set_build_error(dep, sp_format(
      "Failed to spawn build process for {:color cyan} {}",
      dep->info->name,
      SP_FMT_CSTR(SDL_GetError())
    ));

    return 1;
  }

  make.result = spn_sh_read_process(make.process);

  if (make.result.return_code != 0) {
    spn_dep_context_set_build_error(dep, spn_sh_build_make_error(&make));
    return make.result.return_code;
  }

  SDL_DestroyProcess(make.process);

  spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_DONE);

  return 0;
}

void spn_dep_context_set_build_state(spn_dep_context_t* dep, spn_dep_build_state_t state) {
  sp_mutex_lock(&dep->mutex);
  dep->state = state;
  sp_mutex_unlock(&dep->mutex);
}

void spn_dep_context_set_build_error(spn_dep_context_t* dep, sp_str_t error) {
  sp_mutex_lock(&dep->mutex);
  dep->state = SPN_DEP_BUILD_STATE_FAILED;
  dep->build_error = sp_str_copy(error);
  sp_mutex_unlock(&dep->mutex);
}

spn_lock_entry_t* spn_dep_context_get_lock_entry(spn_dep_context_t* dep) {
  sp_dyn_array_for(app.lock, index) {
    spn_lock_entry_t* lock = app.lock + index;
    if (sp_str_equal(lock->name, dep->id)) {
      return lock;
    }
  }

  return SP_NULLPTR;
}

void spn_app_init(spn_app_t* app, u32 num_args, const c8** args) {
  spn_cli_t* cli = &app->cli;

  struct argparse_option options [] = {
    OPT_HELP(),
    OPT_STRING('C', "project-dir", &cli->project_directory, "specify the directory containing spn.toml", SP_NULLPTR),
    OPT_BOOLEAN('n', "no-interactive", &cli->no_interactive, "disable interactive tui", SP_NULLPTR),
    OPT_BOOLEAN('l', "lock", &cli->lock, "use commits from lockfile without prompting for updates", SP_NULLPTR),
    OPT_END(),
  };

  const c8* const usages [] = {
    "spn <command> [options]\n"
    "\n"
    "A modern C/C++ package manager and build tool\n"
    "\n"
    "Commands:\n"
    "  init           Initialize a new spn project in the current directory\n"
    "  add <pkg>      Add a package dependency to your project\n"
    "  build          Build all project dependencies\n"
    "  list           List all available packages\n"
    "  flags <type>   Output compiler flags for dependencies\n"
    "  clean          Remove build and store directories\n"
    "  nuke           Remove all spn data (cache and project file)",
    SP_NULLPTR
  };

  struct argparse argparse;
  argparse_init(&argparse, options, usages, ARGPARSE_STOP_AT_NON_OPTION);

  cli->args = args;
  cli->num_args = num_args;
  cli->num_args = argparse_parse(&argparse, cli->num_args, cli->args);

  if (!cli->num_args || !cli->args[0]) {
    argparse_usage(&argparse);
    exit(0);
  }

  SDL_SetAtomicInt(&app->control, 0);

  app->targets = (spn_targets_t) {
    .build = SP_LIT("spn-build"),
    .clone = SP_LIT("spn-clone"),
    .libs = SP_LIT("spn-package-libs"),
    .url = SP_LIT("spn-package-url")
  };

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
  app->paths.lock = sp_os_join_path(app->paths.project, SP_LIT("spn.lock"));

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

  // Use cache_override from config if set, otherwise use default locations
  if (sp_str_valid(app->config.cache_override)) {
    app->paths.cache = sp_str_copy(app->config.cache_override);
  }
  else if (xdg_cache) {
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

  sp_os_create_directory(app->paths.cache);
  sp_os_create_directory(app->paths.source);
  sp_os_create_directory(app->paths.build);

  spn_config_read(&app->config, app->paths.user_toml);

  if (app->config.spn_dir.len > 0) {
    app->paths.bootstrap = sp_str_copy(app->config.spn_dir);
  } else {
    app->paths.bootstrap = sp_os_join_path(app->paths.source, SP_LIT("spn"));
  }
  app->paths.recipes = sp_os_join_path(app->paths.bootstrap, SP_LIT("asset/recipes"));

  if (!app->config.spn_dir.len) {
    const c8* url = "https://github.com/tspader/spn.git";
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
      u32 num_updates = spn_git_num_updates(app->paths.bootstrap, SPN_GIT_HEAD, SPN_GIT_UPSTREAM);
      if (num_updates > 0) {
        if (app->config.auto_pull_recipes) {
          SP_LOG("Updating spn recipes ({} commits behind)...", SP_FMT_U32(num_updates));
          spn_git_checkout(app->paths.bootstrap, SPN_GIT_ORIGIN_HEAD);
        }
        else {
          SP_LOG("spn has {} recipe updates available (auto_pull_recipes=false)", SP_FMT_U32(num_updates));
        }
      }
    }
  }
  else {
    if (!sp_os_does_path_exist(app->paths.bootstrap)) {
      SP_FATAL("Custom spn_dir {} does not exist", SP_FMT_STR(app->paths.bootstrap));
    }
    if (!sp_os_does_path_exist(app->paths.recipes)) {
      SP_FATAL("Custom spn_dir {} does not contain asset/recipes folder", SP_FMT_STR(app->paths.bootstrap));
    }
  }

  // Read all the recipe files from disk
  sp_os_directory_entry_list_t entries = sp_os_scan_directory(app->paths.recipes);
  for (sp_os_directory_entry_t* entry = entries.data; entry < entries.data + entries.count; entry++) {

    spn_dep_info_t dep = {
      .name = sp_os_extract_stem(entry->file_name),
      .path = sp_str_copy(entry->file_path),
      .url = spn_dep_read_url(entry->file_path),
      .libs = spn_dep_read_libs(entry->file_path)
    };
    sp_dyn_array_push(app->deps, dep);
  }

  qsort(app->deps, sp_dyn_array_size(app->deps), sizeof(spn_dep_info_t), spn_dep_sort_kernel_alphabetical);

  bool needs_project = true;
  if (cli->num_args) {
    if (sp_cstr_equal(cli->args[0], "init") ||
        sp_cstr_equal(cli->args[0], "list") ||
        sp_cstr_equal(cli->args[0], "nuke") ||
        sp_cstr_equal(cli->args[0], "clean")) {
      needs_project = false;
    }
  }

  if (needs_project) {
    if (!sp_os_does_path_exist(app->paths.toml)) {
      SP_FATAL("Expected project TOML file at {:color cyan}, but it did not exist", SP_FMT_STR(app->paths.toml));
    }

    if (!spn_project_read(&app->project, app->paths.toml)) {
      SP_FATAL("Failed to read project TOML file at {:color cyan}", SP_FMT_STR(app->paths.toml));
    }
  }
}

void spn_app_run(spn_app_t* app) {
  spn_cli_t* cli = &app->cli;

  if (!cli->num_args || !cli->args || !cli->args[0]) {
    SP_ASSERT(false);
  }
  else if (sp_cstr_equal("add", cli->args[0])) {
    spn_cli_command_add(cli);
  }
  else if (sp_cstr_equal("init", cli->args[0])) {
    spn_cli_command_init(cli);
  }
  else if (sp_cstr_equal("list", cli->args[0])) {
    spn_cli_command_list(cli);
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
    spn_cli_command_build(cli);
  }
}

void spn_config_read(spn_config_t* config, sp_str_t path) {
  size_t file_size;
  c8* file_data = (c8*)SDL_LoadFile(sp_str_to_cstr(path), &file_size);
  if (!file_data) {
    spn_config_read_from_string(config, SP_LIT(""));
    return;
  }

  sp_str_t content = sp_str_from_cstr_sized(file_data, file_size);
  spn_config_read_from_string(config, content);
  SDL_free(file_data);
}

void spn_config_read_from_string(spn_config_t* config, sp_str_t toml_content) {
  *config = (spn_config_t) {
    .auto_pull_recipes = false,
    .auto_pull_deps = false,
    .toml = SP_NULLPTR,
    .cache_override = SP_ZERO_INITIALIZE(),
    .spn_dir = SP_ZERO_INITIALIZE(),
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
    TOML_READ_BOOL(options, config->auto_pull_deps, "auto_pull_deps");
    TOML_READ_BOOL(options, config->builtin_recipes_enabled, "builtin_recipes_enabled");

    toml_value_t cache_override = toml_table_string(options, "cache_override");
    if (cache_override.ok) {
      config->cache_override = sp_str_from_cstr(cache_override.u.s);
    }

    toml_value_t spn_dir = toml_table_string(options, "spn_dir");
    if (spn_dir.ok) {
      config->spn_dir = sp_str_from_cstr(spn_dir.u.s);
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

void spn_lock_file_from_deps(spn_lock_file_t* lock, spn_build_context_t* context) {
  sp_dyn_array_clear(*lock);

  sp_dyn_array_for(context->deps, i) {
    spn_dep_context_t* dep = &context->deps[i];

    spn_lock_entry_t entry = SP_ZERO_INITIALIZE();
    entry.name = sp_str_copy(dep->id);
    entry.url = spn_git_get_remote_url(dep->paths.source);
    entry.commit = spn_git_get_commit(dep->paths.source);
    entry.build_id = sp_str_copy(dep->build_id);

    sp_dyn_array_push(*lock, entry);
  }
}

bool spn_lock_file_write(spn_lock_file_t* lock, sp_str_t path) {
  sp_str_builder_t builder = SP_ZERO_INITIALIZE();

  sp_dyn_array_for(*lock, i) {
    spn_lock_entry_t* entry = (*lock) + i;

    sp_str_builder_append(&builder, SP_LIT("[[package]]"));
    sp_str_builder_new_line(&builder);
    sp_str_builder_append_fmt(&builder, "name = {}", SP_FMT_QUOTED_STR(entry->name));
    sp_str_builder_new_line(&builder);
    sp_str_builder_append_fmt(&builder, "git_url = {}", SP_FMT_QUOTED_STR(entry->url));
    sp_str_builder_new_line(&builder);
    sp_str_builder_append_fmt(&builder, "git_commit = {}", SP_FMT_QUOTED_STR(entry->commit));
    sp_str_builder_new_line(&builder);
    sp_str_builder_append_fmt(&builder, "build_id = {}", SP_FMT_QUOTED_STR(entry->build_id));
    sp_str_builder_new_line(&builder);
    sp_str_builder_new_line(&builder);
  }

  sp_str_t content = sp_str_builder_write(&builder);

  SDL_IOStream* io = SDL_IOFromFile(sp_str_to_cstr(path), "w");
  SDL_WriteIO(io, content.data, content.len);
  SDL_CloseIO(io);
  return true;
}

bool spn_lock_file_read(spn_lock_file_t* lock, sp_str_t path) {
  size_t file_size;
  void* file_data = SDL_LoadFile(sp_str_to_cstr(path), &file_size);

  if (!file_data) {
    return false;
  }

  c8 err_buf[256];
  toml_table_t* toml = toml_parse((c8*)file_data, err_buf, sizeof(err_buf));
  SDL_free(file_data);

  if (!toml) {
    SP_LOG("Warning: Failed to parse lock file: {}", SP_FMT_CSTR(err_buf));
    return false;
  }

  toml_array_t* packages = toml_table_array(toml, "package");
  if (!packages) {
    return false;
  }

  for (u32 index = 0; index < toml_array_len(packages); index++) {
    toml_table_t* package = toml_array_table(packages, index);

    spn_lock_entry_t entry = SP_ZERO_INITIALIZE();

    toml_value_t name = toml_table_string(package, "name");
    SP_ASSERT(name.ok);
    entry.name = sp_str_from_cstr(name.u.s);

    toml_value_t git_url = toml_table_string(package, "git_url");
    SP_ASSERT(git_url.ok);
    entry.url = sp_str_from_cstr(git_url.u.s);

    toml_value_t git_commit = toml_table_string(package, "git_commit");
    SP_ASSERT(git_commit.ok);
    entry.commit = sp_str_from_cstr(git_commit.u.s);

    toml_value_t build_id = toml_table_string(package, "build_id");
    SP_ASSERT(build_id.ok);
    entry.build_id = sp_str_from_cstr(build_id.u.s);

    sp_dyn_array_push(*lock, entry);
  }

  toml_free(toml);
  return true;
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

void spn_cli_command_build(spn_cli_t* cli) {
  app.build = spn_build_context_from_default_profile();

  spn_lock_file_read(&app.lock, app.paths.lock);

  spn_build_context_prepare(&app.build);

  sp_dyn_array_for(app.build.deps, index) {
    spn_dep_context_t* dep = app.build.deps + index;
    sp_thread_init(&dep->thread, spn_dep_context_build_async, dep);
  }

  spn_tui_init(&app.tui, app.cli.no_interactive ? SPN_TUI_STATE_NONINTERACTIVE : SPN_TUI_STATE_INTERACTIVE);

  while (true) {
    bool building = false;

    sp_dyn_array_for(app.build.deps, index) {
      spn_dep_context_t* dep = app.build.deps + index;

      sp_mutex_lock(&dep->mutex);
      if (!spn_dep_state_is_terminal(dep)) {
        building = true;
      }
      sp_mutex_unlock(&dep->mutex);
    }

    spn_tui_read(&app.tui);
    spn_tui_update(&app.tui);

    if (!building) break;
    SDL_Delay(5);
  }

  spn_tui_cleanup(&app.tui);

  // Generate lock file after successful build
  bool all_succeeded = true;
  sp_dyn_array_for(app.build.deps, i) {
    spn_dep_context_t* dep = &app.build.deps[i];
    if (dep->state != SPN_DEP_BUILD_STATE_DONE) {
      all_succeeded = false;
      break;
    }
  }

  if (all_succeeded) {
    spn_lock_file_from_deps(&app.lock, &app.build);
    if (!spn_lock_file_write(&app.lock, app.paths.lock)) {
      SP_LOG("Warning: Failed to write lock file to {:color cyan}", SP_FMT_STR(app.paths.lock));
    } else {
      SP_LOG("Generated lock file: {:color cyan}", SP_FMT_STR(app.paths.lock));
    }
  }
}

#endif // SPN_IMPLEMENTATION

#endif // SPN_H
