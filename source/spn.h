#ifndef SPN_H
#define SPN_H

#ifdef _WIN32
#define SDL_DECLSPEC __declspec(dllimport)
#define SPN_API __declspec(dllexport)
#define SP_API SPN_API
#else
#define SPN_API
#endif

#define SP_STRNLEN
#define SP_IMPLEMENTATION
#define SP_OS_BACKEND_SDL
#include "sp.h"

#define ARGPARSE_IMPLEMENTATION
#include "argparse.h"

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#ifdef SP_WIN32
  #include "windows.h"
  #include "shlobj.h"
  #include "commdlg.h"
  #include "shellapi.h"
  #include <conio.h>
  #include <io.h>
#endif

#ifdef SP_POSIX
  #include <termios.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <dlfcn.h>
#endif

#ifdef SP_LINUX
#include <libelf.h>
#include <gelf.h>
#include <elf.h>
  #include <link.h>
  #include <unistd.h>
  #include <string.h>
#endif

///////////
// SHELL //
///////////
#define SPN_SH(...) SDL_CreateProcess((const c8* []) { __VA_ARGS__, SP_NULLPTR }, SP_SDL_PIPE_STDIO)

typedef struct {
  sp_str_t output;
  s32 return_code;
} spn_sh_process_result_t;

typedef struct {
  sp_str_t command;
  sp_dyn_array(sp_str_t) args;
  sp_str_t work;

  SDL_PropertiesID shell;
  SDL_Process* process;
  spn_sh_process_result_t result;
} spn_sh_process_context_t;

void spn_sh_run(spn_sh_process_context_t* context);
s32  spn_sh_wait(spn_sh_process_context_t* context);
spn_sh_process_result_t spn_sh_read_process(SDL_Process* process);

/////////
// GIT //
/////////
#define SPN_GIT_ORIGIN_HEAD SP_LIT("origin/HEAD")
#define SPN_GIT_HEAD SP_LIT("HEAD")
#define SPN_GIT_UPSTREAM SP_LIT("@{u}")

sp_str_t spn_git_fetch(sp_str_t repo);
u32      spn_git_num_updates(sp_str_t repo, sp_str_t from, sp_str_t to);
void     spn_git_checkout(sp_str_t repo, sp_str_t commit);
sp_str_t spn_git_get_remote_url(sp_str_t repo_path);
sp_str_t spn_git_get_commit(sp_str_t repo_path, sp_str_t id);
sp_str_t spn_git_get_commit_message(sp_str_t repo_path, sp_str_t id);


// SP
typedef enum {
  SP_OS_LIB_SHARED,
  SP_OS_LIB_STATIC,
} sp_os_lib_kind_t;

SP_API sp_str_t sp_str_truncate(sp_str_t str, u32 n, sp_str_t trailer);
SP_API bool     sp_os_is_glob(sp_str_t path);
SP_API bool     sp_os_is_program_on_path(sp_str_t program);
SP_API void     sp_os_copy(sp_str_t from, sp_str_t to);
SP_API void     sp_os_copy_glob(sp_str_t from, sp_str_t glob, sp_str_t to);
SP_API void     sp_os_copy_file(sp_str_t from, sp_str_t to);
SP_API void     sp_os_copy_directory(sp_str_t from, sp_str_t to);
SP_API sp_str_t sp_os_lib_kind_to_extension(sp_os_lib_kind_t kind);
SP_API sp_str_t sp_os_lib_to_file_name(sp_str_t lib_name, sp_os_lib_kind_t kind);


/////////
// LUA //
/////////
typedef enum {
  SP_LUA_OK,
  SP_LUA_ERROR_PATH_DOES_NOT_EXIST,
  SP_LUA_ERROR_PATH_IS_NOT_DIRECTORY,
  SP_LUA_ERROR_FILE_LOAD_ERROR,
  SP_LUA_ERROR_FILE_RUN_ERROR,
} sp_lua_error_t;

typedef lua_State* sp_lua_context_t;

typedef struct {
  s32 count;
} sp_lua_pop_t;

typedef struct {
  sp_lua_context_t state;
  sp_lua_pop_t pop;
} sp_lua_t;

///////////////
// GENERATOR //
///////////////
typedef enum {
  SPN_GEN_KIND_RAW,
  SPN_GEN_KIND_SHELL,
  SPN_GEN_KIND_MAKE,
} spn_generator_kind_t;

typedef enum {
  SPN_GENERATOR_NONE,
  SPN_GENERATOR_INCLUDE,
  SPN_GENERATOR_LIB_INCLUDE,
  SPN_GENERATOR_LIBS,
  SPN_GENERATOR_SYSTEM_LIBS,
  SPN_GENERATOR_RPATH,
  SPN_GENERATOR_ALL,
} spn_gen_entry_kind_t;

typedef enum {
  SPN_GENERATOR_COMPILER_NONE,
  SPN_GENERATOR_COMPILER_GCC,
} spn_gen_compiler_t;

typedef enum {
  SPN_DIR_STORE,
  SPN_DIR_INCLUDE,
  SPN_DIR_VENDOR,
} spn_cache_dir_kind_t;

typedef struct {
  spn_generator_kind_t kind;
  spn_gen_compiler_t compiler;
  sp_str_t file_name;
  sp_str_t output;

  sp_str_t include;
  sp_str_t lib_include;
  sp_str_t libs;
  sp_str_t system_libs;
  sp_str_t rpath;
} spn_generator_context_t;


spn_generator_kind_t spn_gen_kind_from_str(sp_str_t str);
spn_gen_entry_kind_t spn_gen_entry_from_str(sp_str_t str);
spn_gen_compiler_t   spn_gen_compiler_from_str(sp_str_t str);
spn_cache_dir_kind_t spn_dir_kind_from_str(sp_str_t str);
sp_str_t             spn_gen_format_entry_for_compiler(sp_str_t entry, spn_gen_entry_kind_t kind, spn_gen_compiler_t compiler);

//////////////////
// DEPENDENCIES //
//////////////////
typedef enum {
  SPN_DEP_BUILD_MODE_DEBUG,
  SPN_DEP_BUILD_MODE_RELEASE,
} spn_dep_build_mode_t;

typedef enum {
  SPN_DEP_BUILD_KIND_SHARED = SP_OS_LIB_SHARED,
  SPN_DEP_BUILD_KIND_STATIC = SP_OS_LIB_STATIC,
  SPN_DEP_BUILD_KIND_SOURCE,
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
  SPN_DEP_BUILD_STATE_CHECKING_OUT,
  SPN_DEP_BUILD_STATE_PREPARING,
  SPN_DEP_BUILD_STATE_BUILDING,
  SPN_DEP_BUILD_STATE_DONE,
  SPN_DEP_BUILD_STATE_CANCELED,
  SPN_DEP_BUILD_STATE_FAILED
} spn_dep_build_state_t;

typedef struct {
  spn_dep_build_state_t build;
  spn_dep_repo_state_t repo;
} spn_dep_state_t;

typedef struct {
  sp_str_t source;
  sp_str_t recipe;
} spn_dep_paths_t;

typedef struct {
  sp_str_t std_out;
  sp_str_t std_err;
  sp_str_t std_in;
  sp_str_t source;
  sp_str_t work;
  sp_str_t store;
  sp_str_t include;
  sp_str_t lib;
  sp_str_t vendor;
} spn_dep_build_paths_t;

// Specific to the recipe
typedef struct {
  sp_str_t name;
  sp_str_t git;
  sp_str_t branch;
  sp_str_t lib;
  spn_dep_paths_t paths;
} spn_dep_info_t;

// Specific to a project
typedef struct {
  spn_dep_info_t* info;
  sp_hash_t hash;
  sp_str_t lock;
  spn_dep_build_kind_t kind;
  struct {
    bool include;
    bool vendor;
    bool store;
  } include;
} spn_dep_spec_t;

// Specific to a single build
typedef struct {
  spn_dep_info_t* info;
  spn_dep_spec_t* spec;
  sp_str_t build_id;
  spn_dep_build_mode_t mode;
  spn_dep_build_paths_t paths;

  bool force;
  bool update;

  struct {
    sp_str_t resolved;
    sp_str_t message;
    u32 delta;
  } commits;

  spn_dep_state_t state;
  spn_dep_state_t tui_state;

  sp_lua_t lua;
  SDL_PropertiesID sh;
  SDL_Environment* env;
  struct {
    SDL_IOStream* out;
    SDL_IOStream* err;
  } std;

  sp_thread_t thread;
  sp_mutex_t mutex;
  sp_str_t error;
} spn_dep_build_context_t;

typedef struct {
  sp_str_t name;
  sp_str_t url;
  sp_str_t commit;
  sp_str_t build_id;
} spn_lock_entry_t;

typedef sp_dyn_array(spn_lock_entry_t) spn_lock_file_t;

typedef struct {
  sp_dyn_array(spn_dep_build_context_t) deps;
} spn_build_context_t;

spn_dep_info_t*          spn_dep_find(sp_str_t name);
bool                     spn_dep_state_is_terminal(spn_dep_build_context_t* dep);
s32                      spn_dep_sort_kernel_alphabetical(const void* a, const void* b);
spn_dep_build_mode_t     spn_dep_build_mode_from_str(sp_str_t str);
sp_str_t                 spn_dep_build_mode_to_str(spn_dep_build_mode_t mode);
spn_dep_build_kind_t     spn_dep_build_kind_from_str(sp_str_t str);
sp_str_t                 spn_dep_build_kind_to_str(spn_dep_build_kind_t kind);
sp_str_t                 spn_dep_state_to_str(spn_dep_build_state_t state);
spn_lock_entry_t*        spn_dep_context_get_lock_entry(spn_dep_build_context_t* dep);
void                     spn_dep_context_prepare(spn_dep_build_context_t* context);
void                     spn_dep_context_set_build_state(spn_dep_build_context_t* dep, spn_dep_build_state_t state);
void                     spn_dep_context_set_build_error(spn_dep_build_context_t* dep, sp_str_t error);
void                     spn_dep_context_clone(spn_dep_build_context_t* dep);
sp_str_t                 spn_dep_context_find_latest_commit(spn_dep_build_context_t* dep);
s32                      spn_dep_context_build_async(void* user_data);
bool                     spn_dep_context_is_binary(spn_dep_build_context_t* dep);
spn_dep_build_context_t* spn_build_context_find_dep(spn_build_context_t* build, sp_str_t name);
sp_str_t                 spn_gen_build_entry_for_dep(spn_dep_build_context_t* dep, spn_gen_entry_kind_t kind, spn_gen_compiler_t c);
sp_str_t                 spn_gen_build_entries_for_dep(spn_dep_build_context_t* dep, spn_gen_compiler_t c);
sp_str_t                 spn_gen_build_entries_for_all(spn_gen_entry_kind_t kind, spn_gen_compiler_t c);
sp_str_t                 spn_print_system_deps_only(spn_gen_compiler_t compiler);
sp_str_t                 spn_print_deps_only(spn_gen_entry_kind_t kind, spn_gen_compiler_t compiler);


/////////
// TUI //
/////////
#define SPN_TUI_NUM_OPTIONS 3
#define SP_TUI_PRINT(command) printf("%s", command)

typedef struct {
  u32 std_in;
} sp_tui_checkpoint_t;

typedef enum {
  SPN_TUI_STATE_INTERACTIVE,
  SPN_TUI_STATE_NONINTERACTIVE
} spn_tui_state_t;

typedef struct {
  spn_tui_state_t state;
  u32 num_deps;
  u32 width;

  struct {
#ifdef SP_WIN32
    sp_win32_dword_t original_input_mode;
    sp_win32_dword_t original_output_mode;
    sp_win32_handle_t input_handle;
    sp_win32_handle_t output_handle;
#else
    struct termios ios;
#endif
    bool modified;
  } terminal;
} spn_tui_t;

void sp_tui_print(sp_str_t str);
void sp_tui_up(u32 n);
void sp_tui_down(u32 n);
void sp_tui_clear_line();
void sp_tui_show_cursor();
void sp_tui_hide_cursor();
void sp_tui_home();
void sp_tui_flush();
void sp_tui_checkpoint(spn_tui_t* tui);
void sp_tui_restore(spn_tui_t* tui);
void sp_tui_setup_raw_mode(spn_tui_t* tui);
void spn_tui_update_noninteractive(spn_tui_t* tui);
void spn_tui_update_interactive(spn_tui_t* tui);
void spn_tui_update(spn_tui_t* tui);
void spn_tui_init(spn_tui_t* tui, spn_tui_state_t);
void spn_tui_cleanup(spn_tui_t* tui);
void spn_tui_print_dep(spn_tui_t* tui, spn_dep_build_context_t* dep);



////////////
// CONFIG //
////////////
typedef struct {
  sp_str_t dir;
  sp_str_t   config;
  sp_str_t   lock;
} spn_project_paths_t;

typedef struct {
  sp_str_t install;
  sp_str_t   executable;
  sp_str_t storage;
  sp_str_t   config;
  sp_str_t     user_config;
  sp_str_t   spn;
  sp_str_t     lua;
  sp_str_t     recipes;
  sp_str_t   cache;
  sp_str_t     build;
  sp_str_t     store;
  sp_str_t     source;
  sp_str_t work;
  spn_project_paths_t project;
} spn_paths_t;

typedef struct {
  sp_str_t build;
  sp_str_t clone;
  sp_str_t libs;
  sp_str_t url;
} spn_targets_t;

typedef struct {
  sp_str_t name;
  sp_dyn_array(sp_str_t) system_deps;
  sp_dyn_array(spn_dep_spec_t) deps;
} spn_project_t;

spn_dep_spec_t* spn_project_find_dep(sp_str_t name);


/////////
// CLI //
/////////
typedef struct {
  sp_str_t package;
} spn_cli_add_t;

typedef struct {
} spn_cli_init_t;

typedef struct {
  bool force;
  bool update;
} spn_cli_build_t;

typedef struct {
} spn_cli_list_t;

typedef struct {
  const c8* package;
  const c8* kind;
} spn_cli_dir_t;

typedef struct {
  const c8* generator;
  const c8* compiler;
  const c8* path;
} spn_cli_print_t;

typedef struct {
  const c8* package;
} spn_cli_which_t;

typedef struct {
  const c8* package;
} spn_cli_ls_t;

typedef struct {
  u32 num_args;
  const c8** args;
  const c8* project_directory;
  bool no_interactive;
  spn_cli_add_t add;
  spn_cli_init_t init;
  spn_cli_list_t list;
  spn_cli_dir_t dir;
  spn_cli_print_t print;
  spn_cli_build_t build;
  spn_cli_ls_t ls;
  spn_cli_which_t which;
} spn_cli_t;

void spn_cli_command_init(spn_cli_t* cli);
void spn_cli_command_list(spn_cli_t* cli);
void spn_cli_command_nuke(spn_cli_t* cli);
void spn_cli_command_clean(spn_cli_t* cli);
void spn_cli_command_build(spn_cli_t* cli);
void spn_cli_command_dir(spn_cli_t* cli);
void spn_cli_command_copy(spn_cli_t* cli);
void spn_cli_command_print(spn_cli_t* cli);
void spn_cli_command_ls(spn_cli_t* cli);
void spn_cli_command_which(spn_cli_t* cli);

/////////
// LUA //
/////////
typedef struct {
  struct {
    sp_str_t spn;
  } paths;

  bool pull_recipes;
  bool pull_deps;
} spn_lua_config_t;

typedef struct {
  spn_cli_t* cli;
  spn_paths_t* paths;
  spn_project_t* project;
  spn_build_context_t* build;
  spn_dep_info_t** deps;
  spn_lock_entry_t** lock;
  spn_lua_config_t* config;
} spn_lua_context_t;


/////////
// APP //
/////////
typedef struct {
  spn_cli_t cli;
  spn_paths_t paths;
  spn_project_t project;
  spn_build_context_t build;
  sp_dyn_array(spn_dep_info_t) deps;
  sp_dyn_array(spn_lock_entry_t) lock;
  spn_lua_config_t config;
  spn_lua_context_t context;
  spn_tui_t tui;
  SDL_AtomicInt control;
  sp_lua_t lua;
} spn_app_t;

extern spn_app_t app;

void               spn_app_init(spn_app_t* app, u32 num_args, const c8** args);
void               spn_app_run(spn_app_t* app);
bool               spn_project_write(spn_project_t* project, sp_str_t path);
bool               spn_lock_file_write(spn_lock_file_t* lock, sp_str_t path);
void               spn_lock_file_from_deps(spn_lock_file_t* lock, spn_build_context_t* context);


////////////////////
// IMPLEMENTATION //
////////////////////
#ifdef SPN_IMPLEMENTATION

spn_app_t app;

void sp_lua_pop_add(sp_lua_pop_t* pop) {
  pop->count++;
}

void sp_lua_pop_process(sp_lua_pop_t* pop) {
  lua_pop(app.lua.state, pop->count);
  pop->count = 0;
}

const char* sp_lua_format_file_load_error(const char* error) {
  static char buffer [2048];
  const char* fmt = "  %s";
  snprintf(&buffer[0], 2048, fmt, error);

  return &buffer[0];
}

s32 sp_lua_format_file_load_error_l(sp_lua_context_t l) {
  const char* error = lua_tostring(app.lua.state, 1);
  error = sp_lua_format_file_load_error(error);

  lua_pop(app.lua.state, 1);
  lua_pushstring(app.lua.state, error);
  return 1;
}

sp_str_t sp_lua_error_to_string(sp_lua_error_t error) {
  switch (error) {
    SP_SWITCH_ENUM_TO_STRING_LOWER(SP_LUA_OK)
    SP_SWITCH_ENUM_TO_STRING_LOWER(SP_LUA_ERROR_PATH_DOES_NOT_EXIST)
    SP_SWITCH_ENUM_TO_STRING_LOWER(SP_LUA_ERROR_PATH_IS_NOT_DIRECTORY)
    SP_SWITCH_ENUM_TO_STRING_LOWER(SP_LUA_ERROR_FILE_LOAD_ERROR)
    SP_SWITCH_ENUM_TO_STRING_LOWER(SP_LUA_ERROR_FILE_RUN_ERROR)
  }
}


void sp_lua_check_load_error(sp_lua_error_t result) {
  if (result) {
    const char* error = lua_tostring(app.lua.state, -1);
    SP_FATAL("Load failed: {:fg brightred} {:fg brightblack}", sp_lua_error_to_string(result), SP_FMT_CSTR(error));
  }
}

void sp_lua_check_run_error(sp_lua_error_t result) {
  if (result) {
    const char* error = lua_tostring(app.lua.state, -1);
    SP_FATAL("Run failed: {:color brightred}", SP_FMT_CSTR(error));
  }
}

sp_lua_error_t sp_lua_pcall(sp_str_t fn) {
  lua_pushstring(app.lua.state, sp_str_to_cstr(fn));
  lua_gettable(app.lua.state, -2);
  s32 result = lua_pcall(app.lua.state, 0, 0, 0);
  sp_lua_check_run_error(result);
  return result;
}

sp_lua_error_t sp_lua_script_file(sp_str_t file_path) {
  sp_lua_context_t l = app.lua.state;
  s32 initial_stack_size = lua_gettop(l);

  SP_LOG("sp_lua_script_file(): {:color brightblue}", SP_FMT_STR(file_path));

  lua_pushcfunction(l, &sp_lua_format_file_load_error_l);

  const char* file_path_cstr = sp_str_to_cstr(file_path);
  bool result = luaL_loadfile(l, file_path_cstr);

  // In all error cases, do not return early.
  if (result) {
    // There's a syntax error in the file. Since loadfile doesn't call the
    // function we put on the stack, format the message manually.
    const char* unformatted_error = lua_tostring(l, -1);
    SP_LOG(
      "Error loading {:fg brightblue}: {:fg brightblack}",
      SP_FMT_STR(file_path),
      SP_FMT_CSTR(sp_lua_format_file_load_error(unformatted_error))
    );

    lua_pop(l, 2);

    return SP_LUA_ERROR_FILE_LOAD_ERROR;
  }
  else {
    // The chunk compiled OK. Run it.
    result = lua_pcall(l, 0, 0, -2);

    if (result) {
      // There was a runtime error running the chunk.
      const char* unformatted_error = lua_tostring(l, -1);
      SP_LOG(
        "Error running {:fg brightblue}: {:fg brightblack}",
        SP_FMT_STR(file_path),
        SP_FMT_CSTR(sp_lua_format_file_load_error(unformatted_error))
      );

      lua_pop(l, 2);

      return SP_LUA_ERROR_FILE_RUN_ERROR;
    }

    // The chunk loaded successfully!
    lua_pop(l, 1);
    return SP_LUA_OK;
  }
}

sp_lua_error_t sp_lua_run(sp_lua_t* lua, const c8* script) {
  sp_lua_context_t l = app.lua.state;

  lua_pushcfunction(l, &sp_lua_format_file_load_error_l);
  bool result = luaL_loadstring(l, script);

  if (result) {
    const char* unformatted_error = lua_tostring(l, -1);

    SP_LOG(
      "{:fg brightyellow} {:fg brightblack} {:fg brightred}",
      SP_FMT_CSTR("lua"),
      SP_FMT_CSTR(script),
      SP_FMT_CSTR(sp_lua_format_file_load_error(unformatted_error))
    );

    lua_pop(l, 2);

    return SP_LUA_ERROR_FILE_LOAD_ERROR;
  }
  else {
    // The chunk compiled OK. Run it.
    result = lua_pcall(l, 0, 0, -2);

    if (result) {
      // There was a runtime error running the chunk.
      const char* unformatted_error = lua_tostring(l, -1);
      SP_LOG(
        "Error running script: {:fg brightred}",
        SP_FMT_CSTR(sp_lua_format_file_load_error(unformatted_error))
      );

      lua_pop(l, 2);

      return SP_LUA_ERROR_FILE_RUN_ERROR;
    }

    lua_pop(l, 1);
    return SP_LUA_OK;
  }
}

sp_lua_error_t sp_lua_run_fmt(sp_lua_t* lua, const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  sp_str_t str = sp_format_v(SP_CSTR(fmt), args);
  va_end(args);

  return sp_lua_run(lua, sp_str_to_cstr(str));
}

void spn_lua_init() {
  app.lua.state = luaL_newstate();
  luaL_openlibs(app.lua.state);

  app.context = (spn_lua_context_t) {
    .cli = &app.cli,
    .paths = &app.paths,
    .project = &app.project,
    .build = &app.build,
    .deps = &app.deps,
    .lock = &app.lock,
    .config = &app.config,
  };
}

///////////
// SHELL //
void spn_sh_run(spn_sh_process_context_t* context) {
  SDL_PropertiesID shell = SDL_CreateProperties();
  if (context->shell) {
    SDL_CopyProperties(context->shell, shell);
  }

  sp_dyn_array(const c8*) args = SP_NULLPTR;
  sp_dyn_array_push(args, sp_str_to_cstr(context->command));
  if (context->args) {
    sp_dyn_array_for(context->args, index) {
      sp_dyn_array_push(args, sp_str_to_cstr(context->args[index]));
    }
  }
  sp_dyn_array_push(args, SP_NULLPTR);

  SDL_SetPointerProperty(shell, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, args);

  if (context->work.len) {
    SDL_SetStringProperty(
      shell,
      SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING,
      sp_str_to_cstr(context->work)
    );
  }

  context->process = SDL_CreateProcessWithProperties(shell);
  if (!context->process) {
    SP_LOG(
      "{:fg brightyellow} {:fg brightblack} Failed to create process: {}",
      SP_FMT_STR(sp_str_pad(SP_LIT("shell"), 8)),
      sp_str_join_cstr_n(args, sp_dyn_array_size(args) - 1, SP_LIT(" ")),
      SP_FMT_CSTR(SDL_GetError())
    );
    return;
  }
}

s32 spn_sh_wait(spn_sh_process_context_t* context) {
  SDL_WaitProcess(context->process, true, &context->result.return_code);
  return context->result.return_code;
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

// Platform-specific TUI implementations
#ifdef SP_WIN32

void sp_tui_checkpoint(spn_tui_t* tui) {
  tui->terminal.input_handle = GetStdHandle(STD_INPUT_HANDLE);
  tui->terminal.output_handle = GetStdHandle(STD_OUTPUT_HANDLE);

  GetConsoleMode(tui->terminal.input_handle, (DWORD*)&tui->terminal.original_input_mode);
  GetConsoleMode(tui->terminal.output_handle, (DWORD*)&tui->terminal.original_output_mode);
  tui->terminal.modified = true;
}

void sp_tui_restore(spn_tui_t* tui) {
  if (tui->terminal.modified) {
    SetConsoleMode(tui->terminal.input_handle, (DWORD)tui->terminal.original_input_mode);
    SetConsoleMode(tui->terminal.output_handle, (DWORD)tui->terminal.original_output_mode);
  }
}

void sp_tui_setup_raw_mode(spn_tui_t* tui) {
  // Enable virtual terminal processing for ANSI sequences
  sp_win32_dword_t output_mode = tui->terminal.original_output_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  SetConsoleMode(tui->terminal.output_handle, (DWORD)output_mode);

  // Disable line input and echo for raw character input
  sp_win32_dword_t input_mode = tui->terminal.original_input_mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
  SetConsoleMode(tui->terminal.input_handle, (DWORD)input_mode);


  CONSOLE_CURSOR_INFO info;
  GetConsoleCursorInfo(tui->terminal.output_handle, &info);
  info.bVisible = FALSE;
  info.dwSize = 25;
  SetConsoleCursorInfo(tui->terminal.output_handle, &info);
}


#else // POSIX

void sp_tui_checkpoint(spn_tui_t* tui) {
  tcgetattr(STDIN_FILENO, &tui->terminal.ios);
  tui->terminal.modified = true;
}

void sp_tui_restore(spn_tui_t* tui) {
  if (tui->terminal.modified) {
    tcsetattr(STDIN_FILENO, TCSANOW, &tui->terminal.ios);
  }
}

void sp_tui_setup_raw_mode(spn_tui_t* tui) {
  struct termios ios = tui->terminal.ios;
  ios.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &ios);
  fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

#endif

// CLI
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
  if (sp_os_does_path_exist(app.paths.project.config)) {
    SP_LOG("Project already initialized at {}", SP_FMT_COLOR(SP_ANSI_FG_CYAN), SP_FMT_STR(app.paths.project.config));
    return;
  }

  app.project = (spn_project_t){
    .name = sp_os_extract_file_name(app.paths.project.dir)
  };

  if (!spn_project_write(&app.project, app.paths.project.config)) {
    SP_FATAL("Failed to write project file");
  }
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
      "{:fg brightcyan} {}",
      SP_FMT_STR(sp_str_pad(dep->name, max_name_len)),
      SP_FMT_STR(dep->git)
    );
    sp_str_builder_new_line(&builder);
  }

  sp_log(sp_str_builder_write(&builder));
}

void spn_cli_command_nuke(spn_cli_t* cli) {
  sp_os_remove_directory(app.paths.cache);
  sp_os_remove_file(app.paths.project.config);
}

void spn_cli_command_clean(spn_cli_t* cli) {
  sp_os_remove_directory(app.paths.build);
  sp_os_remove_directory(app.paths.store);
}

spn_cache_dir_kind_t spn_dir_kind_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, ""))         return SPN_DIR_STORE;
  else if (sp_str_equal_cstr(str, "store"))    return SPN_DIR_STORE;
  else if (sp_str_equal_cstr(str, "include"))  return SPN_DIR_INCLUDE;
  else if (sp_str_equal_cstr(str, "vendor"))   return SPN_DIR_VENDOR;

  SP_FATAL("Unknown dir kind {:fg brightyellow}; options are [store, include, vendor]", SP_FMT_STR(str));
}

spn_gen_entry_kind_t spn_gen_entry_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, ""))            return SPN_GENERATOR_ALL;
  else if (sp_str_equal_cstr(str, "include"))     return SPN_GENERATOR_INCLUDE;
  else if (sp_str_equal_cstr(str, "lib-include")) return SPN_GENERATOR_LIB_INCLUDE;
  else if (sp_str_equal_cstr(str, "libs"))        return SPN_GENERATOR_LIBS;
  else if (sp_str_equal_cstr(str, "system-libs")) return SPN_GENERATOR_SYSTEM_LIBS;

  SP_FATAL("Unknown flag {:fg brightyellow}; options are [include, lib-include, libs, system-libs]", SP_FMT_QUOTED_STR(str));
}

spn_gen_compiler_t spn_gen_compiler_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, ""))    return SPN_GENERATOR_COMPILER_NONE;
  else if (sp_str_equal_cstr(str, "gcc")) return SPN_GENERATOR_COMPILER_GCC;

  SP_FATAL("Unknown compiler {:fg brightyellow}; options are [gcc]", SP_FMT_STR(str));
}

spn_generator_kind_t spn_gen_kind_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, ""))           return SPN_GEN_KIND_RAW;
  else if (sp_str_equal_cstr(str, "shell"))      return SPN_GEN_KIND_SHELL;
  else if (sp_str_equal_cstr(str, "make"))       return SPN_GEN_KIND_MAKE;

  SP_FATAL("Unknown generator {:fg brightyellow}; options are [[empty], shell, make]", SP_FMT_STR(str));
}

typedef struct {
  spn_gen_entry_kind_t kind;
  spn_gen_compiler_t compiler;
} spn_gen_format_context_t;

sp_str_t spn_generator_format_entry_kernel(sp_str_map_context_t* context) {
  spn_gen_format_context_t* format = (spn_gen_format_context_t*)context->user_data;
  return spn_gen_format_entry_for_compiler(context->str, format->kind, format->compiler);
}

sp_str_t spn_gen_format_entry_for_compiler(sp_str_t entry, spn_gen_entry_kind_t kind, spn_gen_compiler_t compiler) {
  switch (compiler) {
    case SPN_GENERATOR_COMPILER_NONE: {
      return entry;
    }
    case SPN_GENERATOR_COMPILER_GCC: {
      switch (kind) {
        case SPN_GENERATOR_INCLUDE:     return sp_format("-I{}",          SP_FMT_STR(entry));
        case SPN_GENERATOR_LIB_INCLUDE: return sp_format("-L{}",          SP_FMT_STR(entry));
        case SPN_GENERATOR_LIBS:        return sp_format("{}",            SP_FMT_STR(entry));
        case SPN_GENERATOR_SYSTEM_LIBS: return sp_format("-l{}",          SP_FMT_STR(entry));
        case SPN_GENERATOR_RPATH:       return sp_format("-Wl,-rpath,{}", SP_FMT_STR(entry));
        default: SP_FATAL("Unknown generator entry: {:fg brightred}", SP_FMT_U32(kind));
      }
    }
  }
}

sp_str_t spn_gen_build_entry_for_dep(spn_dep_build_context_t* dep, spn_gen_entry_kind_t kind, spn_gen_compiler_t compiler) {
  sp_dyn_array(sp_str_t) entries = SP_NULLPTR;

  switch (kind) {
    case SPN_GENERATOR_INCLUDE: {
      if (dep->spec->include.include) sp_dyn_array_push(entries, dep->paths.include);
      if (dep->spec->include.vendor) sp_dyn_array_push(entries, dep->paths.vendor);
      if (dep->spec->include.store) sp_dyn_array_push(entries, dep->paths.store);
      break;
    }
    case SPN_GENERATOR_RPATH:
    case SPN_GENERATOR_LIB_INCLUDE:  {
      switch (dep->spec->kind) {
        case SPN_DEP_BUILD_KIND_SHARED:
        case SPN_DEP_BUILD_KIND_STATIC: {
          sp_dyn_array_push(entries, dep->paths.lib);
          break;
        }
        case SPN_DEP_BUILD_KIND_SOURCE: {
          return SP_ZERO_STRUCT(sp_str_t);
        }
      }

      break;
    }
    case SPN_GENERATOR_LIBS: {
      switch (dep->spec->kind) {
        case SPN_DEP_BUILD_KIND_SHARED:
        case SPN_DEP_BUILD_KIND_STATIC: {
          sp_os_lib_kind_t kind = (sp_os_lib_kind_t)dep->spec->kind;
          sp_str_t lib = sp_os_lib_to_file_name(dep->info->lib, kind);
          sp_dyn_array_push(entries, sp_os_join_path(dep->paths.lib, lib));
          break;
        }
        case SPN_DEP_BUILD_KIND_SOURCE: {
          return SP_ZERO_STRUCT(sp_str_t);
        }
      }
      break;
    }
    case SPN_GENERATOR_SYSTEM_LIBS: {
      break;
    }
    default: {
      SP_UNREACHABLE_CASE();
    }
  }

  // Apply the compiler switch to the list of entries
  spn_gen_format_context_t context = {
    .compiler = compiler,
    .kind = kind
  };
  entries = sp_str_map(entries, sp_dyn_array_size(entries), &context, spn_generator_format_entry_kernel);

  return sp_str_join_n(entries, sp_dyn_array_size(entries), sp_str_lit(" "));
}

sp_str_t spn_gen_build_entries_for_dep(spn_dep_build_context_t* dep, spn_gen_compiler_t compiler) {
  spn_gen_entry_kind_t kinds [] = { SPN_GENERATOR_INCLUDE, SPN_GENERATOR_LIB_INCLUDE, SPN_GENERATOR_LIBS };

  sp_dyn_array(sp_str_t) entries = SP_NULLPTR;
  SP_CARR_FOR(kinds, index) {
    sp_dyn_array_push(entries, spn_gen_build_entry_for_dep(dep, kinds[index], compiler));
  }

  return sp_str_join_n(entries, sp_dyn_array_size(entries), sp_str_lit(" "));
}

sp_str_t spn_gen_build_entries_for_all(spn_gen_entry_kind_t kind, spn_gen_compiler_t compiler) {
  sp_dyn_array(sp_str_t) entries = SP_NULLPTR;

  sp_dyn_array_for(app.build.deps, index) {
    spn_dep_build_context_t* dep = app.build.deps + index;
    sp_str_t dep_flags = spn_gen_build_entry_for_dep(dep, kind, compiler);
    if (dep_flags.len > 0) {
      sp_dyn_array_push(entries, dep_flags);
    }
  }

  return sp_str_join_n(entries, sp_dyn_array_size(entries), sp_str_lit(" "));
}

void spn_cli_command_dir(spn_cli_t* cli) {
  spn_cli_dir_t* command = &cli->dir;

  struct argparse argparse;
  argparse_init(
    &argparse,
    (struct argparse_option []) {
      OPT_HELP(),
      OPT_END()
    },
    (const c8* const []) {
      "spn dir <package> [store|include|vendor]",
      "Output the directory path for a package",
      SP_NULLPTR
    },
    SP_NULL
  );
  cli->num_args = argparse_parse(&argparse, cli->num_args, cli->args);

  if (cli->num_args < 1) {
    SP_FATAL("Package name required");
  }

  command->package = cli->args[0];
  command->kind = cli->num_args > 1 ? cli->args[1] : "";

  sp_str_t package = sp_str_view(command->package);
  spn_cache_dir_kind_t kind = spn_dir_kind_from_str(sp_str_view(command->kind));

  spn_dep_build_context_t* dep = spn_build_context_find_dep(&app.build, package);
  if (!dep) {
    SP_FATAL("Package {:fg brightcyan} not found", SP_FMT_STR(package));
  }

  sp_str_t output = SP_ZERO_INITIALIZE();
  switch (kind) {
    case SPN_DIR_STORE:   output = dep->paths.store; break;
    case SPN_DIR_INCLUDE: output = dep->paths.include; break;
    case SPN_DIR_VENDOR:  output = dep->paths.vendor; break;
    default: SP_UNREACHABLE_CASE();
  }

  printf("%.*s", output.len, output.data);
}

void spn_cli_command_copy(spn_cli_t* cli) {
  struct argparse argparse;
  argparse_init(
    &argparse,
    (struct argparse_option []) {
      OPT_HELP(),
      OPT_END()
    },
    (const c8* const []) {
      "spn copy <directory>",
      "Copy all project binary dependencies to a directory",
      SP_NULLPTR
    },
    SP_NULL
  );
  cli->num_args = argparse_parse(&argparse, cli->num_args, cli->args);

  if (cli->num_args < 1) {
    SP_FATAL(
      "You must specify a destination (e.g. {:fg brightyellow})",
      SP_FMT_CSTR("spn copy ./build/lib")
    );
  }

  sp_str_t destination = SP_CSTR(cli->args[0]);
  destination = sp_os_normalize_path(destination);
  sp_str_t to = sp_os_join_path(app.paths.work, destination);
  sp_os_create_directory(to);

  sp_dyn_array_for(app.build.deps, index) {
    spn_dep_build_context_t* dep = app.build.deps + index;
    spn_dep_context_prepare(dep);

    sp_os_directory_entry_list_t entries = sp_os_scan_directory(dep->paths.lib);
    for (u32 i = 0; i < entries.count; i++) {
      sp_os_directory_entry_t* entry = entries.data + i;
      sp_os_copy_file(
        entry->file_path,
        sp_os_join_path(to, sp_os_extract_file_name(entries.data[i].file_path))
      );

      SP_LOG(
        "{}/{:fg brightcyan} -> {}/{:fg brightcyan}",
        SP_FMT_STR(dep->paths.lib),
        SP_FMT_STR(entry->file_name),
        SP_FMT_STR(destination),
        SP_FMT_STR(entry->file_name)
      );
    }
  }
}

void spn_cli_command_print(spn_cli_t* cli) {
  spn_cli_print_t* command = &cli->print;

  struct argparse argparse;
  argparse_init(
    &argparse,
    (struct argparse_option []) {
      OPT_HELP(),
      OPT_STRING('p', "path", &command->path, "write generated flags to a file", SP_NULLPTR),
      OPT_STRING('c', "compiler", &command->compiler, "generate for compiler [*gcc, msvc]", SP_NULLPTR),
      OPT_STRING('g', "generator", &command->generator, "output format [*raw, shell, make]", SP_NULLPTR),
      OPT_END()
    },
    (const c8* const []) {
      "spn print [--generator raw] [--path none] [--compiler gcc]",
      SP_NULLPTR
    },
    SP_NULL
  );
  cli->num_args = argparse_parse(&argparse, cli->num_args, cli->args);

  if (command->path && !command->generator) {
    sp_str_builder_t builder = SP_ZERO_INITIALIZE();
    sp_str_builder_append_fmt(&builder, "Specify a generator to write a file to {:fg brightcyan}", SP_FMT_CSTR(cli->args[0]));
    sp_str_builder_indent(&builder);
    sp_str_builder_new_line(&builder);
    sp_str_builder_append_fmt(&builder, "spn print {:fg yellow} {}", SP_FMT_CSTR("--generator make"), SP_FMT_CSTR(cli->args[0]));

    sp_log(sp_str_builder_write(&builder));
    SP_EXIT_FAILURE();
  }
  if (!command->generator) command->generator = "";
  if (!command->compiler) command->compiler = "gcc";


  sp_dyn_array_for(app.build.deps, index) {
    spn_dep_context_prepare(app.build.deps + index);
  }

  spn_generator_context_t gen = {
    .kind = spn_gen_kind_from_str(SP_CSTR(command->generator)),
    .compiler = spn_gen_compiler_from_str(SP_CSTR(command->compiler))
  };
  gen.include = spn_gen_build_entries_for_all(SPN_GENERATOR_INCLUDE, gen.compiler);
  gen.lib_include = spn_gen_build_entries_for_all(SPN_GENERATOR_LIB_INCLUDE, gen.compiler);
  gen.libs = spn_gen_build_entries_for_all(SPN_GENERATOR_LIBS, gen.compiler);
  gen.rpath = spn_gen_build_entries_for_all(SPN_GENERATOR_RPATH, gen.compiler);

  spn_gen_format_context_t fmt = {
    .kind = SPN_GENERATOR_SYSTEM_LIBS,
    .compiler = gen.compiler
  };
  sp_dyn_array(sp_str_t) entries = sp_str_map(app.project.system_deps, sp_dyn_array_size(app.project.system_deps), &fmt, spn_generator_format_entry_kernel);
  gen.system_libs = sp_str_join_n(entries, sp_dyn_array_size(entries), sp_str_lit(" "));

  switch (gen.kind) {
    case SPN_GEN_KIND_RAW: {
      gen.output = sp_format(
        "{} {} {} {} {}",
        SP_FMT_STR(gen.include),
        SP_FMT_STR(gen.lib_include),
        SP_FMT_STR(gen.libs),
        SP_FMT_STR(gen.system_libs),
        SP_FMT_STR(gen.rpath)
      );
      break;
    }
    case SPN_GEN_KIND_SHELL: {
      gen.file_name = SP_LIT("spn.sh");
      const c8* template =
        "export SPN_INCLUDES={}"                                         "\n"
        "export SPN_LIB_INCLUDES={}"                                     "\n"
        "export SPN_LIBS={}"                                             "\n"
        "export SPN_SYSTEM_LIBS={}"                                      "\n"
        "export SPN_RPATH={}"                                 "\n"
        "export SPN_FLAGS=\"$SPN_INCLUDES $SPN_LIB_INCLUDES $SPN_LIBS $SPN_SYSTEM_LIBS\"" "\n";
      gen.output = sp_format(template,
        SP_FMT_QUOTED_STR(gen.include),
        SP_FMT_QUOTED_STR(gen.lib_include),
        SP_FMT_QUOTED_STR(gen.libs),
        SP_FMT_QUOTED_STR(gen.system_libs),
        SP_FMT_QUOTED_STR(gen.rpath)
      );
      break;
    }

    case SPN_GEN_KIND_MAKE: {
      gen.file_name = SP_LIT("spn.mk");
      const c8* template =
        "SPN_INCLUDES := {}"            "\n"
        "SPN_LIB_INCLUDES := {}"        "\n"
        "SPN_LIBS := {}"                "\n"
        "SPN_SYSTEM_LIBS := {}"         "\n"
        "SPN_RPATH := {}"                "\n"
        "SPN_FLAGS := $(SPN_INCLUDES) $(SPN_LIB_INCLUDES) $(SPN_LIBS) $(SPN_SYSTEM_LIBS)";
      gen.output = sp_format(template,
        SP_FMT_QUOTED_STR(gen.include),
        SP_FMT_QUOTED_STR(gen.lib_include),
        SP_FMT_QUOTED_STR(gen.libs),
        SP_FMT_QUOTED_STR(gen.system_libs),
        SP_FMT_QUOTED_STR(gen.rpath)
      );
      break;
    }

    default: {
      SP_UNREACHABLE();
    }
  }

  if (command->path) {
    sp_str_t destination = SP_CSTR(command->path);
    destination = sp_os_normalize_path(destination);
    destination = sp_os_join_path(app.paths.work, destination);
    sp_os_create_directory(destination);

    sp_str_t file_path = sp_os_join_path(destination, gen.file_name);
    if (!SDL_SaveFile(sp_str_to_cstr(file_path), gen.output.data, gen.output.len)) {
      SP_FATAL("Failed to write {}: {}", SP_FMT_STR(file_path), SP_FMT_CSTR(SDL_GetError()));
    }

    SP_LOG("Generated {:fg brightcyan}", SP_FMT_STR(file_path));
  }
  else {
    printf("%.*s", gen.output.len, gen.output.data);
  }
}

void sp_sh_ls(sp_str_t path) {
  if (!sp_os_does_path_exist(path)) {
    SP_LOG("{:fg brightcyan} hasn't been built for your configuration", SP_FMT_STR(path));
    return;
  }

  struct {
    const c8* command;
    const c8* args [4];
  } tools [4] = {
    { "lsd", "--tree", "--depth", "2" },
    { "tree", "-L", "2" },
    { "ls" },
  };

  SP_CARR_FOR(tools, i) {
    if (sp_os_is_program_on_path(sp_str_view(tools[i].command))) {
      spn_sh_process_context_t context = SP_ZERO_INITIALIZE();
      context.command = sp_str_view(tools[i].command);

      SP_CARR_FOR(tools[i].args, j) {
        const c8* arg = tools[i].args[j];
        if (!arg) break;
        sp_dyn_array_push(context.args, sp_str_view(arg));
      }

      sp_dyn_array_push(context.args, path);

      spn_sh_run(&context);
      return;
    }
  }
}

void spn_cli_command_ls(spn_cli_t* cli) {
  struct argparse argparse;
  argparse_init(
    &argparse,
    (struct argparse_option []) {
      OPT_HELP(),
      OPT_END()
    },
    (const c8* const []) {
      "spn ls <package>",
      "List files in a package's cache directory",
      SP_NULLPTR
    },
    SP_NULL
  );
  cli->num_args = argparse_parse(&argparse, cli->num_args, cli->args);

  if (cli->num_args < 1) {
    if (app.build.deps) {
      SP_FATAL(
        "no package name specified; try {:fg brightyellow} {:fg yellow}",
        SP_FMT_CSTR("spn ls"),
        SP_FMT_STR(app.build.deps[0].info->name)
      );
    }
    else {
      SP_FATAL("you have no dependencies lol");
    }
  }

  sp_str_t package = sp_str_view(cli->args[0]);

  spn_dep_build_context_t* dep = spn_build_context_find_dep(&app.build, package);
  if (!dep) {
    SP_FATAL("{:fg brightyellow} is not in this project", SP_FMT_STR(package));
  }

  spn_dep_context_prepare(dep);

  sp_str_t store_path = dep->paths.store;
  sp_sh_ls(store_path);
}

void spn_cli_command_which(spn_cli_t* cli) {
  struct argparse argparse;
  argparse_init(
    &argparse,
    (struct argparse_option []) {
      OPT_HELP(),
      OPT_END()
    },
    (const c8* const []) {
      "spn which <package>",
      "Print the cache directory for this package for this project",
      SP_NULLPTR
    },
    SP_NULL
  );
  cli->num_args = argparse_parse(&argparse, cli->num_args, cli->args);

  if (cli->num_args < 1) {
    if (app.build.deps) {
      SP_FATAL(
        "no package name specified; try {:fg brightyellow} {:fg yellow}",
        SP_FMT_CSTR("spn which"),
        SP_FMT_STR(app.build.deps[0].info->name)
      );
    }
    else {
      SP_FATAL("you have no dependencies lol");
    }
  }

  sp_str_t package = sp_str_view(cli->args[0]);

  spn_dep_build_context_t* dep = spn_build_context_find_dep(&app.build, package);
  if (!dep) {
    SP_FATAL("{:fg brightyellow} is not in this project", SP_FMT_STR(package));
  }

  spn_dep_context_prepare(dep);
  printf("%.*s", dep->paths.store.len, dep->paths.store.data);
}

void spn_cli_command_build(spn_cli_t* cli) {
  spn_cli_build_t* command = &cli->build;

  struct argparse argparse;
  argparse_init(
    &argparse,
    (struct argparse_option []) {
      OPT_HELP(),
      OPT_BOOLEAN('f', "force", &command->force, "force build, even if it exists in the store", SP_NULLPTR),
      OPT_BOOLEAN('u', "update", &command->update, "pull latest for all deps", SP_NULLPTR),
      OPT_END()
    },
    (const c8* const []) {
      "spn build [--force] [--update]",
      SP_NULLPTR
    },
    SP_NULL
  );
  cli->num_args = argparse_parse(&argparse, cli->num_args, cli->args);

  sp_dyn_array_for(app.build.deps, index) {
    spn_dep_build_context_t* dep = app.build.deps + index;
    dep->force = command->force;
    dep->update = command->update;
    sp_thread_init(&dep->thread, spn_dep_context_build_async, dep);
  }

  spn_tui_init(&app.tui, app.cli.no_interactive ? SPN_TUI_STATE_NONINTERACTIVE : SPN_TUI_STATE_INTERACTIVE);

  while (true) {
    bool building = false;

    sp_dyn_array_for(app.build.deps, index) {
      spn_dep_build_context_t* dep = app.build.deps + index;

      sp_mutex_lock(&dep->mutex);
      if (!spn_dep_state_is_terminal(dep)) {
        building = true;
      }
      sp_mutex_unlock(&dep->mutex);
    }

    spn_tui_update(&app.tui);

    if (!building) {
      break;
    }
    SDL_Delay(5);
  }

  spn_tui_cleanup(&app.tui);

  // Report any failures
  sp_str_builder_t builder = SP_ZERO_INITIALIZE();

  bool failed = false;
  sp_dyn_array_for(app.build.deps, i) {
    spn_dep_build_context_t* dep = &app.build.deps[i];

    switch (dep->state.build) {
      case SPN_DEP_BUILD_STATE_DONE: {
        break;
      }
      case SPN_DEP_BUILD_STATE_FAILED:
      default: {
        failed = true;

        sp_size_t file_size = 0;
        c8* file_data = (c8*)SDL_LoadFile(sp_str_to_cstr(dep->paths.std_err), &file_size);
        if (!file_data) {
          SP_FATAL(
            "Failed to open {:fg brightcyan} while reporting error for {:fg brightcyan}",
            SP_FMT_STR(dep->paths.std_err),
            SP_FMT_STR(dep->info->name)
          );
        }

        sp_str_builder_new_line(&builder);
        sp_str_builder_append_fmt(&builder, "{:fg brightyellow}", SP_FMT_STR(dep->info->name));
        sp_str_builder_new_line(&builder);
        sp_str_builder_append_fmt(&builder, "{:fg brightblack}", SP_FMT_CSTR(file_data));

        SDL_RemovePath(sp_str_to_cstr(dep->paths.store));

        break;
      }
    }
  }

  spn_lock_file_from_deps(&app.lock, &app.build);
  spn_lock_file_write(&app.lock, app.paths.project.lock);
}

///////////
// SHELL //
///////////
spn_sh_process_result_t spn_sh_read_process(SDL_Process* process) {
  spn_sh_process_result_t result = SP_ZERO_INITIALIZE();

  sp_size_t len = 0;
  void* output_data = SDL_ReadProcess(process, &len, &result.return_code);

  result.output.data = (c8*)output_data;
  result.output.len = (u32)len;
  SDL_WaitProcess(process, true, &result.return_code);
  return result;
}

sp_str_t spn_git_fetch(sp_str_t repo_path) {
  SDL_Process* process = SPN_SH("git", "-C", sp_str_to_cstr(repo_path), "fetch", "--quiet");
  SP_ASSERT(process);

  spn_sh_process_result_t result = spn_sh_read_process(process);
  SDL_DestroyProcess(process);
  return result.output;
}

u32 spn_git_num_updates(sp_str_t repo_path, sp_str_t from, sp_str_t to) {
  sp_str_t specifier = sp_format("{}..{}", SP_FMT_STR(from), SP_FMT_STR(to));
  SDL_Process* process = SPN_SH("git", "-C", sp_str_to_cstr(repo_path), "rev-list", sp_str_to_cstr(specifier), "--count");
  SP_ASSERT(process);

  spn_sh_process_result_t result = spn_sh_read_process(process);
  SDL_DestroyProcess(process);

  sp_str_t trimmed = sp_str_trim_right(result.output);
  return sp_parse_u32(trimmed);
}

sp_str_t spn_git_get_remote_url(sp_str_t repo_path) {
  SDL_Process* process = SPN_SH("git", "-C", sp_str_to_cstr(repo_path), "remote", "get-url", "origin");
  SP_ASSERT(process);

  spn_sh_process_result_t result = spn_sh_read_process(process);
  SP_ASSERT_FMT(!result.return_code, "Failed to get remote URL for {:fg brightcyan}", SP_FMT_STR(repo_path));
  SDL_DestroyProcess(process);

  return sp_str_trim_right(result.output);
}

sp_str_t spn_git_get_commit(sp_str_t repo_path, sp_str_t id) {
  SDL_Process* process = SPN_SH("git", "-C", sp_str_to_cstr(repo_path), "rev-parse", "--short=10", sp_str_to_cstr(id));
  SP_ASSERT(process);

  spn_sh_process_result_t result = spn_sh_read_process(process);
  SP_ASSERT_FMT(!result.return_code, "Failed to get revision for {:fg brightcyan}", SP_FMT_STR(repo_path));
  SDL_DestroyProcess(process);

  return sp_str_trim_right(result.output);
}

sp_str_t spn_git_get_commit_message(sp_str_t repo_path, sp_str_t id) {
  SDL_Process* process = SPN_SH("git", "-C", sp_str_to_cstr(repo_path), "log", "--format=%B", "-n", "1", sp_str_to_cstr(id));
  SP_ASSERT(process);

  spn_sh_process_result_t result = spn_sh_read_process(process);
  SP_ASSERT_FMT(!result.return_code, "Failed to get check out {:fg brightcyan} {:fg brightcyan}", SP_FMT_STR(repo_path), SP_FMT_STR(id));
  SDL_DestroyProcess(process);

  return sp_str_trim_right(result.output);
}

void spn_git_checkout(sp_str_t repo, sp_str_t commit) {
  SDL_Process* process = SPN_SH("git", "-C", sp_str_to_cstr(repo), "checkout", "--quiet", sp_str_to_cstr(commit));
  SP_ASSERT(process);

  spn_sh_process_result_t result = spn_sh_read_process(process);
  SP_ASSERT_FMT(!result.return_code, "Failed to get check out {:fg brightcyan} {:fg brightcyan}", SP_FMT_STR(repo), SP_FMT_STR(commit));
  SDL_DestroyProcess(process);
}

///////////////////
// SP EXTENSIONS //
///////////////////
sp_str_t sp_str_truncate(sp_str_t str, u32 n, sp_str_t trailer) {
  if (!n) return sp_str_copy(str);
  if (str.len <= n) return sp_str_copy(str);
  SP_ASSERT(trailer.len <= n);

  str.len = n - trailer.len;
  return sp_str_concat(str, trailer);
}

bool sp_os_is_glob(sp_str_t path) {
  sp_str_t file_name = sp_os_extract_file_name(path);
  return sp_str_contains_n(&file_name, 1, sp_str_lit("*"));
}

bool sp_os_is_program_on_path(sp_str_t program) {
  spn_sh_process_context_t context = SP_ZERO_INITIALIZE();

  context.command = SP_LIT("which");
  sp_dyn_array_push(context.args, program);

  context.shell = SDL_CreateProperties();
  SDL_SetNumberProperty(context.shell, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
  SDL_SetNumberProperty(context.shell, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_NULL);
  SDL_SetBooleanProperty(context.shell, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);

  spn_sh_run(&context);

  SDL_WaitProcess(context.process, true, &context.result.return_code);

  return !context.result.return_code;
}

void sp_os_copy(sp_str_t from, sp_str_t to) {
  if (sp_os_is_glob(from)) {
    sp_os_copy_glob(sp_os_parent_path(from), sp_os_extract_file_name(from), to);
  }
  else if (sp_os_is_directory(from)) {
    SP_ASSERT(sp_os_is_directory(to));
    sp_os_copy_glob(from, sp_str_lit("*"), sp_os_join_path(to, sp_os_extract_stem(from)));
  }
  else if (sp_os_is_regular_file(from)) {
    sp_os_copy_file(from, to);
  }
}

void sp_os_copy_glob(sp_str_t from, sp_str_t glob, sp_str_t to) {
  SDL_CreateDirectory(sp_str_to_cstr(to));

  s32 num_files = 0;
  c8** entries = SDL_GlobDirectory(sp_str_to_cstr(from), sp_str_to_cstr(glob), SDL_GLOB_CASEINSENSITIVE, &num_files);

  if (!entries) return;

  for (u32 i = 0; i < num_files; i++) {
    sp_str_t entry_path = sp_str_view(entries[i]);
    sp_os_copy(sp_os_join_path(from, entry_path), to);
  }
}

void sp_os_copy_file(sp_str_t from, sp_str_t to) {
  if (sp_os_is_directory(to)) {
    sp_os_create_directory(to);
    to = sp_os_join_path(to, sp_os_extract_file_name(from));
  }

  SDL_CopyFile(sp_str_to_cstr(from), sp_str_to_cstr(to));
}

void sp_os_copy_directory(sp_str_t from, sp_str_t to) {
  if (sp_os_is_directory(to)) {
    to = sp_os_join_path(to, sp_os_extract_file_name(from));
  }

  sp_os_copy_glob(from, sp_str_lit("*"), to);
}

#if defined(SP_LINUX)
  sp_str_t sp_elf_get_soname(sp_str_t path) {
    if (elf_version(EV_CURRENT) == EV_NONE) return SP_ZERO_STRUCT(sp_str_t);

    int fd = open(sp_str_to_cstr(path), O_RDONLY);
    if (fd < 0) return SP_ZERO_STRUCT(sp_str_t);

    Elf* elf = elf_begin(fd, ELF_C_READ, SP_NULLPTR);
    if (!elf) {
      close(fd);
      return SP_ZERO_STRUCT(sp_str_t);
    }

    Elf_Scn* scn = SP_NULLPTR;
    GElf_Shdr shdr;

    while ((scn = elf_nextscn(elf, scn)) != SP_NULLPTR) {
      if (gelf_getshdr(scn, &shdr) != &shdr) continue;
      if (shdr.sh_type != SHT_DYNAMIC) continue;

      Elf_Data* data = elf_getdata(scn, SP_NULLPTR);
      if (!data) continue;

      for (size_t i = 0; i < shdr.sh_size / shdr.sh_entsize; i++) {
        GElf_Dyn dyn;
        if (gelf_getdyn(data, i, &dyn) != &dyn) continue;
        if (dyn.d_tag == DT_NULL) break;
        if (dyn.d_tag != DT_SONAME) continue;

        char* soname = elf_strptr(elf, shdr.sh_link, dyn.d_un.d_val);
        SP_ASSERT(soname);

        sp_str_t result = sp_str_from_cstr(soname);
        elf_end(elf);
        close(fd);

        return result;
      }
    }

    elf_end(elf);
    close(fd);
    return SP_ZERO_STRUCT(sp_str_t);
  }


  sp_str_t sp_os_lib_kind_to_extension(sp_os_lib_kind_t kind) {
    switch (kind) {
      case SP_OS_LIB_SHARED: return SP_LIT("so");
      case SP_OS_LIB_STATIC: return SP_LIT("a");
    }

    SP_UNREACHABLE();
  }

  sp_str_t sp_os_lib_to_file_name(sp_str_t lib_name, sp_os_lib_kind_t kind) {
    return sp_format("lib{}.{}", SP_FMT_STR(lib_name), SP_FMT_STR(sp_os_lib_kind_to_extension(kind)));
  }
#elif defined(SP_MACOS)
  sp_str_t sp_os_lib_kind_to_extension(sp_os_lib_kind_t kind) {
    switch (kind) {
      case SP_OS_LIB_SHARED: return SP_LIT("dylib");
      case SP_OS_LIB_STATIC: return SP_LIT("a");
    }

    SP_UNREACHABLE();
  }

  sp_str_t sp_os_lib_to_file_name(sp_str_t lib_name, sp_os_lib_kind_t kind) {
    return sp_format("lib{}.{}", SP_FMT_STR(lib_name), SP_FMT_STR(sp_os_lib_kind_to_extension(kind)));
  }
#elif defined(SP_WIN32)
  sp_str_t sp_os_lib_kind_to_extension(sp_os_lib_kind_t kind) {
    switch (kind) {
      case SP_OS_LIB_SHARED: return SP_LIT("dll");
      case SP_OS_LIB_STATIC: return SP_LIT("lib");
    }

    SP_UNREACHABLE();
  }

  sp_str_t sp_os_lib_to_file_name(sp_str_t lib_name, sp_os_lib_kind_t kind) {
    return sp_format("{}.{}", SP_FMT_STR(lib_name), SP_FMT_STR(sp_os_lib_kind_to_extension(kind)));
  }
#endif

/////////
// TUI //
/////////
void spn_tui_print_dep(spn_tui_t* tui, spn_dep_build_context_t* dep) {
  sp_mutex_lock(&dep->mutex);
  sp_str_t name = sp_str_pad(dep->info->name, tui->width);
  sp_str_t state = sp_str_pad(spn_dep_state_to_str(dep->state.build), 10);
  sp_str_t status;

  switch (dep->state.build) {
    case SPN_DEP_BUILD_STATE_CANCELED: {
      status = sp_format(
        "{} {:color brightyellow} {}",
        SP_FMT_STR(name),
        SP_FMT_STR(state)
      );
      break;
    }
    case SPN_DEP_BUILD_STATE_FAILED: {
      sp_str_t error = dep->error.len ? dep->error : SP_LIT("No error reported");

      status = sp_format(
        "{} {:color brightgreen} {:color brightblack} {} {:color brightcyan} {:color brightred}",
        SP_FMT_STR(name),
        SP_FMT_STR(state),
        SP_FMT_STR(dep->commits.resolved),
        SP_FMT_STR(dep->commits.message),
        SP_FMT_STR(dep->paths.store),
        SP_FMT_STR(error)
      );
      break;
    }
    case SPN_DEP_BUILD_STATE_DONE: {
      if (tui->state == SPN_TUI_STATE_INTERACTIVE) {
        status = sp_format(
          "{} {:color brightgreen} {:color brightblack} {} {:color brightyellow} {:color brightcyan}",
          SP_FMT_STR(name),
          SP_FMT_STR(state),
          SP_FMT_STR(dep->commits.resolved),
          SP_FMT_STR(dep->commits.message),
          SP_FMT_U32(dep->commits.delta),
          SP_FMT_STR(dep->paths.store)
        );
      }
      else {
        status = sp_format(
          "{} {:color brightgreen}",
          SP_FMT_STR(name),
          SP_FMT_STR(state)
        );
      }
      break;
    }
    case SPN_DEP_BUILD_STATE_BUILDING: {
      status = sp_format(
        "{} {:color brightcyan} {:color brightblack} {} {:color brightcyan}",
        SP_FMT_STR(name),
        SP_FMT_STR(state),
        SP_FMT_STR(dep->commits.resolved),
        SP_FMT_STR(dep->commits.message),
        SP_FMT_STR(dep->paths.store)
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

  sp_tui_print(status);
}

void spn_tui_update_noninteractive(spn_tui_t* tui) {
  sp_dyn_array_for(app.build.deps, i) {
    spn_dep_build_context_t* dep = &app.build.deps[i];

    if (dep->state.build != dep->tui_state.build) {
      dep->tui_state = dep->state;
      spn_tui_print_dep(tui, dep);
      sp_tui_print(SP_LIT("\n"));
    }
  }
}

void spn_tui_update_interactive(spn_tui_t* tui) {
  sp_tui_up(sp_dyn_array_size(app.build.deps));

  sp_dyn_array_for(app.build.deps, index) {
    spn_dep_build_context_t* dep = app.build.deps + index;

    sp_tui_home();
    sp_tui_clear_line();
    spn_tui_print_dep(tui, dep);
    sp_tui_down(1);
  }

  sp_tui_flush();
}

void spn_tui_cleanup(spn_tui_t* tui) {
  switch (tui->state) {
    case SPN_TUI_STATE_INTERACTIVE: {
      sp_tui_restore(tui);
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

void spn_tui_init(spn_tui_t* tui, spn_tui_state_t state) {
  tui->state = state;
  tui->num_deps = sp_dyn_array_size(app.build.deps);
  tui->width = 0;
  sp_dyn_array_for(app.build.deps, i) {
    spn_dep_build_context_t* dep = &app.build.deps[i];
    tui->width = SP_MAX(tui->width, dep->info->name.len);
  }

  switch (tui->state) {
    case SPN_TUI_STATE_INTERACTIVE: {
      sp_dyn_array_for(app.build.deps, index) {
        sp_tui_print(SP_LIT("\n"));
      }
      sp_tui_hide_cursor();
      sp_tui_flush();

      sp_tui_checkpoint(tui);
      sp_tui_setup_raw_mode(tui);

      break;
    }
    case SPN_TUI_STATE_NONINTERACTIVE: {
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

///////////
// BUILD //
///////////
spn_dep_build_mode_t spn_dep_build_mode_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, "debug"))   return SPN_DEP_BUILD_MODE_DEBUG;
  else if (sp_str_equal_cstr(str, "release")) return SPN_DEP_BUILD_MODE_RELEASE;

  SP_FATAL("Unknown build mode {:fg brightyellow}; options are [debug, release]", SP_FMT_STR(str));
}

sp_str_t spn_dep_build_mode_to_str(spn_dep_build_mode_t mode) {
  switch (mode) {
    case SPN_DEP_BUILD_MODE_DEBUG:   return sp_str_lit("debug");
    case SPN_DEP_BUILD_MODE_RELEASE: return sp_str_lit("release");
    default: SP_UNREACHABLE();
  }
}

spn_dep_build_kind_t spn_dep_build_kind_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, "shared")) return SPN_DEP_BUILD_KIND_SHARED;
  else if (sp_str_equal_cstr(str, "static")) return SPN_DEP_BUILD_KIND_STATIC;
  else if (sp_str_equal_cstr(str, "source")) return SPN_DEP_BUILD_KIND_SOURCE;

  SP_FATAL("Unknown build kind {:fg brightyellow}; options are [shared, static, source]", SP_FMT_STR(str));
}

sp_str_t spn_dep_build_kind_to_str(spn_dep_build_kind_t kind) {
  switch (kind) {
    case SPN_DEP_BUILD_KIND_SHARED: return sp_str_lit("shared");
    case SPN_DEP_BUILD_KIND_STATIC: return sp_str_lit("static");
    case SPN_DEP_BUILD_KIND_SOURCE: return sp_str_lit("source");
    default: SP_UNREACHABLE();
  }
}

sp_str_t spn_dep_state_to_str(spn_dep_build_state_t state) {
  switch (state) {
    case SPN_DEP_BUILD_STATE_IDLE:         return SP_LIT("idle");
    case SPN_DEP_BUILD_STATE_CLONING:      return SP_LIT("cloning");
    case SPN_DEP_BUILD_STATE_FETCHING:     return SP_LIT("fetching");
    case SPN_DEP_BUILD_STATE_PREPARING:    return SP_LIT("preparing");
    case SPN_DEP_BUILD_STATE_CHECKING_OUT: return SP_LIT("checking out");
    case SPN_DEP_BUILD_STATE_BUILDING:     return SP_LIT("building");
    case SPN_DEP_BUILD_STATE_DONE:         return SP_LIT("done");
    case SPN_DEP_BUILD_STATE_CANCELED:     return SP_LIT("canceled");
    case SPN_DEP_BUILD_STATE_FAILED:       return SP_LIT("failed");
    default: SP_FATAL("spn_dep_state_to_str(): Unknown build state {:fg brightred}", SP_FMT_U32(state));
  }
}

bool spn_dep_state_is_terminal(spn_dep_build_context_t* dep) {
  switch (dep->state.build) {
    case SPN_DEP_BUILD_STATE_FAILED:
    case SPN_DEP_BUILD_STATE_DONE:
    case SPN_DEP_BUILD_STATE_CANCELED: return true;
    default: return false;
  }
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

spn_dep_build_context_t* spn_build_context_find_dep(spn_build_context_t* build, sp_str_t name) {
  sp_dyn_array_for(build->deps, index) {
    spn_dep_build_context_t* dep = build->deps + index;
    if (sp_str_equal(dep->info->name, name)) {
      return dep;
    }
  }

  return SP_NULLPTR;
}

bool spn_dep_context_is_binary(spn_dep_build_context_t* dep) {
  switch (dep->spec->kind) {
    case SPN_DEP_BUILD_KIND_SHARED: return true;
    case SPN_DEP_BUILD_KIND_STATIC: return true;
    case SPN_DEP_BUILD_KIND_SOURCE: return false;
  }

  SP_UNREACHABLE();
}


sp_str_t spn_dep_context_find_latest_commit(spn_dep_build_context_t* dep) {
  return spn_git_get_commit(dep->info->paths.source, sp_format("origin/{}", SP_FMT_STR(dep->info->branch)));
}

void spn_dep_context_clone(spn_dep_build_context_t* dep) {
  sp_str_t url = sp_format("https://github.com/{}.git", SP_FMT_STR(dep->info->git));
  SDL_Process* process = SPN_SH(
    "git", "clone", "--quiet",
    sp_str_to_cstr(url),
    sp_str_to_cstr(dep->info->paths.source)
  );

  if (!process) {
    spn_dep_context_set_build_error(dep, sp_format(
      "Failed to spawn process to clone {:color brightcyan} {}",
      dep->info->name,
      SP_FMT_CSTR(SDL_GetError())
    ));
    return;
  }

  spn_sh_process_result_t result = spn_sh_read_process(process);
  if (result.return_code) {
    spn_dep_context_set_build_error(dep, sp_format(
      "Failed to clone {:fg brightcyan} with exit code {:fg brightred}",
      SP_FMT_STR(dep->info->name),
      SP_FMT_S32(result.return_code)
    ));

    SDL_DestroyProcess(process);
    return;
  }

  SDL_DestroyProcess(process);
}

void spn_dep_checkout(spn_dep_build_context_t *dep) {
  if (dep->info->branch.len) {
    spn_git_checkout(dep->info->paths.source, dep->info->branch);
  }
}

void spn_dep_context_prepare(spn_dep_build_context_t* dep) {
  sp_mutex_init(&dep->mutex, SP_MUTEX_PLAIN);

  sp_mutex_lock(&dep->mutex);
  dep->commits.resolved = spn_git_get_commit(dep->info->paths.source, SPN_GIT_HEAD);
  dep->commits.message = spn_git_get_commit_message(dep->info->paths.source, dep->commits.resolved);
  dep->commits.message = sp_str_truncate(dep->commits.message, 32, SP_LIT("..."));
  dep->commits.message = sp_str_replace_c8(dep->commits.message, '\n', ' ');
  dep->commits.message = sp_str_pad(dep->commits.message, 32);
  dep->commits.delta = spn_git_num_updates(dep->info->paths.source, dep->commits.resolved, spn_dep_context_find_latest_commit(dep));

  dep->mode = SPN_DEP_BUILD_MODE_DEBUG;

  sp_dyn_array(sp_hash_t) hashes = SP_NULLPTR;
  sp_dyn_array_push(hashes, sp_hash_str(dep->commits.resolved));
  sp_dyn_array_push(hashes, dep->spec->hash);
  sp_hash_t hash = sp_hash_combine(hashes, sp_dyn_array_size(hashes));
  dep->build_id = sp_format("{}", SP_FMT_SHORT_HASH(hash));

  sp_str_t build_dir = sp_os_join_path(app.paths.build, dep->info->name);
  sp_str_t store_dir = sp_os_join_path(app.paths.store, dep->info->name);
  dep->paths.source = dep->info->paths.source;
  dep->paths.work = sp_os_join_path(build_dir, dep->build_id);
  dep->paths.store = sp_os_join_path(store_dir, dep->build_id);
  dep->paths.include = sp_os_join_path(dep->paths.store, SP_LIT("include"));
  dep->paths.lib = sp_os_join_path(dep->paths.store, SP_LIT("lib"));
  dep->paths.vendor = sp_os_join_path(dep->paths.store, SP_LIT("vendor"));
  dep->paths.std_out = sp_os_join_path(dep->paths.work, SP_LIT("build.stdout"));
  dep->paths.std_err = sp_os_join_path(dep->paths.work, SP_LIT("build.stderr"));
  dep->paths.std_in  = sp_os_join_path(dep->paths.work, SP_LIT("build.stdin"));
  sp_mutex_unlock(&dep->mutex);
}

s32 spn_dep_context_build_async(void* user_data) {
  spn_dep_build_context_t* dep = (spn_dep_build_context_t*)user_data;

  if (sp_os_does_path_exist(dep->info->paths.source)) {
    spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_FETCHING);
    spn_git_fetch(dep->info->paths.source);

    spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_CHECKING_OUT);
    if (dep->update || sp_str_empty(dep->spec->lock)){
      dep->commits.resolved = spn_dep_context_find_latest_commit(dep);
    }
    else {
      dep->commits.resolved = dep->spec->lock;
    }

    spn_git_checkout(dep->info->paths.source, dep->commits.resolved);
  }
  else {
    spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_CLONING);
    spn_dep_context_clone(dep);
    SP_ASSERT(sp_os_is_directory(dep->info->paths.source));

    sp_str_t branch = sp_format("origin/{}",  SP_FMT_STR(dep->info->branch));
    dep->commits.resolved = spn_git_get_commit(dep->info->paths.source, branch);
    spn_git_checkout(dep->info->paths.source, dep->commits.resolved);
  }

  spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_PREPARING);
  spn_dep_context_prepare(dep);

  // @spader
  // The store is immutable; if there's an entry in the store with this build ID, then we know it was
  // produced a build of this package. If, in practice, that turns out to be untrue, then that's a bug.
  if (sp_os_does_path_exist(dep->paths.store)) {
    if (!dep->force) {
      spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_DONE);
      return 0;
    }
  }

  sp_os_create_directory(dep->paths.work);
  sp_os_create_directory(dep->paths.store);
  sp_os_create_directory(dep->paths.include);
  sp_os_create_directory(dep->paths.lib);
  sp_os_create_directory(dep->paths.vendor);
  dep->std.out = SDL_IOFromFile(sp_str_to_cstr(dep->paths.std_out), "w");
  dep->std.err = SDL_IOFromFile(sp_str_to_cstr(dep->paths.std_err), "w");

  dep->sh = SDL_CreateProperties();
  SDL_SetNumberProperty(dep->sh, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
  SDL_SetNumberProperty(dep->sh, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_REDIRECT);
  SDL_SetPointerProperty(dep->sh, SDL_PROP_PROCESS_CREATE_STDOUT_POINTER, dep->std.out);
  SDL_SetBooleanProperty(dep->sh, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);

  dep->lua.state = luaL_newstate();
  luaL_openlibs(dep->lua.state);

  s32 result = luaL_dostring(dep->lua.state, sp_str_to_cstr(sp_format(
    "package.path = '{}/?.lua;' .. package.path",
    SP_FMT_STR(app.paths.lua)))
  );
  SP_ASSERT_FMT(
    !result,
    "Failed to update {:fg brightyellow} while preparing {:fg cyan}",
    SP_FMT_CSTR("package.path"),
    SP_FMT_STR(dep->info->name)
  );


  // Build
  spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_BUILDING);

  const c8* chunk = "require('spn').internal.build(...)";
  if (luaL_loadstring(dep->lua.state, chunk) != LUA_OK) {
    spn_dep_context_set_build_error(dep, sp_format(
      "{:fg brightred}: {:fg brightblack}",
      SP_FMT_CSTR("failed"),
      SP_FMT_CSTR(lua_tostring(dep->lua.state, -1))
    ));
    return 1;
  }

  lua_pushlightuserdata(dep->lua.state, dep);
  if (lua_pcall(dep->lua.state, 1, 0, 0) != 0) {
    spn_dep_context_set_build_error(dep, sp_format(
      "{:fg brightred}: {:fg brightblack}",
      SP_FMT_CSTR("failed"),
      SP_FMT_CSTR(lua_tostring(dep->lua.state, -1))
    ));
    return 1;
  }
  lua_pop(dep->lua.state, 2);

  // Manually read SONAME from each library produced and create a copy that contains the SONAME. I'm not
  // sure if this is bad. Virtually every library will produce three files to begin with:
  //  - libfoo.so
  //  - libfoo.so.2
  //  - libfoo.so.2.0.69
  //
  //  With the fully versioned one being the real file and the other two being links. But I'd have to
  //  implement globs in the recipe copy entries to get them all. And I don't feel like doing that.
  #ifdef SP_LINUX
  sp_os_directory_entry_list_t entries = sp_os_scan_directory(dep->paths.lib);
  for (u32 i = 0; i < entries.count; i++) {
    sp_os_directory_entry_t* entry = entries.data + i;

    sp_str_t soname = sp_elf_get_soname(entry->file_path);
    if (!sp_str_valid(soname)) continue;

    sp_str_t so_path = sp_os_join_path(dep->paths.lib, soname);
    if (sp_os_does_path_exist(so_path)) continue;

    sp_os_copy_file(entry->file_path, so_path); // @spader symlink?
  }
  #endif


  spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_DONE);
  SDL_CloseIO(dep->std.err);
  SDL_CloseIO(dep->std.out);

  return 0;
}

void spn_dep_context_set_build_state(spn_dep_build_context_t* dep, spn_dep_build_state_t state) {
  sp_mutex_lock(&dep->mutex);
  dep->state.build = state;
  sp_mutex_unlock(&dep->mutex);
}

void spn_dep_context_set_build_error(spn_dep_build_context_t* dep, sp_str_t error) {
  sp_mutex_lock(&dep->mutex);
  dep->state.build = SPN_DEP_BUILD_STATE_FAILED;
  dep->error = sp_str_copy(error);
  sp_mutex_unlock(&dep->mutex);
}

spn_lock_entry_t* spn_dep_context_get_lock_entry(spn_dep_build_context_t* dep) {
  sp_dyn_array_for(app.lock, index) {
    spn_lock_entry_t* lock = app.lock + index;
    if (sp_str_equal(lock->name, dep->info->name)) {
      return lock;
    }
  }

  return SP_NULLPTR;
}

/////////
// APP //
/////////
void spn_app_init(spn_app_t* app, u32 num_args, const c8** args) {
  spn_cli_t* cli = &app->cli;

  struct argparse_option options [] = {
    OPT_HELP(),
    OPT_STRING('C', "project-dir", &cli->project_directory, "specify the directory containing spn.lua", SP_NULLPTR),
    OPT_BOOLEAN('n', "no-interactive", &cli->no_interactive, "disable interactive tui", SP_NULLPTR),
    OPT_END(),
  };

  const c8* const usages [] = {
    "spn <command> [options]\n"
    "\n"
    "A modern C/C++ package manager and build tool\n"
    "\n"
    "Commands:\n"
    "  init           Initialize a new spn project in the current directory\n"
    "  build          Build all project dependencies\n"
    "  list           List all available packages\n"
    "  clean          Remove build and store directories\n"
    "  copy  <dir>    Copy all project binaries to a directory\n"
    "  print <path>   Generate installation script and copy binaries",
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

#ifdef SP_WIN32
  app->paths.storage = sp_os_normalize_path(sp_str_from_cstr(SDL_GetPrefPath(SP_NULLPTR, "spn")));
#else
  sp_str_t _home = sp_os_normalize_path(sp_str_from_cstr(SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "HOME")));
  app->paths.storage = sp_os_join_path(_home, SP_LIT(".local/share/spn"));
#endif

  // Install
  app->paths.executable = sp_os_get_executable_path();
  app->paths.install = sp_os_normalize_path(sp_str_from_cstr(SDL_GetBasePath()));
  app->paths.work = sp_os_normalize_path(sp_str_from_cstr(SDL_GetCurrentDirectory()));

  // Project
  if (app->cli.project_directory) {
    sp_str_t project = sp_os_join_path(app->paths.work, SP_CSTR(app->cli.project_directory));
    app->paths.project.dir = sp_os_canonicalize_path(project);
  }
  else {
    app->paths.project.dir = sp_str_copy(app->paths.work);
  }
  app->paths.project.config = sp_os_join_path(app->paths.project.dir, SP_LIT("spn.lua"));
  app->paths.project.lock = sp_os_join_path(app->paths.project.dir, SP_LIT("spn.lock.lua"));

  // Config
#ifdef SP_WIN32
  app->paths.config = sp_os_join_path(sdl_prefix, SP_LIT("config"));
#else
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
#endif
  app->paths.user_config = sp_os_join_path(app->paths.config, SP_LIT("spn.lua"));

  spn_lua_init();

  // Bootstrap the user config, which tells us if spn itself is installed in the usual,
  // well-known location or in somewhere the user specified (for development)
  if (sp_os_does_path_exist(app->paths.user_config)) {
    sp_str_builder_t builder = SP_ZERO_INITIALIZE();
    sp_str_builder_append_fmt(&builder,  "local loader = loadfile('{}');", SP_FMT_STR(app->paths.user_config));
    sp_str_builder_append_cstr(&builder, "if not loader then return nil end;");
    sp_str_builder_append_cstr(&builder, "local ok, config = pcall(loader);");
    sp_str_builder_append_cstr(&builder, "if not ok then return nil end;");
    sp_str_builder_append_cstr(&builder, "return config and config.spn or nil;");

    if (luaL_dostring(app->lua.state, sp_str_builder_write_cstr(&builder))) {
      const char* error = lua_tostring(app->lua.state, -1);
      lua_pop(app->lua.state, 1);
      SP_FATAL("Failed to run bootstrap Lua chunk: {}", SP_FMT_CSTR(error));
    }

    const c8* spn = lua_tostring(app->lua.state, -1);
    SP_ASSERT(spn);
    app->paths.spn = sp_os_normalize_path(sp_str_view(spn));

    lua_pop(app->lua.state, 1);
  }
  else {
    app->paths.spn = sp_os_join_path(app->paths.storage, SP_LIT("spn"));
  }

  if (!sp_os_does_path_exist(app->paths.spn)) {
    // If the recipe directory doesn't exist, we need to clone it
    const c8* url = "https://github.com/tspader/spn.git";
    SP_LOG(
      "Cloning recipe repository from {:fg brightcyan} to {:fg brightcyan}",
      SP_FMT_CSTR(url),
      SP_FMT_STR(app->paths.spn)
    );

    SDL_Process* process = SPN_SH(
      "git", "clone", "--quiet",
      url,
      sp_str_to_cstr(app->paths.spn)
    );
    spn_sh_process_result_t result = spn_sh_read_process(process);
    if (result.return_code) {
      SP_FATAL(
        "Failed to clone spn recipe sources from {} to {}",
        SP_FMT_CSTR(url),
        SP_FMT_STR(app->paths.spn)
      );
    }
  }
  else {
    // If it does, we need to pull
    u32 num_updates = spn_git_num_updates(app->paths.spn, SPN_GIT_HEAD, SPN_GIT_UPSTREAM);
    if (num_updates > 0) {
      if (app->config.pull_recipes) {
        SP_LOG("Updating spn recipes ({} commits behind)...", SP_FMT_U32(num_updates));
        spn_git_checkout(app->paths.spn, SPN_GIT_ORIGIN_HEAD);
      }
      else {
        SP_LOG("spn has {} recipe updates available (auto_pull_recipes=false)", SP_FMT_U32(num_updates));
      }
    }
  }

  app->paths.lua = sp_os_join_path(app->paths.spn, SP_LIT("source"));
  app->paths.recipes = sp_os_join_path(app->paths.spn, SP_LIT("asset/recipes"));

  SP_ASSERT_FMT(
    sp_os_does_path_exist(app->paths.recipes),
    "Recipe directory {:color brightcyan} does not exist",
    SP_FMT_STR(app->paths.recipes)
  );

  // Find the cache directory after the config has been fully loaded
  app->paths.cache = sp_os_join_path(app->paths.storage, SP_LIT("cache"));
  app->paths.source = sp_os_join_path(app->paths.cache, SP_LIT("source"));
  app->paths.build = sp_os_join_path(app->paths.cache, SP_LIT("build"));
  app->paths.store = sp_os_join_path(app->paths.cache, SP_LIT("store"));

  sp_os_create_directory(app->paths.cache);
  sp_os_create_directory(app->paths.source);
  sp_os_create_directory(app->paths.build);

  // At this point, we know we have all our Lua sources.
  sp_str_t script = sp_format("package.path = '{}/?.lua;' .. package.path", SP_FMT_STR(app->paths.lua));
  s32 result = luaL_dostring(app->lua.state, sp_str_to_cstr(script));
  SP_ASSERT(!result);

  sp_str_builder_t builder = SP_ZERO_INITIALIZE();
  sp_str_builder_append_cstr(&builder, "local spn = require('spn');");
  sp_str_builder_append_cstr(&builder, "spn.internal.init(...);");
  const c8* chunk = sp_str_builder_write_cstr(&builder);

  if (luaL_loadstring(app->lua.state, chunk) != LUA_OK) {
    SP_FATAL(
      "{:fg brightblack} failed to load chunk: {:fg brightred}",
      SP_FMT_CSTR(chunk),
      SP_FMT_CSTR(lua_tostring(app->lua.state, -1))
    );
  }

  lua_pushlightuserdata(app->lua.state, &app->context);
  if (lua_pcall(app->lua.state, 1, 0, 0) != 0) {
    SP_FATAL(
      "{:fg brightblack} failed: {:fg brightred}",
      SP_FMT_CSTR(chunk),
      SP_FMT_CSTR(lua_tostring(app->lua.state, -1))
    );
  }
  lua_pop(app->lua.state, 2);

  sp_dyn_array_for(app->project.deps, index) {
    spn_dep_build_context_t build = SP_ZERO_INITIALIZE();
    build.spec = app->project.deps + index;
    build.info = build.spec->info;
    sp_dyn_array_push(app->build.deps, build);
  }
}

void spn_app_run(spn_app_t* app) {
  spn_cli_t* cli = &app->cli;

  if (!cli->num_args || !cli->args || !cli->args[0]) {
    SP_ASSERT(false);
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
  else if (sp_cstr_equal("build", cli->args[0])) {
    spn_cli_command_build(cli);
  }
  else if (sp_cstr_equal("dir", cli->args[0])) {
    spn_cli_command_dir(cli);
  }
  else if (sp_cstr_equal("copy", cli->args[0])) {
    spn_cli_command_copy(cli);
  }
  else if (sp_cstr_equal("print", cli->args[0])) {
    spn_cli_command_print(cli);
  }
  else if (sp_cstr_equal("ls", cli->args[0])) {
    spn_cli_command_ls(cli);
  }
  else if (sp_cstr_equal("which", cli->args[0])) {
    spn_cli_command_which(cli);
  }
}

/////////////
// PROJECT //
/////////////
spn_dep_spec_t* spn_project_find_dep(sp_str_t name) {
  sp_dyn_array_for(app.project.deps, index) {
    spn_dep_spec_t* dep = app.project.deps + index;
    if (sp_str_equal(dep->info->name, name)) {
      return dep;
    }
  }

  return SP_NULLPTR;
}

bool spn_project_write(spn_project_t* project, sp_str_t path) {
  return true;
}

///////////////
// LOCK FILE //
///////////////
void spn_lock_file_from_deps(spn_lock_file_t* lock, spn_build_context_t* build) {
  sp_dyn_array_clear(*lock);

  sp_dyn_array_for(build->deps, i) {
    spn_dep_build_context_t* dep = &build->deps[i];

    spn_lock_entry_t entry = SP_ZERO_INITIALIZE();
    entry.name = sp_str_copy(dep->info->name);
    entry.url = spn_git_get_remote_url(dep->info->paths.source);
    entry.commit = spn_git_get_commit(dep->info->paths.source, SPN_GIT_HEAD);
    entry.build_id = sp_str_copy(dep->build_id);

    sp_dyn_array_push(*lock, entry);
  }
}

bool spn_lock_file_write(spn_lock_file_t* lock, sp_str_t path) {
  s32 result = sp_lua_run(&app.lua, "require('spn').internal.lock()");
  return result == SP_LUA_OK;
}


#endif // SPN_IMPLEMENTATION

#endif // SPN_H
