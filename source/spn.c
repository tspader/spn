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

#define SP_PS_MAX_ARGS 32
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

#define SPN_VERSION "1.0.0"
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

#define SP_OPT_SOME 1
#define SP_OPT_NONE 0

#define SP_LIMIT_MAX_U32 4294967295U
#define SP_LIMIT_MIN_U32 0U
#define SP_LIMIT_MAX_S32 2147483647
#define SP_LIMIT_MIN_S32 (-2147483647 - 1)
#define SP_LIMIT_MAX_S64 9223372036854775807LL
#define SP_LIMIT_MIN_S64 (-9223372036854775807LL - 1)
#define SP_LIMIT_MAX_F32 3.40282346638528859812e+38F
#define SP_LIMIT_MIN_F32 1.17549435082228750797e-38F
#define SP_LIMIT_MAX_F64 1.7976931348623157081e+308
#define SP_LIMIT_MIN_F64 2.2250738585072014077e-308

#define sp_opt(T) struct { \
  T value; \
  u8 some; \
}

#define sp_opt_set(O, V) do { (O).value = (V); (O).some = SP_OPT_SOME; } while (0)
#define sp_opt_get(O) (O).value
#define sp_opt_some(V) { .value = V, .some = SP_OPT_SOME }

#define sp_dyn_array_sort(arr, fn) qsort(arr, sp_dyn_array_size(arr), sizeof((arr)[0]), fn)
#define sp_ht_collect_keys(ht, da) \
  do { \
    sp_ht_for((ht), __it) { \
      sp_dyn_array_push((da), *sp_ht_it_getkp((ht), __it)); \
    } \
  } while(0)

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
  toml_table_t* metadata;
  toml_array_t*   versions;
  toml_table_t* spn;
  toml_table_t*   package;
  toml_table_t*   lib;
  toml_table_t*     src;
  toml_table_t*     a;
  toml_table_t*     so;
  toml_array_t*   bin;
  toml_array_t*   profile;
  toml_array_t*   registry;
  toml_table_t*   deps;
  toml_table_t*   options;
  toml_table_t*   config;
} spn_toml_package_t;

typedef enum {
  SPN_TOML_CONTEXT_ROOT,
  SPN_TOML_CONTEXT_TABLE,
  SPN_TOML_CONTEXT_ARRAY,
} spn_toml_context_kind_t;

typedef struct {
  spn_toml_context_kind_t kind;
  sp_str_t key;
  bool header_written;
} spn_toml_context_t;

typedef struct {
  sp_str_builder_t builder;
  sp_da(spn_toml_context_t) stack;
} spn_toml_writer_t;

toml_table_t* spn_toml_parse(sp_str_t path);
sp_str_t spn_toml_str(toml_table_t* toml, const c8* key);
sp_str_t spn_toml_arr_str(toml_array_t* toml, u32 it);

spn_toml_writer_t spn_toml_writer_new();
sp_str_t          spn_toml_writer_write(spn_toml_writer_t* writer);
void              spn_toml_ensure_header_written(spn_toml_writer_t* writer);
void              spn_toml_begin_table(spn_toml_writer_t* writer, sp_str_t key);
void              spn_toml_begin_table_cstr(spn_toml_writer_t* writer, const c8* key);
void              spn_toml_end_table(spn_toml_writer_t* writer);
void              spn_toml_begin_array(spn_toml_writer_t* writer, sp_str_t key);
void              spn_toml_begin_array_cstr(spn_toml_writer_t* writer, const c8* key);
void              spn_toml_end_array(spn_toml_writer_t* writer);
void              spn_toml_append_array_table(spn_toml_writer_t* writer);
void              spn_toml_append_str(spn_toml_writer_t* writer, sp_str_t key, sp_str_t value);
void              spn_toml_append_str_cstr(spn_toml_writer_t* writer, const c8* key, sp_str_t value);
void              spn_toml_append_s64(spn_toml_writer_t* writer, sp_str_t key, s64 value);
void              spn_toml_append_s64_cstr(spn_toml_writer_t* writer, const c8* key, s64 value);
void              spn_toml_append_bool(spn_toml_writer_t* writer, sp_str_t key, bool value);
void              spn_toml_append_bool_cstr(spn_toml_writer_t* writer, const c8* key, bool value);
void              spn_toml_append_str_array(spn_toml_writer_t* writer, sp_str_t key, sp_da(sp_str_t) values);
void              spn_toml_append_str_array_cstr(spn_toml_writer_t* writer, const c8* key, sp_da(sp_str_t) values);

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

typedef struct {
  spn_generator_kind_t kind;
  spn_cc_kind_t compiler;
  sp_str_t file_name;
  sp_str_t output;

  sp_str_t include;
  sp_str_t lib_include;
  sp_str_t libs;
  sp_str_t system_libs;
  sp_str_t rpath;
} spn_generator_context_t;

////////////
// SEMVER //
////////////
typedef enum {
  SPN_SEMVER_OP_LT = 0,
  SPN_SEMVER_OP_LEQ = 1,
  SPN_SEMVER_OP_GT = 2,
  SPN_SEMVER_OP_GEQ = 3,
  SPN_SEMVER_OP_EQ = 4,
} spn_semver_op_t;

typedef enum {
  SPN_SEMVER_MOD_NONE,
  SPN_SEMVER_MOD_CARET,
  SPN_SEMVER_MOD_TILDE,
  SPN_SEMVER_MOD_WILDCARD,
  SPN_SEMVER_MOD_CMP,
} spn_semver_mod_t;

typedef struct {
  u32 major;
  u32 minor;
  u32 patch;
} spn_semver_t;

typedef struct {
  bool major;
  bool minor;
  bool patch;
} spn_semver_components_t;

typedef struct {
  spn_semver_t version;
  spn_semver_components_t components;
} spn_semver_parsed_t;

typedef struct {
  spn_semver_t version;
  spn_semver_op_t op;
} spn_semver_bound_t;

typedef struct {
  spn_semver_bound_t low;
  spn_semver_bound_t high;
  spn_semver_mod_t mod;
} spn_semver_range_t;

typedef struct {
  sp_str_t str;
  u32 it;
} spn_semver_parser_t;

c8                  spn_semver_parser_peek(spn_semver_parser_t* parser);
void                spn_semver_parser_eat(spn_semver_parser_t* parser);
void                spn_semver_parser_eat_and_assert(spn_semver_parser_t* parser, c8 c);
bool                spn_semver_parser_is_digit(c8 c);
bool                spn_semver_parser_is_whitespace(c8 c);
bool                spn_semver_parser_is_done(spn_semver_parser_t* parser);
void                spn_semver_parser_eat_whitespace(spn_semver_parser_t* parser);
u32                 spn_semver_parser_parse_number(spn_semver_parser_t* parser);
spn_semver_parsed_t spn_semver_parser_parse_version(spn_semver_parser_t* parser);
spn_semver_t        spn_semver_from_str(sp_str_t);
spn_semver_range_t  spn_semver_range_from_str(sp_str_t str);
sp_str_t            spn_semver_range_to_str(spn_semver_range_t range);
sp_str_t            spn_semver_to_str(spn_semver_t version);
bool                spn_semver_eq(spn_semver_t lhs, spn_semver_t rhs);
bool                spn_semver_geq(spn_semver_t lhs, spn_semver_t rhs);
bool                spn_semver_ge(spn_semver_t lhs, spn_semver_t rhs);
bool                spn_semver_leq(spn_semver_t lhs, spn_semver_t rhs);
bool                spn_semver_le(spn_semver_t lhs, spn_semver_t rhs);
s32                 spn_semver_cmp(spn_semver_t lhs, spn_semver_t rhs);
s32                 spn_semver_sort_kernel(const void* a, const void* b);

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
} spn_dep_state_t;

typedef struct {
  sp_str_t name;
  spn_dep_mode_t mode;
} spn_build_matrix_t;

typedef enum {
  SPN_DEP_OPTION_KIND_BOOL,
  SPN_DEP_OPTION_KIND_S64,
  SPN_DEP_OPTION_KIND_STR,
} spn_dep_option_kind_t;

typedef struct {
  sp_str_t name;
  spn_dep_option_kind_t kind;
  union {
    bool b;
    s64 s;
    sp_str_t str;
  };
} spn_dep_option_t;

typedef sp_ht(sp_str_t, spn_dep_option_t) spn_dep_options_t;

spn_dep_option_t spn_dep_option_from_toml(toml_table_t* toml, const c8* key);
void             spn_toml_append_option(spn_toml_writer_t* writer, sp_str_t key, spn_dep_option_t option);
void             spn_toml_append_option_cstr(spn_toml_writer_t* writer, const c8* key, spn_dep_option_t option);

// Specific to the recipe
typedef enum {
  SPN_PACKAGE_STATE_UNLOADED,
  SPN_PACKAGE_STATE_LOADED,
} spn_package_state_t;

typedef struct {
  sp_str_t source;
  sp_str_t root;
  sp_str_t   manifest;
  sp_str_t   metadata;
  sp_str_t   script;
} spn_package_paths_t;

typedef struct {
  sp_ht(spn_lib_kind_t, bool) enabled;
  sp_str_t name;
} spn_lib_t;

typedef struct {
  sp_str_t name;
  sp_str_t entry;
  sp_opt(sp_str_t) profile;
  sp_da(sp_str_t) source;
  sp_da(sp_str_t) include;
} spn_bin_t;

typedef struct {
  spn_cc_kind_t cc;
  spn_lib_kind_t libc;
  spn_c_standard_t standard;
  spn_dep_mode_t mode;
} spn_profile_t;

typedef struct {
  sp_str_t name;
  spn_semver_range_t range;
} spn_dep_req_t;

typedef struct {
  spn_semver_t version;
  sp_str_t commit;
} spn_metadata_t;

typedef enum {
  SPN_DEP_IMPORT_KIND_EXPLICIT,
  SPN_DEP_IMPORT_KIND_TRANSITIVE
} spn_dep_import_kind_t;

typedef struct {
  sp_str_t name;
  spn_semver_t version;
  sp_str_t commit;
  spn_dep_import_kind_t import_kind;
  sp_da(sp_str_t) deps;
  sp_da(sp_str_t) dependents;
} spn_lock_entry_t;

typedef struct {
  spn_semver_t version;
  sp_str_t commit;
  sp_ht(sp_str_t, spn_lock_entry_t) entries;
} spn_lock_file_t;

typedef struct {
  sp_str_t name;
  sp_str_t location;
  spn_registry_kind_t kind;
} spn_registry_t;

sp_str_t spn_registry_get_path(spn_registry_t* registry);

struct spn_package {
  sp_str_t name;
  sp_str_t repo;
  sp_str_t author;
  sp_str_t maintainer;
  spn_lib_t lib;
  sp_ht(sp_str_t, spn_bin_t) bin;
  sp_ht(sp_str_t, spn_dep_req_t) deps;
  sp_ht(sp_str_t, spn_dep_option_t) options;
  sp_ht(sp_str_t, spn_dep_options_t) config;
  sp_ht(spn_semver_t, spn_metadata_t) metadata;
  sp_da(spn_semver_t) versions;
  sp_ht(sp_str_t, spn_registry_t) registries;

  spn_dep_fn_t on_configure;
  spn_dep_fn_t on_build;
  spn_dep_fn_t on_package;

  spn_toml_package_t toml;
  spn_package_paths_t paths;
};

spn_package_t spn_package_load(sp_str_t file_path);
void spn_package_compile(spn_package_t* package);

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


struct spn_dep_context {
  sp_str_t name;
  spn_lib_kind_t kind;
  spn_dep_mode_t mode;
  spn_metadata_t metadata;
  spn_dep_options_t options;
  sp_hash_t build_id;
  spn_dep_paths_t paths;
  sp_str_t message;

  spn_package_t* package;

  bool force;
  bool update;

  sp_ps_t ps;
  sp_ps_config_t cfg;
  sp_io_stream_t log;
  spn_dep_state_t state;
  sp_thread_t thread;
  sp_mutex_t mutex;
  sp_str_t error;
};

spn_generator_kind_t spn_gen_kind_from_str(sp_str_t str);
spn_gen_entry_kind_t spn_gen_entry_from_str(sp_str_t str);
spn_cc_kind_t   spn_cc_kind_from_str(sp_str_t str);
spn_registry_kind_t spn_registry_kind_from_str(sp_str_t str);
sp_str_t             spn_gen_format_entry_for_compiler(sp_str_t entry, spn_gen_entry_kind_t kind, spn_cc_kind_t compiler);
sp_str_t             spn_cc_kind_to_executable(spn_cc_kind_t compiler);
sp_str_t             spn_cc_c_standard_to_switch(spn_c_standard_t standard);
sp_str_t             spn_cc_lib_kind_to_switch(spn_lib_kind_t kind);
sp_dyn_array(sp_str_t) spn_gen_build_entry_for_dep(spn_dep_context_t* dep, spn_gen_entry_kind_t kind, spn_cc_kind_t c);
sp_str_t             spn_gen_build_entries_for_dep(spn_dep_context_t* dep, spn_cc_kind_t c);
sp_str_t             spn_gen_build_entries_for_all(spn_gen_entry_kind_t kind, spn_cc_kind_t c);
sp_str_t             spn_print_system_deps_only(spn_cc_kind_t compiler);
sp_str_t             spn_print_deps_only(spn_gen_entry_kind_t kind, spn_cc_kind_t compiler);

sp_str_t             spn_opt_cstr_required(const c8* value);
sp_str_t             spn_opt_cstr_optional(const c8* value, const c8* fallback);

spn_dir_kind_t spn_cache_dir_kind_from_str(sp_str_t str);
sp_str_t             spn_cache_dir_kind_to_dep_path(spn_dep_context_t* dep, spn_dir_kind_t kind);
spn_dep_mode_t       spn_dep_build_mode_from_str(sp_str_t str);
sp_str_t             spn_dep_build_mode_to_str(spn_dep_mode_t mode);
spn_lib_kind_t       spn_lib_kind_from_str(sp_str_t str);
sp_str_t             spn_dep_build_kind_to_str(spn_lib_kind_t kind);
sp_str_t             spn_dep_state_to_str(spn_dep_state_t state);
bool                 spn_dep_state_is_terminal(spn_dep_context_t* dep);
sp_os_lib_kind_t     spn_lib_kind_to_sp_os_lib_kind(spn_lib_kind_t kind);
void                 spn_dep_context_init(spn_dep_context_t* dep, spn_package_t* recipe);
s32                  spn_dep_thread_resolve(void* user_data);
s32                  spn_dep_thread_build(void* user_data);
s32                  spn_dep_context_resolve(spn_dep_context_t* dep);
s32                  spn_dep_context_build(spn_dep_context_t* dep);
spn_err_t            spn_dep_context_sync_remote(spn_dep_context_t* dep);
spn_err_t            spn_dep_context_sync_local(spn_dep_context_t* dep);
spn_err_t            spn_dep_context_resolve_commit(spn_dep_context_t* dep);
bool                 spn_dep_context_is_build_stamped(spn_dep_context_t* context);
void                 spn_dep_context_set_build_state(spn_dep_context_t* dep, spn_dep_state_t state);
void                 spn_dep_context_set_build_error(spn_dep_context_t* dep, sp_str_t error);
bool                 spn_dep_context_is_binary(spn_dep_context_t* dep);
void                 spn_recipe_load(spn_tcc_t* tcc, spn_package_t* recipe);
spn_package_t*        spn_recipe_find(sp_str_t name);
s32                  spn_sort_kernel_dep_ptr(const void* a, const void* b);

void                 spn_update_lock_file();
void                 spn_update_project_toml();
spn_lock_file_t      spn_load_lock_file();

void spn_bin_build(spn_bin_t bin);

typedef struct {
  sp_opt(u32) low;
  sp_opt(u32) high;
  spn_dep_req_t source;
} spn_dep_version_range_t;

typedef struct {
  sp_ht(sp_str_t, sp_da(spn_dep_version_range_t)) ranges;
  sp_ht(sp_str_t, spn_semver_t) versions;
  sp_ht(sp_str_t, bool) visited;
} spn_resolver_t;


void spn_resolver_init(spn_resolver_t* resolver);
void spn_resolver_add_package_constraints(spn_resolver_t* resolver, spn_package_t* package);
void spn_resolver_resolve_from_lock_file(spn_resolver_t* resolver);
void spn_resolver_resolve_from_solver(spn_resolver_t* resolver);
void spn_resolver_resolve(spn_resolver_t* resolver);

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
  sp_ht(sp_str_t, spn_registry_t) registries;
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
  sp_ht(sp_str_t, spn_dep_state_t) state;

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
void spn_tui_print_dep(spn_tui_t* tui, spn_dep_context_t* dep);


/////////
// CLI //
/////////
typedef struct {
  const c8* package;
} spn_cli_add_t;

typedef struct {
  const c8* package;
} spn_cli_update_t;

typedef struct {
  const c8* command;
} spn_cli_tool_t;

typedef struct {
  bool bare;
} spn_cli_init_t;

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
  spn_cli_update_t update;
  spn_cli_tool_t tool;
  spn_cli_init_t init;
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

void spn_app_add(sp_str_t name);
void spn_app_update(sp_str_t name);
void spn_cli_init(spn_cli_t* cli);
void spn_cli_add(spn_cli_t* cli);
void spn_cli_update(spn_cli_t* cli);
void spn_cli_tool(spn_cli_t* cli);

void spn_cli_list(spn_cli_t* cli);
void spn_cli_ls(spn_cli_t* cli);
void spn_cli_which(spn_cli_t* cli);
void spn_cli_recipe(spn_cli_t* cli);



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
  sp_opt(spn_lock_file_t) lock;
  spn_resolver_t resolver;
  spn_registry_t registry;
  sp_da(sp_str_t) search;
  sp_ht(sp_str_t, spn_package_t) packages;
  sp_ht(sp_str_t, spn_dep_context_t) deps;
  sp_ht(sp_str_t, spn_build_matrix_t) matrices;
  sp_ht(sp_str_t, spn_profile_t) profiles;
  sp_opt(sp_str_t) default_profile;
  sp_da(sp_str_t) system_deps;
} spn_app_t;

spn_app_t app;

void spn_app_init(spn_app_t* app, u32 num_args, const c8** args);
void spn_app_prepare();
void spn_cli_run(spn_app_t* app);


////////////////////
// IMPLEMENTATION //
////////////////////

////////////
// LIBSPN //
////////////
struct spn_make {
  spn_dep_context_t* dep;
  sp_str_t target;
};

struct spn_autoconf {
  spn_dep_context_t* dep;
};

typedef enum {
  SPN_CC_TARGET_NONE,
  SPN_CC_TARGET_SHARED_LIB,
  SPN_CC_TARGET_STATIC_LIB,
  SPN_CC_TARGET_EXECUTABLE,
} spn_cc_target_kind_t;

typedef struct {
  sp_str_t name;
} spn_cc_executable_t;

typedef struct {
  sp_str_t name;
} spn_cc_shared_lib_t;

typedef struct {
  sp_str_t name;
} spn_cc_static_lib_t;

typedef struct {
  sp_str_t build;
  sp_str_t   profile;
  sp_str_t     output;
} spn_cc_target_paths_t;

typedef struct {
  sp_str_t name;
  sp_da(sp_str_t) source;
  sp_da(sp_str_t) include;
  sp_str_t profile;

  spn_cc_target_kind_t kind;
  union {
    spn_cc_executable_t exe;
    spn_cc_shared_lib_t shared_lib;
    spn_cc_static_lib_t static_lib;
  };
} spn_cc_target_t;

struct spn_cc {
  sp_da(sp_str_t) include;
  sp_str_t profile;
  sp_str_t dir;
  sp_da(spn_cc_target_t) targets;
};

void spn_cc_add_include(spn_cc_t* cc, sp_str_t dir);
void spn_cc_set_profile(spn_cc_t* cc, sp_str_t profile);
void spn_cc_set_output_dir(spn_cc_t* cc, sp_str_t dir);
spn_cc_target_t* spn_cc_add_target(spn_cc_t* cc, spn_cc_target_kind_t kind, sp_str_t name);
void spn_cc_target_add_source(spn_cc_target_t* cc, sp_str_t file_path);
sp_err_t spn_cc_build(spn_cc_t* cc);

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
  APPLY(spn_dep_log) \
  APPLY(spn_dep_set_s64) \
  APPLY(spn_copy)

#define SPN_DEFINE_LIB_ENTRY(SYM) { .symbol = SP_MACRO_STR(SYM), .fn = SYM },

spn_lib_fn_t spn_lib [] = {
  SPN_LIB_ENTRIES(SPN_DEFINE_LIB_ENTRY)
};

void spn_make(spn_dep_context_t* build) {
  spn_make_t* make = spn_make_new(build);
  spn_make_run(make);
}

spn_make_t* spn_make_new(spn_dep_context_t* dep) {
  spn_make_t* make = SP_ALLOC(spn_make_t);
  make->dep = dep;
  return make;
}

void spn_make_add_target(spn_make_t* make, const c8* target) {
  make->target = sp_str_from_cstr(target);
}

void spn_make_run(spn_make_t* make) {
  spn_dep_context_t* dep = make->dep;

  sp_ps_config_t ps = sp_ps_config_copy(&dep->cfg);
  ps.command = SP_LIT("make");
  sp_ps_config_add_arg(&ps, SP_LIT("--quiet"));
  sp_ps_config_add_arg(&ps, SP_LIT("--directory"));
  sp_ps_config_add_arg(&ps, dep->paths.work);
  if (!sp_str_empty(make->target)) {
    sp_ps_config_add_arg(&ps, make->target);
  }

  sp_ps_run(ps);
}

void spn_autoconf(spn_dep_context_t* build) {
  spn_autoconf_t* autoconf = spn_autoconf_new(build);
  spn_autoconf_run(autoconf);
}

spn_autoconf_t* spn_autoconf_new(spn_dep_context_t* dep) {
  spn_autoconf_t* autoconf = SP_ALLOC(spn_autoconf_t);
  autoconf->dep = dep;
  return autoconf;
}

void spn_autoconf_run(spn_autoconf_t* autoconf) {
  spn_dep_context_t* dep = autoconf->dep;

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

void spn_cc_set_profile(spn_cc_t* cc, sp_str_t profile) {
  SP_ASSERT(sp_ht_key_exists(app.profiles, profile));
  cc->profile = profile;
}

void spn_cc_add_include(spn_cc_t* cc, sp_str_t dir) {
  sp_dyn_array_push(cc->include, sp_os_join_path(app.paths.project.dir, dir));
}

void spn_cc_target_add_source(spn_cc_target_t* target, sp_str_t file_path) {
  sp_dyn_array_push(target->source, sp_os_join_path(app.paths.project.dir, file_path));
}

void spn_cc_target_add_include(spn_cc_target_t* target, sp_str_t dir) {
  sp_dyn_array_push(target->include, sp_os_join_path(app.paths.project.dir, dir));
}

void spn_cc_set_output_dir(spn_cc_t* cc, sp_str_t dir) {
  cc->dir = sp_str_copy(dir);
}

spn_cc_target_t* spn_cc_add_target(spn_cc_t* cc, spn_cc_target_kind_t kind, sp_str_t name) {
  spn_cc_target_t target = {
    .name = sp_str_copy(name),
    .kind = kind
  };
  sp_dyn_array_push(cc->targets, target);
  return sp_dyn_array_back(cc->targets);
}

sp_err_t spn_cc_build(spn_cc_t* cc) {
  sp_dyn_array_for(cc->targets, it) {
    spn_cc_target_t target = cc->targets[it];
    spn_cc_target_paths_t paths = SP_ZERO_INITIALIZE();
    paths.build = sp_os_join_path(app.paths.project.dir, cc->dir);
    paths.profile = sp_os_join_path(paths.build, cc->profile);
    paths.output = sp_os_join_path(paths.profile, target.name);

    switch (target.kind) {
      case SPN_CC_TARGET_EXECUTABLE: {
        spn_profile_t* profile = sp_ht_getp(app.profiles, target.profile);
        if (!profile) {
          profile = sp_ht_getp(app.profiles, cc->profile);
        }
        SP_ASSERT(profile);

        sp_ps_config_t cfg = SP_ZERO_INITIALIZE();
        cfg.command = spn_cc_kind_to_executable(profile->cc);

        sp_dyn_array_for(target.source, it) {
          sp_ps_config_add_arg(&cfg, target.source[it]);
        }

        sp_dyn_array_for(target.include, it) {
          sp_ps_config_add_arg(&cfg, spn_gen_format_entry_for_compiler(target.include[it], SPN_GENERATOR_INCLUDE, profile->cc));
        }
        sp_dyn_array_for(cc->include, it) {
          sp_ps_config_add_arg(&cfg, spn_gen_format_entry_for_compiler(cc->include[it], SPN_GENERATOR_INCLUDE, profile->cc));
        }

        sp_ht_for(app.deps, it) {
          spn_dep_context_t* dep = sp_ht_it_getp(app.deps, it);

          sp_dyn_array(sp_str_t) includes = spn_gen_build_entry_for_dep(dep, SPN_GENERATOR_INCLUDE, profile->cc);
          sp_dyn_array_for(includes, i) {
            sp_ps_config_add_arg(&cfg, includes[i]);
          }

          sp_dyn_array(sp_str_t) lib_includes = spn_gen_build_entry_for_dep(dep, SPN_GENERATOR_LIB_INCLUDE, profile->cc);
          sp_dyn_array_for(lib_includes, i) {
            sp_ps_config_add_arg(&cfg, lib_includes[i]);
          }

          sp_dyn_array(sp_str_t) libs = spn_gen_build_entry_for_dep(dep, SPN_GENERATOR_LIBS, profile->cc);
          sp_dyn_array_for(libs, i) {
            sp_ps_config_add_arg(&cfg, libs[i]);
          }

          sp_dyn_array(sp_str_t) rpath = spn_gen_build_entry_for_dep(dep, SPN_GENERATOR_RPATH, profile->cc);
          sp_dyn_array_for(rpath, i) {
            sp_ps_config_add_arg(&cfg, rpath[i]);
          }
        }

        sp_ps_config_add_arg(&cfg, spn_cc_c_standard_to_switch(profile->standard));

        if (profile->mode == SPN_DEP_BUILD_MODE_DEBUG) {
          sp_ps_config_add_arg(&cfg, sp_str_lit("-g"));
        }

        sp_ps_config_add_arg(&cfg, spn_cc_lib_kind_to_switch(profile->libc));

        sp_ps_config_add_arg(&cfg, sp_str_lit("-o"));
        sp_ps_config_add_arg(&cfg, paths.output);

        sp_os_create_directory(paths.profile);
        sp_ps_output_t result = sp_ps_run(cfg);
        if (result.status.exit_code) {
          return SP_ERR_LAZY;
        }

        break;
      }
      default: {
        SP_UNREACHABLE_CASE();
      }
    }
  }

  return SP_ERR_OK;
}

void spn_dep_log(spn_dep_context_t* dep, const c8* message) {
  sp_io_write_cstr(&dep->log, message);
}

void spn_dep_set_s64(spn_dep_context_t* dep, const c8* name, s64 value) {
  spn_dep_option_t option = {
    .kind = SPN_DEP_OPTION_KIND_S64,
    .name = sp_str_from_cstr(name),
    .s = value
  };
  sp_ht_insert(dep->options, option.name, option);
}

void spn_copy(spn_dep_context_t* dep, spn_dir_kind_t from_kind, const c8* from_path, spn_dir_kind_t to_kind, const c8* to_path) {
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

spn_lib_kind_t spn_lib_kind_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, "shared")) return SPN_LIB_KIND_SHARED;
  else if (sp_str_equal_cstr(str, "static")) return SPN_LIB_KIND_STATIC;
  else if (sp_str_equal_cstr(str, "source")) return SPN_LIB_KIND_SOURCE;
  else if (sp_str_equal_cstr(str, "none"))   return SPN_LIB_KIND_NONE;

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

sp_str_t spn_cc_lib_kind_to_switch(spn_lib_kind_t kind) {
  switch (kind) {
    case SPN_LIB_KIND_STATIC: return sp_str_lit("-static");
    case SPN_LIB_KIND_SHARED:
    case SPN_LIB_KIND_SOURCE:
    case SPN_LIB_KIND_NONE:
    default: return sp_str_lit("");
  }
}

sp_str_t spn_cc_c_standard_to_switch(spn_c_standard_t standard) {
  switch (standard) {
    case SPN_C89: return sp_str_lit("-std=c89");
    case SPN_C99: return sp_str_lit("-std=c99");
    case SPN_C11: return sp_str_lit("-std=c11");
    default: return sp_str_lit("");
  }
}

sp_str_t spn_cc_kind_to_executable(spn_cc_kind_t compiler) {
  switch (compiler) {
    case SPN_CC_GCC: return sp_str_lit("gcc");
    case SPN_CC_NONE:
    default: return sp_str_lit("gcc");
  }
}

sp_str_t spn_dep_state_to_str(spn_dep_state_t state) {
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

sp_os_lib_kind_t spn_lib_kind_to_sp_os_lib_kind(spn_lib_kind_t kind) {
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

spn_cc_kind_t spn_cc_kind_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, ""))    return SPN_CC_NONE;
  else if (sp_str_equal_cstr(str, "gcc")) return SPN_CC_GCC;

  SP_FATAL("Unknown compiler {:fg brightyellow}; options are [gcc]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_CC_NONE);
}

spn_c_standard_t spn_c_standard_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, "c89")) return SPN_C89;
  else if (sp_str_equal_cstr(str, "c99")) return SPN_C99;
  else if (sp_str_equal_cstr(str, "c11")) return SPN_C11;

  SP_FATAL("Unknown C standard {:fg brightyellow}; options are [c89, c99, c11]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_C99);
}

spn_registry_kind_t spn_registry_kind_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, "workspace")) return SPN_REGISTRY_KIND_WORKSPACE;
  else if (sp_str_equal_cstr(str, "user"))      return SPN_REGISTRY_KIND_USER;
  else if (sp_str_equal_cstr(str, "remote"))    return SPN_REGISTRY_KIND_REMOTE;

  SP_FATAL("Unknown registry kind {:fg brightyellow}; options are [workspace, user, remote]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_REGISTRY_KIND_WORKSPACE);
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

sp_str_t spn_cache_dir_kind_to_dep_path(spn_dep_context_t* dep, spn_dir_kind_t kind) {
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
  spn_cc_kind_t compiler;
} spn_gen_format_context_t;

sp_str_t spn_generator_format_entry_kernel(sp_str_map_context_t* context) {
  spn_gen_format_context_t* format = (spn_gen_format_context_t*)context->user_data;
  return spn_gen_format_entry_for_compiler(context->str, format->kind, format->compiler);
}

sp_str_t spn_gen_format_entry_for_compiler(sp_str_t entry, spn_gen_entry_kind_t kind, spn_cc_kind_t compiler) {
  switch (compiler) {
    case SPN_CC_NONE: {
      return entry;
    }
    case SPN_CC_GCC: {
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

sp_dyn_array(sp_str_t) spn_gen_build_entry_for_dep(spn_dep_context_t* dep, spn_gen_entry_kind_t kind, spn_cc_kind_t compiler) {
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
          return entries;
        }
      }

      break;
    case SPN_GENERATOR_LIB_INCLUDE:  {
      switch (dep->kind) {
        case SPN_LIB_KIND_SHARED: {
          sp_dyn_array_push(entries, dep->paths.lib);
          break;
        }
        case SPN_LIB_KIND_NONE:
        case SPN_LIB_KIND_STATIC:
        case SPN_LIB_KIND_SOURCE: {
          return entries;
        }
      }

      break;
    }
    case SPN_GENERATOR_LIBS: {
      switch (dep->kind) {
        case SPN_LIB_KIND_NONE:
        case SPN_LIB_KIND_SHARED:
        case SPN_LIB_KIND_STATIC: {
          sp_os_lib_kind_t kind = spn_lib_kind_to_sp_os_lib_kind(dep->kind);
          sp_str_t lib = dep->package->lib.name;
          lib = sp_os_lib_to_file_name(lib, kind);
          lib = sp_os_join_path(dep->paths.lib, lib);
          sp_dyn_array_push(entries, lib);
          break;
        }
        case SPN_LIB_KIND_SOURCE: {
          return entries;
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

  return entries;
}

sp_str_t spn_gen_build_entries_for_dep(spn_dep_context_t* dep, spn_cc_kind_t compiler) {
  spn_gen_entry_kind_t kinds [] = { SPN_GENERATOR_INCLUDE, SPN_GENERATOR_LIB_INCLUDE, SPN_GENERATOR_LIBS };

  sp_dyn_array(sp_str_t) entries = SP_NULLPTR;
  SP_CARR_FOR(kinds, index) {
    sp_dyn_array(sp_str_t) dep_entries = spn_gen_build_entry_for_dep(dep, kinds[index], compiler);
    sp_dyn_array_for(dep_entries, i) {
      sp_dyn_array_push(entries, dep_entries[i]);
    }
  }

  return sp_str_join_n(entries, sp_dyn_array_size(entries), sp_str_lit(" "));
}

sp_str_t spn_gen_build_entries_for_all(spn_gen_entry_kind_t kind, spn_cc_kind_t compiler) {
  sp_dyn_array(sp_str_t) entries = SP_NULLPTR;

  sp_ht_for(app.deps, it) {
    spn_dep_context_t* dep = sp_ht_it_getp(app.deps, it);
    sp_dyn_array(sp_str_t) dep_entries = spn_gen_build_entry_for_dep(dep, kind, compiler);
    sp_str_t dep_flags = sp_str_join_n(dep_entries, sp_dyn_array_size(dep_entries), sp_str_lit(" "));
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
  if (!sp_os_does_path_exist(path)) return SP_NULLPTR;

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

spn_toml_writer_t spn_toml_writer_new() {
  spn_toml_writer_t writer = SP_ZERO_INITIALIZE();

  spn_toml_context_t root = {
    .kind = SPN_TOML_CONTEXT_ROOT,
    .key = sp_str_lit(""),
    .header_written = true
  };
  sp_dyn_array_push(writer.stack, root);

  return writer;
}

void spn_toml_ensure_header_written(spn_toml_writer_t* writer) {
  u32 depth = sp_dyn_array_size(writer->stack);
  SP_ASSERT(depth > 0);

  spn_toml_context_t* top = &writer->stack[depth - 1];
  if (top->header_written) return;

  sp_dyn_array(sp_str_t) path_parts = SP_NULLPTR;
  for (u32 i = 1; i < depth; i++) {
    sp_dyn_array_push(path_parts, writer->stack[i].key);
  }

  sp_str_t path = sp_str_join_n(path_parts, sp_dyn_array_size(path_parts), sp_str_lit("."));

  if (top->kind == SPN_TOML_CONTEXT_TABLE) {
    sp_str_builder_append_fmt(&writer->builder, "[{}]", SP_FMT_STR(path));
  }

  sp_str_builder_new_line(&writer->builder);
  top->header_written = true;
}

void spn_toml_begin_table(spn_toml_writer_t* writer, sp_str_t key) {
  spn_toml_context_t context = {
    .kind = SPN_TOML_CONTEXT_TABLE,
    .key = key,
    .header_written = false
  };
  sp_dyn_array_push(writer->stack, context);
}

void spn_toml_begin_table_cstr(spn_toml_writer_t* writer, const c8* key) {
  spn_toml_begin_table(writer, sp_str_view(key));
}

void spn_toml_end_table(spn_toml_writer_t* writer) {
  u32 depth = sp_dyn_array_size(writer->stack);
  SP_ASSERT(depth > 1);

  spn_toml_context_t* top = &writer->stack[depth - 1];
  SP_ASSERT(top->kind == SPN_TOML_CONTEXT_TABLE);

  sp_dyn_array_pop(writer->stack);
  sp_str_builder_new_line(&writer->builder);
}

void spn_toml_begin_array(spn_toml_writer_t* writer, sp_str_t key) {
  spn_toml_context_t context = {
    .kind = SPN_TOML_CONTEXT_ARRAY,
    .key = key,
    .header_written = false
  };
  sp_dyn_array_push(writer->stack, context);
}

void spn_toml_begin_array_cstr(spn_toml_writer_t* writer, const c8* key) {
  spn_toml_begin_array(writer, sp_str_view(key));
}

void spn_toml_end_array(spn_toml_writer_t* writer) {
  u32 depth = sp_dyn_array_size(writer->stack);
  SP_ASSERT(depth > 1);

  spn_toml_context_t* top = &writer->stack[depth - 1];
  SP_ASSERT(top->kind == SPN_TOML_CONTEXT_ARRAY);

  sp_dyn_array_pop(writer->stack);
  sp_str_builder_new_line(&writer->builder);
}

void spn_toml_append_array_table(spn_toml_writer_t* writer) {
  u32 depth = sp_dyn_array_size(writer->stack);
  SP_ASSERT(depth > 1);

  spn_toml_context_t* top = &writer->stack[depth - 1];
  SP_ASSERT(top->kind == SPN_TOML_CONTEXT_ARRAY);

  if (top->header_written) {
    sp_str_builder_new_line(&writer->builder);
  }

  sp_dyn_array(sp_str_t) path_parts = SP_NULLPTR;
  for (u32 i = 1; i < depth; i++) {
    sp_dyn_array_push(path_parts, writer->stack[i].key);
  }

  sp_str_t path = sp_str_join_n(path_parts, sp_dyn_array_size(path_parts), sp_str_lit("."));
  sp_str_builder_append_fmt(&writer->builder, "[[{}]]", SP_FMT_STR(path));
  sp_str_builder_new_line(&writer->builder);

  top->header_written = true;
}

void spn_toml_append_str(spn_toml_writer_t* writer, sp_str_t key, sp_str_t value) {
  spn_toml_ensure_header_written(writer);
  sp_str_builder_append_fmt(&writer->builder, "{} = {}",
    SP_FMT_STR(key),
    SP_FMT_QUOTED_STR(value));
  sp_str_builder_new_line(&writer->builder);
}

void spn_toml_append_str_cstr(spn_toml_writer_t* writer, const c8* key, sp_str_t value) {
  spn_toml_append_str(writer, sp_str_view(key), value);
}

void spn_toml_append_s64(spn_toml_writer_t* writer, sp_str_t key, s64 value) {
  spn_toml_ensure_header_written(writer);
  sp_str_builder_append_fmt(&writer->builder, "{} = {}",
    SP_FMT_STR(key),
    SP_FMT_S64(value));
  sp_str_builder_new_line(&writer->builder);
}

void spn_toml_append_s64_cstr(spn_toml_writer_t* writer, const c8* key, s64 value) {
  spn_toml_append_s64(writer, sp_str_view(key), value);
}

void spn_toml_append_bool(spn_toml_writer_t* writer, sp_str_t key, bool value) {
  spn_toml_ensure_header_written(writer);
  sp_str_builder_append_fmt(&writer->builder, "{} = {}",
    SP_FMT_STR(key),
    SP_FMT_CSTR(value ? "true" : "false"));
  sp_str_builder_new_line(&writer->builder);
}

void spn_toml_append_bool_cstr(spn_toml_writer_t* writer, const c8* key, bool value) {
  spn_toml_append_bool(writer, sp_str_view(key), value);
}

void spn_toml_append_option(spn_toml_writer_t* writer, sp_str_t key, spn_dep_option_t option) {
  switch (option.kind) {
    case SPN_DEP_OPTION_KIND_BOOL: {
      spn_toml_append_bool(writer, key, option.b);
      break;
    }
    case SPN_DEP_OPTION_KIND_S64: {
      spn_toml_append_s64(writer, key, option.s);
      break;
    }
    case SPN_DEP_OPTION_KIND_STR: {
      spn_toml_append_str(writer, key, option.str);
      break;
    }
    default: {
      SP_UNREACHABLE_CASE();
    }
  }
}

void spn_toml_append_option_cstr(spn_toml_writer_t* writer, const c8* key, spn_dep_option_t option) {
  spn_toml_append_option(writer, sp_str_view(key), option);
}

void spn_toml_append_str_array(spn_toml_writer_t* writer, sp_str_t key, sp_da(sp_str_t) values) {
  spn_toml_ensure_header_written(writer);

  sp_str_builder_append_fmt(&writer->builder, "{} = [", SP_FMT_STR(key));

  u32 count = sp_dyn_array_size(values);
  for (u32 i = 0; i < count; i++) {
    sp_str_builder_append_fmt(&writer->builder, "{}", SP_FMT_QUOTED_STR(values[i]));
    if (i < count - 1) {
      sp_str_builder_append_cstr(&writer->builder, ", ");
    }
  }

  sp_str_builder_append_c8(&writer->builder, ']');
  sp_str_builder_new_line(&writer->builder);
}

void spn_toml_append_str_array_cstr(spn_toml_writer_t* writer, const c8* key, sp_da(sp_str_t) values) {
  spn_toml_append_str_array(writer, sp_str_view(key), values);
}

sp_str_t spn_toml_writer_write(spn_toml_writer_t* writer) {
  u32 depth = sp_dyn_array_size(writer->stack);
  SP_ASSERT(depth == 1);

  return sp_str_builder_write(&writer->builder);
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
void spn_tui_print_dep(spn_tui_t* tui, spn_dep_context_t* dep) {
  sp_mutex_lock(&dep->mutex);
  sp_str_t name = sp_str_pad(dep->package->name, tui->width);
  sp_str_t state = sp_str_pad(spn_dep_state_to_str(dep->state), 10);
  sp_str_t status;

  switch (dep->state) {
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
        SP_FMT_STR(dep->metadata.commit),
        SP_FMT_STR(dep->message),
        SP_FMT_U32(0),
        SP_FMT_STR(dep->paths.log)
    );
      break;
    }
    case SPN_DEP_BUILD_STATE_DONE: {
      status = sp_format(
        "{} {:color green} {:color brightblack :pad 10} {} {:color brightyellow} {:color brightcyan}",
        SP_FMT_STR(name),
        SP_FMT_STR(state),
        SP_FMT_STR(dep->metadata.commit),
        SP_FMT_STR(dep->message),
        SP_FMT_U32(0),
        SP_FMT_STR(dep->paths.store)
      );
      break;
    }
    case SPN_DEP_BUILD_STATE_BUILDING: {
      status = sp_format(
        "{} {:color brightcyan} {:color brightblack} {} {:color brightcyan}",
        SP_FMT_STR(name),
        SP_FMT_STR(state),
        SP_FMT_STR(dep->metadata.commit),
        SP_FMT_STR(dep->message),
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
        spn_dep_context_t* dep = sp_ht_it_getp(app.deps, it);
        sp_mutex_lock(&dep->mutex);
        if (!spn_dep_state_is_terminal(dep)) {
          dep->state = SPN_DEP_BUILD_STATE_FAILED;
        }
        sp_mutex_unlock(&dep->mutex);
      }
      break;
    }

    bool building = false;

    sp_ht_for(app.deps, it) {
      spn_dep_context_t* dep = sp_ht_it_getp(app.deps, it);

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
        spn_dep_context_t* dep = sp_ht_it_getp(app.deps, it);
        sp_ht_insert(tui->state, dep->name, dep->state);
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
    spn_dep_context_t* dep = sp_ht_it_getp(app.deps, it);

    if (dep->log.file.fd) {
      sp_io_close(&dep->log);
    }

    switch (dep->state) {
      case SPN_DEP_BUILD_STATE_NONE:
      case SPN_DEP_BUILD_STATE_DONE: {
        break;
      }
      case SPN_DEP_BUILD_STATE_FAILED:
      default: {
        failed = true;

        sp_str_builder_new_line(&builder);
        sp_str_builder_append_fmt(&builder, "> {:fg brightyellow}", SP_FMT_STR(dep->package->name));
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
  sp_ht_set_fns(tui->state, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);

  sp_ht_for(app.deps, it) {
    spn_dep_context_t* dep = sp_ht_it_getp(app.deps, it);
    tui->width = SP_MAX(tui->width, dep->package->name.len);
    sp_ht_insert(tui->state, dep->name, SPN_DEP_BUILD_STATE_IDLE);
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
        spn_dep_context_t* dep = sp_ht_it_getp(app.deps, it);

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
        spn_dep_context_t* dep = sp_ht_it_getp(app.deps, it);
        spn_dep_state_t state = *sp_ht_getp(tui->state, dep->name);

        if (dep->state != state) {
          sp_ht_insert(tui->state, dep->name, dep->state);
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

void spn_lock_file_init(spn_lock_file_t* lock) {
  sp_ht_set_fns(lock->entries, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
}

spn_lock_file_t spn_build_lock_file() {
  spn_lock_file_t lock = SP_ZERO_INITIALIZE();
  spn_lock_file_init(&lock);

  // Add an entry for each dep
  sp_ht_for(app.deps, it) {
    spn_dep_context_t* dep = sp_ht_it_getp(app.deps, it);
    spn_package_t* pkg = sp_ht_getp(app.packages, dep->name);

    spn_lock_entry_t entry = {
      .name = sp_str_copy(dep->name),
      .version = dep->metadata.version,
      .commit = dep->metadata.commit,
      .import_kind = sp_ht_key_exists(app.package.deps, pkg->name),
    };

    sp_ht_for(pkg->deps, n) {
      spn_dep_req_t* request = sp_ht_it_getp(pkg->deps, n);
      sp_dyn_array_push(entry.deps, request->name);
    }

    sp_ht_insert(lock.entries, entry.name, entry);
  }

  // Now that everyone has a node, go back and add the reverse references
  sp_ht_for(lock.entries, it) {
    spn_lock_entry_t* entry = sp_ht_it_getp(lock.entries, it);

    sp_dyn_array_for(entry->deps, n) {
      spn_lock_entry_t* dep = sp_ht_getp(lock.entries, entry->deps[n]);
      sp_dyn_array_push(dep->dependents, entry->name);
    }
  }

  return lock;
}

spn_lock_file_t spn_load_lock_file() {
  spn_lock_file_t lock = SP_ZERO_INITIALIZE();
  spn_lock_file_init(&lock);

  if (!sp_os_does_path_exist(app.paths.project.lock)) {
    return lock;
  }

  toml_table_t* root = spn_toml_parse(app.paths.project.lock);
  SP_ASSERT(root);

  toml_array_t* packages = toml_table_array(root, "package");
  SP_ASSERT(packages);

  spn_toml_arr_for(packages, it) {
    toml_table_t* pkg = toml_array_table(packages, it);
    SP_ASSERT(pkg);

    spn_lock_entry_t entry = {
      .name = spn_toml_str(pkg, "name"),
      .version = spn_semver_from_str(spn_toml_str(pkg, "version")),
      .commit = spn_toml_str(pkg, "commit"),
      .deps = spn_toml_arr_strs(toml_table_array(pkg, "deps")),
    };
    entry.import_kind = sp_ht_getp(app.package.deps, entry.name)
      ? SPN_DEP_IMPORT_KIND_EXPLICIT
      : SPN_DEP_IMPORT_KIND_TRANSITIVE;
    sp_ht_insert(lock.entries, entry.name, entry);
  }

  sp_ht_for(lock.entries, it) {
    spn_lock_entry_t* entry = sp_ht_it_getp(lock.entries, it);

    sp_dyn_array_for(entry->deps, n) {
      spn_lock_entry_t* dep = sp_ht_getp(lock.entries, entry->deps[n]);
      sp_dyn_array_push(dep->dependents, entry->name);
    }
  }

  return lock;
}

void spn_update_lock_file() {
  spn_lock_file_t lock = spn_build_lock_file();

  sp_da(sp_str_t) keys = SP_NULLPTR;
  sp_ht_collect_keys(lock.entries, keys);
  sp_dyn_array_sort(keys, sp_str_sort_kernel_alphabetical);

  spn_toml_writer_t toml = spn_toml_writer_new();

  spn_toml_begin_table_cstr(&toml, "spn");
  spn_toml_append_str_cstr(&toml, "version", sp_str_lit(SPN_VERSION));
  spn_toml_append_str_cstr(&toml, "commit", sp_str_lit(SPN_COMMIT));
  spn_toml_end_table(&toml);

  spn_toml_begin_array_cstr(&toml, "package");
  sp_dyn_array_for(keys, it) {
    spn_lock_entry_t* entry = sp_ht_getp(lock.entries, keys[it]);

    spn_toml_append_array_table(&toml);
    spn_toml_append_str_cstr(&toml, "name", entry->name);
    spn_toml_append_str_cstr(&toml, "version", spn_semver_to_str(entry->version));
    spn_toml_append_str_cstr(&toml, "commit", entry->commit);

    if (sp_dyn_array_size(entry->deps)) {
      spn_toml_append_str_array_cstr(&toml, "deps", entry->deps);
    }
  }
  spn_toml_end_array(&toml);

  sp_str_t output = spn_toml_writer_write(&toml);
  sp_io_stream_t file = sp_io_from_file(app.paths.project.lock, SP_IO_MODE_WRITE);
  sp_io_write_str(&file, output);
  sp_io_close(&file);
}

void spn_update_project_toml() {
  spn_toml_writer_t toml = spn_toml_writer_new();

  spn_toml_begin_table_cstr(&toml, "package");
  spn_toml_append_str_cstr(&toml, "name", app.package.name);
  if (!sp_str_empty(app.package.repo)) {
    spn_toml_append_str_cstr(&toml, "repo", app.package.repo);
  }
  if (!sp_str_empty(app.package.author)) {
    spn_toml_append_str_cstr(&toml, "author", app.package.author);
  }
  if (!sp_str_empty(app.package.maintainer)) {
    spn_toml_append_str_cstr(&toml, "maintainer", app.package.maintainer);
  }
  spn_toml_end_table(&toml);

  if (sp_ht_size(app.package.deps)) {
    spn_toml_begin_table_cstr(&toml, "deps");
    sp_ht_for(app.package.deps, it) {
      sp_str_t* name = sp_ht_it_getkp(app.package.deps, it);
      spn_dep_req_t* req = sp_ht_it_getp(app.package.deps, it);
      spn_toml_append_str(&toml, *name, spn_semver_range_to_str(req->range));
    }
    spn_toml_end_table(&toml);
  }

  if (sp_ht_size(app.package.lib.enabled)) {
    spn_toml_begin_table_cstr(&toml, "lib");
    sp_da(sp_str_t) kinds = SP_NULLPTR;
    sp_ht_for(app.package.lib.enabled, it) {
      spn_lib_kind_t* kind = sp_ht_it_getkp(app.package.lib.enabled, it);
      bool* enabled = sp_ht_it_getp(app.package.lib.enabled, it);
      if (*enabled) {
        sp_dyn_array_push(kinds, spn_dep_build_kind_to_str(*kind));
      }
    }
    if (sp_dyn_array_size(kinds)) {
      spn_toml_append_str_array_cstr(&toml, "kinds", kinds);
    }
    if (sp_str_valid(app.package.lib.name)) {
      spn_toml_append_str_cstr(&toml, "name", app.package.lib.name);
    }
    spn_toml_end_table(&toml);
  }

  if (sp_ht_size(app.package.bin)) {
    spn_toml_begin_array_cstr(&toml, "bin");
    sp_ht_for(app.package.bin, it) {
      spn_bin_t* bin = sp_ht_it_getp(app.package.bin, it);
      spn_toml_append_array_table(&toml);
      spn_toml_append_str_cstr(&toml, "name", bin->name);
      if (sp_dyn_array_size(bin->source)) {
        spn_toml_append_str_array_cstr(&toml, "source", bin->source);
      }
      if (sp_dyn_array_size(bin->include)) {
        spn_toml_append_str_array_cstr(&toml, "include", bin->include);
      }
      if (bin->profile.some) {
        spn_toml_append_str_cstr(&toml, "profile", sp_opt_get(bin->profile));
      }
    }
    spn_toml_end_array(&toml);
  }

  if (sp_ht_size(app.package.options)) {
    spn_toml_begin_table_cstr(&toml, "options");
    sp_ht_for(app.package.options, it) {
      sp_str_t* key = sp_ht_it_getkp(app.package.options, it);
      spn_dep_option_t* option = sp_ht_it_getp(app.package.options, it);
      spn_toml_append_option(&toml, *key, *option);
    }
    spn_toml_end_table(&toml);
  }

  if (sp_ht_size(app.package.config)) {
    spn_toml_begin_table_cstr(&toml, "config");

    sp_ht_for(app.package.config, it) {
      sp_str_t name = *sp_ht_it_getkp(app.package.config, it);
      spn_dep_options_t* options = sp_ht_it_getp(app.package.config, it);

      spn_toml_begin_table(&toml, name);
      sp_ht_for(*options, n) {
        sp_str_t key = *sp_ht_it_getkp(*options, n);
        spn_dep_option_t option = *sp_ht_it_getp(*options, n);
        spn_toml_append_option(&toml, key, option);
      }
      spn_toml_end_table(&toml);
    }

    spn_toml_end_table(&toml);
  }

  sp_str_t output = spn_toml_writer_write(&toml);
  output = sp_str_trim_right(output);
  sp_io_stream_t file = sp_io_from_file(app.paths.project.toml, SP_IO_MODE_WRITE);
  sp_io_write_str(&file, output);
  sp_io_close(&file);
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

bool spn_dep_state_is_terminal(spn_dep_context_t* dep) {
  switch (dep->state) {
    case SPN_DEP_BUILD_STATE_NONE:
    case SPN_DEP_BUILD_STATE_FAILED:
    case SPN_DEP_BUILD_STATE_DONE:
    case SPN_DEP_BUILD_STATE_CANCELED: return true;
    default: return false;
  }
}

bool spn_dep_context_is_binary(spn_dep_context_t* dep) {
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
  spn_dep_context_t* da = *(spn_dep_context_t**)a;
  spn_dep_context_t* db = *(spn_dep_context_t**)b;
  return sp_str_compare_alphabetical(da->name, db->name);
}


bool spn_dep_context_is_build_stamped(spn_dep_context_t* context) {
  return sp_os_does_path_exist(context->paths.stamp);
}

void spn_dep_context_init(spn_dep_context_t* dep, spn_package_t* package) {
  SP_ASSERT(package);
  dep->name = sp_str_copy(package->name);
  dep->package = package;
  sp_mutex_init(&dep->mutex, SP_MUTEX_PLAIN);
}

spn_err_t spn_dep_context_sync_remote(spn_dep_context_t* dep) {
  if (!sp_os_does_path_exist(dep->package->paths.source)) {
    spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_CLONING);

    sp_str_t url = sp_format("https://github.com/{}.git", SP_FMT_STR(dep->package->repo));
    if (spn_git_clone(url, dep->package->paths.source)) {
      spn_dep_context_set_build_error(dep, sp_format(
        "Failed to clone {:fg brightcyan}",
        SP_FMT_STR(dep->package->name)
      ));

      return SPN_ERROR;
    }

    SP_ASSERT(sp_os_is_directory(dep->package->paths.source));
  }
  else {
    spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_FETCHING);
    if (spn_git_fetch(dep->package->paths.source)) {
      spn_dep_context_set_build_error(dep, sp_format(
        "Failed to fetch {:fg brightcyan}",
        SP_FMT_STR(dep->package->name)
      ));

      return SPN_ERROR;
    }
  }

  return SPN_OK;
}

spn_err_t spn_dep_context_resolve_commit(spn_dep_context_t* dep) {
  spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_RESOLVING);

  sp_str_t message = spn_git_get_commit_message(dep->package->paths.source, dep->metadata.commit);
  message = sp_str_truncate(message, 32, SP_LIT("..."));
  message = sp_str_replace_c8(message, '\n', ' ');
  message = sp_str_pad(message, 32);

  sp_mutex_lock(&dep->mutex);
  dep->message = message;
  sp_mutex_unlock(&dep->mutex);

  return SPN_OK;
}

spn_err_t spn_dep_context_sync_local(spn_dep_context_t* dep) {
  spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_CHECKING_OUT);

  return spn_git_checkout(dep->package->paths.source, dep->metadata.commit);
}

s32 spn_dep_thread_resolve(void* user_data) {
  spn_dep_context_t* dep = (spn_dep_context_t*)user_data;
  SP_ASSERT(!spn_dep_context_sync_remote(dep));
  SP_ASSERT(!spn_dep_context_resolve_commit(dep));
  spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_DONE);
  return 0;
}

s32 spn_dep_thread_build(void* user_data) {
  spn_dep_context_t* dep = (spn_dep_context_t*)user_data;
  SP_ASSERT(!spn_dep_context_sync_remote(dep));
  SP_ASSERT(!spn_dep_context_resolve_commit(dep));
  SP_ASSERT(!spn_dep_context_sync_local(dep));
  SP_ASSERT(!spn_dep_context_build(dep));

  spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_DONE);

  return 0;
}

s32 spn_dep_context_build(spn_dep_context_t* dep) {
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

  spn_package_compile(dep->package);

  spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_BUILDING);
  if (dep->package->on_build) {
    dep->package->on_build(dep);
  }

  spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_PACKAGING);
  if (dep->package->on_package) {
    dep->package->on_package(dep);
  }

  spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_STAMPING);
  sp_os_create_file(dep->paths.stamp);

  return SPN_OK;
}

void spn_dep_context_set_build_state(spn_dep_context_t* dep, spn_dep_state_t state) {
  sp_mutex_lock(&dep->mutex);
  dep->state = state;
  sp_mutex_unlock(&dep->mutex);
}

void spn_dep_context_set_build_error(spn_dep_context_t* dep, sp_str_t error) {
  sp_mutex_lock(&dep->mutex);
  dep->state = SPN_DEP_BUILD_STATE_FAILED;
  dep->error = sp_str_copy(error);
  sp_mutex_unlock(&dep->mutex);
}


////////////
// SEMVER //
////////////
c8 spn_semver_parser_peek(spn_semver_parser_t* parser) {
  if (spn_semver_parser_is_done(parser)) return '\0';
  return sp_str_at(parser->str, parser->it);
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

bool spn_semver_parser_is_whitespace(c8 c) {
  return c == ' ' || c == '\t' || c == '\n';
}

bool spn_semver_parser_is_done(spn_semver_parser_t* parser) {
  return parser->it >= parser->str.len;
}

void spn_semver_parser_eat_whitespace(spn_semver_parser_t* parser) {
  while (true) {
    if (spn_semver_parser_is_done(parser)) break;
    if (!spn_semver_parser_is_whitespace(spn_semver_parser_peek(parser))) break;

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

spn_semver_parsed_t spn_semver_parser_parse_version(spn_semver_parser_t* parser) {
  spn_semver_parsed_t parsed = SP_ZERO_INITIALIZE();

  parsed.version.major = spn_semver_parser_parse_number(parser);
  parsed.components.major = true;

  if (spn_semver_parser_is_done(parser)) return parsed;
  if (spn_semver_parser_peek(parser) != '.') return parsed;

  spn_semver_parser_eat(parser);
  parsed.version.minor = spn_semver_parser_parse_number(parser);
  parsed.components.minor = true;

  if (spn_semver_parser_is_done(parser)) return parsed;
  if (spn_semver_parser_peek(parser) != '.') return parsed;

  spn_semver_parser_eat(parser);
  parsed.version.patch = spn_semver_parser_parse_number(parser);
  parsed.components.patch = true;

  return parsed;
}

spn_semver_range_t spn_semver_caret_to_range(spn_semver_parsed_t parsed) {
  spn_semver_range_t range = {
    .mod = SPN_SEMVER_MOD_CARET
  };

  if (parsed.version.major > 0) {
    range.low.op = SPN_SEMVER_OP_GEQ;
    range.low.version = parsed.version;
    range.high.op = SPN_SEMVER_OP_LT;
    range.high.version.major = parsed.version.major + 1;
    range.high.version.minor = 0;
    range.high.version.patch = 0;
  } else if (parsed.version.minor > 0) {
    range.low.op = SPN_SEMVER_OP_GEQ;
    range.low.version = parsed.version;
    range.high.op = SPN_SEMVER_OP_LT;
    range.high.version.major = parsed.version.major;
    range.high.version.minor = parsed.version.minor + 1;
    range.high.version.patch = 0;
  } else {
    range.low.op = SPN_SEMVER_OP_GEQ;
    range.low.version = parsed.version;
    range.high.op = SPN_SEMVER_OP_LT;
    range.high.version.major = parsed.version.major;
    range.high.version.minor = parsed.version.minor;
    range.high.version.patch = parsed.version.patch + 1;
  }

  return range;
}

spn_semver_range_t spn_semver_tilde_to_range(spn_semver_parsed_t parsed) {
  spn_semver_range_t range = {
    .mod = SPN_SEMVER_MOD_TILDE
  };

  if (parsed.components.patch) {
    range.low.op = SPN_SEMVER_OP_GEQ;
    range.low.version = parsed.version;
    range.high.op = SPN_SEMVER_OP_LT;
    range.high.version.major = parsed.version.major;
    range.high.version.minor = parsed.version.minor + 1;
    range.high.version.patch = 0;
  } else if (parsed.components.minor) {
    range.low.op = SPN_SEMVER_OP_GEQ;
    range.low.version = parsed.version;
    range.high.op = SPN_SEMVER_OP_LT;
    range.high.version.major = parsed.version.major;
    range.high.version.minor = parsed.version.minor + 1;
    range.high.version.patch = 0;
  } else {
    range.low.op = SPN_SEMVER_OP_GEQ;
    range.low.version = parsed.version;
    range.high.op = SPN_SEMVER_OP_LT;
    range.high.version.major = parsed.version.major + 1;
    range.high.version.minor = 0;
    range.high.version.patch = 0;
  }

  return range;
}

spn_semver_range_t spn_semver_wildcard_to_range(spn_semver_parsed_t parsed) {
  spn_semver_range_t range = {
    .mod = SPN_SEMVER_MOD_WILDCARD
  };

  if (!parsed.components.major) {
    range.low.op = SPN_SEMVER_OP_GEQ;
    range.low.version.major = 0;
    range.low.version.minor = 0;
    range.low.version.patch = 0;
    range.high.op = SPN_SEMVER_OP_LT;
    range.high.version.major = 0xFFFFFFFF;
    range.high.version.minor = 0xFFFFFFFF;
    range.high.version.patch = 0xFFFFFFFF;
  } else if (!parsed.components.minor) {
    range.low.op = SPN_SEMVER_OP_GEQ;
    range.low.version.major = parsed.version.major;
    range.low.version.minor = 0;
    range.low.version.patch = 0;
    range.high.op = SPN_SEMVER_OP_LT;
    range.high.version.major = parsed.version.major + 1;
    range.high.version.minor = 0;
    range.high.version.patch = 0;
  } else {
    range.low.op = SPN_SEMVER_OP_GEQ;
    range.low.version.major = parsed.version.major;
    range.low.version.minor = parsed.version.minor;
    range.low.version.patch = 0;
    range.high.op = SPN_SEMVER_OP_LT;
    range.high.version.major = parsed.version.major;
    range.high.version.minor = parsed.version.minor + 1;
    range.high.version.patch = 0;
  }

  return range;
}

spn_semver_range_t spn_semver_comparison_to_range(spn_semver_op_t op, spn_semver_t version) {
  spn_semver_range_t range = {
    .mod = SPN_SEMVER_MOD_CMP
  };

  switch (op) {
    case SPN_SEMVER_OP_EQ: {
      range.low.op = SPN_SEMVER_OP_GEQ;
      range.low.version = version;
      range.high.op = SPN_SEMVER_OP_LEQ;
      range.high.version = version;
      break;
    }
    case SPN_SEMVER_OP_GEQ: {
      range.low.op = SPN_SEMVER_OP_GEQ;
      range.low.version = version;
      range.high.op = SPN_SEMVER_OP_LEQ;
      range.high.version = (spn_semver_t){SP_LIMIT_MAX_U32, SP_LIMIT_MAX_U32, SP_LIMIT_MAX_U32};
      break;
    }
    case SPN_SEMVER_OP_GT: {
      range.low.op = SPN_SEMVER_OP_GT;
      range.low.version = version;
      range.high.op = SPN_SEMVER_OP_LEQ;
      range.high.version = (spn_semver_t){SP_LIMIT_MAX_U32, SP_LIMIT_MAX_U32, SP_LIMIT_MAX_U32};
      break;
    }
    case SPN_SEMVER_OP_LEQ: {
      range.low.op = SPN_SEMVER_OP_GEQ;
      range.low.version = (spn_semver_t){SP_LIMIT_MIN_U32, SP_LIMIT_MIN_U32, SP_LIMIT_MIN_U32};
      range.high.op = SPN_SEMVER_OP_LEQ;
      range.high.version = version;
      break;
    }
    case SPN_SEMVER_OP_LT: {
      range.low.op = SPN_SEMVER_OP_GEQ;
      range.low.version = (spn_semver_t){SP_LIMIT_MIN_U32, SP_LIMIT_MIN_U32, SP_LIMIT_MIN_U32};
      range.high.op = SPN_SEMVER_OP_LT;
      range.high.version = version;
      break;
    }
  }

  return range;
}

spn_semver_range_t spn_semver_range_from_str(sp_str_t str) {
  spn_semver_parser_t parser = { .str = str, .it = 0 };
  spn_semver_range_t range = {0};

  spn_semver_parser_eat_whitespace(&parser);

  c8 c = spn_semver_parser_peek(&parser);

  if (c == '^') {
    spn_semver_parser_eat(&parser);
    spn_semver_parsed_t parsed = spn_semver_parser_parse_version(&parser);
    range = spn_semver_caret_to_range(parsed);
  }
  else if (c == '~') {
    spn_semver_parser_eat(&parser);
    spn_semver_parsed_t parsed = spn_semver_parser_parse_version(&parser);
    range = spn_semver_tilde_to_range(parsed);
  }
  else if (c == '*') {
    spn_semver_parser_eat(&parser);
    range = spn_semver_wildcard_to_range((spn_semver_parsed_t){0});
  }
  else if (spn_semver_parser_is_digit(c)) {
    u32 saved_it = parser.it;
    spn_semver_parsed_t parsed = spn_semver_parser_parse_version(&parser);

    if (!spn_semver_parser_is_done(&parser)) {
      c8 next = spn_semver_parser_peek(&parser);
      if (next == '.') {
        spn_semver_parser_eat(&parser);
        SP_ASSERT(!spn_semver_parser_is_done(&parser));
        if (spn_semver_parser_peek(&parser) == '*') {
          spn_semver_parser_eat(&parser);
          range = spn_semver_wildcard_to_range(parsed);
          return range;
        }
        parser.it = saved_it;
      }
    }

    parser.it = saved_it;
    parsed = spn_semver_parser_parse_version(&parser);
    range = spn_semver_caret_to_range(parsed);
  }
  else if (c == '>' || c == '<' || c == '=') {
    spn_semver_op_t op;
    if (c == '>') {
      spn_semver_parser_eat(&parser);

      bool done = spn_semver_parser_is_done(&parser);
      if (done) {
        op = SPN_SEMVER_OP_GT;
      }
      else if (spn_semver_parser_peek(&parser) == '=') {
        spn_semver_parser_eat(&parser);
        op = SPN_SEMVER_OP_GEQ;
      }
      else {
        op = SPN_SEMVER_OP_GT;
      }
    }
    else if (c == '<') {
      spn_semver_parser_eat(&parser);

      bool done = spn_semver_parser_is_done(&parser);
      if (done) {
        op = SPN_SEMVER_OP_LT;
      }
      else if (spn_semver_parser_peek(&parser) == '=') {
        spn_semver_parser_eat(&parser);
        op = SPN_SEMVER_OP_LEQ;
      }
      else {
        op = SPN_SEMVER_OP_LT;
      }
    }
    else {
      spn_semver_parser_eat(&parser);
      op = SPN_SEMVER_OP_EQ;
    }

    spn_semver_parser_eat_whitespace(&parser);
    SP_ASSERT(!spn_semver_parser_is_done(&parser));
    spn_semver_parsed_t parsed = spn_semver_parser_parse_version(&parser);
    range = spn_semver_comparison_to_range(op, parsed.version);
  }
  else {
    SP_ASSERT(false && "unsupported semver format");
  }

  return range;
}

spn_semver_t spn_semver_from_str(sp_str_t str) {
  spn_semver_parser_t parser = {
    .str = str
  };
  spn_semver_parsed_t parsed = spn_semver_parser_parse_version(&parser);
  return parsed.version;
}

sp_str_t spn_semver_op_to_str(spn_semver_op_t op) {
  switch (op) {
    case SPN_SEMVER_OP_EQ: return sp_str_lit("==");
    case SPN_SEMVER_OP_GEQ: return sp_str_lit(">=");
    case SPN_SEMVER_OP_GT: return sp_str_lit(">");
    case SPN_SEMVER_OP_LEQ: return sp_str_lit("<=");
    case SPN_SEMVER_OP_LT: return sp_str_lit("<");
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

sp_str_t spn_semver_mod_to_str(spn_semver_mod_t mod, spn_semver_op_t op) {
  switch (mod) {
    case SPN_SEMVER_MOD_TILDE: return sp_str_lit("~");
    case SPN_SEMVER_MOD_CARET: return sp_str_lit(""); // ^ is implied
    case SPN_SEMVER_MOD_WILDCARD: return sp_str_lit("*");
    case SPN_SEMVER_MOD_CMP: return spn_semver_op_to_str(op);
    case SPN_SEMVER_MOD_NONE: return sp_str_lit("");
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

sp_str_t spn_semver_to_str(spn_semver_t version) {
  return sp_format(
    "{}.{}.{}",
    SP_FMT_U32(version.major),
    SP_FMT_U32(version.minor),
    SP_FMT_U32(version.patch)
  );
}

sp_str_t spn_semver_range_to_str(spn_semver_range_t range) {
  return sp_format(
    "{}{}",
    SP_FMT_STR(spn_semver_mod_to_str(range.mod, range.low.op)),
    SP_FMT_STR(spn_semver_to_str(range.low.version))
  );
}

bool spn_semver_eq(spn_semver_t lhs, spn_semver_t rhs) {
  return lhs.major == rhs.major && lhs.minor == rhs.minor && lhs.patch == rhs.patch;
}

bool spn_semver_geq(spn_semver_t lhs, spn_semver_t rhs) {
  if (lhs.major != rhs.major) return lhs.major > rhs.major;
  if (lhs.minor != rhs.minor) return lhs.minor > rhs.minor;
  return lhs.patch >= rhs.patch;
}

bool spn_semver_ge(spn_semver_t lhs, spn_semver_t rhs) {
  if (lhs.major != rhs.major) return lhs.major > rhs.major;
  if (lhs.minor != rhs.minor) return lhs.minor > rhs.minor;
  return lhs.patch > rhs.patch;
}

bool spn_semver_leq(spn_semver_t lhs, spn_semver_t rhs) {
  if (lhs.major != rhs.major) return lhs.major < rhs.major;
  if (lhs.minor != rhs.minor) return lhs.minor < rhs.minor;
  return lhs.patch <= rhs.patch;
}

bool spn_semver_le(spn_semver_t lhs, spn_semver_t rhs) {
  if (lhs.major != rhs.major) return lhs.major < rhs.major;
  if (lhs.minor != rhs.minor) return lhs.minor < rhs.minor;
  return lhs.patch < rhs.patch;
}

s32 spn_semver_cmp(spn_semver_t lhs, spn_semver_t rhs) {
  if (spn_semver_eq(lhs, rhs)) return SP_QSORT_EQUAL;
  if (spn_semver_leq(lhs, rhs)) return SP_QSORT_A_FIRST;
  return SP_QSORT_B_FIRST;
}

s32 spn_semver_sort_kernel(const void* a, const void* b) {
  const spn_semver_t* lhs = (const spn_semver_t*)a;
  const spn_semver_t* rhs = (const spn_semver_t*)b;
  return spn_semver_cmp(*lhs, *rhs);
}

bool spn_semver_satisfies(spn_semver_t version, spn_semver_t bound_version, spn_semver_op_t op) {
  switch (op) {
    case SPN_SEMVER_OP_EQ: {
      return spn_semver_eq(version, bound_version);
    }
    case SPN_SEMVER_OP_LT: {
      return spn_semver_le(version, bound_version);
    }
    case SPN_SEMVER_OP_LEQ: {
      return spn_semver_leq(version, bound_version);
    }
    case SPN_SEMVER_OP_GT: {
      return spn_semver_ge(version, bound_version);
    }
    case SPN_SEMVER_OP_GEQ: {
      return spn_semver_geq(version, bound_version);
    }
    default: {
      SP_UNREACHABLE_CASE();
    }
  }
}

spn_dep_option_t spn_dep_option_from_toml(toml_table_t* toml, const c8* key) {
  toml_unparsed_t unparsed = toml_table_unparsed(toml, key);
  SP_ASSERT(unparsed);

  bool b;
  s64 s;
  f32 f;
  c8* cstr;
  s32 len;
  void* ptr;

  if (!toml_value_string(unparsed, &cstr, &len)) {
    return (spn_dep_option_t) {
      .kind = SPN_DEP_OPTION_KIND_STR,
      .name = sp_str_from_cstr(key),
      .str = sp_str_from_cstr(cstr)
    };
  }
  else if (!toml_value_int(unparsed, &s)) {
    return (spn_dep_option_t) {
      .kind = SPN_DEP_OPTION_KIND_S64,
      .name = sp_str_from_cstr(key),
      .s = s
    };
  }
  else if (!toml_value_bool(unparsed, &b)) {
    return (spn_dep_option_t) {
      .kind = SPN_DEP_OPTION_KIND_BOOL,
      .name = sp_str_from_cstr(key),
      .b = b
    };
  }

  SP_UNREACHABLE_RETURN(SP_ZERO_STRUCT(spn_dep_option_t));
}

// @spader make this return spn_err_t, bubble from the tcc util fns, give a separate error for add/reloc/other errors, !exist isnt an error
void spn_package_compile(spn_package_t* package) {
  if (!sp_os_does_path_exist(package->paths.script)) return;

  spn_tcc_t* tcc = spn_tcc_new();
  spn_tcc_add_file(tcc, package->paths.script);
  tcc_relocate(tcc);
  package->on_package = tcc_get_symbol(tcc, "package");
  package->on_build = tcc_get_symbol(tcc, "build");
}

void spn_package_init(spn_package_t* package) {
  sp_ht_set_fns(package->bin, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(package->deps, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(package->options, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(package->config, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
}

void spn_package_set_manifest(spn_package_t* package, sp_str_t manifest_path) {
  package->paths.root = sp_os_parent_path(manifest_path);
  package->paths.manifest = sp_str_copy(manifest_path);
  package->paths.metadata = sp_os_join_path(package->paths.root, sp_str_lit("metadata.toml"));
  package->paths.script = sp_os_join_path(package->paths.root, sp_str_lit("spn.c"));
}

void spn_package_set_name(spn_package_t* package, sp_str_t name) {
  package->name = sp_str_copy(name);
  package->paths.source = sp_os_join_path(app.paths.source, name);
}

spn_package_t spn_package_load(sp_str_t manifest_path) {
  spn_package_t package = SP_ZERO_INITIALIZE();
  spn_package_init(&package);
  spn_package_set_manifest(&package, manifest_path);

  spn_toml_package_t toml = SP_ZERO_INITIALIZE();
  toml.spn = spn_toml_parse(package.paths.manifest);
  toml.package = toml_table_table(toml.spn, "package");
  toml.lib = toml_table_table(toml.spn, "lib");
  toml.bin = toml_table_array(toml.spn, "bin");
  toml.profile = toml_table_array(toml.spn, "profile");
  toml.registry = toml_table_array(toml.spn, "registry");
  toml.deps = toml_table_table(toml.spn, "deps");
  toml.options = toml_table_table(toml.spn, "options");
  toml.config = toml_table_table(toml.spn, "config");
  toml.metadata = spn_toml_parse(package.paths.metadata);

  package.toml = toml;
  spn_package_set_name(&package, spn_toml_str(toml.package, "name"));
  package.repo = spn_toml_str_opt(toml.package, "repo", "");
  package.author = spn_toml_str_opt(toml.package, "author", "");
  package.maintainer = spn_toml_str_opt(toml.package, "maintainer", "");

  if (toml.lib) {
    toml_array_t* kinds = toml_table_array(toml.lib, "kinds");
    spn_toml_arr_for(kinds, it) {
      toml_value_t value = toml_array_string(kinds, it);
      SP_ASSERT(value.ok);

      spn_lib_kind_t kind = spn_lib_kind_from_str(sp_str_view(value.u.s));
      sp_ht_insert(package.lib.enabled, kind, true);
    }

    package.lib.name = spn_toml_str_opt(toml.lib, "name", "");

    if (sp_ht_key_exists(package.lib.enabled, SPN_LIB_KIND_SHARED)) SP_ASSERT(!sp_str_empty(package.lib.name));
    if (sp_ht_key_exists(package.lib.enabled, SPN_LIB_KIND_STATIC)) SP_ASSERT(!sp_str_empty(package.lib.name));
  }

  if (toml.profile) {
    spn_toml_arr_for(toml.profile, n) {
      toml_table_t* it = toml_array_table(toml.profile, n);
      spn_profile_t profile = SP_ZERO_INITIALIZE();

      profile.cc = spn_cc_kind_from_str(spn_toml_str_opt(it, "cc", "gcc"));
      profile.libc = spn_lib_kind_from_str(spn_toml_str_opt(it, "libc", "shared"));
      profile.standard = spn_c_standard_from_str(spn_toml_str_opt(it, "standard", "c99"));
      profile.mode = spn_dep_build_mode_from_str(spn_toml_str_opt(it, "mode", "debug"));

      sp_str_t profile_name = spn_toml_str(it, "name");
      sp_ht_insert(app.profiles, profile_name, profile);

      if (!app.default_profile.some) {
        sp_opt_set(app.default_profile, profile_name);
      }
    }
  }

  if (toml.registry) {
    spn_toml_arr_for(toml.registry, n) {
      toml_table_t* it = toml_array_table(toml.registry, n);

      spn_registry_t registry = {
        .name = spn_toml_str(it, "name"),
        .location = spn_toml_str(it, "location"),
        .kind = SPN_REGISTRY_KIND_WORKSPACE,
      };

      sp_ht_insert(package.registries, registry.name, registry);
    }
  }

  if (toml.bin) {
    spn_toml_arr_for(toml.bin, n) {
      toml_table_t* it = toml_array_table(toml.bin, n);
      spn_bin_t bin = SP_ZERO_INITIALIZE();
      bin.name = spn_toml_str(it, "name");

      toml_value_t profile = toml_table_string(it, "profile");
      if (profile.ok) {
        sp_opt_set(bin.profile, sp_str_from_cstr(profile.u.s));
      }

      bin.source = spn_toml_arr_strs(toml_table_array(it, "source"));
      bin.include = spn_toml_arr_strs(toml_table_array(it, "include"));

      sp_ht_insert(package.bin, bin.name, bin);
    }
  }

  if (toml.deps) {
    const c8* key = SP_NULLPTR;
    spn_toml_for(toml.deps, n, key) {
      spn_dep_req_t dep = {
        .name = sp_str_from_cstr(key),
        .range = spn_semver_range_from_str(spn_toml_str(toml.deps, key)),
      };

      sp_ht_insert(package.deps, dep.name, dep);
    }
  }

  if (toml.options) {
    const c8* key = SP_NULLPTR;
    spn_toml_for(toml.options, n, key) {
      spn_dep_option_t option = spn_dep_option_from_toml(toml.options, key);
      sp_ht_insert(package.options, option.name, option);
    }
  }

  if (toml.config) {
    sp_da(toml_table_t*) configs = SP_ZERO_INITIALIZE();
    const c8* key = SP_NULLPTR;
    spn_toml_for(toml.config, n, key) {
      sp_dyn_array_push(configs, toml_table_table(toml.config, key));
    }

    sp_dyn_array_for(configs, it) {
      toml_table_t* config = configs[it];

      sp_str_t name = sp_str_from_cstr(config->key);

      spn_dep_options_t options = SP_NULLPTR;
      sp_ht_set_fns(options, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);

      const c8* key = SP_NULLPTR;
      spn_toml_for(config, n, key) {
        spn_dep_option_t option = spn_dep_option_from_toml(config, key);
        sp_ht_insert(options, option.name, option);
      }

      sp_ht_insert(package.config, name, options);
    }
  }

  if (toml.metadata) {
    toml.versions = toml_table_array(toml.metadata, "versions");

    const c8* key = SP_NULLPTR;
    spn_toml_arr_for(toml.versions, it) {
      toml_table_t* entry = toml_array_table(toml.versions, it);

      spn_semver_t version = spn_semver_from_str(spn_toml_str(entry, "version"));
      spn_metadata_t metadata = {
        .version = version,
        .commit = spn_toml_str(entry, "commit"),
      };

      sp_ht_insert(package.metadata, version, metadata);
      sp_dyn_array_push(package.versions, version);
    }

    sp_dyn_array_sort(package.versions, spn_semver_sort_kernel);
  }

  return package;
}

void spn_resolver_init(spn_resolver_t* resolver) {
  sp_ht_set_fns(resolver->ranges, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(resolver->versions, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(resolver->visited, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
}

void spn_resolver_add_package_constraints(spn_resolver_t* resolver, spn_package_t* package) {
  if (sp_ht_key_exists(resolver->visited, package->name)) {
    // @spader keep a stack to provide a real error message
    SP_FATAL("{:fg brightcyan} transitively includes itself", SP_FMT_STR(package->name));
  }

  // mark as visiting; until we finish this subtree, we can't see this package again (no circular deps)
  sp_ht_insert(resolver->visited, package->name, true);

  sp_ht_for(package->deps, it) {
    spn_dep_req_t request = *sp_ht_it_getp(package->deps, it);
    spn_package_t* dep = sp_ht_getp(app.packages, request.name);

    if (!sp_ht_key_exists(resolver->ranges, dep->name)) {
      sp_ht_insert(resolver->ranges, dep->name, SP_NULLPTR);
    }
    sp_da(spn_dep_version_range_t)* ranges = sp_ht_getp(resolver->ranges, dep->name);

    // collect versions which satisfy this range
    spn_dep_version_range_t range = {
      .source = request
    };

    spn_semver_t low = request.range.low.version;
    spn_semver_t high = request.range.high.version;

    sp_dyn_array_for(dep->versions, it) {
      spn_semver_t version = dep->versions[it];

      if (!range.low.some) {
        if (spn_semver_satisfies(version, low, request.range.low.op)) {
          sp_opt_set(range.low, it);
        }
      }

      if (spn_semver_satisfies(version, high, request.range.high.op)) {
        sp_opt_set(range.high, it);
      }
    }

    sp_dyn_array_push(*ranges, range);
  }

  sp_ht_for(package->deps, it) {
    spn_dep_req_t* request = sp_ht_it_getp(package->deps, it);
    spn_package_t* dep = sp_ht_getp(app.packages, request->name);
    spn_resolver_add_package_constraints(resolver, dep);
  }


  sp_ht_erase(resolver->visited, package->name);
}

void spn_resolver_resolve_from_lock_file(spn_resolver_t* resolver) {
  SP_ASSERT(app.lock.some);

  sp_ht_for(app.lock.value.entries, it) {
    spn_lock_entry_t* entry = sp_ht_it_getp(app.lock.value.entries, it);
    sp_ht_insert(resolver->versions, entry->name, entry->version);
  }
}

void spn_resolver_resolve_from_solver(spn_resolver_t* resolver) {
  spn_resolver_init(resolver);
  spn_resolver_add_package_constraints(resolver, &app.package);
  sp_ht_for(resolver->ranges, it) {
    sp_str_t name = *sp_ht_it_getkp(resolver->ranges, it);
    spn_package_t* dep = sp_ht_getp(app.packages, name);

    sp_da(spn_dep_version_range_t) ranges = *sp_ht_it_getp(resolver->ranges, it);
    SP_ASSERT(sp_dyn_array_size(ranges));

    spn_dep_req_t sl, sh = SP_ZERO_INITIALIZE();
    u32 low = SP_LIMIT_MIN_U32, high = SP_LIMIT_MAX_U32;
    sp_dyn_array_for(ranges, n) {
      spn_dep_version_range_t range = ranges[n];
      SP_ASSERT(range.low.some);
      SP_ASSERT(range.high.some);

      if (sp_opt_get(range.low) >= low) {
        low = sp_opt_get(range.low);
        sl = range.source;
      }
      if (sp_opt_get(range.high) <= high) {
        high = sp_opt_get(range.high);
        sh = range.source;
      }
    }

    if (low > high) {
      sp_str_builder_t builder = SP_ZERO_INITIALIZE();
      sp_str_builder_append_fmt(&builder, "{:fg brightcyan} cannot be resolved:", SP_FMT_STR(name));
      sp_str_builder_indent(&builder);
      sp_str_builder_new_line(&builder);
      sp_str_builder_append_fmt(&builder, "{:fg brightcyan} requires {:fg brightred}", SP_FMT_STR(sl.name), SP_FMT_STR(spn_semver_range_to_str(sl.range)));
      sp_str_builder_new_line(&builder);
      sp_str_builder_append_fmt(&builder, "{:fg brightcyan} requires {:fg brightred}", SP_FMT_STR(sh.name), SP_FMT_STR(spn_semver_range_to_str(sh.range)));

      SP_FATAL("{}", SP_FMT_STR(sp_str_builder_move(&builder)));
    }

    spn_semver_t version = dep->versions[high];
    sp_ht_insert(resolver->versions, name, version);
    SP_LOG("{:fg brightcyan}: {}", SP_FMT_STR(name), SP_FMT_STR(spn_semver_to_str(version)));
  }
}

void spn_resolver_resolve(spn_resolver_t* resolver) {
  if (app.lock.some) {
    spn_resolver_resolve_from_lock_file(resolver);
  } else {
    spn_resolver_resolve_from_solver(resolver);
  }
}

void spn_app_prepare() {
  sp_ht_for(app.resolver.versions, it) {
    sp_str_t name = *sp_ht_it_getkp(app.resolver.versions, it);
    spn_semver_t version = *sp_ht_it_getp(app.resolver.versions, it);

    spn_package_t* package = sp_ht_getp(app.packages, name);
    SP_ASSERT(package);

    spn_metadata_t* metadata = sp_ht_getp(package->metadata, version);
    SP_ASSERT(metadata);
    SP_ASSERT(!sp_str_empty(metadata->commit));

    spn_dep_context_t dep = {
      .name = name,
      .mode = SPN_DEP_BUILD_MODE_DEBUG,
      .metadata = *metadata,
      .package = package,
    };

    spn_dep_options_t* options = sp_ht_getp(app.package.config, name);
    if (options) {
      spn_dep_option_t* kind = sp_ht_getp(*options, sp_str_lit("kind"));
      if (kind) {
        dep.kind = spn_lib_kind_from_str(kind->str);
      }
    }

    if (!dep.kind) {
      spn_lib_kind_t kinds [] = {
        SPN_LIB_KIND_SOURCE, SPN_LIB_KIND_STATIC, SPN_LIB_KIND_SHARED
      };
      SP_CARR_FOR(kinds, it) {
        if (sp_ht_getp(package->lib.enabled, kinds[it])) {
          dep.kind = kinds[it];
        }
      }
    }

    sp_dyn_array(sp_hash_t) hashes = SP_NULLPTR;
    sp_dyn_array_push(hashes, sp_hash_str(dep.metadata.commit));
    sp_dyn_array_push(hashes, sp_hash_bytes(&dep.metadata.version, sizeof(spn_semver_t), 0)); // padding?
    dep.build_id = sp_hash_combine(hashes, sp_dyn_array_size(hashes));
    sp_str_t build_id = sp_format("{}", SP_FMT_SHORT_HASH(dep.build_id));

    sp_str_t build_dir = sp_os_join_path(app.paths.build, dep.package->name);
    sp_str_t store_dir = sp_os_join_path(app.paths.store, dep.package->name);
    dep.paths.source = dep.package->paths.source;
    dep.paths.work = sp_os_join_path(build_dir, build_id);
    dep.paths.store = sp_os_join_path(store_dir, build_id);
    dep.paths.include = sp_os_join_path(dep.paths.store, SP_LIT("include"));
    dep.paths.lib = sp_os_join_path(dep.paths.store, SP_LIT("lib"));
    dep.paths.vendor = sp_os_join_path(dep.paths.store, SP_LIT("vendor"));
    dep.paths.log = sp_os_join_path(dep.paths.work, SP_LIT("spn.log"));
    dep.paths.stamp = sp_os_join_path(dep.paths.store, SP_LIT("spn.stamp"));

    sp_ht_insert(app.deps, name, dep);
  }
}

sp_str_t spn_registry_get_path(spn_registry_t* registry) {
  switch (registry->kind) {
    case SPN_REGISTRY_KIND_WORKSPACE: {
      return sp_os_join_path(app.paths.project.dir, registry->location);
    }
    case SPN_REGISTRY_KIND_BUILTIN:
    case SPN_REGISTRY_KIND_USER: {
      return sp_str_copy(registry->location);
    }
    case SPN_REGISTRY_KIND_REMOTE: {
      SP_UNREACHABLE();
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
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
    "  add <package>  Add a package to your project\n"
    "  update <package> Update a package to the latest version\n"
    "  tool           Manage spn tools\n"
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
  sp_ht_set_fns(app->matrices, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(app->profiles, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);

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
  app->paths.config = sp_os_join_path(app->paths.config_dir, SP_LIT("spn.toml")); // @llm user config is here

  if (sp_os_does_path_exist(app->paths.config)) {
    toml_table_t* toml = spn_toml_parse(app->paths.config);

    toml_value_t spn = toml_table_string(toml, "spn");
    if (spn.ok) {
      app->paths.spn = sp_str_view(spn.u.s);
    }

    toml_array_t* registries = toml_table_array(toml, "registry");
    if (registries) {
      spn_toml_arr_for(registries, n) {
        toml_table_t* it = toml_array_table(registries, n);
        spn_registry_t registry = {
          .name = spn_toml_str(it, "name"),
          .location = spn_toml_str(it, "location"),
          .kind = SPN_REGISTRY_KIND_USER
        };

        sp_ht_insert(app->config.registries, registry.name, registry);
      }
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

  // Initialize builtin registry
  app->registry = (spn_registry_t) {
    .name = sp_str_lit("builtin"),
    .location = sp_os_join_path(app->paths.spn, sp_str_lit("recipes")),
    .kind = SPN_REGISTRY_KIND_BUILTIN
  };

  // Find the cache directory after the config has been fully loaded
  app->paths.cache = sp_os_join_path(app->paths.storage, SP_LIT("cache"));
  app->paths.source = sp_os_join_path(app->paths.cache, SP_LIT("source"));
  app->paths.build = sp_os_join_path(app->paths.cache, SP_LIT("build"));
  app->paths.store = sp_os_join_path(app->paths.cache, SP_LIT("store"));

  sp_os_create_directory(app->paths.cache);
  sp_os_create_directory(app->paths.source);
  sp_os_create_directory(app->paths.build);

  if (sp_os_does_path_exist(app->paths.project.toml)) {
    app->package = spn_package_load(app->paths.project.toml);
  }

  sp_ht_for(app->package.registries, it) {
    spn_registry_t* registry = sp_ht_it_getp(app->package.registries, it);
    sp_dyn_array_push(app->search, spn_registry_get_path(registry));
  }

  sp_ht_for(app->config.registries, it) {
    spn_registry_t* registry = sp_ht_it_getp(app->config.registries, it);
    sp_dyn_array_push(app->search, spn_registry_get_path(registry));
  }

  sp_dyn_array_push(app->search, spn_registry_get_path(&app->registry));

  sp_da(sp_str_t) search_queue = SP_NULLPTR;
  sp_dyn_array_for(app->search, it) {
    sp_dyn_array_push(search_queue, app->search[it]);
  }

  sp_da(sp_str_t) files = SP_NULLPTR;
  sp_dyn_array_for(search_queue, i) {
    sp_str_t path = search_queue[i];
    if (!sp_os_does_path_exist(path)) continue;
    if (!sp_os_is_directory(path)) {
      SP_FATAL("{:fg brightcyan} is on the search path, but it's not a directory", SP_FMT_STR(path));
    }

    sp_da(sp_os_dir_entry_t) entries = sp_os_scan_directory(path);
    sp_dyn_array_for(entries, i) {
      sp_os_dir_entry_t entry = entries[i];
      if (sp_os_is_directory(entry.file_path)) {
        sp_dyn_array_push(search_queue, sp_str_copy(entry.file_path));
      }
      else {
        if (!sp_str_equal_cstr(entry.file_name, "spn.toml")) continue;

        sp_dyn_array_push(files, sp_str_copy(entry.file_path));
      }
    }
  }

  sp_dyn_array_for(files, it) {
    sp_str_t file_path = files[it];
    spn_package_t package = spn_package_load(file_path);
    sp_ht_insert(app->packages, package.name, package);
  }


  if (sp_ht_empty(app->profiles)) {
    spn_profile_t debug = {
      .cc = SPN_CC_GCC,
      .libc = SPN_LIB_KIND_SHARED,
      .standard = SPN_C11,
      .mode = SPN_DEP_BUILD_MODE_DEBUG
    };
    sp_ht_insert(app->profiles, sp_str_lit("debug"), debug);

    spn_profile_t release = {
      .cc = SPN_CC_GCC,
      .libc = SPN_LIB_KIND_SHARED,
      .standard = SPN_C11,
      .mode = SPN_DEP_BUILD_MODE_RELEASE
    };
    sp_ht_insert(app->profiles, sp_str_lit("release"), release);

    sp_opt_set(app->default_profile, sp_str_lit("debug"));
  }

  if (sp_os_does_path_exist(app->paths.project.lock)) {
    sp_opt_set(app->lock, spn_load_lock_file());
  }
}

/////////
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
  else if (sp_cstr_equal("init", cli->args[0])) {
    spn_cli_init(cli);
  }
  else if (sp_cstr_equal("add", cli->args[0])) {
    spn_cli_add(cli);
  }
  else if (sp_cstr_equal("update", cli->args[0])) {
    spn_cli_update(cli);
  }
  else if (sp_cstr_equal("tool", cli->args[0])) {
    spn_cli_tool(cli);
  }
}


spn_dep_context_t* spn_cli_assert_dep_exists(sp_str_t name) {
  spn_dep_context_t* dep = sp_ht_getp(app.deps, name);
  SP_ASSERT_FMT(dep, "{:fg brightyellow} is not in this project", SP_FMT_STR(name));
  return dep;
}


void spn_cli_init(spn_cli_t* cli) {
  spn_cli_init_t* command = &cli->init;

  struct argparse argparse;
  argparse_init(
    &argparse,
    (struct argparse_option []) {
      OPT_HELP(),
      OPT_BOOLEAN('b', "bare", &command->bare, "create minimal project without sp dependency or main.c", SP_NULLPTR),
      OPT_END()
    },
    (const c8* const []) {
      "spn init [options]",
      "Initialize a new spn project",
      SP_NULLPTR
    },
    SP_NULL
  );
  cli->num_args = argparse_parse(&argparse, cli->num_args, cli->args);

  // Get current directory name for project name
  sp_str_t cwd = sp_os_get_cwd();
  sp_str_t project_name = sp_os_extract_file_name(cwd);

  // Check if files already exist before writing anything
  sp_str_t spn_toml_path = sp_str_lit("spn.toml");
  sp_str_t main_c_path = sp_str_lit("main.c");

  if (sp_os_does_path_exist(spn_toml_path)) {
    SP_FATAL("{:fg brightyellow} already exists", SP_FMT_STR(spn_toml_path));
  }

  if (!command->bare && sp_os_does_path_exist(main_c_path)) {
    SP_FATAL("{:fg brightyellow} already exists", SP_FMT_STR(main_c_path));
  }

  spn_package_init(&app.package);
  spn_package_set_manifest(&app.package, spn_toml_path);
  spn_package_set_name(&app.package, project_name);

  if (!command->bare) {
    spn_app_add(sp_str_lit("sp"));

    // Add binary entry for main.c
    spn_bin_t bin = {
      .name = project_name,
      .profile = sp_str_lit("debug")
    };
    sp_dyn_array_push(bin.source, sp_str_lit("main.c"));
    sp_ht_insert(app.package.bin, bin.name, bin);
  }

  spn_update_project_toml();

  if (!command->bare) {
    sp_io_stream_t main_file = sp_io_from_file(main_c_path, SP_IO_MODE_WRITE);

    sp_str_t main_content = sp_str_lit(
      "#define SP_IMPLEMENTATION\n"
      "#include \"sp.h\"\n"
      "\n"
      "s32 main(s32 num_args, const c8** args) {\n"
      "  SP_LOG(\"hello, {:fg brightcyan}\", SP_FMT_CSTR(\"world\"));\n"
      "  SP_EXIT_SUCCESS();\n"
      "}\n"
    );

    if (sp_io_write_str(&main_file, main_content) != main_content.len) {
      SP_FATAL("Failed to write {:fg brightyellow}", SP_FMT_STR(main_c_path));
    }

    sp_io_close(&main_file);
  }

  SP_LOG("Initialized {:fg brightcyan} project{:fg brightcyan}",
    command->bare ? SP_FMT_CSTR("bare") : SP_FMT_STR(project_name));
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
    spn_dep_context_t* dep = sp_ht_it_getp(app.deps, it);

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
    spn_dep_context_t* dep = spn_cli_assert_dep_exists(sp_str_view(package));

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

  const c8* package = SP_NULLPTR;
  if (cli->num_args > 0 && cli->args[0]) {
    package = cli->args[0];
  }


  spn_resolver_resolve_from_lock_file(&app.resolver);
  spn_app_prepare();

  if (package) {
    spn_dep_context_t* dep = spn_cli_assert_dep_exists(sp_str_view(package));

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

  spn_dep_context_t* dep = spn_cli_assert_dep_exists(sp_str_view(cli->args[0]));

  sp_str_t recipe_path = dep->package->paths.manifest;
  sp_str_t recipe = sp_io_read_file(recipe_path);
  if (!sp_str_valid(recipe)) {
    SP_FATAL("failed to read recipe file: {:fg brightyellow}", SP_FMT_STR(recipe_path));
  }

  sp_os_log(recipe);
}

void spn_app_add(sp_str_t name) {
  if (sp_ht_getp(app.deps, name)) {
    SP_FATAL("{:fg brightyellow} is already in your project", SP_FMT_STR(name));
  }

  spn_package_t* package = sp_ht_getp(app.packages, name);
  if (!package) {
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

  if (sp_dyn_array_empty(package->versions)) {
    SP_FATAL("{:fg brightcyan} has no known versions", SP_FMT_STR(package->name));
  }

  spn_semver_parsed_t version = {
    .version = *sp_dyn_array_back(package->versions),
    .components = { true, true, true }
  };
  spn_dep_req_t dep = {
    .name = sp_str_copy(package->name),
    .range = spn_semver_caret_to_range(version)
  };
  sp_ht_insert(app.package.deps, dep.name, dep);

  spn_resolver_resolve_from_solver(&app.resolver);
  spn_app_prepare();

  spn_update_project_toml();
}

void spn_app_update(sp_str_t name) {
  spn_dep_context_t* dep = sp_ht_getp(app.deps, name);
  if (!dep) {
    SP_FATAL("{:fg brightyellow} is not in your project", SP_FMT_STR(name));
  }

  spn_package_t* package = sp_ht_getp(app.packages, name);
  if (!package) {
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

  if (sp_dyn_array_empty(package->versions)) {
    SP_FATAL("{:fg brightcyan} has no known versions", SP_FMT_STR(package->name));
  }

  spn_dep_req_t request = {
    .name = sp_str_copy(package->name),
    .range = spn_semver_comparison_to_range(SPN_SEMVER_OP_GEQ, *sp_dyn_array_back(package->versions))
  };

  sp_ht_insert(app.package.deps, request.name, request);

  spn_resolver_resolve_from_solver(&app.resolver);
  spn_app_prepare();

  spn_update_lock_file();
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
  spn_app_add(name);
}

void spn_cli_update(spn_cli_t* cli) {
  struct argparse argparse;
  argparse_init(
    &argparse,
    (struct argparse_option []) {
      OPT_HELP(),
      OPT_END()
    },
    (const c8* []) {
      "spn update",
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
  spn_app_update(name);
}

void spn_cli_tool(spn_cli_t* cli) {
  struct argparse argparse;
  argparse_init(
    &argparse,
    (struct argparse_option []) {
      OPT_HELP(),
      OPT_END()
    },
    (const c8* []) {
      "spn tool",
      SP_NULLPTR
    },
    SP_NULL
  );
  cli->num_args = argparse_parse(&argparse, cli->num_args, cli->args);

  if (!cli->num_args || !cli->args[0]) {
    sp_str_builder_t builder = SP_ZERO_INITIALIZE();

    sp_str_builder_append_fmt(&builder, "Usage: {:fg brightcyan}spn tool [OPTIONS] <COMMAND>");
    sp_str_builder_new_line(&builder);
    sp_str_builder_new_line(&builder);
    sp_str_builder_append_cstr(&builder, "Manage spn tools");
    sp_str_builder_new_line(&builder);
    sp_str_builder_new_line(&builder);
    sp_str_builder_append_cstr(&builder, "Commands:");
    sp_str_builder_new_line(&builder);

    sp_str_builder_indent(&builder);
    sp_str_builder_append_fmt(&builder, "{:fg brightcyan}{:fg brightyellow}     Install commands provided by a package", SP_FMT_CSTR("install"), SP_FMT_CSTR("<package>"));
    sp_str_builder_new_line(&builder);
    sp_str_builder_append_fmt(&builder, "{:fg brightcyan}{:fg brightyellow}      Uninstall a tool", SP_FMT_CSTR("uninstall"), SP_FMT_CSTR("<tool>"));
    sp_str_builder_new_line(&builder);
    sp_str_builder_append_fmt(&builder, "{:fg brightcyan}{:fg brightyellow} [args...]   Run a command provided by a package", SP_FMT_CSTR("run"), SP_FMT_CSTR("<tool>"));
    sp_str_builder_new_line(&builder);
    sp_str_builder_append_fmt(&builder, "{:fg brightcyan}                  List installed tools", SP_FMT_CSTR("list"));
    sp_str_builder_new_line(&builder);
    sp_str_builder_append_fmt(&builder, "{:fg brightcyan} [tool]        Upgrade installed tools", SP_FMT_CSTR("upgrade"), SP_FMT_CSTR("[tool]"));
    sp_str_builder_new_line(&builder);
    sp_str_builder_append_fmt(&builder, "{:fg brightcyan}         Ensure that tool executable directory is on the PATH", SP_FMT_CSTR("update-shell"));
    sp_str_builder_dedent(&builder);

    SP_LOG("{}", SP_FMT_STR(sp_str_builder_write(&builder)));
    return;
  }

  const c8* subcommand = cli->args[0];

  if (sp_cstr_equal("install", subcommand)) {
    SP_LOG("tool {:fg brightcyan}install - not implemented yet", SP_FMT_CSTR("install"));
  }
  else if (sp_cstr_equal("uninstall", subcommand)) {
    SP_LOG("tool {:fg brightcyan}uninstall - not implemented yet", SP_FMT_CSTR("uninstall"));
  }
  else if (sp_cstr_equal("run", subcommand)) {
    SP_LOG("tool {:fg brightcyan}run - not implemented yet", SP_FMT_CSTR("run"));
  }
  else if (sp_cstr_equal("list", subcommand)) {
    SP_LOG("tool {:fg brightcyan}list - not implemented yet", SP_FMT_CSTR("list"));
  }
  else if (sp_cstr_equal("upgrade", subcommand)) {
    SP_LOG("tool {:fg brightcyan}upgrade - not implemented yet", SP_FMT_CSTR("upgrade"));
  }
  else if (sp_cstr_equal("update-shell", subcommand)) {
    SP_LOG("tool {:fg brightcyan}update-shell - not implemented yet", SP_FMT_CSTR("update-shell"));
  }
  else {
    SP_FATAL("Unknown tool subcommand: {:fg yellow}{}", SP_FMT_CSTR(subcommand));
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
    SP_FATAL(
      "output path was specified, but no generator. try e.g.:\n  spn print --path {} {:fg yellow}",
      SP_FMT_CSTR(command->path),
      SP_FMT_CSTR("--generator make")
    );
  }
  if (!command->generator) command->generator = "";
  if (!command->compiler) command->compiler = "gcc";

  if (!app.lock.some) {
    SP_FATAL("No lock file found. Run {:fg yellow} first.", SP_FMT_CSTR("spn build"));
  }

  spn_resolver_resolve_from_lock_file(&app.resolver);
  spn_app_prepare();

  spn_generator_context_t gen = {
    .kind = spn_gen_kind_from_str(sp_str_view(command->generator)),
    .compiler = spn_cc_kind_from_str(sp_str_view(command->compiler))
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

sp_str_t spn_compiler_to_str(spn_cc_kind_t compiler) {
  return sp_str_lit("clang");
}

void spn_bin_build(spn_bin_t bin) {
  spn_cc_t cc = SP_ZERO_INITIALIZE();
  sp_str_t profile_name = bin.profile.some ? sp_opt_get(bin.profile) : sp_opt_get(app.default_profile);
  spn_cc_set_profile(&cc, profile_name);
  spn_cc_set_output_dir(&cc, sp_str_lit("build"));

  spn_cc_target_t* target = spn_cc_add_target(&cc, SPN_CC_TARGET_EXECUTABLE, bin.name);
  sp_dyn_array_for(bin.source, it) {
    spn_cc_target_add_source(target, bin.source[it]);
  }
  sp_dyn_array_for(bin.include, it) {
    spn_cc_target_add_include(target, bin.include[it]);
  }
  SP_ASSERT(!spn_cc_build(&cc));
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

  spn_resolver_resolve(&app.resolver);
  spn_app_prepare();

  // basic TUI api for making tables?
  // name+version state commit message cache

  // offer unoptimized but insanely fast builds by setting up a TCC toolchain for them where possible
  // build_deps? model all of the spn API as a package?

  // pitch:
  //  - UV/cargo but for C
  //  - build deps from source
  //  - use git as a source backend, not tarballs
  //  - single file binary, no deps besides a compiler, static musl/ancient glibc builds run anywhere
  //  - give C an equivalent to cargo.toml + build.rs
  //  - recipes are C programs compiled on the fly with tcc (no binary distribution headache + no interpreter dep)
  //  - i'll figure out how to build most libraries with tcc for ~instant unoptimized builds
  //    - `spn run main.c` should start in less than a second if compiling sqlite from source
  //  - all packages are semver (either from upstream or bolted on)
  spn_tui_mode_t mode = app.cli.output ?
    spn_output_mode_from_str(sp_str_view(app.cli.output)) :
    SPN_OUTPUT_MODE_INTERACTIVE;

  sp_ht_for(app.deps, it) {
    spn_dep_context_t* dep = sp_ht_it_getp(app.deps, it);
    dep->force = command->force;
    dep->update = command->update;
    dep->state = SPN_DEP_BUILD_STATE_IDLE;
    sp_thread_init(&dep->thread, spn_dep_thread_build, dep);
  }

  spn_tui_init(&app.tui, mode);
  spn_tui_run(&app.tui);

  spn_update_lock_file();

  sp_ht_for(app.package.bin, it) {
    spn_bin_t* bin = sp_ht_it_getp(app.package.bin, it);
    SP_ASSERT(bin);

    spn_bin_build(*bin);
  }
}

s32 main(s32 num_args, const c8** args) {
  app = SP_ZERO_STRUCT(spn_app_t);
  spn_app_init(&app, num_args, args);
  spn_cli_run(&app);

  return 0;
}
