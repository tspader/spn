#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif

  #ifndef NOMINMAX
    #define NOMINMAX
  #endif

  #include <windows.h>
  #include <shlobj.h>
  #include <commdlg.h>
  #include <shellapi.h>
  #include <conio.h>
  #include <io.h>
#endif

#define SP_IMPLEMENTATION
#include "sp.h"

#define ARGPARSE_IMPLEMENTATION
#include "argparse.h"

#define TOML_IMPLEMENTATION
#include "toml.h"

#include "libtcc.h"

#ifdef SP_POSIX
#include <signal.h>
#endif

#ifdef SP_POSIX
  #include <termios.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <dlfcn.h>
#endif

#ifdef SP_LINUX
  #include <link.h>
  #include <unistd.h>
  #include <string.h>
#endif

#include "spn/spn.h"

#define SPN_VERSION 1
#define SPN_COMMIT "00c0fa98"


#define SPN_ERR(X) \
  X(SPN_OK, "ok") \
  X(SPN_ERROR, "error")

typedef enum {
  SPN_ERR(SP_X_NAMED_ENUM_DEFINE)
} spn_err_t;


////////
// SP //
////////
#define SP_FMT_QSTR(STR) SP_FMT_QUOTED_STR(STR)
#define SP_FMT_QCSTR(CSTR) SP_FMT_QUOTED_STR(sp_str_view(CSTR))
#define SP_ALLOC(T) (T*)sp_alloc(sizeof(T))
#define _SP_MSTR(x) #x
#define SP_MSTR(x) _SP_MSTR(x)
#define _SP_MCAT(x, y) x##y
#define SP_MCAT(x, y) _SP_MCAT(x, y)

sp_str_t sp_str_map_kernel_colorize(sp_str_map_context_t* context) {
  sp_str_t id = *(sp_str_t*)context->user_data;
  sp_str_t ansi = sp_format_color_id_to_ansi_fg(id);
  return sp_format("{}{}{}", SP_FMT_STR(ansi), SP_FMT_STR(context->str), SP_FMT_CSTR(SP_ANSI_RESET));
}

/////////
// GIT //
/////////
#define SPN_GIT_ORIGIN_HEAD SP_LIT("origin/HEAD")
#define SPN_GIT_HEAD SP_LIT("HEAD")
#define SPN_GIT_UPSTREAM SP_LIT("@{u}")

bool      spn_git_clone(sp_str_t url, sp_str_t path);
spn_err_t spn_git_fetch(sp_str_t repo);
u32       spn_git_num_updates(sp_str_t repo, sp_str_t from, sp_str_t to);
spn_err_t spn_git_checkout(sp_str_t repo, sp_str_t commit);
sp_str_t  spn_git_get_remote_url(sp_str_t repo_path);
sp_str_t  spn_git_get_commit(sp_str_t repo_path, sp_str_t id);
sp_str_t  spn_git_get_commit_message(sp_str_t repo_path, sp_str_t id);


//////////
// TOML //
//////////
#define spn_toml_arr_for(arr, it) for (u32 it = 0; it < toml_array_len(arr); it++)
#define spn_toml_for(tbl, it, key) \
    for (s32 it = 0, SP_UNIQUE_ID() = 0; it < toml_table_len(tbl) && (key = toml_table_key(tbl, it, &SP_UNIQUE_ID())); it++)
typedef struct {
  toml_table_t* root;
  toml_table_t* package;
  toml_table_t* lib;
  toml_table_t* src;
  toml_table_t* a;
  toml_table_t* so;
  toml_array_t* bin;
  toml_table_t* deps;
} spn_toml_package_t;

toml_table_t* spn_toml_parse(sp_str_t path);
sp_str_t spn_toml_str(toml_table_t* toml, const c8* key);
sp_str_t spn_toml_arr_str(toml_array_t* toml, u32 it);

/////////
// TCC //
/////////
typedef TCCState spn_tcc_t;

spn_tcc_t* spn_tcc_new();
void       spn_tcc_add_file(spn_tcc_t* tcc, sp_str_t file_path);
void       spn_tcc_register(spn_tcc_t* tcc);
void       spn_tcc_error(void* opaque, const char* message);
void       spn_tcc_list_fn(void* opaque, const char* name, const void* value);


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

// SEMVER //
///////////
typedef enum {
  SPN_SEMVER_OP_LT = 0,
  SPN_SEMVER_OP_LEQ = 1,
  SPN_SEMVER_OP_GT = 2,
  SPN_SEMVER_OP_GEQ = 3,
  SPN_SEMVER_OP_EQ = 4,
} spn_semver_op_t;

typedef struct {
  u32 major;
  u32 minor;
  u32 patch;
} spn_semver_t;

typedef struct {
  spn_semver_t version;
  spn_semver_op_t op;
} spn_semver_bound_t;

typedef struct {
  spn_semver_bound_t low;
  spn_semver_bound_t high;
} spn_version_range_t;

typedef struct {
  sp_str_t fmt;
  u32 it;
} spn_semver_parser_t;

c8           spn_semver_parser_peek(spn_semver_parser_t* parser);
void         spn_semver_parser_eat(spn_semver_parser_t* parser);
void         spn_semver_parser_eat_and_assert(spn_semver_parser_t* parser, c8 c);
bool         spn_semver_parser_is_digit(c8 c);
bool         spn_semver_parser_is_whitespace(spn_semver_parser_t* parser);
bool         spn_semver_parser_is_done(spn_semver_parser_t* parser);
void         spn_semver_parser_eat_whitespace(spn_semver_parser_t* parser);
u32          spn_semver_parser_parse_number(spn_semver_parser_t* parser);
spn_semver_t spn_semver_parser_parse_version(spn_semver_parser_t* parser, bool* has_minor, bool* has_patch);

//////////////////
// DEPENDENCIES //
//////////////////
typedef enum {
  SPN_DEP_REPO_STATE_NOT_CLONED,
  SPN_DEP_REPO_STATE_UNLOCKED,
  SPN_DEP_REPO_STATE_LOCKED,
} spn_dep_repo_state_t;

typedef enum {
  SPN_DEP_BUILD_STATE_NONE,
  SPN_DEP_BUILD_STATE_IDLE,
  SPN_DEP_BUILD_STATE_CLONING,
  SPN_DEP_BUILD_STATE_FETCHING,
  SPN_DEP_BUILD_STATE_CHECKING_OUT,
  SPN_DEP_BUILD_STATE_RESOLVING,
  SPN_DEP_BUILD_STATE_BUILDING,
  SPN_DEP_BUILD_STATE_PACKAGING,
  SPN_DEP_BUILD_STATE_STAMPING,
  SPN_DEP_BUILD_STATE_DONE,
  SPN_DEP_BUILD_STATE_CANCELED,
  SPN_DEP_BUILD_STATE_FAILED
} spn_dep_build_state_t;

typedef struct {
  sp_str_t name;
  spn_dep_mode_t mode;
} spn_build_matrix_t;

typedef struct {
  spn_dep_build_state_t build;
} spn_dep_state_t;

// Specific to the recipe
typedef enum {
  SPN_PACKAGE_STATE_UNLOADED,
  SPN_PACKAGE_STATE_LOADED,
} spn_package_state_t;

typedef struct {
  sp_str_t source;
  sp_str_t file;
} spn_package_paths_t;

typedef enum {
  SPN_VERSION_EQ,
  SPN_VERSION_GEQ,
  SPN_VERSION_GT,
  SPN_VERSION_LEQ,
  SPN_VERSION_LT,
} spn_version_op_t;

typedef struct {
  u32 major;
  u32 minor;
  u32 patch;
} spn_version_t;

typedef struct {
  spn_version_t low;
  spn_version_t high;
  struct {
    spn_version_op_t low;
    spn_version_op_t high;
  } ops;
} spn_version_range_t;

typedef struct {
  sp_da(sp_str_t) headers;
  sp_da(sp_str_t) sources;
} spn_source_lib_t;

typedef struct {
  sp_da(sp_str_t) libs;
} spn_static_lib_t;

typedef struct {
  sp_da(sp_str_t) libs;
} spn_shared_lib_t;

typedef union {
  spn_source_lib_t src;
  spn_static_lib_t a;
  spn_shared_lib_t so;
} spn_lib_t;

typedef struct {
  sp_str_t name;
  sp_str_t entry;
} spn_bin_t;

typedef struct {
  sp_str_t name;
  spn_version_range_t version;
} spn_dep_req_t;

struct spn_package {
  spn_toml_package_t toml;
  sp_str_t name;
  sp_str_t repo;
  spn_version_t version;
  spn_package_paths_t paths;
  sp_ht(spn_lib_kind_t, spn_lib_t) lib;
  sp_ht(sp_str_t, spn_bin_t) bin;
  sp_ht(sp_str_t, spn_dep_req_t) deps;

  spn_dep_fn_t info;
  spn_dep_fn_t configure;
  spn_dep_fn_t build;
  spn_dep_fn_t package;

  spn_package_state_t state;
};

spn_package_t spn_package_load(sp_str_t file_path);
spn_version_range_t spn_version_range_from_str(sp_str_t str);

// Specific to a single build
typedef struct {
  sp_str_t log;
  sp_str_t stamp;
  sp_str_t source;
  sp_str_t work;
  sp_str_t store;
  sp_str_t include;
  sp_str_t lib;
  sp_str_t vendor;
} spn_dep_paths_t;

typedef enum {
  SPN_DEP_OPTION_KIND_BOOL,
  SPN_DEP_OPTION_KIND_U32,
  SPN_DEP_OPTION_KIND_F32,
  SPN_DEP_OPTION_KIND_STR,
  SPN_DEP_OPTION_KIND_PTR,
} spn_dep_option_kind_t;

typedef struct {
  spn_dep_option_kind_t kind;
  sp_str_t name;
  union {
    bool boolean;
    u32 u32;
    f32 f32;
    c8* str;
    void* ptr;
  };
} spn_dep_option_t;

typedef sp_ht(sp_str_t, spn_dep_option_t) spn_dep_options_t;

struct spn_dep {
  sp_str_t name;
  spn_lib_kind_t kind;
  spn_dep_mode_t mode;
  sp_hash_t hash;
  spn_dep_options_t options;

  spn_package_t* recipe;
  sp_str_t build_id;
  spn_dep_paths_t paths;

  bool force;
  bool update;

  struct {
    sp_str_t resolved;
    sp_str_t message;
    u32 delta;
  } commits;
  sp_str_t lock;

  spn_dep_state_t state;
  spn_dep_state_t tui_state;

  sp_ps_config_t cfg;
  sp_ps_t ps;
  sp_io_stream_t log;

  sp_thread_t thread;
  sp_mutex_t mutex;
  sp_str_t error;
};

spn_generator_kind_t spn_gen_kind_from_str(sp_str_t str);
spn_gen_entry_kind_t spn_gen_entry_from_str(sp_str_t str);
spn_gen_compiler_t   spn_gen_compiler_from_str(sp_str_t str);
sp_str_t             spn_gen_format_entry_for_compiler(sp_str_t entry, spn_gen_entry_kind_t kind, spn_gen_compiler_t compiler);
sp_str_t             spn_gen_build_entry_for_dep(spn_dep_t* dep, spn_gen_entry_kind_t kind, spn_gen_compiler_t c);
sp_str_t             spn_gen_build_entries_for_dep(spn_dep_t* dep, spn_gen_compiler_t c);
sp_str_t             spn_gen_build_entries_for_all(spn_gen_entry_kind_t kind, spn_gen_compiler_t c);
sp_str_t             spn_print_system_deps_only(spn_gen_compiler_t compiler);
sp_str_t             spn_print_deps_only(spn_gen_entry_kind_t kind, spn_gen_compiler_t compiler);

sp_str_t             spn_opt_cstr_required(const c8* value);
sp_str_t             spn_opt_cstr_optional(const c8* value, const c8* fallback);

spn_dir_kind_t spn_cache_dir_kind_from_str(sp_str_t str);
sp_str_t             spn_cache_dir_kind_to_dep_path(spn_dep_t* dep, spn_dir_kind_t kind);
spn_dep_mode_t       spn_dep_build_mode_from_str(sp_str_t str);
sp_str_t             spn_dep_build_mode_to_str(spn_dep_mode_t mode);
spn_lib_kind_t       spn_dep_build_kind_from_str(sp_str_t str);
sp_str_t             spn_dep_build_kind_to_str(spn_lib_kind_t kind);
sp_str_t             spn_dep_state_to_str(spn_dep_build_state_t state);
bool                 spn_dep_state_is_terminal(spn_dep_t* dep);
sp_os_lib_kind_t     spn_dep_kind_to_os_lib_kind(spn_lib_kind_t kind);
void                 spn_dep_context_init(spn_dep_t* dep, spn_package_t* recipe);
s32                  spn_dep_thread_resolve(void* user_data);
s32                  spn_dep_thread_build(void* user_data);
s32                  spn_dep_context_resolve(spn_dep_t* dep);
s32                  spn_dep_context_build(spn_dep_t* dep);
spn_err_t            spn_dep_context_sync_remote(spn_dep_t* dep);
spn_err_t            spn_dep_context_sync_local(spn_dep_t* dep);
void                 spn_dep_context_set_commit(spn_dep_t* dep, sp_str_t commit);
spn_err_t            spn_dep_context_resolve_commit(spn_dep_t* dep);
spn_err_t            spn_dep_context_resolve_build_id(spn_dep_t* dep);
bool                 spn_dep_context_is_build_stamped(spn_dep_t* context);
void                 spn_dep_context_set_build_state(spn_dep_t* dep, spn_dep_build_state_t state);
void                 spn_dep_context_set_build_error(spn_dep_t* dep, sp_str_t error);
bool                 spn_dep_context_is_binary(spn_dep_t* dep);
void                 spn_recipe_load(spn_tcc_t* tcc, spn_package_t* recipe);
spn_package_t*        spn_recipe_find(sp_str_t name);
s32                  spn_sort_kernel_dep_ptr(const void* a, const void* b);

void                 spn_update_lock_file();

////////////
// CONFIG //
////////////
typedef struct {
  sp_str_t dir;
  sp_str_t   toml;
  sp_str_t   lock;
} spn_project_paths_t;

struct spn_config {
  sp_str_t spn;
};


/////////
// TUI //
/////////
#define SPN_TUI_NUM_OPTIONS 3
#define SP_TUI_PRINT(command) printf("%s", command)

typedef struct {
  u32 std_in;
} sp_tui_checkpoint_t;

#define SPN_OUTPUT_MODE_X(X) \
  X(SPN_OUTPUT_MODE_INTERACTIVE) \
  X(SPN_OUTPUT_MODE_NONINTERACTIVE) \
  X(SPN_OUTPUT_MODE_QUIET) \
  X(SPN_OUTPUT_MODE_NONE)

typedef enum {
  SPN_OUTPUT_MODE_X(SP_X_ENUM_DEFINE)
} spn_tui_mode_t;

spn_tui_mode_t spn_output_mode_from_str(sp_str_t str);
sp_str_t       spn_output_mode_to_str(spn_tui_mode_t mode);

typedef struct {
  spn_tui_mode_t mode;
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
void spn_tui_update(spn_tui_t* tui);
void spn_tui_init(spn_tui_t* tui, spn_tui_mode_t mode);
void spn_tui_run(spn_tui_t* tui);
void spn_tui_print_dep(spn_tui_t* tui, spn_dep_t* dep);


/////////
// CLI //
/////////
typedef struct {
  const c8* package;
} spn_cli_add_t;

typedef struct {
  bool force;
  bool update;
} spn_cli_build_t;

typedef struct {
  const c8* generator;
  const c8* compiler;
  const c8* path;
} spn_cli_print_t;

typedef struct {
  const c8* dir;
} spn_cli_which_t;

typedef struct {
  const c8* dir;
} spn_cli_ls_t;

typedef struct {
  const c8* package;
} spn_cli_recipe_t;

typedef struct {
  u32 num_args;
  const c8** args;
  const c8* project_directory;
  const c8* project_file;
  const c8* matrix;
  const c8* output;

  spn_cli_add_t add;
  spn_cli_print_t print;
  spn_cli_build_t build;
  spn_cli_ls_t ls;
  spn_cli_which_t which;
  spn_cli_recipe_t recipe;
} spn_cli_t;

void spn_cli_clean(spn_cli_t* cli);

void spn_cli_build(spn_cli_t* cli);
void spn_cli_print(spn_cli_t* cli);
void spn_cli_copy(spn_cli_t* cli);

void spn_cli_init(spn_cli_t* cli);
void spn_cli_add(spn_cli_t* cli);

void spn_cli_list(spn_cli_t* cli);
void spn_cli_ls(spn_cli_t* cli);
void spn_cli_which(spn_cli_t* cli);
void spn_cli_recipe(spn_cli_t* cli);
void spn_cli_assert_dep_is_locked(spn_dep_t* dep);



///////
// APP //
/////////
typedef struct {
  spn_project_paths_t project;
  sp_str_t work;
  sp_str_t executable;
  sp_str_t storage;
  sp_str_t   config_dir;
  sp_str_t     config;
  sp_str_t   spn;
  sp_str_t     recipes;
  sp_str_t   cache;
  sp_str_t     build;
  sp_str_t     store;
  sp_str_t     source;
} spn_paths_t;

typedef struct {
  spn_cli_t cli;
  spn_paths_t paths;
  spn_tui_t tui;
  sp_atomic_s32 control;
  spn_tcc_t* tcc;
  sp_str_t tcc_error;

  spn_config_t config;
  spn_package_t package;
  sp_dyn_array(sp_str_t) search;
  sp_ht(sp_str_t, spn_package_t) packages;
  sp_ht(sp_str_t, spn_dep_t) deps;
  sp_ht(sp_str_t, spn_dep_t) builds;
  sp_ht(sp_str_t, spn_build_matrix_t) matrices;
  sp_dyn_array(sp_str_t) system_deps;
} spn_app_t;

spn_app_t app;

void spn_app_init(spn_app_t* app, u32 num_args, const c8** args);
void spn_cli_run(spn_app_t* app);


////////////////////
// IMPLEMENTATION //
////////////////////
s32 main(s32 num_args, const c8** args) {
  app = SP_ZERO_STRUCT(spn_app_t);
  spn_app_init(&app, num_args, args);
  spn_cli_run(&app);

  return 0;
}

////////////
// LIBSPN //
////////////
struct spn_make {
  spn_dep_t* dep;
  sp_dyn_array(sp_str_t) targets;
};

struct spn_autoconf {
  spn_dep_t* dep;
};

typedef struct {
  const c8* symbol;
  void* fn;
} spn_lib_fn_t;

#define SPN_LIB_ENTRIES(APPLY) \
  APPLY(spn_make) \
  APPLY(spn_make_new) \
  APPLY(spn_make_add_target) \
  APPLY(spn_make_run) \
  APPLY(spn_autoconf) \
  APPLY(spn_autoconf_new) \
  APPLY(spn_autoconf_run) \
  APPLY(spn_dep_set_u32) \
  APPLY(spn_copy)

#define SPN_DEFINE_LIB_ENTRY(SYM) { .symbol = SP_MACRO_STR(SYM), .fn = SYM },

spn_lib_fn_t spn_lib [] = {
  SPN_LIB_ENTRIES(SPN_DEFINE_LIB_ENTRY)
};

void spn_make(spn_dep_t* build) {
  spn_make_t* make = spn_make_new(build);
  spn_make_run(make);
}

spn_make_t* spn_make_new(spn_dep_t* dep) {
  spn_make_t* make = SP_ALLOC(spn_make_t);
  make->dep = dep;
  return make;
}

void spn_make_add_target(spn_make_t* make, const c8* target) {
  sp_dyn_array_push(make->targets, sp_str_from_cstr(target));
}

void spn_make_run(spn_make_t* make) {
  spn_dep_t* dep = make->dep;

  sp_ps_config_t ps = {
    .command = SP_LIT("make"),
    .args = {
      SP_LIT("--quiet"),
      SP_LIT("--directory"), dep->paths.work
    },
    .cwd = dep->paths.work
  };

  sp_dyn_array_for(make->targets, it) {
    sp_str_t target = make->targets[it];
    ps.args[3] = target;
    sp_ps_run(ps);
  }
}

void spn_autoconf(spn_dep_t* build) {
  spn_autoconf_t* autoconf = spn_autoconf_new(build);
  spn_autoconf_run(autoconf);
}

spn_autoconf_t* spn_autoconf_new(spn_dep_t* dep) {
  spn_autoconf_t* autoconf = SP_ALLOC(spn_autoconf_t);
  autoconf->dep = dep;
  return autoconf;
}

void spn_autoconf_run(spn_autoconf_t* autoconf) {
  spn_dep_t* dep = autoconf->dep;

  sp_ps_output_t result = sp_ps_run((sp_ps_config_t) {
    .command = sp_os_join_path(dep->paths.source, SP_LIT("configure")),
    .args = {
      sp_format("--prefix={}", SP_FMT_STR(dep->paths.store)),
      dep->kind == SPN_LIB_KIND_SHARED ?
        SP_LIT("--enable-shared") :
        SP_LIT("--disable-shared"),
      dep->kind == SPN_LIB_KIND_STATIC ?
        SP_LIT("--enable-static") :
        SP_LIT("--disable-static"),
    },
    .cwd = dep->paths.work
  });
}

void spn_dep_set_u32(spn_dep_t* dep, const c8* name, u32 value) {
  spn_dep_option_t option = {
    .kind = SPN_DEP_OPTION_KIND_U32,
    .name = sp_str_from_cstr(name),
    .u32 = value
  };
  sp_ht_insert(dep->options, option.name, option);
}

void spn_copy(spn_dep_t* dep, spn_dir_kind_t from_kind, const c8* from_path, spn_dir_kind_t to_kind, const c8* to_path) {
  sp_str_t from = sp_os_join_path(spn_cache_dir_kind_to_dep_path(dep, from_kind), sp_str_view(from_path));
  sp_str_t to = sp_os_join_path(spn_cache_dir_kind_to_dep_path(dep, to_kind), sp_str_view(to_path));
  sp_os_copy(from, to);

}


/////////
// TUI //
/////////
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
#endif

#if defined(SP_POSIX)
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


///////////
// ENUMS //
///////////
spn_tui_mode_t spn_output_mode_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, "interactive"))    return SPN_OUTPUT_MODE_INTERACTIVE;
  else if (sp_str_equal_cstr(str, "noninteractive")) return SPN_OUTPUT_MODE_NONINTERACTIVE;
  else if (sp_str_equal_cstr(str, "quiet"))          return SPN_OUTPUT_MODE_QUIET;
  else if (sp_str_equal_cstr(str, "none"))           return SPN_OUTPUT_MODE_NONE;

  SP_FATAL("Unknown output mode {:fg brightyellow}; options are [interactive, noninteractive, quiet, none]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_OUTPUT_MODE_NONE);
}

sp_str_t spn_output_mode_to_str(spn_tui_mode_t mode) {
  switch (mode) {
    SPN_OUTPUT_MODE_X(SP_X_ENUM_CASE_TO_STRING_LOWER)
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_dep_mode_t spn_dep_build_mode_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, "debug"))   return SPN_DEP_BUILD_MODE_DEBUG;
  else if (sp_str_equal_cstr(str, "release")) return SPN_DEP_BUILD_MODE_RELEASE;

  SP_FATAL("Unknown build mode {:fg brightyellow}; options are [debug, release]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_DEP_BUILD_MODE_DEBUG);
}

sp_str_t spn_dep_build_mode_to_str(spn_dep_mode_t mode) {
  switch (mode) {
    case SPN_DEP_BUILD_MODE_DEBUG:   return sp_str_lit("debug");
    case SPN_DEP_BUILD_MODE_RELEASE: return sp_str_lit("release");
    default: SP_UNREACHABLE_RETURN(sp_str_lit(""));
  }
}

spn_lib_kind_t spn_dep_build_kind_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, "shared")) return SPN_LIB_KIND_SHARED;
  else if (sp_str_equal_cstr(str, "static")) return SPN_LIB_KIND_STATIC;
  else if (sp_str_equal_cstr(str, "source")) return SPN_LIB_KIND_SOURCE;

  SP_FATAL("Unknown build kind {:fg brightyellow}; options are [shared, static, source]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_LIB_KIND_SHARED);
}

sp_str_t spn_dep_build_kind_to_str(spn_lib_kind_t kind) {
  switch (kind) {
    case SPN_LIB_KIND_SHARED: return sp_str_lit("shared");
    case SPN_LIB_KIND_STATIC: return sp_str_lit("static");
    case SPN_LIB_KIND_SOURCE: return sp_str_lit("source");
    default: SP_UNREACHABLE_RETURN(sp_str_lit(""));
  }
}

sp_str_t spn_dep_state_to_str(spn_dep_build_state_t state) {
  switch (state) {
    case SPN_DEP_BUILD_STATE_IDLE:         return SP_LIT("idle");
    case SPN_DEP_BUILD_STATE_CLONING:      return SP_LIT("cloning");
    case SPN_DEP_BUILD_STATE_FETCHING:     return SP_LIT("fetching");
    case SPN_DEP_BUILD_STATE_RESOLVING:    return SP_LIT("resolving");
    case SPN_DEP_BUILD_STATE_CHECKING_OUT: return SP_LIT("checking out");
    case SPN_DEP_BUILD_STATE_BUILDING:     return SP_LIT("building");
    case SPN_DEP_BUILD_STATE_DONE:         return SP_LIT("done");
    case SPN_DEP_BUILD_STATE_CANCELED:     return SP_LIT("canceled");
    case SPN_DEP_BUILD_STATE_FAILED:       return SP_LIT("failed");
    case SPN_DEP_BUILD_STATE_STAMPING:     return SP_LIT("stamping");
    case SPN_DEP_BUILD_STATE_PACKAGING:    return SP_LIT("packaging");
    default: SP_UNREACHABLE_RETURN(sp_str_lit(""));
  }
}

sp_os_lib_kind_t spn_dep_kind_to_os_lib_kind(spn_lib_kind_t kind) {
  switch (kind) {
    case SPN_LIB_KIND_SHARED: return SP_OS_LIB_SHARED;
    case SPN_LIB_KIND_STATIC: return SP_OS_LIB_STATIC;
    case SPN_LIB_KIND_SOURCE:
    case SPN_LIB_KIND_NONE: return 0;
  }

  SP_UNREACHABLE_RETURN(0);
}

spn_dir_kind_t spn_cache_dir_kind_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, ""))         return SPN_DIR_STORE;
  else if (sp_str_equal_cstr(str, "cache"))    return SPN_DIR_CACHE;
  else if (sp_str_equal_cstr(str, "store"))    return SPN_DIR_STORE;
  else if (sp_str_equal_cstr(str, "include"))  return SPN_DIR_INCLUDE;
  else if (sp_str_equal_cstr(str, "vendor"))   return SPN_DIR_VENDOR;
  else if (sp_str_equal_cstr(str, "lib"))      return SPN_DIR_LIB;
  else if (sp_str_equal_cstr(str, "source"))   return SPN_DIR_SOURCE;
  else if (sp_str_equal_cstr(str, "work"))     return SPN_DIR_WORK;

  SP_FATAL("Unknown dir kind {:fg brightyellow}; options are [cache, store, include, vendor, lib, source, work]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_DIR_CACHE);
}

spn_gen_entry_kind_t spn_gen_entry_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, ""))            return SPN_GENERATOR_ALL;
  else if (sp_str_equal_cstr(str, "include"))     return SPN_GENERATOR_INCLUDE;
  else if (sp_str_equal_cstr(str, "lib-include")) return SPN_GENERATOR_LIB_INCLUDE;
  else if (sp_str_equal_cstr(str, "libs"))        return SPN_GENERATOR_LIBS;
  else if (sp_str_equal_cstr(str, "system-libs")) return SPN_GENERATOR_SYSTEM_LIBS;

  SP_FATAL("Unknown flag {:fg brightyellow}; options are [include, lib-include, libs, system-libs]", SP_FMT_QUOTED_STR(str));
  SP_UNREACHABLE_RETURN(SPN_GENERATOR_ALL);
}

spn_gen_compiler_t spn_gen_compiler_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, ""))    return SPN_GENERATOR_COMPILER_NONE;
  else if (sp_str_equal_cstr(str, "gcc")) return SPN_GENERATOR_COMPILER_GCC;

  SP_FATAL("Unknown compiler {:fg brightyellow}; options are [gcc]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_GENERATOR_COMPILER_NONE);
}

spn_generator_kind_t spn_gen_kind_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, ""))           return SPN_GEN_KIND_RAW;
  else if (sp_str_equal_cstr(str, "shell"))      return SPN_GEN_KIND_SHELL;
  else if (sp_str_equal_cstr(str, "make"))       return SPN_GEN_KIND_MAKE;

  SP_FATAL("Unknown generator {:fg brightyellow}; options are [[empty], shell, make]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_GEN_KIND_RAW);
}

sp_str_t spn_cache_dir_kind_to_path(spn_dir_kind_t kind) {
  switch (kind) {
    case SPN_DIR_CACHE:   return app.paths.cache;
    case SPN_DIR_STORE:   return app.paths.store;
    case SPN_DIR_SOURCE:  return app.paths.source;
    case SPN_DIR_WORK:    return app.paths.work;
    default: SP_UNREACHABLE_RETURN(sp_str_lit(""));
  }
}

sp_str_t spn_cache_dir_kind_to_dep_path(spn_dep_t* dep, spn_dir_kind_t kind) {
  switch (kind) {
    case SPN_DIR_STORE:   return dep->paths.store;
    case SPN_DIR_INCLUDE: return dep->paths.include;
    case SPN_DIR_LIB:     return dep->paths.lib;
    case SPN_DIR_VENDOR:  return dep->paths.vendor;
    case SPN_DIR_SOURCE:  return dep->paths.source;
    case SPN_DIR_WORK:    return dep->paths.work;
    case SPN_DIR_CACHE:   SP_ASSERT(false);
    default: SP_UNREACHABLE_RETURN(sp_str_lit(""));
  }
}


///////////////
// GENERATOR //
///////////////
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
        default: SP_UNREACHABLE_RETURN(sp_str_lit(""));
      }
    }
    default: SP_UNREACHABLE_RETURN(sp_str_lit(""));
  }
}

sp_str_t spn_gen_build_entry_for_dep(spn_dep_t* dep, spn_gen_entry_kind_t kind, spn_gen_compiler_t compiler) {
  sp_dyn_array(sp_str_t) entries = SP_NULLPTR;

  switch (kind) {
    case SPN_GENERATOR_INCLUDE: {
      sp_dyn_array_push(entries, dep->paths.include);
      break;
    }
    case SPN_GENERATOR_RPATH:
      switch (dep->kind) {
        case SPN_LIB_KIND_SHARED: {
          sp_dyn_array_push(entries, dep->paths.lib);
          break;
        }
        case SPN_LIB_KIND_NONE:
        case SPN_LIB_KIND_STATIC:
        case SPN_LIB_KIND_SOURCE: {
          return SP_ZERO_STRUCT(sp_str_t);
        }
      }

      break;
    case SPN_GENERATOR_LIB_INCLUDE:  {
      switch (dep->kind) {
        case SPN_LIB_KIND_SHARED:
        case SPN_LIB_KIND_STATIC: {
          sp_dyn_array_push(entries, dep->paths.lib);
          break;
        }
        case SPN_LIB_KIND_NONE:
        case SPN_LIB_KIND_SOURCE: {
          return SP_ZERO_STRUCT(sp_str_t);
        }
      }

      break;
    }
    case SPN_GENERATOR_LIBS: {
      switch (dep->kind) {
        case SPN_LIB_KIND_NONE:
        case SPN_LIB_KIND_SHARED:
        case SPN_LIB_KIND_STATIC: {
          sp_os_lib_kind_t kind = spn_dep_kind_to_os_lib_kind(dep->kind);

          // sp_dyn_array_for(dep->recipe->libs, index) {
          //   sp_str_t lib = dep->recipe->libs[index];
          //   lib = sp_os_lib_to_file_name(lib, kind);
          //   lib = sp_os_join_path(dep->paths.lib, lib);
          //   sp_dyn_array_push(entries, lib);
          // }
          break;
        }
        case SPN_LIB_KIND_SOURCE: {
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

sp_str_t spn_gen_build_entries_for_dep(spn_dep_t* dep, spn_gen_compiler_t compiler) {
  spn_gen_entry_kind_t kinds [] = { SPN_GENERATOR_INCLUDE, SPN_GENERATOR_LIB_INCLUDE, SPN_GENERATOR_LIBS };

  sp_dyn_array(sp_str_t) entries = SP_NULLPTR;
  SP_CARR_FOR(kinds, index) {
    sp_dyn_array_push(entries, spn_gen_build_entry_for_dep(dep, kinds[index], compiler));
  }

  return sp_str_join_n(entries, sp_dyn_array_size(entries), sp_str_lit(" "));
}

sp_str_t spn_gen_build_entries_for_all(spn_gen_entry_kind_t kind, spn_gen_compiler_t compiler) {
  sp_dyn_array(sp_str_t) entries = SP_NULLPTR;

  sp_ht_for(app.deps, it) {
    spn_dep_t* dep = sp_ht_it_getp(app.deps, it);
    sp_str_t dep_flags = spn_gen_build_entry_for_dep(dep, kind, compiler);
    if (dep_flags.len > 0) {
      sp_dyn_array_push(entries, dep_flags);
    }
  }

  return sp_str_join_n(entries, sp_dyn_array_size(entries), sp_str_lit(" "));
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
      sp_ps_config_t config = SP_ZERO_INITIALIZE();
      config.command = sp_str_view(tools[i].command);

      SP_CARR_FOR(tools[i].args, j) {
        const c8* arg = tools[i].args[j];
        if (!arg) break;
        sp_ps_config_add_arg(&config, sp_str_view(arg));
      }

      sp_ps_config_add_arg(&config, path);

      sp_ps_output_t result = sp_ps_run(config);
      SP_ASSERT(!result.status.exit_code);
      return;
    }
  }
}


/////////
// GIT //
/////////
bool spn_git_clone(sp_str_t url, sp_str_t path) {
  sp_ps_output_t result = sp_ps_run((sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("clone"), SP_LIT("--quiet"),
      url,
      path
    },
  });

  return result.status.exit_code;
}

spn_err_t spn_git_fetch(sp_str_t repo) {
  sp_ps_output_t result = sp_ps_run((sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), repo,
      SP_LIT("fetch"), SP_LIT("--quiet")
    },
  });

  return result.status.exit_code;
}

u32 spn_git_num_updates(sp_str_t repo, sp_str_t from, sp_str_t to) {
  sp_ps_output_t result = sp_ps_run((sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), repo,
      SP_LIT("rev-list"), sp_format("{}..{}", SP_FMT_STR(from), SP_FMT_STR(to)),
      SP_LIT("--count")
    },
  });
  SP_ASSERT_FMT(!result.status.exit_code, "Failed to get commit delta for {:fg brightcyan}", SP_FMT_STR(repo));

  sp_str_t trimmed = sp_str_trim_right(result.out);
  return sp_parse_u32(trimmed);
}

sp_str_t spn_git_get_remote_url(sp_str_t repo) {
  sp_ps_output_t result = sp_ps_run((sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), repo,
      SP_LIT("remote"), SP_LIT("get-url"), SP_LIT("origin")
    },
  });

  SP_ASSERT_FMT(!result.status.exit_code, "Failed to get remote URL for {:fg brightcyan}", SP_FMT_STR(repo));
  return sp_str_trim_right(result.out);
}

sp_str_t spn_git_get_commit(sp_str_t repo, sp_str_t id) {
  sp_ps_output_t result = sp_ps_run((sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), repo,
      SP_LIT("rev-parse"),
      SP_LIT("--short=10"),
      id
    }
  });
  SP_ASSERT_FMT(!result.status.exit_code, "Failed to get {:fg brightyellow}:{:fg brightcyan}", SP_FMT_STR(repo), SP_FMT_STR(id));

  return sp_str_trim_right(result.out);
}

sp_str_t spn_git_get_commit_message(sp_str_t repo, sp_str_t id) {
  sp_ps_output_t result = sp_ps_run((sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), repo,
      SP_LIT("log"),
      SP_LIT("--format=%B"),
      SP_LIT("-n"),
      SP_LIT("1"),
      id
    }
  });
  SP_ASSERT_FMT(!result.status.exit_code, "Failed to log {:fg brightyellow}:{:fg brightcyan}", SP_FMT_STR(repo), SP_FMT_STR(id));

  return sp_str_trim_right(result.out);
}

spn_err_t spn_git_checkout(sp_str_t repo, sp_str_t id) {
  if (sp_str_empty(id)) return SPN_ERROR;
  if (!sp_os_does_path_exist(repo)) return SPN_ERROR;

  sp_ps_output_t result = sp_ps_run((sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), repo,
      SP_LIT("checkout"),
      SP_LIT("--quiet"),
      id
    }
  });

  if (result.status.exit_code) return SPN_ERROR;
  return SPN_OK;
}


/////////////
// SIGNALS //
/////////////
#ifdef SP_POSIX
void spn_signal_handler(int signum) {
  if (signum == SIGINT) {
    sp_atomic_s32_set(&app.control, 1);
    printf("\n");
    fflush(stdout);
  }
}

void spn_install_signal_handlers() {
  struct sigaction sa;
  sa.sa_handler = spn_signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);
}
#else
sp_win32_bool_t spn_windows_console_handler(sp_win32_dword_t ctrl_type) {
  if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_BREAK_EVENT) {
    sp_atomic_s32_set(&app->control, 1);
    printf("\n");
    fflush(stdout);
    return TRUE;
  }
  return FALSE;
}

void spn_install_signal_handlers() {
  SetConsoleCtrlHandler((PHANDLER_ROUTINE)spn_windows_console_handler, TRUE);
}
#endif

//////////
// TOML //
//////////
toml_table_t* spn_toml_parse(sp_str_t path) {
  sp_str_t file = sp_io_read_file(path);
  return toml_parse(sp_str_to_cstr(file), SP_NULLPTR, 0);
}

sp_str_t spn_toml_str(toml_table_t* toml, const c8* key) {
  toml_value_t value = toml_table_string(toml, key);
  SP_ASSERT(value.ok);
  return sp_str_view(value.u.s);
}

sp_str_t spn_toml_str_opt(toml_table_t* toml, const c8* key, const c8* fallback) {
  toml_value_t value = toml_table_string(toml, key);
  if (!value.ok) {
    return sp_str_view(fallback);
  }

  return sp_str_view(value.u.s);
}

sp_str_t spn_toml_arr_str(toml_array_t* toml, u32 it) {
  toml_value_t value = toml_array_string(toml, it);
  SP_ASSERT(value.ok);
  return sp_str_view(value.u.s);
}

sp_da(sp_str_t) spn_toml_arr_strs(toml_array_t* toml) {
  if (!toml) return SP_NULLPTR;

  sp_da(sp_str_t) strs = SP_NULLPTR;
  for (u32 it = 0; it < toml_array_len(toml); it++) {
    sp_dyn_array_push(strs, spn_toml_arr_str(toml, it));
  }

  return strs;
}

/////////
// TCC //
/////////
spn_tcc_t* spn_tcc_new() {
  spn_tcc_t* tcc = tcc_new();
  tcc_set_error_func(tcc, SP_NULLPTR, spn_tcc_error);
  tcc_set_output_type(tcc, TCC_OUTPUT_MEMORY);
  tcc_add_include_path(tcc, "/home/spader/source/spn/include");
  tcc_add_include_path(tcc, "/Users/spader/source/spn/include");
  tcc_define_symbol(tcc, "SPN", "");
  spn_tcc_register(tcc);
  return tcc;
}

void spn_tcc_register(spn_tcc_t* tcc) {
  SP_CARR_FOR(spn_lib, i) {
    tcc_add_symbol(tcc, spn_lib[i].symbol, spn_lib[i].fn);
  }
}

void spn_tcc_add_file(spn_tcc_t* tcc, sp_str_t file_path) {
  tcc_add_file(tcc, sp_str_to_cstr(file_path));
}

void spn_tcc_error(void* opaque, const char* message) {
  app.tcc_error = sp_str_from_cstr(message);
}

void spn_tcc_list_fn(void* opaque, const char* name, const void* value) {
  sp_da(sp_str_t) syms = (sp_da(sp_str_t))opaque;
  sp_dyn_array_push(syms, sp_str_from_cstr(name));
}

#if defined(SP_ELF)
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
#endif


/////////
// TUI //
/////////
void spn_tui_print_dep(spn_tui_t* tui, spn_dep_t* dep) {
  sp_mutex_lock(&dep->mutex);
  sp_str_t name = sp_str_pad(dep->recipe->name, tui->width);
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
        "{} {:color red} {:color brightblack} {} {:color brightyellow} {:color brightcyan}",
        SP_FMT_STR(name),
        SP_FMT_STR(state),
        SP_FMT_STR(dep->commits.resolved),
        SP_FMT_STR(dep->commits.message),
        SP_FMT_U32(dep->commits.delta),
        SP_FMT_STR(dep->paths.log)
    );
      break;
    }
    case SPN_DEP_BUILD_STATE_DONE: {
      status = sp_format(
        "{} {:color green} {:color brightblack :pad 10} {} {:color brightyellow} {:color brightcyan}",
        SP_FMT_STR(name),
        SP_FMT_STR(state),
        SP_FMT_STR(dep->commits.resolved),
        SP_FMT_STR(dep->commits.message),
        SP_FMT_U32(dep->commits.delta),
        SP_FMT_STR(dep->paths.store)
      );
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

void spn_tui_run(spn_tui_t* tui) {
  while (true) {
    if (sp_atomic_s32_get(&app.control) != 0) {
      sp_ht_for(app.deps, it) {
        spn_dep_t* dep = sp_ht_it_getp(app.deps, it);
        sp_mutex_lock(&dep->mutex);
        if (!spn_dep_state_is_terminal(dep)) {
          dep->state.build = SPN_DEP_BUILD_STATE_FAILED;
        }
        sp_mutex_unlock(&dep->mutex);
      }
      break;
    }

    bool building = false;

    sp_ht_for(app.deps, it) {
      spn_dep_t* dep = sp_ht_it_getp(app.deps, it);

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
    sp_os_sleep_ms(10);
  }

  // Clean up
  switch (tui->mode) {
    case SPN_OUTPUT_MODE_INTERACTIVE: {
      sp_tui_restore(tui);
      sp_tui_show_cursor();
      sp_tui_home();
      sp_tui_flush();
      break;
    }
    case SPN_OUTPUT_MODE_NONINTERACTIVE: {
      break;
    }
    case SPN_OUTPUT_MODE_QUIET: {
      sp_ht_for(app.deps, it) {
        spn_dep_t* dep = sp_ht_it_getp(app.deps, it);
        dep->tui_state = dep->state;
        spn_tui_print_dep(tui, dep);
        sp_tui_print(SP_LIT("\n"));
      }

      break;
    }
    case SPN_OUTPUT_MODE_NONE: {
      break;
    }
  }

  // Report any failures
  sp_str_builder_t builder = SP_ZERO_INITIALIZE();

  bool failed = false;
  sp_ht_for(app.deps, it) {
    spn_dep_t* dep = sp_ht_it_getp(app.deps, it);

    if (dep->log.file.fd) {
      sp_io_close(&dep->log);
    }

    switch (dep->state.build) {
      case SPN_DEP_BUILD_STATE_NONE:
      case SPN_DEP_BUILD_STATE_DONE: {
        break;
      }
      case SPN_DEP_BUILD_STATE_FAILED:
      default: {
        failed = true;

        sp_str_builder_new_line(&builder);
        sp_str_builder_append_fmt(&builder, "> {:fg brightyellow}", SP_FMT_STR(dep->recipe->name));
        sp_str_builder_new_line(&builder);
        sp_str_builder_append_fmt(&builder, "{:fg brightblack}", SP_FMT_STR(sp_io_read_file(dep->paths.log)));
        sp_log(sp_str_builder_write(&builder));

        break;
      }
    }
  }
}

void spn_tui_init(spn_tui_t* tui, spn_tui_mode_t mode) {
  tui->mode = mode;
  tui->num_deps = sp_ht_size(app.deps);
  tui->width = 0;
  sp_ht_for(app.deps, it) {
    spn_dep_t* dep = sp_ht_it_getp(app.deps, it);
    tui->width = SP_MAX(tui->width, dep->recipe->name.len);
  }

  switch (tui->mode) {
    case SPN_OUTPUT_MODE_INTERACTIVE: {
      sp_ht_for(app.deps, it) {
        sp_tui_print(SP_LIT("\n"));
      }
      sp_tui_hide_cursor();
      sp_tui_flush();

      sp_tui_checkpoint(tui);
      sp_tui_setup_raw_mode(tui);

      break;
    }
    case SPN_OUTPUT_MODE_QUIET:
    case SPN_OUTPUT_MODE_NONINTERACTIVE:
    case SPN_OUTPUT_MODE_NONE: {
      break;
    }
  }
}

void spn_tui_update(spn_tui_t* tui) {
  switch (tui->mode) {
    case SPN_OUTPUT_MODE_INTERACTIVE: {
      sp_tui_up(sp_ht_size(app.deps));

      sp_ht_for(app.deps, it) {
        spn_dep_t* dep = sp_ht_it_getp(app.deps, it);

        sp_tui_home();
        sp_tui_clear_line();
        spn_tui_print_dep(tui, dep);
        sp_tui_down(1);
      }

      sp_tui_flush();
      break;
    }
    case SPN_OUTPUT_MODE_NONINTERACTIVE: {
      sp_ht_for(app.deps, it) {
        spn_dep_t* dep = sp_ht_it_getp(app.deps, it);

        if (dep->state.build != dep->tui_state.build) {
          dep->tui_state = dep->state;
          spn_tui_print_dep(tui, dep);
          sp_tui_print(SP_LIT("\n"));
        }
      }

      break;
    }
    case SPN_OUTPUT_MODE_QUIET:
    case SPN_OUTPUT_MODE_NONE: {
      break;
    }
  }
}

void spn_update_lock_file() {
  sp_str_t haystack = sp_io_read_file(app.paths.project.toml);
  sp_str_t needle = sp_str_lit("#include \"spn/gen/project.h\"");
  sp_str_t mark = SP_ZERO_INITIALIZE();
  while (true) {
    switch (sp_str_at(haystack, 0)) {
      case '#': {
        if (sp_str_starts_with(haystack, needle)) {
          SP_ASSERT(haystack.len >= needle.len);
          mark = sp_str_sub(haystack, needle.len, haystack.len - needle.len);
          break;
        }
      }
      default: {
        haystack.data++;
        haystack.len--;
        break;
      }
    }

    if (sp_str_empty(haystack)) break;
    if (sp_str_valid(mark)) break;
  }

  SP_ASSERT(sp_str_valid(mark));

  sp_io_stream_t io = sp_io_from_file(app.paths.project.toml, SP_IO_MODE_WRITE);
  // version
  // commit

  sp_da(spn_dep_t*) sorted = SP_NULLPTR;
  sp_ht_for(app.deps, it) {
    spn_dep_t* dep = sp_ht_it_getp(app.deps, it);
    sp_dyn_array_push(sorted, dep);
  }
  qsort(sorted, sp_dyn_array_size(sorted), sizeof(spn_dep_t*), spn_sort_kernel_dep_ptr);

  sp_da(sp_str_t) deps = SP_NULLPTR;
  sp_dyn_array_push(deps, SP_LIT("#define SPN_DEPS()"));

  sp_da(sp_str_t) locks = SP_NULLPTR;
  sp_dyn_array_push(locks, SP_LIT("#define SPN_LOCKS()"));

  sp_dyn_array_for(sorted, it) {
    spn_dep_t* dep = sorted[it];
    sp_dyn_array_push(deps, sp_format("  SPN_DEP({})", SP_FMT_STR(dep->name)));
    sp_dyn_array_push(locks, sp_format("  SPN_LOCK({}, {})", SP_FMT_STR(dep->name), SP_FMT_QSTR(dep->commits.resolved)));
  }

  sp_io_write_str(&io, sp_str_join_n(deps, sp_dyn_array_size(deps), SP_LIT(" \\\n")));
  sp_io_write_cstr(&io, "\n\n");

  sp_io_write_str(&io, sp_str_join_n(locks, sp_dyn_array_size(locks), SP_LIT(" \\\n")));
  sp_io_write_cstr(&io, "\n\n");

  sp_io_write_str(&io, needle);

  sp_io_write_str(&io, mark);

  sp_io_close(&io);
}

///////////
// BUILD //
///////////
sp_str_t spn_opt_cstr_required(const c8* value) {
  SP_ASSERT(value);
  return sp_str_from_cstr(value);
}

sp_str_t spn_opt_cstr_optional(const c8* value, const c8* fallback) {
  sp_str_t result = sp_str_view(value);
  if (sp_str_empty(result)) {
    return sp_str_from_cstr(fallback);
  }

  return sp_str_copy(result);
}

bool spn_dep_state_is_terminal(spn_dep_t* dep) {
  switch (dep->state.build) {
    case SPN_DEP_BUILD_STATE_NONE:
    case SPN_DEP_BUILD_STATE_FAILED:
    case SPN_DEP_BUILD_STATE_DONE:
    case SPN_DEP_BUILD_STATE_CANCELED: return true;
    default: return false;
  }
}

bool spn_dep_context_is_binary(spn_dep_t* dep) {
  switch (dep->kind) {
    case SPN_LIB_KIND_SHARED: return true;
    case SPN_LIB_KIND_STATIC: return true;
    case SPN_LIB_KIND_NONE:
    case SPN_LIB_KIND_SOURCE: return false;
  }

  SP_UNREACHABLE_RETURN(false);
}

spn_package_t* spn_recipe_find(sp_str_t name) {
  return sp_ht_getp(app.packages, name);
}

s32 spn_sort_kernel_dep_ptr(const void* a, const void* b) {
  spn_dep_t* da = *(spn_dep_t**)a;
  spn_dep_t* db = *(spn_dep_t**)b;
  return sp_str_compare_alphabetical(da->name, db->name);
}


bool spn_dep_context_is_build_stamped(spn_dep_t* context) {
  return sp_os_does_path_exist(context->paths.stamp);
}

void spn_dep_context_init(spn_dep_t* dep, spn_package_t* recipe) {
  SP_ASSERT(recipe);
  dep->name = sp_str_copy(recipe->name);
  dep->recipe = recipe;
  sp_mutex_init(&dep->mutex, SP_MUTEX_PLAIN);
}

spn_err_t spn_dep_context_resolve_build_id(spn_dep_t* dep) {
  if (sp_str_empty(dep->commits.resolved)) {
    return SPN_ERROR;
  }

  sp_mutex_lock(&dep->mutex);

  sp_dyn_array(sp_hash_t) hashes = SP_NULLPTR;
  sp_dyn_array_push(hashes, sp_hash_str(dep->commits.resolved));
  sp_dyn_array_push(hashes, dep->hash);
  sp_hash_t hash = sp_hash_combine(hashes, sp_dyn_array_size(hashes));
  dep->build_id = sp_format("{}", SP_FMT_SHORT_HASH(hash));

  sp_str_t build_dir = sp_os_join_path(app.paths.build, dep->recipe->name);
  sp_str_t store_dir = sp_os_join_path(app.paths.store, dep->recipe->name);
  dep->paths.source = dep->recipe->paths.source;
  dep->paths.work = sp_os_join_path(build_dir, dep->build_id);
  dep->paths.store = sp_os_join_path(store_dir, dep->build_id);
  dep->paths.include = sp_os_join_path(dep->paths.store, SP_LIT("include"));
  dep->paths.lib = sp_os_join_path(dep->paths.store, SP_LIT("lib"));
  dep->paths.vendor = sp_os_join_path(dep->paths.store, SP_LIT("vendor"));
  dep->paths.log = sp_os_join_path(dep->paths.work, SP_LIT("spn.log"));
  dep->paths.stamp = sp_os_join_path(dep->paths.store, SP_LIT("spn.stamp"));

  sp_mutex_unlock(&dep->mutex);

  return SPN_OK;
}

spn_err_t spn_dep_context_sync_remote(spn_dep_t* dep) {
  if (!sp_os_does_path_exist(dep->recipe->paths.source)) {
    spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_CLONING);

    sp_str_t url = sp_format("https://github.com/{}.git", SP_FMT_STR(dep->recipe->repo));
    if (spn_git_clone(url, dep->recipe->paths.source)) {
      spn_dep_context_set_build_error(dep, sp_format(
        "Failed to clone {:fg brightcyan}",
        SP_FMT_STR(dep->recipe->name)
      ));

      return SPN_ERROR;
    }

    SP_ASSERT(sp_os_is_directory(dep->recipe->paths.source));
  }
  else {
    spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_FETCHING);
    if (spn_git_fetch(dep->recipe->paths.source)) {
      spn_dep_context_set_build_error(dep, sp_format(
        "Failed to fetch {:fg brightcyan}",
        SP_FMT_STR(dep->recipe->name)
      ));

      return SPN_ERROR;
    }
  }

  return SPN_OK;
}

void spn_dep_context_set_commit(spn_dep_t* dep, sp_str_t commit) {
  sp_mutex_lock(&dep->mutex);
  dep->commits.resolved = commit;
  sp_mutex_unlock(&dep->mutex);
}

spn_err_t spn_dep_context_resolve_commit(spn_dep_t* dep) {
  spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_RESOLVING);

  sp_str_t commit = SP_ZERO_INITIALIZE();

  sp_str_t message = spn_git_get_commit_message(dep->recipe->paths.source, commit);
  message = sp_str_truncate(message, 32, SP_LIT("..."));
  message = sp_str_replace_c8(message, '\n', ' ');
  message = sp_str_pad(message, 32);

  sp_mutex_lock(&dep->mutex);

  dep->commits.resolved = commit;
  dep->commits.message = message;

  sp_mutex_unlock(&dep->mutex);

  return SPN_OK;
}

spn_err_t spn_dep_context_sync_local(spn_dep_t* dep) {
  spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_CHECKING_OUT);

  return spn_git_checkout(dep->recipe->paths.source, dep->commits.resolved);
}

s32 spn_dep_thread_resolve(void* user_data) {
  spn_dep_t* dep = (spn_dep_t*)user_data;
  if (spn_dep_context_resolve(dep)) return 1;
  spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_DONE);
}

s32 spn_dep_thread_build(void* user_data) {
  spn_dep_t* dep = (spn_dep_t*)user_data;
  if (spn_dep_context_resolve(dep)) return 1;
  if (spn_dep_context_build(dep)) return 1;
  spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_DONE);
}

s32 spn_dep_context_resolve(spn_dep_t* dep) {
  SP_ASSERT(!spn_dep_context_sync_remote(dep));
  SP_ASSERT(!spn_dep_context_resolve_commit(dep));

  return SPN_OK;
}

s32 spn_dep_context_build(spn_dep_t* dep) {
  SP_ASSERT(!spn_dep_context_sync_local(dep));
  SP_ASSERT(!spn_dep_context_resolve_build_id(dep));

  if (spn_dep_context_is_build_stamped(dep)) {
    if (!dep->force) {
      return 0;
    }
  }

  sp_os_create_directory(dep->paths.work);
  sp_os_create_directory(dep->paths.store);
  sp_os_create_directory(dep->paths.include);
  sp_os_create_directory(dep->paths.lib);
  sp_os_create_directory(dep->paths.vendor);
  dep->log = sp_io_from_file(dep->paths.log, SP_IO_MODE_WRITE);

  dep->cfg = (sp_ps_config_t) {
    .io = {
      .in = { .mode = SP_PS_IO_MODE_NULL },
      .out = { .mode = SP_PS_IO_MODE_EXISTING, .stream = dep->log },
      .err = { .mode = SP_PS_IO_MODE_REDIRECT }
    },
    .cwd = dep->paths.work
  };

  spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_BUILDING);
  if (dep->recipe->build) {
    dep->recipe->build(dep);
  }

  spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_PACKAGING);
  if (dep->recipe->package) {
    dep->recipe->package(dep);
  }

  spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_STAMPING);
  sp_os_create_file(dep->paths.stamp);

  return SPN_OK;
}

void spn_dep_context_set_build_state(spn_dep_t* dep, spn_dep_build_state_t state) {
  sp_mutex_lock(&dep->mutex);
  dep->state.build = state;
  sp_mutex_unlock(&dep->mutex);
}

void spn_dep_context_set_build_error(spn_dep_t* dep, sp_str_t error) {
  sp_mutex_lock(&dep->mutex);
  dep->state.build = SPN_DEP_BUILD_STATE_FAILED;
  dep->error = sp_str_copy(error);
  sp_mutex_unlock(&dep->mutex);
}




c8 spn_semver_parser_peek(spn_semver_parser_t* parser) {
  if (spn_semver_parser_is_done(parser)) return '\0';
  return sp_str_at(parser->fmt, parser->it);
}

void spn_semver_parser_eat(spn_semver_parser_t* parser) {
  parser->it++;
}

void spn_semver_parser_eat_and_assert(spn_semver_parser_t* parser, c8 c) {
  SP_ASSERT(spn_semver_parser_peek(parser) == c);
  spn_semver_parser_eat(parser);
}

bool spn_semver_parser_is_digit(c8 c) {
  return c >= '0' && c <= '9';
}

bool spn_semver_parser_is_whitespace(spn_semver_parser_t* parser) {
  c8 c = sp_str_at(parser->fmt, parser->it);
  return c == ' ' || c == '\t' || c == '\n';
}

bool spn_semver_parser_is_done(spn_semver_parser_t* parser) {
  return parser->it >= parser->fmt.len;
}

void spn_semver_parser_eat_whitespace(spn_semver_parser_t* parser) {
  while (true) {
    if (spn_semver_parser_is_done(parser)) break;
    if (!spn_semver_parser_is_whitespace(parser)) break;

    spn_semver_parser_eat(parser);
  }
}

u32 spn_semver_parser_parse_number(spn_semver_parser_t* parser) {
  u32 result = 0;
  while (true) {
    if (spn_semver_parser_is_done(parser)) break;
    if (!spn_semver_parser_is_digit(spn_semver_parser_peek(parser))) break;

    c8 c = spn_semver_parser_peek(parser);
    result = result * 10 + (c - '0');
    spn_semver_parser_eat(parser);
  }

  return result;
}

spn_semver_t spn_semver_parser_parse_version(spn_semver_parser_t* parser, bool* has_minor, bool* has_patch) {
  spn_semver_t version = SP_ZERO_INITIALIZE();

  version.major = spn_semver_parser_parse_number(parser);

  if (!spn_semver_parser_is_done(parser) && spn_semver_parser_peek(parser) == '.') {
    spn_semver_parser_eat(parser);
    version.minor = spn_semver_parser_parse_number(parser);
    if (has_minor) *has_minor = true;

    if (!spn_semver_parser_is_done(parser) && spn_semver_parser_peek(parser) == '.') {
      spn_semver_parser_eat(parser);
      version.patch = spn_semver_parser_parse_number(parser);
      if (has_patch) *has_patch = true;
    }
  }

  return version;
}

spn_version_range_t spn_semver_caret_to_range(spn_semver_t version, bool has_minor, bool has_patch) {
  spn_version_range_t range = {0};
  if (version.major > 0) {
    range.low = (spn_semver_bound_t){ version, SPN_SEMVER_OP_GEQ };
    range.high = (spn_semver_bound_t){ { version.major + 1, 0, 0 }, SPN_SEMVER_OP_LT };
  } else if (version.minor > 0) {
    range.low = (spn_semver_bound_t){ version, SPN_SEMVER_OP_GEQ };
    range.high = (spn_semver_bound_t){ { version.major, version.minor + 1, 0 }, SPN_SEMVER_OP_LT };
  } else {
    range.low = (spn_semver_bound_t){ version, SPN_SEMVER_OP_GEQ };
    range.high = (spn_semver_bound_t){ { version.major, version.minor, version.patch + 1 }, SPN_SEMVER_OP_LT };
  }

  return range;
}

spn_version_range_t spn_semver_tilde_to_range(spn_semver_t version, bool has_minor, bool has_patch) {
  spn_version_range_t range = {0};
  if (has_patch) {
    range.low = (spn_semver_bound_t){ version, SPN_SEMVER_OP_GEQ };
    range.high = (spn_semver_bound_t){ { version.major, version.minor + 1, 0 }, SPN_SEMVER_OP_LT };
  } else if (has_minor) {
    range.low = (spn_semver_bound_t){ version, SPN_SEMVER_OP_GEQ };
    range.high = (spn_semver_bound_t){ { version.major, version.minor + 1, 0 }, SPN_SEMVER_OP_LT };
  } else {
    range.low = (spn_semver_bound_t){ version, SPN_SEMVER_OP_GEQ };
    range.high = (spn_semver_bound_t){ { version.major + 1, 0, 0 }, SPN_SEMVER_OP_LT };
  }

  return range;
}

spn_version_range_t spn_semver_wildcard_to_range(spn_semver_t version, bool has_major, bool has_minor) {
  spn_version_range_t range = {0};

  if (!has_major) {
    range.low = (spn_semver_bound_t){ { 0, 0, 0 }, SPN_SEMVER_OP_GEQ };
    range.high = (spn_semver_bound_t){ { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF }, SPN_SEMVER_OP_LT };
  } else if (!has_minor) {
    range.low = (spn_semver_bound_t){ { version.major, 0, 0 }, SPN_SEMVER_OP_GEQ };
    range.high = (spn_semver_bound_t){ { version.major + 1, 0, 0 }, SPN_SEMVER_OP_LT };
  } else {
    range.low = (spn_semver_bound_t){ { version.major, version.minor, 0 }, SPN_SEMVER_OP_GEQ };
    range.high = (spn_semver_bound_t){ { version.major, version.minor + 1, 0 }, SPN_SEMVER_OP_LT };
  }

  return range;
}

spn_version_range_t spn_semver_comparison_to_range(spn_semver_op_t op, spn_semver_t version) {
  spn_version_range_t range = {0};

  range.low = (spn_semver_bound_t){ version, op };
  range.high = (spn_semver_bound_t){ { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF }, SPN_SEMVER_OP_LT };

  return range;
}

spn_version_range_t spn_version_range_from_str(sp_str_t str) {
  spn_semver_parser_t parser = { .fmt = str, .it = 0 };
  spn_version_range_t range = {0};

  spn_semver_parser_eat_whitespace(&parser);

  c8 c = spn_semver_parser_peek(&parser);

  if (c == '^') {
    spn_semver_parser_eat(&parser);
    bool has_minor = false, has_patch = false;
    spn_semver_t version = spn_semver_parser_parse_version(&parser, &has_minor, &has_patch);
    range = spn_semver_caret_to_range(version, has_minor, has_patch);
  }
  else if (c == '~') {
    spn_semver_parser_eat(&parser);
    bool has_minor = false, has_patch = false;
    spn_semver_t version = spn_semver_parser_parse_version(&parser, &has_minor, &has_patch);
    range = spn_semver_tilde_to_range(version, has_minor, has_patch);
  }
  else if (c == '*') {
    spn_semver_parser_eat(&parser);
    range = spn_semver_wildcard_to_range((spn_semver_t){0}, false, false);
  }
  else if (spn_semver_parser_is_digit(c)) {
    u32 saved_it = parser.it;
    bool has_major = false, has_minor = false;
    spn_semver_t version = spn_semver_parser_parse_version(&parser, &has_minor, &has_minor);
    has_major = true;

    if (!spn_semver_parser_is_done(&parser) && spn_semver_parser_peek(&parser) == '.') {
      spn_semver_parser_eat(&parser);
      if (!spn_semver_parser_is_done(&parser) && spn_semver_parser_peek(&parser) == '*') {
        spn_semver_parser_eat(&parser);
        if (!has_minor) {
          range = spn_semver_wildcard_to_range(version, true, false);
        } else {
          range = spn_semver_wildcard_to_range(version, true, true);
        }
        return range;
      } else {
        parser.it = saved_it;
      }
    }

    parser.it = saved_it;
    bool has_minor_v2 = false, has_patch_v2 = false;
    version = spn_semver_parser_parse_version(&parser, &has_minor_v2, &has_patch_v2);
    range = spn_semver_caret_to_range(version, has_minor_v2, has_patch_v2);
  }
  else if (c == '>' || c == '<' || c == '=') {
    spn_semver_op_t op;
    if (c == '>') {
      spn_semver_parser_eat(&parser);
      if (!spn_semver_parser_is_done(&parser) && spn_semver_parser_peek(&parser) == '=') {
        spn_semver_parser_eat(&parser);
        op = SPN_SEMVER_OP_GEQ;
      } else {
        op = SPN_SEMVER_OP_GT;
      }
    } else if (c == '<') {
      spn_semver_parser_eat(&parser);
      if (!spn_semver_parser_is_done(&parser) && spn_semver_parser_peek(&parser) == '=') {
        spn_semver_parser_eat(&parser);
        op = SPN_SEMVER_OP_LEQ;
      } else {
        op = SPN_SEMVER_OP_LT;
      }
    } else {
      spn_semver_parser_eat(&parser);
      op = SPN_SEMVER_OP_EQ;
    }

    spn_semver_parser_eat_whitespace(&parser);
    bool has_minor = false, has_patch = false;
    spn_semver_t version = spn_semver_parser_parse_version(&parser, &has_minor, &has_patch);
    range = spn_semver_comparison_to_range(op, version);
  }
  else {
    SP_ASSERT(false && "unsupported semver format");
  }

  return range;
}

spn_package_t spn_package_load(sp_str_t file_path) {
  spn_toml_package_t toml = SP_ZERO_INITIALIZE();
  toml.root = spn_toml_parse(file_path);
  toml.package = toml_table_table(toml.root, "package");
  toml.lib = toml_table_table(toml.root, "lib");
  toml.bin = toml_table_array(toml.root, "bin");
  toml.deps = toml_table_table(toml.root, "deps");

  spn_package_t package = SP_ZERO_INITIALIZE();
  sp_ht_set_fns(package.bin, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(package.deps, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);

  package.toml = toml;
  package.name = spn_toml_str(toml.package, "name");
  package.repo = spn_toml_str_opt(toml.package, "repo", "");
  package.paths = (spn_package_paths_t) {
    .source = sp_os_join_path(app.paths.source, package.name),
    .file = sp_str_copy(file_path),
  };
  package.state = SPN_PACKAGE_STATE_UNLOADED;

  if (toml.lib) {
    toml.src = toml_table_table(toml.lib, "source");
    toml.a = toml_table_table(toml.lib, "static");
    toml.so = toml_table_table(toml.lib, "shared");

    if (toml.src) {
      spn_lib_t lib = {
        .src = {
          .sources = spn_toml_arr_strs(toml_table_array(toml.src, "sources")),
          .headers = spn_toml_arr_strs(toml_table_array(toml.src, "headers")),
        }
      };
      sp_ht_insert(package.lib, SPN_LIB_KIND_SOURCE, lib);
    }

    SP_ASSERT(!toml.a);
    SP_ASSERT(!toml.so);
  }

  if (toml.bin) {
    spn_toml_arr_for(toml.bin, n) {
      toml_table_t* it = toml_array_table(toml.bin, n);
      spn_bin_t bin = {
        .name = spn_toml_str(it, "name"),
        .entry = spn_toml_str(it, "entry"),
      };

      sp_ht_insert(package.bin, bin.name, bin);
    }
  }

  if (toml.deps) {
    const c8* key = SP_NULLPTR;
    spn_toml_for(toml.deps, n, key) {
      spn_dep_req_t dep = {
        .name = sp_str_from_cstr(key),
        .version = spn_version_range_from_str(spn_toml_str(toml.deps, key)),
      };

      sp_ht_insert(package.deps, dep.name, dep);
    }
  }

  return package;
}

/////////
// APP //
/////////
void spn_app_init(spn_app_t* app, u32 num_args, const c8** args) {
  spn_cli_t* cli = &app->cli;

  struct argparse_option options [] = {
    OPT_HELP(),
    OPT_STRING(
      'C',
      "project-dir",
      &cli->project_directory,
      "specify the directory containing project file",
      SP_NULLPTR
    ),
    OPT_STRING(
      'f',
      "file",
      &cli->project_file,
      "specify the project file path",
      SP_NULLPTR
    ),
    OPT_STRING(
      'm',
      "matrix",
      &cli->matrix,
      "",
      SP_NULLPTR
    ),
    OPT_STRING(
      'o',
      "output",
      &cli->output,
      "output mode: interactive (update in place), noninteractive (update serially), quiet (print result at end), none (print nothing)",
      SP_NULLPTR
    ),
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
    SP_EXIT_SUCCESS();
  }

  sp_atomic_s32_set(&app->control, 0);

  spn_install_signal_handlers();

  sp_ht_set_fns(app->packages, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(app->deps, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(app->builds, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(app->matrices, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);

  app->paths.storage = sp_os_join_path(sp_os_get_storage_path(), SP_LIT("spn"));

  // Install
  app->paths.executable = sp_os_get_executable_path();
  app->paths.work = sp_os_get_cwd();

  // Project
  sp_str_t project_file = sp_str_lit("spn.toml");

  if (app->cli.project_file) {
    sp_str_t file_path = sp_str_view(app->cli.project_file);
    if (app->cli.project_file[0] != '/') {
      file_path = sp_os_join_path(app->paths.work, file_path);
    }

    file_path = sp_os_canonicalize_path(file_path);
    project_file = sp_os_extract_file_name(file_path);
    app->paths.project.dir = sp_os_parent_path(file_path);
  }
  else if (app->cli.project_directory) {
    sp_str_t project = sp_os_join_path(app->paths.work, sp_str_view(app->cli.project_directory));
    project = sp_os_canonicalize_path(project);
    app->paths.project.dir = project;
  }
  else {
    app->paths.project.dir = sp_str_copy(app->paths.work);
  }

  app->paths.project.toml = sp_os_join_path(app->paths.project.dir, project_file);
  app->paths.project.lock = sp_os_join_path(app->paths.project.dir, SP_LIT("spn.lock"));

  // Config
  app->paths.config_dir = sp_os_join_path(sp_os_get_config_path(), SP_LIT("spn"));
  app->paths.config = sp_os_join_path(app->paths.config_dir, SP_LIT("spn.toml"));

  // Bootstrap the user config, which tells us if spn itself is installed in the usual,
  // well-known location or in somewhere the user specified (for development)
  if (sp_os_does_path_exist(app->paths.config)) {
    toml_table_t* toml = spn_toml_parse(app->paths.config);

    toml_value_t spn = toml_table_string(toml, "spn");
    if (spn.ok) {
      app->paths.spn = sp_str_view(spn.u.s);
    }
  }

  if (!sp_str_valid(app->paths.spn)) {
    app->paths.spn = sp_os_join_path(app->paths.storage, SP_LIT("spn"));
  }

  if (!sp_os_does_path_exist(app->paths.spn)) {
    sp_str_t url = SP_LIT("https://github.com/tspader/spn.git");
    SP_LOG(
      "Cloning recipes from {:fg brightcyan} to {:fg brightcyan}",
      SP_FMT_STR(url),
      SP_FMT_STR(app->paths.spn)
    );

    SP_ASSERT(!spn_git_clone(url, app->paths.spn));
  }

  sp_dyn_array_push(app->search, sp_os_join_path(app->paths.spn, SP_LIT("recipes")));

  // Find the cache directory after the config has been fully loaded
  app->paths.cache = sp_os_join_path(app->paths.storage, SP_LIT("cache"));
  app->paths.source = sp_os_join_path(app->paths.cache, SP_LIT("source"));
  app->paths.build = sp_os_join_path(app->paths.cache, SP_LIT("build"));
  app->paths.store = sp_os_join_path(app->paths.cache, SP_LIT("store"));

  sp_os_create_directory(app->paths.cache);
  sp_os_create_directory(app->paths.source);
  sp_os_create_directory(app->paths.build);

  sp_da(sp_str_t) search = SP_NULLPTR;
  sp_dyn_array_for(app->search, it) {
    sp_dyn_array_push(search, app->search[it]);
  }

  sp_da(sp_str_t) files = SP_NULLPTR;

  sp_dyn_array_for(search, i) {
    sp_str_t path = search[i];
    if (!sp_os_does_path_exist(path)) continue;
    if (!sp_os_is_directory(path)) continue;

    sp_da(sp_os_dir_entry_t) entries = sp_os_scan_directory(path);
    sp_dyn_array_for(entries, i) {
      sp_os_dir_entry_t entry = entries[i];
      if (sp_os_is_directory(entry.file_path)) {
        sp_dyn_array_push(search, sp_str_copy(entry.file_path));
      }
      else {
        sp_str_t ext = sp_os_extract_extension(entry.file_name);
        if (!sp_str_equal_cstr(ext, "toml")) continue;

        sp_dyn_array_push(files, sp_str_copy(entry.file_path));
      }
    }
  }

  sp_dyn_array_for(files, it) {
    sp_str_t file_path = files[it];
    spn_package_t package = spn_package_load(file_path);
    sp_ht_insert(app->packages, package.name, package);
  }

  app->package = spn_package_load(app->paths.project.toml);


}

///////
// CLI //
/////////
void spn_cli_run(spn_app_t* app) {
  spn_cli_t* cli = &app->cli;

  if (!cli->num_args || !cli->args || !cli->args[0]) {
    SP_ASSERT(false);
  }
  else if (sp_cstr_equal("list", cli->args[0])) {
    spn_cli_list(cli);
  }
  else if (sp_cstr_equal("clean", cli->args[0])) {
    spn_cli_clean(cli);
  }
  else if (sp_cstr_equal("build", cli->args[0])) {
    spn_cli_build(cli);
  }
  else if (sp_cstr_equal("copy", cli->args[0])) {
    spn_cli_copy(cli);
  }
  else if (sp_cstr_equal("print", cli->args[0])) {
    spn_cli_print(cli);
  }
  else if (sp_cstr_equal("ls", cli->args[0])) {
    spn_cli_ls(cli);
  }
  else if (sp_cstr_equal("which", cli->args[0])) {
    spn_cli_which(cli);
  }
  else if (sp_cstr_equal("recipe", cli->args[0])) {
    spn_cli_recipe(cli);
  }
  else if (sp_cstr_equal("add", cli->args[0])) {
    spn_cli_add(cli);
  }
}


spn_dep_t* spn_cli_assert_dep_exists(sp_str_t name) {
  spn_dep_t* dep = sp_ht_getp(app.deps, name);
  SP_ASSERT_FMT(dep, "{:fg brightyellow} is not in this project", SP_FMT_STR(name));
  return dep;
}

void spn_cli_assert_dep_is_locked(spn_dep_t* dep) {
  if (sp_str_empty(dep->lock)) {
    SP_FATAL(
      "{:fg brightcyan} hasn't been built yet. Run {:fg brightyellow} first.",
      SP_FMT_STR(dep->recipe->name),
      SP_FMT_CSTR("spn build")
    );
  }
}

void spn_cli_init(spn_cli_t* cli) {

}

void spn_cli_list(spn_cli_t* cli) {
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

  sp_ht_for(app.packages, it) {
    spn_package_t* recipe = sp_ht_it_getp(app.packages, it);
    max_name_len = SP_MAX(max_name_len, recipe->name.len);
  }

  sp_str_builder_t builder = SP_ZERO_INITIALIZE();
  sp_ht_for(app.packages, it) {
    spn_package_t* recipe = sp_ht_it_getp(app.packages, it);
    sp_str_builder_append_fmt(
      &builder,
      "{:fg brightcyan} {}",
      SP_FMT_STR(sp_str_pad(recipe->name, max_name_len)),
      SP_FMT_STR(recipe->repo)
    );
    sp_str_builder_new_line(&builder);
  }

  sp_log(sp_str_builder_write(&builder));
}

void spn_cli_clean(spn_cli_t* cli) {
  SP_LOG("Removing {:fg brightcyan}", SP_FMT_STR(app.paths.build));
  sp_os_remove_directory(app.paths.build);
  SP_LOG("Removing {:fg brightcyan}", SP_FMT_STR(app.paths.store));
  sp_os_remove_directory(app.paths.store);
}


void spn_cli_copy(spn_cli_t* cli) {
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

  sp_ht_for(app.deps, it) {
    spn_dep_t* dep = sp_ht_it_getp(app.deps, it);
    spn_cli_assert_dep_is_locked(dep);
    spn_dep_context_set_commit(dep, dep->lock);
    spn_dep_context_resolve_build_id(dep);

    sp_dyn_array(sp_os_dir_entry_t) entries = sp_os_scan_directory(dep->paths.lib);
    sp_dyn_array_for(entries, i) {
      sp_os_dir_entry_t* entry = entries + i;
      sp_os_copy_file(
        entry->file_path,
        sp_os_join_path(to, sp_os_extract_file_name(entry->file_path))
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

void spn_cli_print(spn_cli_t* cli) {
  spn_cli_print_t* command = &cli->print;

  struct argparse argparse;
  argparse_init(
    &argparse,
    (struct argparse_option []) {
      OPT_HELP(),
      OPT_STRING('p', "path", &command->path, "write generated flags to a file", SP_NULLPTR),
      OPT_STRING('c', "compiler", &command->compiler, "generate for compiler [*gcc, msvc]", SP_NULLPTR),
      OPT_STRING('g', "generator", &command->generator, "output format [*raw, shell, make]", SP_NULLPTR),
      OPT_STRING('o', "output", &cli->output, "output mode: interactive, noninteractive, quiet, none", SP_NULLPTR),
      OPT_STRING('m', "matrix", &cli->matrix, "build matrix to use", SP_NULLPTR),
      OPT_END()
    },
    (const c8* const []) {
      "spn print [options]",
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

  sp_ht_for(app.deps, it) {
    spn_dep_t* dep = sp_ht_it_getp(app.deps, it);
    spn_cli_assert_dep_is_locked(dep);
    spn_dep_context_resolve_commit(dep);
    spn_dep_context_resolve_build_id(dep);
  }

  spn_generator_context_t gen = {
    .kind = spn_gen_kind_from_str(sp_str_view(command->generator)),
    .compiler = spn_gen_compiler_from_str(sp_str_view(command->compiler))
  };
  gen.include = spn_gen_build_entries_for_all(SPN_GENERATOR_INCLUDE, gen.compiler);
  gen.lib_include = spn_gen_build_entries_for_all(SPN_GENERATOR_LIB_INCLUDE, gen.compiler);
  gen.libs = spn_gen_build_entries_for_all(SPN_GENERATOR_LIBS, gen.compiler);
  gen.rpath = spn_gen_build_entries_for_all(SPN_GENERATOR_RPATH, gen.compiler);

  spn_gen_format_context_t fmt = {
    .kind = SPN_GENERATOR_SYSTEM_LIBS,
    .compiler = gen.compiler
  };
  sp_dyn_array(sp_str_t) entries = sp_str_map(app.system_deps, sp_dyn_array_size(app.system_deps), &fmt, spn_generator_format_entry_kernel);
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
    sp_io_stream_t file = sp_io_from_file(sp_os_join_path(destination, gen.file_name), SP_IO_MODE_WRITE);
    if (sp_io_write_str(&file, gen.output)) {
      SP_FATAL("Failed to write {}", SP_FMT_STR(file_path));
    }

    SP_LOG("Generated {:fg brightcyan}", SP_FMT_STR(file_path));
  }
  else {
    printf("%.*s", gen.output.len, gen.output.data);
  }
}


void spn_cli_ls(spn_cli_t* cli) {
  struct argparse argparse;
  argparse_init(
    &argparse,
    (struct argparse_option []) {
      OPT_HELP(),
      OPT_STRING('d', "dir", &cli->ls.dir, "which directory to list (store, include, lib, source, work, vendor)", SP_NULLPTR),
      OPT_END()
    },
    (const c8* const []) {
      "spn ls [package]",
      "List files in a package's cache directory",
      SP_NULLPTR
    },
    SP_NULL
  );
  cli->num_args = argparse_parse(&argparse, cli->num_args, cli->args);

  // Get package name from positional argument if provided
  const c8* package = SP_NULLPTR;
  if (cli->num_args > 0 && cli->args[0]) {
    package = cli->args[0];
  }

  if (package) {
    spn_dep_t* dep = spn_cli_assert_dep_exists(sp_str_view(package));
    spn_cli_assert_dep_is_locked(dep);
    spn_dep_context_set_commit(dep, dep->lock);
    spn_dep_context_resolve_build_id(dep);

    spn_dir_kind_t kind = SPN_DIR_STORE;
    if (cli->ls.dir) {
      kind = spn_cache_dir_kind_from_str(sp_str_view(cli->ls.dir));
    }

    sp_str_t dir = spn_cache_dir_kind_to_dep_path(dep, kind);
    sp_sh_ls(dir);
  }
  else {
    spn_dir_kind_t kind = SPN_DIR_CACHE;
    if (cli->ls.dir) {
      kind = spn_cache_dir_kind_from_str(sp_str_view(cli->ls.dir));
    }

    sp_str_t dir = spn_cache_dir_kind_to_path(kind);
    sp_sh_ls(dir);
  }
}

void spn_cli_which(spn_cli_t* cli) {
  struct argparse argparse;
  argparse_init(
    &argparse,
    (struct argparse_option []) {
      OPT_HELP(),
      OPT_STRING('d', "dir", &cli->which.dir, "which directory to show (store, include, lib, source, work, vendor)", SP_NULLPTR),
      OPT_END()
    },
    (const c8* const []) {
      "spn which [package]",
      "Print the cache directory for this package for this project",
      SP_NULLPTR
    },
    SP_NULL
  );
  cli->num_args = argparse_parse(&argparse, cli->num_args, cli->args);

  // Get package name from positional argument if provided
  const c8* package = SP_NULLPTR;
  if (cli->num_args > 0 && cli->args[0]) {
    package = cli->args[0];
  }

  if (package) {
    spn_dep_t* dep = spn_cli_assert_dep_exists(sp_str_view(package));
    spn_cli_assert_dep_is_locked(dep);
    spn_dep_context_set_commit(dep, dep->lock);
    spn_dep_context_resolve_build_id(dep);

    spn_dir_kind_t kind = SPN_DIR_STORE;
    if (cli->which.dir) {
      kind = spn_cache_dir_kind_from_str(sp_str_view(cli->which.dir));
    }

    sp_str_t dir = spn_cache_dir_kind_to_dep_path(dep, kind);
    printf("%.*s", dir.len, dir.data);

  }
  else {
    spn_dir_kind_t kind = SPN_DIR_CACHE;
    if (cli->which.dir) {
      kind = spn_cache_dir_kind_from_str(sp_str_view(cli->which.dir));
    }

    sp_str_t dir = spn_cache_dir_kind_to_path(kind);
    printf("%.*s", dir.len, dir.data);
  }
}

void spn_cli_recipe(spn_cli_t* cli) {
  struct argparse argparse;
  argparse_init(
    &argparse,
    (struct argparse_option []) {
      OPT_HELP(),
      OPT_END()
    },
    (const c8* const []) {
      "spn recipe <package>",
      "Print the recipe contents for this package",
      SP_NULLPTR
    },
    SP_NULL
  );
  cli->num_args = argparse_parse(&argparse, cli->num_args, cli->args);

  if (!cli->num_args) {
    SP_FATAL("no package name specified");
  }

  spn_dep_t* dep = spn_cli_assert_dep_exists(sp_str_view(cli->args[0]));
  spn_cli_assert_dep_is_locked(dep);
  spn_dep_context_set_commit(dep, dep->lock);
  spn_dep_context_resolve_build_id(dep);

  sp_str_t recipe_path = dep->recipe->paths.file;
  sp_str_t recipe = sp_io_read_file(recipe_path);
  if (!sp_str_valid(recipe)) {
    SP_FATAL("failed to read recipe file: {:fg brightyellow}", SP_FMT_STR(recipe_path));
  }

  sp_os_log(recipe);
}

void spn_cli_add(spn_cli_t* cli) {
  struct argparse argparse;
  argparse_init(
    &argparse,
    (struct argparse_option []) {
      OPT_HELP(),
      OPT_END()
    },
    (const c8* []) {
      "spn add",
      SP_NULLPTR
    },
    SP_NULL
  );
  cli->num_args = argparse_parse(&argparse, cli->num_args, cli->args);

  if (!cli->num_args || !cli->args[0]) {
    argparse_usage(&argparse);
    return;
  }

  sp_str_t name = sp_str_from_cstr(cli->args[0]);

  if (sp_ht_getp(app.deps, name)) {
    SP_FATAL("{:fg brightyellow} is already in your project", SP_FMT_STR(name));
  }

  spn_package_t* recipe = sp_ht_getp(app.packages, name);
  if (!recipe) {
    sp_str_t prefix = sp_str_lit("  > ");
    sp_str_t color = sp_str_lit("brightcyan");
    sp_da(sp_str_t) search = app.search;
    search = sp_str_map(search, sp_dyn_array_size(search), &color, sp_str_map_kernel_colorize);
    search = sp_str_map(search, sp_dyn_array_size(search), &prefix, sp_str_map_kernel_prepend);

    SP_FATAL(
      "Could not find {:fg yellow} on search path: \n{}",
      SP_FMT_STR(name),
      SP_FMT_STR(sp_str_join_n(search, sp_dyn_array_size(search), sp_str_lit("\n")))
    );
  }

  sp_ht_insert(app.deps, name, SP_ZERO_STRUCT(spn_dep_t));
  spn_dep_t* dep = sp_ht_getp(app.deps, name);
  spn_dep_context_init(dep, recipe);

  sp_ht_for(app.deps, it) {
    spn_dep_t* dep = sp_ht_it_getp(app.deps, it);
    dep->state.build = SPN_DEP_BUILD_STATE_IDLE;
    sp_thread_init(&dep->thread, spn_dep_thread_resolve, dep);
  }

  spn_tui_mode_t mode = app.cli.output ?
    spn_output_mode_from_str(sp_str_view(app.cli.output)) :
    SPN_OUTPUT_MODE_NONINTERACTIVE;
  spn_tui_init(&app.tui, mode);
  spn_tui_run(&app.tui);

  spn_update_lock_file();
}

void spn_cli_build(spn_cli_t* cli) {
  spn_cli_build_t* command = &cli->build;

  struct argparse argparse;
  argparse_init(
    &argparse,
    (struct argparse_option []) {
      OPT_HELP(),
      OPT_BOOLEAN('f', "force", &command->force, "force build, even if it exists in the store", SP_NULLPTR),
      OPT_BOOLEAN('u', "update", &command->update, "pull latest for all deps", SP_NULLPTR),
      OPT_STRING('o', "output", &cli->output, "output mode: interactive, noninteractive, quiet, none", SP_NULLPTR),
      OPT_STRING('m', "matrix", &cli->matrix, "build matrix to use", SP_NULLPTR),
      OPT_END()
    },
    (const c8* const []) {
      "spn build [options]",
      SP_NULLPTR
    },
    SP_NULL
  );
  cli->num_args = argparse_parse(&argparse, cli->num_args, cli->args);

  spn_tui_mode_t mode = app.cli.output ?
    spn_output_mode_from_str(sp_str_view(app.cli.output)) :
    SPN_OUTPUT_MODE_INTERACTIVE;

  sp_ht_for(app.deps, it) {
    spn_dep_t* dep = sp_ht_it_getp(app.deps, it);
    dep->force = command->force;
    dep->update = command->update;
    dep->state.build = SPN_DEP_BUILD_STATE_IDLE;
    sp_thread_init(&dep->thread, spn_dep_thread_build, dep);
  }

  spn_tui_init(&app.tui, mode);
  spn_tui_run(&app.tui);

  spn_update_lock_file();
}
