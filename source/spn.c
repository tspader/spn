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

#include "spn.h"

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

#define SP_WARN(FMT, ...) \
  do { \
    sp_str_t __sp_warn_message = sp_format((FMT), ##__VA_ARGS__); \
    SP_LOG("{:color brightyellow}: {}", SP_FMT_CSTR("SP_WARN()"), SP_FMT_STR(__sp_warn_message)); \
  } while (0)

typedef enum {
  SP_OPT_NONE = 0,
  SP_OPT_SOME = 1,
} sp_optional_t;

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
  sp_optional_t some; \
}

#define sp_opt_set(O, V) do { (O).value = (V); (O).some = SP_OPT_SOME; } while (0)
#define sp_opt_get(O) (O).value
#define sp_opt_some(V) { .value = V, .some = SP_OPT_SOME }
#define sp_opt_none(V) { .some = SP_OPT_NONE }
#define sp_opt_is_null(V) ((V).some == SP_OPT_NONE)

#define sp_carr_empty(CARR) ( \
  (CARR) == NULL || \
  sp_os_is_memory_equal(&(CARR)[0], &(sp_env_var_t){SP_ZERO_INITIALIZE()}, sizeof((CARR)[0])) \
)

#define sp_carr_len_nt(CARR) ( \
  (CARR) == NULL ? 0 : ({ \
    u32 len = 0; \
    while (!sp_os_is_memory_equal(&(CARR)[len], &(sp_env_var_t){SP_ZERO_INITIALIZE()}, sizeof((CARR)[0]))) { \
      len++; \
    } \
    len; \
  }) \
)

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

sp_str_t sp_os_get_bin_path() {
  sp_str_t path = sp_os_get_env_var(SP_LIT("HOME"));
  SP_ASSERT(!sp_str_empty(path));

  return sp_os_join_path(path, sp_str_lit(".local/bin"));
}

sp_str_t sp_ps_config_render(sp_ps_config_t ps) {
  sp_str_builder_t b = SP_ZERO_INITIALIZE();
  sp_str_builder_append(&b, ps.command);

  sp_carr_for(ps.args, it) {
    sp_str_t arg = ps.args[it];
    if (sp_str_empty(arg)) break;

    sp_str_builder_append_c8(&b, ' ');
    sp_str_builder_append(&b, arg);
  }

  return sp_str_builder_write(&b);
}

void sp_io_write_new_line(sp_io_stream_t* io) {
  sp_io_write_cstr(io, "\n");
}

void sp_io_write_line(sp_io_stream_t* io, sp_str_t line) {
  sp_io_write_str(io, line);
  sp_io_write_new_line(io);
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
  toml_table_t* manifest;
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

toml_table_t*     spn_toml_parse(sp_str_t path);
sp_str_t          spn_toml_str(toml_table_t* toml, const c8* key);
sp_str_t          spn_toml_arr_str(toml_array_t* toml, u32 it);
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
void              spn_toml_append_str_carr(spn_toml_writer_t* writer, sp_str_t key, sp_str_t* values, u32 len);
void              spn_toml_append_str_carr_cstr(spn_toml_writer_t* writer, const c8* key, sp_str_t* values, u32 len);

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
  SPN_GENERATOR_DEFINE,
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

#define spn_semver_lit(major, minor, patch) (spn_semver_t) { major, minor, patch }

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

typedef enum {
  SPN_PACKAGE_STATE_UNLOADED,
  SPN_PACKAGE_STATE_LOADED,
} spn_package_state_t;

typedef struct {
  sp_str_t dir;
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
  sp_da(sp_str_t) source;
  sp_da(sp_str_t) include;
  sp_da(sp_str_t) define;
} spn_bin_t;

typedef enum {
  SPN_PROFILE_BUILTIN,
  SPN_PROFILE_USER,
} spn_profile_kind_t;

typedef struct {
  sp_str_t name;
  spn_lib_kind_t linkage;
  spn_libc_kind_t libc;
  spn_c_standard_t standard;
  spn_dep_mode_t mode;
  spn_profile_kind_t kind;
  struct {
    spn_cc_kind_t kind;
    sp_str_t exe;
  } cc;
} spn_profile_t;

typedef struct {
  sp_str_t name;
  spn_package_kind_t kind;
  union {
    spn_semver_range_t range;
    sp_str_t file;
  };
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
  spn_package_kind_t kind;
} spn_registry_t;

sp_str_t spn_registry_get_path(spn_registry_t* registry);

struct spn_package {
  sp_str_t name;
  sp_str_t repo;
  sp_str_t author;
  sp_str_t maintainer;
  spn_semver_t version;
  spn_lib_t lib;
  sp_ht(sp_str_t, spn_bin_t) bin;
  sp_ht(sp_str_t, spn_dep_req_t) deps;
  sp_ht(sp_str_t, spn_dep_option_t) options;
  sp_ht(sp_str_t, spn_dep_options_t) config;
  sp_ht(spn_semver_t, spn_metadata_t) metadata;
  sp_da(spn_semver_t) versions;
  sp_da(spn_registry_t) registries;
  sp_da(sp_str_t) include;
  sp_da(sp_str_t) define;
  sp_ht(sp_str_t, spn_profile_t) profiles;
  spn_package_kind_t kind;

  spn_dep_fn_t on_configure;
  spn_dep_fn_t on_build;
  spn_dep_fn_t on_package;

  spn_toml_package_t toml;
  spn_package_paths_t paths;
};

void          spn_package_init(spn_package_t* package);
void          spn_package_set_index(spn_package_t* package, sp_str_t path);
void          spn_package_add_version(spn_package_t* package, spn_semver_t version, sp_str_t commit);
spn_package_t spn_package_load(sp_str_t path);
spn_package_t spn_package_load_from_index(sp_str_t path);
spn_package_t spn_package_load_from_file(sp_str_t path);
void          spn_package_compile(spn_package_t* package);
void          spn_package_add_dep_from_index(spn_package_t* package, sp_str_t name);
void          spn_package_add_dep_from_manifest(spn_package_t* package, sp_str_t file_path);
void          spn_package_add_dep_request(spn_package_t* package, spn_dep_req_t request);

// Specific to a single build
typedef struct {
  sp_str_t log;
  sp_str_t stamp;
  sp_str_t source;
  sp_str_t work;
  sp_str_t store;
  sp_str_t include;
  sp_str_t lib;
  sp_str_t bin;
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
  spn_profile_t profile;
  spn_package_t* package;

  bool force;

  sp_da(sp_ps_config_t) commands;
  sp_ps_t ps;
  sp_io_stream_t log;
  spn_dep_state_t state;
  sp_thread_t thread;
  sp_mutex_t mutex;
  sp_str_t error;
};

spn_generator_kind_t   spn_gen_kind_from_str(sp_str_t str);
spn_gen_entry_kind_t   spn_gen_entry_from_str(sp_str_t str);
spn_cc_kind_t          spn_cc_kind_from_str(sp_str_t str);
spn_package_kind_t    spn_registry_kind_from_str(sp_str_t str);
sp_str_t               spn_gen_format_entry_for_compiler(sp_str_t entry, spn_gen_entry_kind_t kind, spn_cc_kind_t compiler);
sp_str_t               spn_cc_kind_to_executable(spn_cc_kind_t compiler);
sp_str_t               spn_cc_c_standard_to_switch(spn_c_standard_t standard);
sp_str_t               spn_cc_lib_kind_to_switch(spn_lib_kind_t kind);
sp_dyn_array(sp_str_t) spn_gen_build_entry_for_dep(spn_dep_context_t* dep, spn_gen_entry_kind_t kind, spn_cc_kind_t c);
sp_str_t               spn_gen_build_entries_for_dep(spn_dep_context_t* dep, spn_cc_kind_t c);
sp_str_t               spn_gen_build_entries_for_all(spn_gen_entry_kind_t kind, spn_cc_kind_t c);
sp_str_t               spn_print_system_deps_only(spn_cc_kind_t compiler);
sp_str_t               spn_print_deps_only(spn_gen_entry_kind_t kind, spn_cc_kind_t compiler);
spn_c_standard_t       spn_c_standard_from_str(sp_str_t str);
sp_str_t               spn_c_standard_to_str(spn_c_standard_t standard);
sp_str_t               spn_opt_cstr_required(const c8* value);
sp_str_t               spn_opt_cstr_optional(const c8* value, const c8* fallback);

spn_dir_kind_t   spn_cache_dir_kind_from_str(sp_str_t str);
sp_str_t         spn_cache_dir_kind_to_dep_path(spn_dep_context_t* dep, spn_dir_kind_t kind);
spn_dep_mode_t   spn_dep_build_mode_from_str(sp_str_t str);
sp_str_t         spn_dep_build_mode_to_str(spn_dep_mode_t mode);
spn_lib_kind_t   spn_lib_kind_from_str(sp_str_t str);
sp_str_t         spn_dep_build_kind_to_str(spn_lib_kind_t kind);
sp_str_t         spn_dep_state_to_str(spn_dep_state_t state);
bool             spn_dep_state_is_terminal(spn_dep_context_t* dep);
sp_os_lib_kind_t spn_lib_kind_to_sp_os_lib_kind(spn_lib_kind_t kind);
void             spn_dep_context_init(spn_dep_context_t* dep, spn_package_t* package);
s32              spn_dep_thread_resolve(void* user_data);
s32              spn_dep_thread_build(void* user_data);
void             spn_dep_context_finish(spn_dep_context_t* dep);
void             spn_dep_context_log_failure(spn_dep_context_t* dep);
s32              spn_dep_context_resolve(spn_dep_context_t* dep);
spn_err_t        spn_dep_context_sync_remote(spn_dep_context_t* dep);
spn_err_t        spn_dep_context_sync_local(spn_dep_context_t* dep);
void             spn_dep_context_stamp(spn_dep_context_t* dep);
void             spn_dep_context_run_build_script(spn_dep_context_t* dep);
sp_ps_output_t   spn_dep_context_subprocess(spn_dep_context_t* build, sp_ps_config_t config);
spn_err_t        spn_dep_context_build_binary(spn_dep_context_t* build, spn_bin_t bin);
spn_err_t        spn_dep_context_build_binaries(spn_dep_context_t* build);
s32              spn_dep_context_build_from_index(spn_dep_context_t* dep);
spn_err_t        spn_dep_context_build(spn_dep_context_t* dep);
spn_err_t        spn_dep_context_resolve_commit(spn_dep_context_t* dep);
bool             spn_dep_context_is_build_stamped(spn_dep_context_t* context);
void             spn_dep_context_set_build_state(spn_dep_context_t* dep, spn_dep_state_t state);
void             spn_dep_context_set_build_error(spn_dep_context_t* dep, sp_str_t error);
bool             spn_dep_context_is_binary(spn_dep_context_t* dep);
spn_lock_file_t  spn_load_lock_file(sp_str_t path);
spn_dep_req_t    spn_dep_req_from_str(sp_str_t str);
sp_str_t         spn_dep_req_to_str(spn_dep_req_t req);


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



////////////
// CONFIG //
////////////
typedef struct {
  sp_str_t dir;
  sp_str_t   manifest;
  sp_str_t   lock;
} spn_project_paths_t;

struct spn_config {
  sp_str_t spn;
  sp_da(spn_registry_t) registries;
};



typedef struct {
  sp_str_t dir;
  sp_str_t manifest;
  sp_str_t lock;
} spn_tools_paths_t;

typedef struct {
  sp_str_t dir;
  sp_str_t bin;
  sp_str_t lib;
} spn_tool_paths_t;

void spn_tool_ensure_manifest();
void spn_tool_install(sp_str_t name);
void spn_tool_list();
void spn_tool_run(sp_str_t package_name, sp_da(sp_str_t) args);
void spn_tool_upgrade(sp_str_t package_name);


// GEN
typedef struct {
  spn_gen_entry_kind_t kind;
  spn_cc_kind_t compiler;
} spn_gen_format_context_t;

sp_str_t spn_generator_format_entry_kernel(sp_str_map_context_t* context);

/////////
// TUI //
/////////
#define SPN_TUI_NUM_OPTIONS 3
#define SP_TUI_PRINT(command) printf("%s", command)

typedef struct {
  u32 std_in;
} sp_tui_checkpoint_t;

#define SPN_OUTPUT_MODE(X) \
  X(SPN_OUTPUT_MODE_INTERACTIVE) \
  X(SPN_OUTPUT_MODE_NONINTERACTIVE) \
  X(SPN_OUTPUT_MODE_QUIET) \
  X(SPN_OUTPUT_MODE_NONE)

typedef enum {
  SPN_OUTPUT_MODE(SP_X_ENUM_DEFINE)
} spn_tui_mode_t;

spn_tui_mode_t spn_output_mode_from_str(sp_str_t str);
sp_str_t       spn_output_mode_to_str(spn_tui_mode_t mode);

typedef enum {
  SP_TUI_TABLE_NONE,
  SP_TUI_TABLE_SETUP,      // Between begin_table and header_row
  SP_TUI_TABLE_BUILDING,   // Building rows
} sp_tui_table_state_e;

typedef struct {
  u32 row;
  u32 col;
} sp_tui_cursor_t;

typedef struct {
  sp_da(sp_str_t) names;               // Column names only
  sp_da(sp_da(sp_str_t)) rows;         // Array of rows, each row is array of cells
  sp_tui_cursor_t cursor;
  sp_tui_table_state_e state;
  u32 columns;                         // Column count
  u32 indent;                          // Indentation level for rendering
} sp_tui_table_t;

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

  sp_tui_table_t table;
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

void     sp_tui_begin_table(spn_tui_t* tui);
void     sp_tui_table_setup_column(spn_tui_t* tui, sp_str_t name);
void     sp_tui_table_header_row(spn_tui_t* tui);
void     sp_tui_table_next_row(spn_tui_t* tui);
void     sp_tui_table_column(spn_tui_t* tui, u32 n);
void     sp_tui_table_column_named(spn_tui_t* tui, sp_str_t name);
void     sp_tui_table_fmt(spn_tui_t* tui, const c8* fmt, ...);
void     sp_tui_table_str(spn_tui_t* tui, sp_str_t str);
void     sp_tui_table_set_indent(spn_tui_t* tui, u32 indent);
void     sp_tui_table_end(spn_tui_t* tui);
sp_str_t sp_tui_render(spn_tui_t* tui);


/////////
// CLI //
/////////
#define SPN_CLI_MAX_SUBCOMMANDS 16
#define SPN_CLI_MAX_ARGS 4
#define SPN_CLI_MAX_OPTS 8
#define SPN_CLI_NO_PLACEHOLDER SP_NULLPTR

#define SPN_CLI_COMMAND(X) \
  X(SPN_CLI_INIT, "init") \
  X(SPN_CLI_ADD, "add") \
  X(SPN_CLI_BUILD, "build") \
  X(SPN_CLI_PRINT, "print") \
  X(SPN_CLI_LINK, "link") \
  X(SPN_CLI_UPDATE, "update") \
  X(SPN_CLI_WHICH, "which") \
  X(SPN_CLI_LS, "ls") \
  X(SPN_CLI_MANIFEST, "manifest")

typedef enum {
  SPN_CLI_COMMAND(SP_X_NAMED_ENUM_DEFINE)
} spn_cli_cmd_t;


#define SPN_TOOL_SUBCOMMAND(X) \
  X(SPN_TOOL_INSTALL, "install") \
  X(SPN_TOOL_UNINSTALL, "uninstall") \
  X(SPN_TOOL_RUN, "run") \
  X(SPN_TOOL_LIST, "list") \
  X(SPN_TOOL_UPDATE, "update")

typedef enum {
  SPN_TOOL_SUBCOMMAND(SP_X_NAMED_ENUM_DEFINE)
} spn_tool_cmd_t;


#define SPN_CLI_ARG_KIND(X) \
  X(SPN_CLI_ARG_KIND_REQUIRED, "required") \
  X(SPN_CLI_ARG_KIND_OPTIONAL, "opional")

typedef enum {
  SPN_CLI_ARG_KIND(SP_X_NAMED_ENUM_DEFINE)
} spn_cli_arg_kind_t;

#define SPN_CLI_OPT_KIND(X) \
  X(SPN_CLI_OPT_KIND_BOOLEAN, "boolean") \
  X(SPN_CLI_OPT_KIND_STRING, "string") \
  X(SPN_CLI_OPT_KIND_INTEGER, "integer")

typedef enum {
  SPN_CLI_OPT_KIND(SP_X_NAMED_ENUM_DEFINE)
} spn_cli_opt_kind_t;

sp_str_t spn_cli_arg_kind_to_str(spn_cli_arg_kind_t kind);
spn_cli_arg_kind_t spn_cli_arg_kind_from_str(sp_str_t str);

typedef struct {
  const c8* name;
  spn_cli_arg_kind_t kind;
  const c8* summary;
  void* ptr;
} spn_cli_arg_usage_t;

typedef struct {
  const c8* brief;
  const c8* name;
  spn_cli_opt_kind_t kind;
  const c8* summary;
  const c8* placeholder;
  void* ptr;
} spn_cli_opt_usage_t;

typedef struct {
  const c8* name;
  spn_cli_opt_usage_t opts [SPN_CLI_MAX_OPTS];
  spn_cli_arg_usage_t args [SPN_CLI_MAX_ARGS];
  const c8* usage;
  const c8* summary;
} spn_cli_command_usage_t;

typedef struct {
  const c8* usage;
  const c8* summary;
  spn_cli_command_usage_t commands [SPN_CLI_MAX_SUBCOMMANDS];
} spn_cli_usage_t;

typedef struct {
  sp_str_t name;
  spn_cli_arg_kind_t kind;
  sp_str_t summary;
} spn_cli_arg_info_t;

typedef struct {
  sp_str_t brief;
  sp_str_t name;
  spn_cli_opt_kind_t kind;
  sp_str_t summary;
  sp_str_t placeholder;
} spn_cli_opt_info_t;

typedef struct {
  sp_str_t name;
  sp_str_t usage;
  sp_str_t summary;
  sp_da(spn_cli_opt_info_t) opts;
  sp_da(spn_cli_arg_info_t) args;
  sp_da(sp_str_t) brief;
} spn_cli_command_info_t;

typedef struct {
  struct {
    u32 name;
    u32 opts;
    u32 args;
  } width;
  sp_da(spn_cli_command_info_t) commands;
} spn_cli_usage_info_t;

typedef struct {
  c8** argv;
  u32 argc;
  spn_cli_command_usage_t cli;
  bool skip_help;
  bool stop_at_non_option;  // Stop parsing when we hit first non-option (for global options)
  u32 it;
  sp_str_t positionals[SPN_CLI_MAX_ARGS];
  u32 num_positionals;

  sp_str_t err;
} spn_cli_parser_t;

typedef struct {
  sp_str_t str;
  u32 it;
  bool found;
} spn_cli_str_parser_t;

typedef struct {
  sp_str_t name;
  sp_str_t value;
  bool has_value;
  bool found;
} spn_cli_named_opt_t;

void      spn_cli_print_help(spn_cli_parser_t* parser);
bool      spn_cli_parser_is_done(spn_cli_parser_t* p);
sp_str_t  spn_cli_parser_peek(spn_cli_parser_t* p);
void      spn_cli_parser_eat(spn_cli_parser_t* p);
bool      spn_cli_parser_is_opt(spn_cli_parser_t* p);
bool      spn_cli_str_parser_is_done(spn_cli_str_parser_t* p);
c8        spn_cli_str_parser_peek(spn_cli_str_parser_t* p);
void      spn_cli_str_parser_eat(spn_cli_str_parser_t* p);
sp_str_t  spn_cli_str_parser_rest(spn_cli_str_parser_t* p);
void      spn_cli_assign_bool(void* ptr, bool value);
void      spn_cli_assign_str(void* ptr, sp_str_t value);
void      spn_cli_assign_s64(void* ptr, s64 value);
void      spn_cli_assign(spn_cli_opt_usage_t opt, sp_str_t value);
spn_err_t spn_cli_parse_command(spn_cli_parser_t* p);
sp_str_t  spn_cli_usage(spn_cli_usage_t* cli);
sp_str_t spn_cli_command_usage(spn_cli_command_usage_t cmd);


typedef struct {
  sp_str_t package;
} spn_cli_add_t;

typedef struct {
  sp_str_t package;
} spn_cli_update_t;

typedef struct {
  const c8* command;
} spn_cli_tool_t;

typedef struct {
  bool bare;
} spn_cli_init_t;

typedef struct {
  bool force;
  sp_str_t target;
  sp_str_t profile;
  sp_str_t target_arg;
  sp_str_t profile_arg;
} spn_cli_build_t;

typedef struct {
  sp_str_t generator;
  sp_str_t compiler;
  sp_str_t path;
} spn_cli_print_t;

typedef struct {
  sp_str_t dir;
  sp_str_t package;
} spn_cli_which_t;

typedef struct {
  sp_str_t dir;
  sp_str_t package;
} spn_cli_ls_t;

typedef struct {
  sp_str_t package;
} spn_cli_manifest_t;

typedef struct {
  sp_str_t directory;
} spn_cli_copy_t;

typedef struct {
  u32 num_args;
  const c8** args;
  sp_str_t project_directory;
  sp_str_t project_file;
  sp_str_t output;
  bool help;

  spn_cli_add_t add;
  spn_cli_update_t update;
  spn_cli_tool_t tool;
  spn_cli_init_t init;
  spn_cli_print_t print;
  spn_cli_build_t build;
  spn_cli_ls_t ls;
  spn_cli_which_t which;
  spn_cli_manifest_t manifest;
  spn_cli_copy_t copy;
} spn_cli_t;

void spn_cli_assert_num_args(spn_cli_t* cli, u32 n, sp_str_t help);
sp_str_t spn_cli_get_arg(spn_cli_t* cli, u32 n);

void spn_cli_clean(spn_cli_t* cli);

void spn_cli_build(spn_cli_t* cli);
void spn_cli_print(spn_cli_t* cli);
void spn_cli_copy(spn_cli_t* cli);

void spn_app_update(sp_str_t name);
void spn_cli_init(spn_cli_t* cli);
void spn_cli_add(spn_cli_t* cli);
void spn_cli_update(spn_cli_t* cli);
void spn_cli_tool(spn_cli_t* cli);

void spn_cli_list(spn_cli_t* cli);
void spn_cli_ls(spn_cli_t* cli);
void spn_cli_which(spn_cli_t* cli);
void spn_cli_manifest(spn_cli_t* cli);



///////
// APP //
/////////
typedef struct {
  spn_package_t* package;
  spn_bin_t bin;
  sp_str_t profile;
  sp_thread_t thread;
} spn_compile_thread_ctx_t;

typedef struct {
  spn_tools_paths_t tools;
  sp_str_t work;
  sp_str_t executable;
  sp_str_t bin;
  sp_str_t storage;
  sp_str_t   config_dir;
  sp_str_t     config;
  sp_str_t   spn;
  sp_str_t     include;
  sp_str_t     index;
  sp_str_t   cache;
  sp_str_t     build;
  sp_str_t     store;
  sp_str_t     source;
} spn_paths_t;

typedef struct {
  spn_project_paths_t paths;
  spn_package_t package;
  sp_opt(spn_lock_file_t) lock;
  spn_profile_t profile;
  spn_resolver_t resolver;
  sp_da(sp_str_t) search;
  sp_ht(sp_str_t, spn_package_t) packages;
  sp_ht(sp_str_t, spn_dep_context_t) deps;
  sp_da(sp_str_t) system_deps;
  sp_ht(sp_str_t, spn_compile_thread_ctx_t) threads;
  sp_ht(sp_str_t, sp_str_t) index;
} spn_app_t;

typedef struct {
  sp_str_t manifest;
} spn_app_config_t;

typedef enum {
  SPN_APP_INIT_NORMAL,
  SPN_APP_INIT_BARE,
} spn_app_init_mode_t;

spn_app_t app;

spn_app_t      spn_app_load(spn_app_config_t config);
void           spn_app_resolve(spn_app_t* app);
void           spn_app_prepare_deps(spn_app_t* app);
void           spn_app_resolve_from_solver(spn_app_t* app);
void           spn_app_update_lock_file(spn_app_t* app);
void           spn_app_resolve_from_lock_file(spn_app_t* app);
void           spn_app_write_manifest(spn_package_t* package, sp_str_t path);
spn_package_t* spn_app_find_package(spn_app_t* app, spn_dep_req_t dep);
spn_package_t  spn_app_new(sp_str_t path, sp_str_t name, spn_app_init_mode_t mode);
s32            spn_app_thread_build_binary(void* user_data);
void           spn_app_add_package_constraints(spn_app_t* app, spn_package_t* package);
void           spn_resolver_init(spn_resolver_t* r);

void spn_cli_run();

typedef struct {
  spn_cli_t cli;
  spn_paths_t paths;
  spn_tui_t tui;
  sp_atomic_s32 control;
  sp_str_t tcc_error;
  spn_config_t config;
  spn_registry_t registry;
} spn_ctx_t;

spn_ctx_t spn;
void spn_init(u32 num_args, const c8** args);



////////////////////
// IMPLEMENTATION //
////////////////////

////////////
// LIBSPN //
////////////
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
  sp_da(sp_str_t) define;

  spn_cc_target_kind_t kind;
  union {
    spn_cc_executable_t exe;
    spn_cc_shared_lib_t shared_lib;
    spn_cc_static_lib_t static_lib;
  };
} spn_cc_target_t;

struct spn_cc {
  spn_dep_context_t* build;
  sp_da(sp_str_t) include;
  sp_da(sp_str_t) define;
  sp_str_t dir;
  sp_da(spn_cc_target_t) targets;
};

struct spn_make {
  spn_dep_context_t* build;
  sp_str_t target;
};

struct spn_autoconf {
  spn_dep_context_t* build;
  sp_da(sp_str_t) flags;
};

spn_cc_t         spn_cc_new(spn_dep_context_t* build);
void             spn_cc_add_include(spn_cc_t* cc, sp_str_t dir);
void             spn_cc_add_define(spn_cc_t* cc, sp_str_t var);
void             spn_cc_set_output_dir(spn_cc_t* cc, sp_str_t dir);
spn_cc_target_t* spn_cc_add_target(spn_cc_t* cc, spn_cc_target_kind_t kind, sp_str_t name);
void             spn_cc_target_add_source(spn_cc_target_t* cc, sp_str_t file_path);
void             spn_cc_target_add_define(spn_cc_target_t* cc, sp_str_t var);
spn_err_t        spn_cc_run(spn_cc_t* cc);

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
  APPLY(spn_autoconf_add_flag) \
  APPLY(spn_dep_log) \
  APPLY(spn_dep_get_libc) \
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
  make->build = dep;
  return make;
}

void spn_make_add_target(spn_make_t* make, const c8* target) {
  make->target = sp_str_from_cstr(target);
}

void spn_make_run(spn_make_t* make) {
  spn_dep_context_t* dep = make->build;

  sp_ps_config_t ps = SP_ZERO_INITIALIZE();
  ps.command = SP_LIT("make");
  sp_ps_config_add_arg(&ps, SP_LIT("--quiet"));
  sp_ps_config_add_arg(&ps, SP_LIT("--directory"));
  sp_ps_config_add_arg(&ps, dep->paths.work);
  if (!sp_str_empty(make->target)) {
    sp_ps_config_add_arg(&ps, make->target);
  }

  spn_dep_context_subprocess(dep, ps);
}

void spn_autoconf(spn_dep_context_t* build) {
  spn_autoconf_t* autoconf = spn_autoconf_new(build);
  spn_autoconf_run(autoconf);
}

spn_autoconf_t* spn_autoconf_new(spn_dep_context_t* dep) {
  spn_autoconf_t* autoconf = SP_ALLOC(spn_autoconf_t);
  autoconf->build = dep;
  return autoconf;
}

void spn_autoconf_run(spn_autoconf_t* autoconf) {
  spn_dep_context_t* build = autoconf->build;

  sp_ps_config_t config = {
    .command = sp_os_join_path(build->paths.source, SP_LIT("configure")),
    .args = {
      sp_format("--prefix={}", SP_FMT_STR(build->paths.store)),
      build->kind == SPN_LIB_KIND_SHARED ?
        SP_LIT("--enable-shared") :
        SP_LIT("--disable-shared"),
      build->kind == SPN_LIB_KIND_STATIC ?
        SP_LIT("--enable-static") :
        SP_LIT("--disable-static"),
    },
  };

  sp_da_for(autoconf->flags, it) {
    sp_ps_config_add_arg(&config, autoconf->flags[it]);
  }

  sp_ps_output_t result = spn_dep_context_subprocess(build, config);
}

void spn_autoconf_add_flag(spn_autoconf_t* autoconf, const c8* flag) {
  sp_da_push(autoconf->flags, sp_str_from_cstr(flag));
}

spn_cc_t spn_cc_new(spn_dep_context_t* build) {
  return (spn_cc_t) {
    .build = build,
  };
}

void spn_cc_add_include(spn_cc_t* cc, sp_str_t dir) {
  sp_dyn_array_push(cc->include, sp_os_join_path(app.paths.dir, dir));
}

void spn_cc_add_define(spn_cc_t* cc, sp_str_t var) {
  sp_dyn_array_push(cc->define, var);
}

void spn_cc_target_add_source(spn_cc_target_t* target, sp_str_t file_path) {
  sp_dyn_array_push(target->source, sp_os_join_path(app.paths.dir, file_path));
}

void spn_cc_target_add_include(spn_cc_target_t* target, sp_str_t dir) {
  sp_dyn_array_push(target->include, sp_os_join_path(app.paths.dir, dir));
}

void spn_cc_target_add_define(spn_cc_target_t* target, sp_str_t var) {
  sp_dyn_array_push(target->define, var);
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

spn_err_t spn_cc_run(spn_cc_t* cc) {
  sp_dyn_array_for(cc->targets, it) {
    spn_cc_target_t target = cc->targets[it];

    switch (target.kind) {
      case SPN_CC_TARGET_EXECUTABLE: {
        spn_profile_t* profile = &cc->build->profile;

        sp_ps_config_t cfg = SP_ZERO_INITIALIZE();
        cfg.command = sp_str_copy(profile->cc.exe);

        sp_dyn_array_for(target.source, it) {
          sp_ps_config_add_arg(&cfg, target.source[it]);
        }

        sp_da(sp_str_t) includes [] = { target.include, cc->include };
        sp_carr_for(includes, n) {
          sp_dyn_array_for(includes[n], it) {
            sp_ps_config_add_arg(&cfg, spn_gen_format_entry_for_compiler(includes[n][it], SPN_GENERATOR_INCLUDE, profile->cc.kind));
          }
        }

        sp_da(sp_str_t) defines [] = { target.define, cc->define };
        sp_carr_for(defines, n) {
          sp_dyn_array_for(defines[n], it) {
            sp_ps_config_add_arg(&cfg, spn_gen_format_entry_for_compiler(defines[n][it], SPN_GENERATOR_DEFINE, profile->cc.kind));
          }
        }

        sp_ht_for(app.deps, it) {
          spn_dep_context_t* dep = sp_ht_it_getp(app.deps, it);

          sp_dyn_array(sp_str_t) includes = spn_gen_build_entry_for_dep(dep, SPN_GENERATOR_INCLUDE, profile->cc.kind);
          sp_dyn_array_for(includes, i) {
            sp_ps_config_add_arg(&cfg, includes[i]);
          }

          sp_dyn_array(sp_str_t) lib_includes = spn_gen_build_entry_for_dep(dep, SPN_GENERATOR_LIB_INCLUDE, profile->cc.kind);
          sp_dyn_array_for(lib_includes, i) {
            sp_ps_config_add_arg(&cfg, lib_includes[i]);
          }

          sp_dyn_array(sp_str_t) libs = spn_gen_build_entry_for_dep(dep, SPN_GENERATOR_LIBS, profile->cc.kind);
          sp_dyn_array_for(libs, i) {
            sp_ps_config_add_arg(&cfg, libs[i]);
          }

          sp_dyn_array(sp_str_t) rpath = spn_gen_build_entry_for_dep(dep, SPN_GENERATOR_RPATH, profile->cc.kind);
          sp_dyn_array_for(rpath, i) {
            sp_ps_config_add_arg(&cfg, rpath[i]);
          }
        }

        sp_ps_config_add_arg(&cfg, spn_cc_c_standard_to_switch(profile->standard));

        if (profile->mode == SPN_DEP_BUILD_MODE_DEBUG) {
          sp_ps_config_add_arg(&cfg, sp_str_lit("-g"));
        }

        sp_ps_config_add_arg(&cfg, spn_cc_lib_kind_to_switch(profile->linkage));

        sp_ps_config_add_arg(&cfg, sp_str_lit("-o"));
        sp_ps_config_add_arg(&cfg, sp_os_join_path(cc->build->paths.bin, target.name));

        if (profile->linkage == SPN_LIB_KIND_STATIC) {
          sp_ps_config_add_arg(&cfg, sp_str_lit("-static"));
        }

        SP_LOG(
          "{:fg brightcyan} {:fg brightyellow}",
          SP_FMT_STR(cfg.command),
          SP_FMT_STR(sp_str_join_n(cfg.dyn_args, sp_dyn_array_size(cfg.dyn_args), sp_str_lit(" ")))
        );

        sp_ps_output_t result = spn_dep_context_subprocess(cc->build, cfg);
        if (result.status.exit_code) {
          return SPN_ERROR;
        }

        break;
      }
      default: {
        SP_UNREACHABLE_CASE();
      }
    }
  }

  return SPN_OK;
}

void spn_dep_log(spn_dep_context_t* dep, const c8* message) {
  sp_io_write_line(&dep->log, sp_tm_to_iso8601(sp_tm_now_epoch()));
  sp_io_write_line(&dep->log, sp_str_view(message));
}

spn_libc_kind_t spn_dep_get_libc(spn_dep_context_t* dep) {
  return dep->profile.libc;
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
spn_cli_arg_kind_t spn_cli_arg_kind_from_str(sp_str_t str) {
  SPN_CLI_ARG_KIND(SP_X_NAMED_ENUM_STR_TO_ENUM)
  SP_UNREACHABLE_RETURN(SPN_CLI_ARG_KIND_REQUIRED);
}

sp_str_t spn_cli_opt_kind_to_str(spn_cli_opt_kind_t kind) {
  switch (kind) {
    SPN_CLI_OPT_KIND(SP_X_NAMED_ENUM_CASE_TO_STRING_LOWER)
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

sp_str_t spn_cli_arg_kind_to_str(spn_cli_arg_kind_t kind) {
  switch (kind) {
    SPN_CLI_ARG_KIND(SP_X_NAMED_ENUM_CASE_TO_STRING_LOWER)
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_cli_cmd_t spn_cli_command_from_str(sp_str_t str) {
  SPN_CLI_COMMAND(SP_X_NAMED_ENUM_STR_TO_ENUM)
  SP_UNREACHABLE_RETURN(SPN_CLI_LS);
}

sp_str_t spn_cli_command_to_str(spn_cli_cmd_t cmd) {
  switch (cmd) {
    SPN_CLI_COMMAND(SP_X_NAMED_ENUM_CASE_TO_STRING_LOWER)
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_tool_cmd_t spn_tool_subcommand_from_str(sp_str_t str) {
  SPN_TOOL_SUBCOMMAND(SP_X_NAMED_ENUM_STR_TO_ENUM)
  SP_UNREACHABLE_RETURN(SPN_TOOL_LIST);
}

sp_str_t spn_tool_subcommand_to_str(spn_tool_cmd_t cmd) {
  switch (cmd) {
    SPN_TOOL_SUBCOMMAND(SP_X_NAMED_ENUM_CASE_TO_STRING_LOWER)
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

sp_str_t spn_output_mode_to_str(spn_tui_mode_t mode) {
  switch (mode) {
    SPN_OUTPUT_MODE(SP_X_ENUM_CASE_TO_STRING_LOWER)
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_tui_mode_t spn_output_mode_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, "interactive"))    return SPN_OUTPUT_MODE_INTERACTIVE;
  else if (sp_str_equal_cstr(str, "noninteractive")) return SPN_OUTPUT_MODE_NONINTERACTIVE;
  else if (sp_str_equal_cstr(str, "quiet"))          return SPN_OUTPUT_MODE_QUIET;
  else if (sp_str_equal_cstr(str, "none"))           return SPN_OUTPUT_MODE_NONE;

  SP_FATAL("Unknown output mode {:fg brightyellow}; options are [interactive, noninteractive, quiet, none]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_OUTPUT_MODE_NONE);
}

spn_libc_kind_t spn_libc_kind_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, "gnu"))          return SPN_LIBC_GNU;
  else if (sp_str_equal_cstr(str, "musl"))         return SPN_LIBC_MUSL;
  else if (sp_str_equal_cstr(str, "cosmopolitan")) return SPN_LIBC_COSMOPOLITAN;
  else                                             return SPN_LIBC_CUSTOM;
}

sp_str_t spn_libc_kind_to_str(spn_libc_kind_t libc) {
  switch (libc) {
    case SPN_LIBC_GNU:          return sp_str_lit("gnu");
    case SPN_LIBC_MUSL:         return sp_str_lit("musl");
    case SPN_LIBC_COSMOPOLITAN: return sp_str_lit("cosmopolitan");
    case SPN_LIBC_CUSTOM:       return sp_str_lit("custom");
  }
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
    case SPN_CC_TCC:      return sp_str_lit("tcc");
    case SPN_CC_GCC:      return sp_str_lit("gcc");
    case SPN_CC_MUSL_GCC: return sp_str_lit("musl-gcc");
    case SPN_CC_CUSTOM:   SP_FALLTHROUGH();
    case SPN_CC_NONE:     return sp_str_lit("gcc");
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
  else if (sp_str_equal_cstr(str, "tcc")) return SPN_CC_TCC;
  else if (sp_str_equal_cstr(str, "gcc")) return SPN_CC_GCC;
  else if (sp_str_equal_cstr(str, "musl-gcc")) return SPN_CC_MUSL_GCC;

  SP_WARN("Unknown compiler {:fg brightyellow}; we'll assume a gcc command line when generating switches", SP_FMT_STR(str));
  return SPN_CC_CUSTOM;
}

sp_str_t spn_c_standard_to_str(spn_c_standard_t standard) {
  switch (standard) {
    case SPN_C11: return sp_str_lit("c11");
    case SPN_C99: return sp_str_lit("c99");
    case SPN_C89: return sp_str_lit("c89");
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_c_standard_t spn_c_standard_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, "c89")) return SPN_C89;
  else if (sp_str_equal_cstr(str, "c99")) return SPN_C99;
  else if (sp_str_equal_cstr(str, "c11")) return SPN_C11;

  SP_FATAL("Unknown C standard {:fg brightyellow}; options are [c89, c99, c11]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_C99);
}

spn_package_kind_t spn_registry_kind_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, "workspace")) return SPN_PACKAGE_KIND_WORKSPACE;
  else if (sp_str_equal_cstr(str, "user"))      return SPN_PACKAGE_KIND_FILE;
  else if (sp_str_equal_cstr(str, "remote"))    return SPN_PACKAGE_KIND_REMOTE;

  SP_FATAL("Unknown registry kind {:fg brightyellow}; options are [workspace, user, remote]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_PACKAGE_KIND_WORKSPACE);
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
    case SPN_DIR_CACHE:   return spn.paths.cache;
    case SPN_DIR_STORE:   return spn.paths.store;
    case SPN_DIR_SOURCE:  return spn.paths.source;
    case SPN_DIR_WORK:    return spn.paths.work;
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
sp_str_t spn_generator_format_entry_kernel(sp_str_map_context_t* context) {
  spn_gen_format_context_t* format = (spn_gen_format_context_t*)context->user_data;
  return spn_gen_format_entry_for_compiler(context->str, format->kind, format->compiler);
}

sp_str_t spn_gen_format_entry_for_compiler(sp_str_t entry, spn_gen_entry_kind_t kind, spn_cc_kind_t compiler) {
  switch (compiler) {
    case SPN_CC_NONE: {
      return entry;
    }
    case SPN_CC_CUSTOM:
    case SPN_CC_TCC:
    case SPN_CC_MUSL_GCC:
    case SPN_CC_GCC: {
      switch (kind) {
        case SPN_GENERATOR_INCLUDE:     return sp_format("-I{}",          SP_FMT_STR(entry));
        case SPN_GENERATOR_LIB_INCLUDE: return sp_format("-L{}",          SP_FMT_STR(entry));
        case SPN_GENERATOR_LIBS:        return sp_format("{}",            SP_FMT_STR(entry));
        case SPN_GENERATOR_SYSTEM_LIBS: return sp_format("-l{}",          SP_FMT_STR(entry));
        case SPN_GENERATOR_RPATH:       return sp_format("-Wl,-rpath,{}", SP_FMT_STR(entry));
        case SPN_GENERATOR_DEFINE:      return sp_format("-D{}",          SP_FMT_STR(entry));
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
    sp_atomic_s32_set(&spn.control, 1);
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
  SP_ASSERT_FMT(value.ok, "missing string key: {:fg brightcyan}", SP_FMT_CSTR(key));
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

sp_da(sp_str_t) spn_toml_arr_to_str_arr(toml_array_t* toml) {
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

void spn_toml_append_str_carr(spn_toml_writer_t* writer, sp_str_t key, sp_str_t* values, u32 len) {
  spn_toml_ensure_header_written(writer);

  sp_str_builder_append_fmt(&writer->builder, "{} = [", SP_FMT_STR(key));

  for (u32 i = 0; i < len; i++) {
    sp_str_builder_append_fmt(&writer->builder, "{}", SP_FMT_QUOTED_STR(values[i]));
    if (i < len - 1) {
      sp_str_builder_append_cstr(&writer->builder, ", ");
    }
  }

  sp_str_builder_append_c8(&writer->builder, ']');
  sp_str_builder_new_line(&writer->builder);
}

void spn_toml_append_str_carr_cstr(spn_toml_writer_t* writer, const c8* key, sp_str_t* values, u32 len) {
  spn_toml_append_str_carr(writer, sp_str_view(key), values, len);
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
  tcc_add_include_path(tcc, sp_str_to_cstr(spn.paths.include));
  tcc_define_symbol(tcc, "SPN", "");
  spn_tcc_register(tcc);
  return tcc;
}

void spn_tcc_register(spn_tcc_t* tcc) {
  sp_carr_for(spn_lib, it) {
    tcc_add_symbol(tcc, spn_lib[it].symbol, spn_lib[it].fn);
  }
}

void spn_tcc_add_file(spn_tcc_t* tcc, sp_str_t file_path) {
  tcc_add_file(tcc, sp_str_to_cstr(file_path));
}

void spn_tcc_error(void* opaque, const char* message) {
  spn.tcc_error = sp_str_from_cstr(message);
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
    if (sp_atomic_s32_get(&spn.control) != 0) {
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

    spn_tui_update(&spn.tui);

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

  sp_ht_for(app.deps, it) {
    spn_dep_context_t* dep = sp_ht_it_getp(app.deps, it);
    spn_dep_context_log_failure(dep);
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
    case SPN_OUTPUT_MODE_QUIET:
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
    case SPN_OUTPUT_MODE_NONE: {
      break;
    }
  }
}

//////////////
// TUI TABLE //
//////////////

// Calculate visual width of string (excluding ANSI escape sequences)
u32 sp_str_visual_len(sp_str_t str) {
  u32 visual_len = 0;
  bool in_escape = false;

  for (u32 i = 0; i < str.len; i++) {
    if (str.data[i] == '\033') {
      in_escape = true;
    } else if (in_escape && str.data[i] == 'm') {
      in_escape = false;
    } else if (!in_escape) {
      visual_len++;
    }
  }

  return visual_len;
}

// Pad string to visual width (accounting for ANSI codes)
sp_str_t sp_str_visual_pad(sp_str_t str, u32 target_visual_width) {
  u32 current_visual_len = sp_str_visual_len(str);
  s32 delta = (s32)target_visual_width - (s32)current_visual_len;

  if (delta <= 0) return str;

  sp_str_builder_t builder = SP_ZERO_INITIALIZE();
  sp_str_builder_append(&builder, str);
  for (u32 i = 0; i < delta; i++) {
    sp_str_builder_append_c8(&builder, ' ');
  }

  return sp_str_builder_write(&builder);
}

void sp_tui_begin_table(spn_tui_t* tui) {
  SP_ASSERT(tui->table.state == SP_TUI_TABLE_NONE);

  tui->table.names = SP_NULLPTR;
  tui->table.rows = SP_NULLPTR;
  tui->table.cursor = (sp_tui_cursor_t) { .row = 0, .col = 0 };
  tui->table.state = SP_TUI_TABLE_SETUP;
  tui->table.columns = 0;
  tui->table.indent = 0;
}

void sp_tui_table_setup_column(spn_tui_t* tui, sp_str_t name) {
  SP_ASSERT(tui->table.state == SP_TUI_TABLE_SETUP);
  sp_dyn_array_push(tui->table.names, name);
  tui->table.columns++;
}

void sp_tui_table_header_row(spn_tui_t* tui) {
  SP_ASSERT(tui->table.state == SP_TUI_TABLE_SETUP);
  SP_ASSERT(tui->table.columns > 0);
  tui->table.state = SP_TUI_TABLE_BUILDING;
  tui->table.cursor.row = 0;
  tui->table.cursor.col = 0;
}

void sp_tui_table_next_row(spn_tui_t* tui) {
  SP_ASSERT(tui->table.state == SP_TUI_TABLE_BUILDING);

  // Set cursor to the next row index (which is current array size)
  tui->table.cursor.row = sp_dyn_array_size(tui->table.rows);

  // Add new empty row
  sp_da(sp_str_t) new_row = SP_NULLPTR;
  sp_dyn_array_push(tui->table.rows, new_row);

  tui->table.cursor.col = 0;
}

void sp_tui_table_column(spn_tui_t* tui, u32 n) {
  SP_ASSERT(tui->table.state == SP_TUI_TABLE_BUILDING);
  SP_ASSERT(n < tui->table.columns);
  tui->table.cursor.col = n;
}

void sp_tui_table_column_named(spn_tui_t* tui, sp_str_t name) {
  SP_ASSERT(tui->table.state == SP_TUI_TABLE_BUILDING);

  sp_dyn_array_for(tui->table.names, i) {
    if (sp_str_equal(tui->table.names[i], name)) {
      tui->table.cursor.col = i;
      return;
    }
  }

  SP_ASSERT(false && "Column name not found");
}

void sp_tui_table_str(spn_tui_t* tui, sp_str_t str) {
  SP_ASSERT(tui->table.state == SP_TUI_TABLE_BUILDING);
  SP_ASSERT(tui->table.cursor.col < tui->table.columns);
  SP_ASSERT(tui->table.cursor.row < sp_dyn_array_size(tui->table.rows));

  // Get current row (was created in sp_tui_table_next_row)
  sp_da(sp_str_t)* row = &tui->table.rows[tui->table.cursor.row];

  // Ensure row has enough cells
  while (sp_dyn_array_size(*row) <= tui->table.cursor.col) {
    sp_dyn_array_push(*row, sp_str_lit(""));
  }

  // Set the cell
  (*row)[tui->table.cursor.col] = str;
  tui->table.cursor.col++;
}

void sp_tui_table_fmt(spn_tui_t* tui, const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  sp_str_t str = sp_format_v(SP_CSTR(fmt), args);
  va_end(args);

  sp_tui_table_str(tui, str);
}

void sp_tui_table_set_indent(spn_tui_t* tui, u32 indent) {
  tui->table.indent = indent;
}

void sp_tui_table_end(spn_tui_t* tui) {
  SP_ASSERT(tui->table.state == SP_TUI_TABLE_BUILDING);
  tui->table.state = SP_TUI_TABLE_NONE;
}

static void sp_tui_apply_indent(sp_str_builder_t* builder, u32 indent) {
  for (u32 i = 0; i < indent; i++) {
    sp_str_builder_append_c8(builder, ' ');
    sp_str_builder_append_c8(builder, ' ');
  }
}

sp_str_t sp_tui_render(spn_tui_t* tui) {
  SP_ASSERT(tui->table.state == SP_TUI_TABLE_NONE);

  if (tui->table.columns == 0) {
    return sp_str_lit("");
  }

  // Calculate column widths based on visual width (excluding ANSI codes)
  sp_da(u32) widths = SP_NULLPTR;
  for (u32 col = 0; col < tui->table.columns; col++) {
    u32 max_width = 0;

    sp_dyn_array_for(tui->table.rows, row_idx) {
      sp_da(sp_str_t)* row = &tui->table.rows[row_idx];
      if (col < sp_dyn_array_size(*row)) {
        max_width = SP_MAX(max_width, sp_str_visual_len((*row)[col]));
      }
    }

    sp_dyn_array_push(widths, max_width);
  }

  // Build output
  sp_str_builder_t builder = SP_ZERO_INITIALIZE();

  // Render rows
  sp_dyn_array_for(tui->table.rows, row_idx) {
    sp_da(sp_str_t)* row = &tui->table.rows[row_idx];

    // Apply indentation for each row
    sp_tui_apply_indent(&builder, tui->table.indent);

    for (u32 col = 0; col < tui->table.columns; col++) {
      sp_str_t cell = (col < sp_dyn_array_size(*row)) ? (*row)[col] : sp_str_lit("");
      sp_str_t padded = sp_str_visual_pad(cell, widths[col]);

      sp_str_builder_append_fmt(&builder, "{}", SP_FMT_STR(padded));

      if (col < tui->table.columns - 1) {
        sp_str_builder_append_c8(&builder, ' ');
      }
    }
    sp_str_builder_new_line(&builder);
  }

  return sp_str_builder_move(&builder);
}

void spn_lock_file_init(spn_lock_file_t* lock) {
  sp_ht_set_fns(lock->entries, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
}

spn_lock_file_t spn_build_lock_file() {
  spn_lock_file_t lock = SP_ZERO_INITIALIZE();
  spn_lock_file_init(&lock);

  // Add an entry for each dep
  sp_ht_for(app.package.deps, it) {
    spn_dep_req_t request = *sp_ht_it_getp(app.package.deps, it);
    spn_dep_context_t* dep = sp_ht_getp(app.deps, request.name);
    spn_package_t* package = spn_app_find_package(&app, request);

    spn_lock_entry_t entry = {
      .name = sp_str_copy(dep->name),
      .version = dep->metadata.version,
      .commit = dep->metadata.commit,
      .import_kind = sp_ht_key_exists(app.package.deps, package->name),
    };

    sp_ht_for(package->deps, n) {
      spn_dep_req_t* request = sp_ht_it_getp(package->deps, n);
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

spn_lock_file_t spn_load_lock_file(sp_str_t path) {
  spn_lock_file_t lock = SP_ZERO_INITIALIZE();
  spn_lock_file_init(&lock);

  SP_ASSERT(sp_os_does_path_exist(path));

  toml_table_t* root = spn_toml_parse(path);
  SP_ASSERT(root);

  toml_array_t* packages = toml_table_array(root, "package");
  if (!packages) return lock;

  spn_toml_arr_for(packages, it) {
    toml_table_t* pkg = toml_array_table(packages, it);
    SP_ASSERT(pkg);

    spn_lock_entry_t entry = {
      .name = spn_toml_str(pkg, "name"),
      .version = spn_semver_from_str(spn_toml_str(pkg, "version")),
      .commit = spn_toml_str(pkg, "commit"),
      .deps = spn_toml_arr_to_str_arr(toml_table_array(pkg, "deps")),
    };
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
  if (!sp_os_does_path_exist(dep->paths.source)) {
    spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_CLONING);

    sp_str_t url = sp_format("https://github.com/{}.git", SP_FMT_STR(dep->package->repo));
    if (spn_git_clone(url, dep->paths.source)) {
      spn_dep_context_set_build_error(dep, sp_format(
        "Failed to clone {:fg brightcyan}",
        SP_FMT_STR(dep->package->name)
      ));

      return SPN_ERROR;
    }

    SP_ASSERT(sp_os_is_directory(dep->paths.source));
  }
  else {
    spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_FETCHING);
    if (spn_git_fetch(dep->paths.source)) {
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

  sp_str_t message = spn_git_get_commit_message(dep->paths.source, dep->metadata.commit);
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

  return spn_git_checkout(dep->paths.source, dep->metadata.commit);
}

void spn_dep_context_stamp(spn_dep_context_t* dep) {
  spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_STAMPING);
  sp_io_stream_t io = sp_io_from_file(dep->paths.stamp, SP_IO_MODE_WRITE);

  spn_toml_writer_t writer = spn_toml_writer_new();
  spn_toml_begin_table_cstr(&writer, "package");
  spn_toml_append_str_cstr(&writer, "name", dep->package->name);
  spn_toml_append_str_cstr(&writer, "version", spn_semver_to_str(dep->metadata.version));
  spn_toml_append_str_cstr(&writer, "commit", dep->metadata.commit);
  spn_toml_append_str_cstr(&writer, "source", dep->paths.source);
  spn_toml_append_str_cstr(&writer, "work", dep->paths.work);
  spn_toml_end_table(&writer);

  spn_toml_begin_table_cstr(&writer, "profile");
  spn_toml_append_str_cstr(&writer, "name", dep->profile.name);
  spn_toml_append_str_cstr(&writer, "cc", dep->profile.cc.exe);
  spn_toml_append_str_cstr(&writer, "linkage", spn_dep_build_kind_to_str(dep->profile.linkage));
  spn_toml_append_str_cstr(&writer, "libc", spn_libc_kind_to_str(dep->profile.libc));
  spn_toml_append_str_cstr(&writer, "mode", spn_dep_build_mode_to_str(dep->profile.mode));
  spn_toml_append_str_cstr(&writer, "standard", spn_c_standard_to_str(dep->profile.standard));
  spn_toml_end_table(&writer);

  spn_toml_begin_array_cstr(&writer, "command");
  sp_da_for(dep->commands, it) {
    sp_ps_config_t command = dep->commands[it];
    spn_toml_append_array_table(&writer);
    spn_toml_append_str(&writer, sp_str_lit("command"), command.command);

    // Add arguments as an array if there are any
    u32 num_args = sp_carr_len_nt(command.args);
    if (num_args) {
      spn_toml_append_str_carr_cstr(&writer, "args", command.args, num_args);
    }

    if (!sp_ht_empty(command.env.env.vars) || sp_carr_len_nt(command.env.extra) > 0) {
      spn_toml_begin_table_cstr(&writer, "env");

      sp_ht_for(command.env.env.vars, it) {
        sp_str_t key = *sp_ht_it_getkp(command.env.env.vars, it);
        sp_str_t val = *sp_ht_it_getp(command.env.env.vars, it);
        spn_toml_append_str(&writer, key, val);
      }

      sp_carr_for(command.env.extra, it) {
        sp_env_var_t var = command.env.extra[it];
        if (sp_str_empty(var.key)) break;
        spn_toml_append_str(&writer, var.key, var.value);
      }

      spn_toml_end_table(&writer);
    }
  }

  spn_toml_end_array(&writer);

  sp_io_write_str(&io, spn_toml_writer_write(&writer));
  sp_io_close(&io);
}

void spn_dep_context_run_build_script(spn_dep_context_t* dep) {
  spn_package_compile(dep->package);

  if (dep->package->on_build) {
    spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_BUILDING);
    dep->package->on_build(dep);
  }

  if (dep->package->on_package) {
    spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_PACKAGING);
    dep->package->on_package(dep);
  }
}

sp_ps_output_t spn_dep_context_subprocess(spn_dep_context_t* build, sp_ps_config_t config) {
  config.io = (sp_ps_io_config_t) {
    .in = { .mode = SP_PS_IO_MODE_NULL },
    .out = { .mode = SP_PS_IO_MODE_EXISTING, .stream = build->log },
    .err = { .mode = SP_PS_IO_MODE_REDIRECT }
  };
  config.cwd = build->paths.work;

  u32 it = sp_carr_len_nt(config.env.extra);
  config.env.extra[it] = (sp_env_var_t) {
    .key = sp_str_lit("CC"),
    .value = build->profile.cc.exe
  };

  sp_da_push(build->commands, sp_ps_config_copy(&config));

  sp_ps_t ps = sp_ps_create(config);
  return sp_ps_output(&ps);
}

s32 spn_dep_context_build_from_index(spn_dep_context_t* dep) {
  SP_ASSERT(!spn_dep_context_sync_remote(dep));
  SP_ASSERT(!spn_dep_context_resolve_commit(dep));
  SP_ASSERT(!spn_dep_context_sync_local(dep));

  if (spn_dep_context_is_build_stamped(dep)) {
    if (!dep->force) {
      return 0;
    }
  }

  spn_dep_context_build_binaries(dep);
  spn_dep_context_run_build_script(dep);
  spn_dep_context_stamp(dep);

  return SPN_OK;
}

spn_err_t spn_dep_context_build(spn_dep_context_t* dep) {
  spn_err_t result = SPN_OK;

  switch (dep->package->kind) {
    case SPN_PACKAGE_KIND_INDEX: {
      SP_ASSERT(!spn_dep_context_build_from_index(dep));
      break;
    }
    case SPN_PACKAGE_KIND_FILE: {
      result = spn_dep_context_build_binaries(dep);
      if (result) {
        goto done;
      }

      spn_dep_context_run_build_script(dep);
      break;
    }
    case SPN_PACKAGE_KIND_WORKSPACE:
    case SPN_PACKAGE_KIND_REMOTE: {
      SP_BROKEN();
      break;
    }
  }

done:
  spn_dep_context_finish(dep);
  return result;
}

void spn_dep_context_finish(spn_dep_context_t* dep) {
  if (dep->log.file.fd) {
    sp_io_close(&dep->log);
  }
}

void spn_dep_context_log_failure(spn_dep_context_t* dep) {
  sp_str_builder_t builder = SP_ZERO_INITIALIZE();

  switch (dep->state) {
    case SPN_DEP_BUILD_STATE_NONE:
    case SPN_DEP_BUILD_STATE_DONE: {
      break;
    }
    case SPN_DEP_BUILD_STATE_FAILED:
    default: {
      sp_str_builder_new_line(&builder);
      sp_str_builder_append_fmt(&builder, "> {:fg brightyellow}", SP_FMT_STR(dep->package->name));
      sp_str_builder_new_line(&builder);
      sp_str_builder_append_fmt(&builder, "{:fg brightblack}", SP_FMT_STR(sp_io_read_file(dep->paths.log)));
      sp_log(sp_str_builder_write(&builder));

      break;
    }
  }
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

s32 spn_dep_thread_resolve(void* user_data) {
  spn_dep_context_t* dep = (spn_dep_context_t*)user_data;
  SP_ASSERT(!spn_dep_context_sync_remote(dep));
  SP_ASSERT(!spn_dep_context_resolve_commit(dep));
  spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_DONE);
  return 0;
}

s32 spn_dep_thread_build(void* user_data) {
  spn_dep_context_t* dep = (spn_dep_context_t*)user_data;
  spn_dep_context_build(dep);
  spn_dep_context_set_build_state(dep, SPN_DEP_BUILD_STATE_DONE);

  return 0;
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
    SP_FATAL("failed to parse version: {:fg brightred}", SP_FMT_QSTR(str));
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

void spn_package_compile(spn_package_t* package) {
  if (!sp_os_does_path_exist(package->paths.script)) return;

  spn_tcc_t* tcc = spn_tcc_new();
  spn_tcc_add_file(tcc, package->paths.script);
  tcc_set_options(tcc, "-nostdlib");
  s32 v = tcc_relocate(tcc);
  sp_str_t e = spn.tcc_error;
  package->on_package = tcc_get_symbol(tcc, "package");
  package->on_build = tcc_get_symbol(tcc, "build");
}

void spn_package_init(spn_package_t* package) {
  sp_ht_set_fns(package->bin, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(package->deps, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(package->options, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(package->config, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(package->profiles, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
}

void spn_package_set_index(spn_package_t* package, sp_str_t path) {
  package->kind = SPN_PACKAGE_KIND_INDEX;
  package->paths.dir = sp_str_copy(path);
  package->paths.manifest = sp_os_join_path(package->paths.dir, sp_str_lit("spn.toml"));
  package->paths.metadata = sp_os_join_path(package->paths.dir, sp_str_lit("metadata.toml"));
  package->paths.script = sp_os_join_path(package->paths.dir, sp_str_lit("spn.c"));
}

void spn_package_set_file(spn_package_t* package, sp_str_t path) {
  package->kind = SPN_PACKAGE_KIND_FILE;
  package->paths.dir = sp_os_parent_path(path);
  package->paths.manifest = sp_str_copy(path);
  package->paths.metadata = sp_os_join_path(package->paths.dir, sp_str_lit("metadata.toml"));
  package->paths.script = sp_os_join_path(package->paths.dir, sp_str_lit("spn.c"));
}

void spn_package_add_version(spn_package_t* package, spn_semver_t version, sp_str_t commit) {
  spn_metadata_t metadata = {
    .version = version,
    .commit = sp_str_copy(commit)
  };

  sp_ht_insert(package->metadata, version, metadata);
  sp_dyn_array_push(package->versions, version);
}

spn_package_t spn_package_from_default(sp_str_t name) {
  spn_package_t package = SP_ZERO_INITIALIZE();
  spn_package_init(&package);
  package.name = sp_str_copy(name);
  package.version = spn_semver_lit(0, 1, 0);
  sp_dyn_array_push(package.versions, package.version);
  return package;
}

spn_package_t spn_package_load_from_index(sp_str_t path) {
  sp_str_t manifest = sp_os_join_path(path, sp_str_lit("spn.toml"));
  SP_ASSERT(sp_os_does_path_exist(manifest));

  spn_package_t package = spn_package_load(manifest);
  spn_package_set_index(&package, path);

  toml_table_t* metadata = spn_toml_parse(package.paths.metadata);
  if (metadata) {
    toml_array_t* versions = toml_table_array(metadata, "versions");

    const c8* key = SP_NULLPTR;
    spn_toml_arr_for(versions, it) {
      toml_table_t* entry = toml_array_table(versions, it);

      spn_semver_t version = spn_semver_from_str(spn_toml_str(entry, "version"));
      spn_package_add_version(&package, version, spn_toml_str(entry, "commit"));
    }

    sp_dyn_array_sort(package.versions, spn_semver_sort_kernel);
  }

  return package;
}

spn_package_t spn_package_load_from_file(sp_str_t manifest) {
  SP_ASSERT(sp_os_does_path_exist(manifest));

  spn_package_t package = spn_package_load(manifest);
  spn_package_set_file(&package, manifest);
  return package;
}

spn_package_t spn_package_load(sp_str_t manifest_path) {
  spn_package_t package = SP_ZERO_INITIALIZE();
  spn_package_init(&package);

  spn_toml_package_t toml = SP_ZERO_INITIALIZE();
  toml.manifest = spn_toml_parse(manifest_path);
  toml.package = toml_table_table(toml.manifest, "package");
  toml.lib = toml_table_table(toml.manifest, "lib");
  toml.bin = toml_table_array(toml.manifest, "bin");
  toml.profile = toml_table_array(toml.manifest, "profile");
  toml.registry = toml_table_array(toml.manifest, "registry");
  toml.deps = toml_table_table(toml.manifest, "deps");
  toml.options = toml_table_table(toml.manifest, "options");
  toml.config = toml_table_table(toml.manifest, "config");

  package.toml = toml;
  package.name = spn_toml_str(toml.package, "name");
  package.repo = spn_toml_str_opt(toml.package, "repo", "");
  package.author = spn_toml_str_opt(toml.package, "author", "");
  package.maintainer = spn_toml_str_opt(toml.package, "maintainer", "");

  sp_str_t commit = spn_toml_str_opt(toml.package, "commit", "");
  package.version = spn_semver_from_str(spn_toml_str(toml.package, "version"));
  spn_package_add_version(&package, package.version, commit);

  if (toml.package) {
    toml_array_t* include = toml_table_array(toml.package, "include");
    if (include) {
      package.include = spn_toml_arr_to_str_arr(include);
    }

    toml_array_t* define = toml_table_array(toml.package, "define");
    if (define) {
      package.define = spn_toml_arr_to_str_arr(define);
    }
  }

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

      profile.name = spn_toml_str(it, "name");
      profile.cc.exe = spn_toml_str_opt(it, "cc", "gcc");
      profile.cc.kind = spn_cc_kind_from_str(profile.cc.exe);
      profile.linkage = spn_lib_kind_from_str(spn_toml_str_opt(it, "linkage", "shared"));
      profile.libc = spn_libc_kind_from_str(spn_toml_str_opt(it, "libc", "gnu"));
      profile.standard = spn_c_standard_from_str(spn_toml_str_opt(it, "standard", "c99"));
      profile.mode = spn_dep_build_mode_from_str(spn_toml_str_opt(it, "mode", "debug"));
      profile.kind = SPN_PROFILE_USER;

      sp_ht_insert(package.profiles, profile.name, profile);
    }
  }

  if (toml.registry) {
    spn_toml_arr_for(toml.registry, n) {
      toml_table_t* it = toml_array_table(toml.registry, n);

      spn_registry_t registry = {
        .name = spn_toml_str_opt(it, "name", ""),
        .location = spn_toml_str(it, "location"),
        .kind = SPN_PACKAGE_KIND_WORKSPACE,
      };

      sp_dyn_array_push(package.registries, registry);
    }
  }

  if (toml.bin) {
    spn_toml_arr_for(toml.bin, n) {
      toml_table_t* it = toml_array_table(toml.bin, n);
      spn_bin_t bin = SP_ZERO_INITIALIZE();
      bin.name = spn_toml_str(it, "name");
      bin.source = spn_toml_arr_to_str_arr(toml_table_array(it, "source"));
      bin.include = spn_toml_arr_to_str_arr(toml_table_array(it, "include"));
      bin.define = spn_toml_arr_to_str_arr(toml_table_array(it, "define"));

      sp_ht_insert(package.bin, bin.name, bin);
    }
  }

  if (toml.deps) {
    const c8* key = SP_NULLPTR;
    spn_toml_for(toml.deps, n, key) {
      spn_dep_req_t dep = spn_dep_req_from_str(spn_toml_str(toml.deps, key));
      dep.name = sp_str_from_cstr(key);

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

  return package;
}

void spn_resolver_init(spn_resolver_t* resolver) {
  sp_ht_set_fns(resolver->ranges, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(resolver->versions, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(resolver->visited, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
}

void spn_app_add_package_constraints(spn_app_t* app, spn_package_t* package) {
  spn_resolver_t* resolver = &app->resolver;

  if (sp_ht_key_exists(resolver->visited, package->name)) {
    // @spader keep a stack to provide a real error message
    SP_FATAL("{:fg brightcyan} transitively includes itself", SP_FMT_STR(package->name));
  }

  // mark as visiting; until we finish this subtree, we can't see this package again (no circular deps)
  sp_ht_insert(resolver->visited, package->name, true);

  sp_ht_for(package->deps, it) {
    spn_dep_req_t request = *sp_ht_it_getp(package->deps, it);
    spn_package_t* dep = spn_app_find_package(app, request);

    if (!sp_ht_key_exists(resolver->ranges, dep->name)) {
      sp_ht_insert(resolver->ranges, dep->name, SP_NULLPTR);
    }
    sp_da(spn_dep_version_range_t)* ranges = sp_ht_getp(resolver->ranges, dep->name);

    // collect the range of versions which satisfy the request
    spn_dep_version_range_t range = {
      .source = request
    };

    switch (request.kind) {
      case SPN_PACKAGE_KIND_FILE: {
        u32 num_versions = sp_dyn_array_size(dep->versions);
        if (num_versions != 1) {
          SP_FATAL(
            "Local dependency {:fg brightcyan} has {} versions",
            SP_FMT_STR(dep->name),
            SP_FMT_U32(num_versions)
          );
        }
        sp_opt_set(range.low, 0);
        sp_opt_set(range.high, 0);

        break;
      }
      case SPN_PACKAGE_KIND_INDEX: {
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

        break;
      }
      case SPN_PACKAGE_KIND_REMOTE:
      case SPN_PACKAGE_KIND_WORKSPACE: {
        SP_BROKEN();
        break;
      }
    }

    sp_dyn_array_push(*ranges, range);
  }

  sp_ht_for(package->deps, it) {
    spn_dep_req_t request = *sp_ht_it_getp(package->deps, it);
    spn_package_t* dep = spn_app_find_package(app, request);
    spn_app_add_package_constraints(app, dep);
  }

  sp_ht_erase(resolver->visited, package->name);
}

void spn_app_resolve_from_lock_file(spn_app_t* app) {
  spn_resolver_init(&app->resolver);
  SP_ASSERT(app->lock.some);

  spn_lock_file_t* lock = &app->lock.value;
  sp_ht_for(lock->entries, it) {
    spn_lock_entry_t* entry = sp_ht_it_getp(lock->entries, it);
    sp_ht_insert(app->resolver.versions, entry->name, entry->version);
  }
}

void spn_app_resolve_from_solver(spn_app_t* app) {
  spn_resolver_init(&app->resolver);
  spn_app_add_package_constraints(app, &app->package);

  sp_ht_for(app->package.deps, it) {
    spn_dep_req_t request = *sp_ht_it_getp(app->package.deps, it);
    sp_str_t name = request.name;
    spn_package_t* dep = spn_app_find_package(app, request);

    sp_da(spn_dep_version_range_t) ranges = *sp_ht_it_getp(app->resolver.ranges, it);
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
    sp_ht_insert(app->resolver.versions, name, version);
    SP_LOG("{:fg brightcyan}: {}", SP_FMT_STR(name), SP_FMT_STR(spn_semver_to_str(version)));
  }
}

void spn_app_resolve(spn_app_t* app) {
  switch (app->lock.some) {
    case SP_OPT_SOME: {
      spn_app_resolve_from_lock_file(app);
      break;
    }
    case SP_OPT_NONE: {
      spn_app_resolve_from_solver(app);
      break;
    }
  }
}

void spn_dep_context_prepare(spn_dep_context_t* dep) {
  dep->paths.include = sp_os_join_path(dep->paths.store, SP_LIT("include"));
  dep->paths.bin = sp_os_join_path(dep->paths.store, SP_LIT("bin"));
  dep->paths.lib = sp_os_join_path(dep->paths.store, SP_LIT("lib"));
  dep->paths.vendor = sp_os_join_path(dep->paths.store, SP_LIT("vendor"));
  dep->paths.stamp = sp_os_join_path(dep->paths.store, SP_LIT("spn.stamp"));
  dep->paths.log = sp_os_join_path(dep->paths.work, SP_LIT("spn.log"));

  sp_os_create_directory(dep->paths.work);
  sp_os_create_directory(dep->paths.store);
  sp_os_create_directory(dep->paths.bin);
  sp_os_create_directory(dep->paths.include);
  sp_os_create_directory(dep->paths.lib);
  sp_os_create_directory(dep->paths.vendor);

  sp_os_create_file(dep->paths.log);
  dep->log = sp_io_from_file(dep->paths.log, SP_IO_MODE_WRITE);
  spn_dep_log(dep, "build");
}

spn_dep_context_t spn_app_prepare_project(spn_app_t* app) {
  spn_dep_context_t dep = {
    .name = app->package.name,
    .mode = SPN_DEP_BUILD_MODE_DEBUG,
    .package = &app->package,
    .profile = app->profile
  };

  sp_str_t build = sp_os_join_path(app->package.paths.dir, sp_str_lit("build"));
  sp_str_t profile = sp_os_join_path(build, dep.profile.name);
  dep.paths.store = sp_os_join_path(profile, sp_str_lit("store"));
  dep.paths.work = sp_os_join_path(dep.paths.store, sp_str_lit("work"));
  dep.paths.source = sp_str_copy(app->package.paths.dir);

  spn_dep_context_prepare(&dep);

  return dep;
}

void spn_app_prepare_deps(spn_app_t* app) {
  sp_ht_for(app->package.deps, it) {
    spn_dep_req_t request = *sp_ht_it_getp(app->package.deps, it);
    sp_str_t name = request.name;
    spn_semver_t version = *sp_ht_getp(app->resolver.versions, name);

    spn_package_t* package = spn_app_find_package(app, request);
    SP_ASSERT(package);

    spn_metadata_t* metadata = sp_ht_getp(package->metadata, version);
    SP_ASSERT(metadata);

    // add a new build context for this dep
    spn_dep_context_t dep = {
      .name = name,
      .mode = SPN_DEP_BUILD_MODE_DEBUG,
      .metadata = *metadata,
      .profile = app->profile,
    };

    spn_dep_options_t* options = sp_ht_getp(app->package.config, name);
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

    switch (request.kind) {
      case SPN_PACKAGE_KIND_INDEX: {
        sp_str_t work = sp_os_join_path(spn.paths.build, package->name);
        sp_str_t store = sp_os_join_path(spn.paths.store, package->name);

        dep.paths.work = sp_os_join_path(work, build_id);
        dep.paths.store = sp_os_join_path(store, build_id);
        dep.paths.source = sp_os_join_path(spn.paths.source, package->name);

        break;
      }
      case SPN_PACKAGE_KIND_FILE:
      case SPN_PACKAGE_KIND_WORKSPACE:
      case SPN_PACKAGE_KIND_REMOTE: {
        SP_FATAL("Tried to prepare {:fg brightcyan}, but kind was {:fg brightyellow}", SP_FMT_STR(dep.name), SP_FMT_STR(spn_dep_req_to_str(request)));
        SP_BROKEN();
        break;
      }
    }

    spn_dep_context_prepare(&dep);
    sp_ht_insert(app->deps, name, dep);
  }

  sp_ht_for(app->package.deps, it) {
    spn_dep_req_t request = *sp_ht_it_getp(app->package.deps, it);
    spn_dep_context_t* dep = sp_ht_getp(app->deps, request.name);
    dep->package = spn_app_find_package(app, request);
  }
}

void spn_app_update_lock_file(spn_app_t* app) {
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
  sp_io_stream_t file = sp_io_from_file(app->paths.lock, SP_IO_MODE_WRITE);
  sp_io_write_str(&file, output);
  sp_io_close(&file);
}

void spn_app_write_manifest(spn_package_t* package, sp_str_t path) {
  spn_toml_writer_t toml = spn_toml_writer_new();

  spn_toml_begin_table_cstr(&toml, "package");
  spn_toml_append_str_cstr(&toml, "name", package->name);
  spn_toml_append_str_cstr(&toml, "version", spn_semver_to_str(package->version));
  if (!sp_str_empty(package->repo)) {
    spn_toml_append_str_cstr(&toml, "repo", package->repo);
  }
  if (!sp_str_empty(package->author)) {
    spn_toml_append_str_cstr(&toml, "author", package->author);
  }
  if (!sp_str_empty(package->maintainer)) {
    spn_toml_append_str_cstr(&toml, "maintainer", package->maintainer);
  }
  if (!sp_dyn_array_empty(package->include)) {
    spn_toml_append_str_array_cstr(&toml, "include", package->include);
  }
  spn_toml_end_table(&toml);

  if (sp_ht_size(package->deps)) {
    spn_toml_begin_table_cstr(&toml, "deps");
    sp_ht_for(package->deps, it) {
      sp_str_t name = *sp_ht_it_getkp(package->deps, it);
      spn_dep_req_t req = *sp_ht_it_getp(package->deps, it);
      spn_toml_append_str(&toml, name, spn_dep_req_to_str(req));
    }
    spn_toml_end_table(&toml);
  }

  if (sp_ht_size(package->profiles)) {
    spn_toml_begin_array_cstr(&toml, "bin");
    sp_ht_for(package->profiles, it) {
      spn_profile_t profile = *sp_ht_it_getp(package->profiles, it);
      if (profile.kind != SPN_PROFILE_BUILTIN) {
        spn_toml_append_array_table(&toml);
        spn_toml_append_str_cstr(&toml, "name", profile.name);
        spn_toml_append_str_cstr(&toml, "cc", profile.cc.exe);
        spn_toml_append_str_cstr(&toml, "linkage", spn_dep_build_kind_to_str(profile.linkage));
        spn_toml_append_str_cstr(&toml, "libc", spn_libc_kind_to_str(profile.libc));
        spn_toml_append_str_cstr(&toml, "standard", spn_c_standard_to_str(profile.standard));
        spn_toml_append_str_cstr(&toml, "mode", spn_dep_build_mode_to_str(profile.mode));
      }
    }
    spn_toml_end_array(&toml);
  }

  if (sp_ht_size(package->lib.enabled)) {
    spn_toml_begin_table_cstr(&toml, "lib");
    sp_da(sp_str_t) kinds = SP_NULLPTR;
    sp_ht_for(package->lib.enabled, it) {
      spn_lib_kind_t* kind = sp_ht_it_getkp(package->lib.enabled, it);
      bool* enabled = sp_ht_it_getp(package->lib.enabled, it);
      if (*enabled) {
        sp_dyn_array_push(kinds, spn_dep_build_kind_to_str(*kind));
      }
    }
    if (sp_dyn_array_size(kinds)) {
      spn_toml_append_str_array_cstr(&toml, "kinds", kinds);
    }
    if (sp_str_valid(package->lib.name)) {
      spn_toml_append_str_cstr(&toml, "name", package->lib.name);
    }
    spn_toml_end_table(&toml);
  }

  if (sp_ht_size(package->bin)) {
    spn_toml_begin_array_cstr(&toml, "bin");
    sp_ht_for(package->bin, it) {
      spn_bin_t* bin = sp_ht_it_getp(package->bin, it);
      spn_toml_append_array_table(&toml);
      spn_toml_append_str_cstr(&toml, "name", bin->name);
      if (sp_dyn_array_size(bin->source)) {
        spn_toml_append_str_array_cstr(&toml, "source", bin->source);
      }
      if (sp_dyn_array_size(bin->include)) {
        spn_toml_append_str_array_cstr(&toml, "include", bin->include);
      }
      if (sp_dyn_array_size(bin->define)) {
        spn_toml_append_str_array_cstr(&toml, "include", bin->define);
      }
    }
    spn_toml_end_array(&toml);
  }

  if (sp_ht_size(package->options)) {
    spn_toml_begin_table_cstr(&toml, "options");
    sp_ht_for(package->options, it) {
      sp_str_t* key = sp_ht_it_getkp(package->options, it);
      spn_dep_option_t* option = sp_ht_it_getp(package->options, it);
      spn_toml_append_option(&toml, *key, *option);
    }
    spn_toml_end_table(&toml);
  }

  if (sp_ht_size(package->config)) {
    spn_toml_begin_table_cstr(&toml, "config");

    sp_ht_for(package->config, it) {
      sp_str_t name = *sp_ht_it_getkp(package->config, it);
      spn_dep_options_t* options = sp_ht_it_getp(package->config, it);

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

  if (sp_dyn_array_size(package->registries)) {
    spn_toml_begin_array_cstr(&toml, "registry");
    sp_dyn_array_for(package->registries, it) {
      spn_registry_t registry = package->registries[it];

      spn_toml_append_array_table(&toml);

      if (!sp_str_empty(registry.name)) {
        spn_toml_append_str_cstr(&toml, "name", registry.name);
      }
      spn_toml_append_str_cstr(&toml, "location", package->registries[it].location);
    }
    spn_toml_end_array(&toml);
  }

  sp_str_t output = spn_toml_writer_write(&toml);
  output = sp_str_trim_right(output);
  sp_io_stream_t file = sp_io_from_file(path, SP_IO_MODE_WRITE);
  sp_io_write_str(&file, output);
  sp_io_close(&file);
}

sp_str_t spn_registry_get_path(spn_registry_t* registry) {
  switch (registry->kind) {
    case SPN_PACKAGE_KIND_WORKSPACE: {
      return sp_os_join_path(app.paths.dir, registry->location);
    }
    case SPN_PACKAGE_KIND_INDEX: {
      return sp_str_copy(registry->location);
    }
    case SPN_PACKAGE_KIND_FILE:
    case SPN_PACKAGE_KIND_REMOTE: {
      SP_UNREACHABLE();
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_package_t* spn_app_find_package(spn_app_t* app, spn_dep_req_t request) {
  sp_str_t name = request.name;

  if (!sp_ht_key_exists(app->packages, name)) {
    switch (request.kind) {
      case SPN_PACKAGE_KIND_FILE: {
        sp_str_t prefix = sp_str_lit("file://");
        sp_str_t manifest = {
          .data = request.file.data + prefix.len,
          .len = request.file.len - prefix.len
        };
        spn_package_t package = spn_package_load_from_file(manifest);
        sp_ht_insert(app->packages, name, package);

        break;
      }
      case SPN_PACKAGE_KIND_INDEX: {
        sp_str_t* path = sp_ht_getp(app->index, name);
        if (!path) {
          SP_FATAL("{:fg brightcyan} was not found on search paths", SP_FMT_STR(name));
        }

        spn_package_t package = spn_package_load_from_index(*path);
        sp_ht_insert(app->packages, name, package);

        break;
      }
      case SPN_PACKAGE_KIND_REMOTE:
      case SPN_PACKAGE_KIND_WORKSPACE: {
        SP_FATAL("unimplemented find_package");
        break;
      }
    }
  }

  spn_package_t* package = sp_ht_getp(app->packages, name);
  SP_ASSERT(package->kind == request.kind);
  return package;
}


void spn_tool_install(sp_str_t id) {
  if (!sp_os_does_path_exist(spn.paths.tools.manifest)) {
    spn_package_t package = spn_app_new(spn.paths.tools.dir, sp_str_lit("spn_tools"), SPN_APP_INIT_BARE);
    spn_app_write_manifest(&package, spn.paths.tools.manifest);
  }

  spn_package_t package = spn_package_load(spn.paths.tools.manifest);

  sp_str_t dir = sp_os_join_path(spn.paths.work, id);
  sp_str_t manifest = sp_os_join_path(dir, sp_str_lit("spn.toml"));
  if (sp_os_does_path_exist(manifest)) {
    spn_package_add_dep_from_manifest(&package, manifest);
  }

  spn_app_write_manifest(&package, spn.paths.tools.manifest);
}

/////////
// APP //
/////////
spn_package_t spn_app_new(sp_str_t path, sp_str_t name, spn_app_init_mode_t mode) {
  spn_package_t package = spn_package_from_default(name);
  spn_package_set_file(&package, sp_os_join_path(path, sp_str_lit("spn.toml")));

  sp_str_t paths [] = {
    package.paths.manifest,
    package.paths.script
  };
  sp_carr_for(paths, it) {
    sp_str_t path = paths[it];

    if (sp_os_does_path_exist(path)) {
      SP_FATAL("{:fg brightcyan} already exists", SP_FMT_STR(path));
    }
  }

  switch (mode) {
    case SPN_APP_INIT_NORMAL: {
      // everyone gets sp.h
      spn_package_add_dep_from_index(&package, sp_str_lit("sp"));

      // and a default hello target
      spn_bin_t bin = {
        .name = sp_str_copy(package.name),
      };
      sp_dyn_array_push(bin.source, sp_str_lit("main.c"));
      sp_ht_insert(package.bin, bin.name, bin);

      // and main.c
      sp_str_t main = sp_os_join_path(path, sp_str_lit("main.c"));
      sp_io_stream_t io = sp_io_from_file(main, SP_IO_MODE_WRITE);

      sp_str_t content = sp_str_lit(
        "#define SP_IMPLEMENTATION\n"
        "#include \"sp.h\"\n"
        "\n"
        "s32 main(s32 num_args, const c8** args) {\n"
        "  SP_LOG(\"hello, {:fg brightcyan}\", SP_FMT_CSTR(\"world\"));\n"
        "  SP_EXIT_SUCCESS();\n"
        "}\n"
      );

      if (sp_io_write_str(&io, content) != content.len) {
        SP_FATAL("Failed to write {:fg brightyellow}", SP_FMT_STR(main));
      }

      sp_io_close(&io);

      break;
    }
    case SPN_APP_INIT_BARE: {
      break;
    }
  }

  spn_app_write_manifest(&package, package.paths.manifest);
  return package;
}

spn_app_t spn_app_load(spn_app_config_t config) {
  spn_app_t app = SP_ZERO_INITIALIZE();

  spn_install_signal_handlers();

  sp_ht_set_fns(app.packages, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(app.deps, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(app.threads, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(app.index, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);

  // Project
  app.paths.manifest = sp_str_copy(config.manifest);
  app.paths.dir = sp_os_parent_path(app.paths.manifest);
  app.paths.lock = sp_os_join_path(app.paths.dir, SP_LIT("spn.lock"));

  // Load the top level package
  if (sp_os_does_path_exist(app.paths.manifest)) {
    app.package = spn_package_load_from_file(app.paths.manifest);
  }

  // Now that we know all the registries, discover all packages
  sp_dyn_array_push(app.search, spn_registry_get_path(&spn.registry));

  sp_dyn_array_for(spn.config.registries, it) {
    spn_registry_t* registry = &spn.config.registries[it];
    sp_dyn_array_push(app.search, spn_registry_get_path(registry));
  }

  sp_dyn_array_for(app.package.registries, it) {
    spn_registry_t* registry = &app.package.registries[it];
    sp_dyn_array_push(app.search, spn_registry_get_path(registry));
  }

  sp_dyn_array_for(app.search, i) {
    sp_str_t path = app.search[i];
    if (!sp_os_does_path_exist(path)) continue;
    if (!sp_os_is_directory(path)) {
      SP_FATAL(
        "{:fg brightcyan} is on the search path, but it's not a directory",
        SP_FMT_STR(path)
      );
    }

    sp_da(sp_os_dir_ent_t) entries = sp_os_scan_directory(path);
    sp_dyn_array_for(entries, i) {
      sp_os_dir_ent_t entry = entries[i];
      sp_str_t stem = sp_os_extract_stem(entry.file_path);
      sp_ht_insert(app.index, stem, entry.file_path);
    }
  }

  // Load the lock file
  if (sp_os_does_path_exist(app.paths.lock)) {
    sp_opt_set(app.lock, spn_load_lock_file(app.paths.lock));
  }

  // The project is loaded; apply any defaults
  //


  if (!sp_ht_empty(app.package.profiles)) {
    sp_ht_for(app.package.profiles, it) {
      app.profile = *sp_ht_it_getp(app.package.profiles, it);
      break;
    }
  }
  else {
    spn_profile_t debug = {
      .name = sp_str_lit("debug"),
      .cc = { SPN_CC_GCC, sp_str_lit("gcc") },
      .linkage = SPN_LIB_KIND_SHARED,
      .libc = SPN_LIBC_GNU,
      .standard = SPN_C11,
      .mode = SPN_DEP_BUILD_MODE_DEBUG,
      .kind = SPN_PROFILE_BUILTIN,
    };
    sp_ht_insert(app.package.profiles, debug.name, debug);

    spn_profile_t release = {
      .name = sp_str_lit("release"),
      .cc = { SPN_CC_GCC, sp_str_lit("gcc") },
      .linkage = SPN_LIB_KIND_SHARED,
      .libc = SPN_LIBC_GNU,
      .standard = SPN_C11,
      .mode = SPN_DEP_BUILD_MODE_RELEASE,
      .kind = SPN_PROFILE_BUILTIN,
    };
    sp_ht_insert(app.package.profiles, release.name, release);

    app.profile = debug;
  }

  return app;
}

void spn_init(u32 num_args, const c8** args) {
  spn_cli_t* cli = &spn.cli;

  // Parse global options using the CLI parser
  spn_cli_parser_t global_parser = {
    .argv = (c8**)args + 1,  // Skip argv[0] (program name)
    .argc = num_args - 1,
    .stop_at_non_option = true,  // Stop at subcommand name
    .skip_help = true,  // Don't auto-print help and exit - we handle it below
    .cli = (spn_cli_command_usage_t) {
      .name = "spn",
      .summary = "A package manager and build tool for modern C",
      .opts = {
        { "h", "help", SPN_CLI_OPT_KIND_BOOLEAN, "Print help message", SPN_CLI_NO_PLACEHOLDER, &cli->help },
        { "C", "project-dir", SPN_CLI_OPT_KIND_STRING, "Specify the directory containing project file", "DIR", &cli->project_directory },
        { "f", "file", SPN_CLI_OPT_KIND_STRING, "Specify the project file path", "FILE", &cli->project_file },
        { "o", "output", SPN_CLI_OPT_KIND_STRING, "Output mode: interactive, noninteractive, quiet, none", "MODE", &cli->output }
      },
      .args = {}
    }
  };
  spn_cli_parse_command(&global_parser);

  // Update cli args to point to remaining args (subcommand and its args)
  cli->args = &args[1 + global_parser.it];
  cli->num_args = num_args - 1 - global_parser.it;

  spn_cli_usage_t usage = {
    .summary = "A package manager and build tool for modern C",
    .commands = {
      {
        .name = "init",
        .opts = {
          { .brief = "b", .name = "bare", .kind = SPN_CLI_OPT_KIND_BOOLEAN, .summary = "Create minimal project without sp dependency or main.c" }
        },
        .summary = "Initialize a project in the current directory"
      },
      {
        .name = "add",
        .args = { { .name = "package", .kind = SPN_CLI_ARG_KIND_REQUIRED, .summary = "The package to add" } },
        .summary = "Add the latest version of a package to the project"
      },
      {
        .name = "build",
        .opts = {
          { .brief = "f", .name = "force", .kind = SPN_CLI_OPT_KIND_BOOLEAN, .summary = "Force build, even if it exists in store" },
          { .brief = "p", .name = "profile", .kind = SPN_CLI_OPT_KIND_STRING, .summary = "Profile to use for building", .placeholder = "PROFILE" },
          { .brief = "t", .name = "target", .kind = SPN_CLI_OPT_KIND_STRING, .summary = "Target to build", .placeholder = "TARGET" }
        },
        .args = {
          { .name = "target", .kind = SPN_CLI_ARG_KIND_OPTIONAL, .summary = "Target to build; if omitted, build all targets" },
          { .name = "profile", .kind = SPN_CLI_ARG_KIND_OPTIONAL, .summary = "Profile to use; if omitted, default to first defined profile" }
        },
        .summary = "Build the project, including dependencies, from source"
      },
      {
        .name = "print",
        .opts = {
          { .brief = "g", .name = "generator", .kind = SPN_CLI_OPT_KIND_STRING, .summary = "Generator type", .placeholder = "TYPE" },
          { .brief = "c", .name = "compiler", .kind = SPN_CLI_OPT_KIND_STRING, .summary = "Compiler to use", .placeholder = "COMPILER" },
          { .brief = "p", .name = "path", .kind = SPN_CLI_OPT_KIND_STRING, .summary = "Package path", .placeholder = "PATH" }
        },
        .summary = "Print or write the compiler flags needed to consume a package"
      },
      {
        .name = "link",
        .args = { { .name = "kind", .kind = SPN_CLI_ARG_KIND_REQUIRED, .summary = "The link kind" } },
        .summary = "Link or copy the binary outputs of your dependencies"
      },
      {
        .name = "update",
        .args = { { .name = "package", .kind = SPN_CLI_ARG_KIND_REQUIRED, .summary = "The package to update" } },
        .summary = "Update an existing package to the latest version in the project"
      },
      {
        .name = "list",
        .summary = "List all known packages in all registries"
      },
      {
        .name = "which",
        .args = {
          { .name = "package", .kind = SPN_CLI_ARG_KIND_REQUIRED, .summary = "The package name" },
          { .name = "dir", .kind = SPN_CLI_ARG_KIND_REQUIRED, .summary = "The cache directory" }
        },
        .summary = "Print the absolute path of a cache dir for a package"
      },
      {
        .name = "ls",
        .args = {
          { .name = "package", .kind = SPN_CLI_ARG_KIND_REQUIRED, .summary = "The package name" },
          { .name = "dir", .kind = SPN_CLI_ARG_KIND_REQUIRED, .summary = "The cache directory" }
        },
        .summary = "Run ls against a cache dir for a package (e.g. to see build output)"
      },
      {
        .name = "manigest",
        .args = { { .name = "package", .kind = SPN_CLI_ARG_KIND_REQUIRED, .summary = "The package name" } },
        .summary = "Print the full manifest source for a package"
      },
    }
  };
  sp_str_t help = spn_cli_usage(&usage);

  if (cli->help || !cli->num_args) {
    sp_log(help);
    SP_EXIT_FAILURE();
  }

  sp_atomic_s32_set(&spn.control, 0);

  spn.paths.work = sp_os_get_cwd();
  spn.paths.storage = sp_os_join_path(sp_os_get_storage_path(), sp_str_lit("spn"));
  spn.paths.tools.dir = sp_os_join_path(spn.paths.storage, sp_str_lit("tools"));
  spn.paths.tools.manifest = sp_os_join_path(spn.paths.tools.dir, sp_str_lit("spn.toml"));
  spn.paths.tools.lock = sp_os_join_path(spn.paths.storage, sp_str_lit("spn.lock"));

  // Config
  spn.paths.config_dir = sp_os_join_path(sp_os_get_config_path(), SP_LIT("spn"));
  spn.paths.config = sp_os_join_path(spn.paths.config_dir, SP_LIT("spn.toml"));

  if (sp_os_does_path_exist(spn.paths.config)) {
    toml_table_t* toml = spn_toml_parse(spn.paths.config);

    toml_value_t dir = toml_table_string(toml, "spn");
    if (dir.ok) {
      spn.paths.spn = sp_str_view(dir.u.s);
    }

    toml_array_t* registries = toml_table_array(toml, "registry");
    if (registries) {
      spn_toml_arr_for(registries, n) {
        toml_table_t* it = toml_array_table(registries, n);
        spn_registry_t registry = {
          .location = spn_toml_str(it, "location"),
          .kind = SPN_PACKAGE_KIND_FILE
        };

        sp_dyn_array_push(spn.config.registries, registry);
      }
    }
  }

  if (!sp_str_valid(spn.paths.spn)) {
    spn.paths.spn = sp_os_join_path(spn.paths.storage, sp_str_lit("spn"));
  }

  spn.paths.index = sp_os_join_path(spn.paths.spn, sp_str_lit("packages"));
  spn.paths.include = sp_os_join_path(spn.paths.spn, sp_str_lit("include"));

  if (!sp_os_does_path_exist(spn.paths.spn)) {
    sp_str_t url = SP_LIT("https://github.com/tspader/spn.git");
    SP_LOG(
      "Cloning index from {:fg brightcyan} to {:fg brightcyan}",
      SP_FMT_STR(url),
      SP_FMT_STR(spn.paths.spn)
    );

    SP_ASSERT(!spn_git_clone(url, spn.paths.spn));
  }

  // Initialize builtin registry
  spn.registry = (spn_registry_t) {
    .location = spn.paths.index,
    .kind = SPN_PACKAGE_KIND_INDEX
  };

  // Find the cache directory after the config has been fully loaded
  spn.paths.cache = sp_os_join_path(spn.paths.storage, SP_LIT("cache"));
  spn.paths.source = sp_os_join_path(spn.paths.cache, SP_LIT("source"));
  spn.paths.build = sp_os_join_path(spn.paths.cache, SP_LIT("build"));
  spn.paths.store = sp_os_join_path(spn.paths.cache, SP_LIT("store"));

  sp_os_create_directory(spn.paths.cache);
  sp_os_create_directory(spn.paths.source);
  sp_os_create_directory(spn.paths.build);
  sp_os_create_directory(spn.paths.store);

  app = spn_app_load((spn_app_config_t) {
    .manifest = sp_os_join_path(spn.paths.work, sp_str_lit("spn.toml"))
  });
}

/////////
// CLI //
/////////
spn_err_t spn_cli_parser_err(spn_cli_parser_t* parser, sp_str_t err) {
  if (!parser->skip_help) {
    sp_log(parser->err);
    spn_cli_print_help(parser);
    SP_EXIT_FAILURE();
  }

  parser->err = sp_str_copy(err);
  return SPN_ERROR;
}

void spn_cli_print_help(spn_cli_parser_t* parser) {
  sp_log(spn_cli_command_usage(parser->cli));
}

bool spn_cli_parser_is_done(spn_cli_parser_t* p) {
  return p->it >= p->argc;
}

sp_str_t spn_cli_parser_peek(spn_cli_parser_t* p) {
  return SP_CSTR(p->argv[p->it]);
}

void spn_cli_parser_eat(spn_cli_parser_t* p) {
  p->it++;
}

bool spn_cli_parser_is_opt(spn_cli_parser_t* p) {
  if (spn_cli_parser_is_done(p)) return false;
  c8* arg = p->argv[p->it];
  return arg[0] == '-';
}

bool spn_cli_str_parser_is_done(spn_cli_str_parser_t* p) {
  return p->it >= p->str.len;
}

c8 spn_cli_str_parser_peek(spn_cli_str_parser_t* p) {
  return sp_str_at(p->str, p->it);
}

void spn_cli_str_parser_eat(spn_cli_str_parser_t* p) {
  p->it++;
}

sp_str_t spn_cli_str_parser_rest(spn_cli_str_parser_t* p) {
  return sp_str_sub(p->str, p->it, p->str.len - p->it);
}

void spn_cli_assign_bool(void* ptr, bool value) {
  if (ptr) {
    bool* b = (bool*)ptr;
    *b = value;
  }
}

void spn_cli_assign_str(void* ptr, sp_str_t value) {
  if (ptr) {
    sp_str_t* str = (sp_str_t*)ptr;
    *str = value;
  }
}

void spn_cli_assign_s64(void* ptr, s64 value) {
  if (ptr) {
    s64* n = (s64*)ptr;
    *n = value;
  }
}

void spn_cli_assign(spn_cli_opt_usage_t opt, sp_str_t value) {
  switch (opt.kind) {
    case SPN_CLI_OPT_KIND_BOOLEAN: { spn_cli_assign_bool(opt.ptr, true); break; }
    case SPN_CLI_OPT_KIND_STRING: { spn_cli_assign_str(opt.ptr, value); break; }
    case SPN_CLI_OPT_KIND_INTEGER: { spn_cli_assign_s64(opt.ptr, sp_parse_s64(value)); break; }
  }
}

spn_err_t spn_cli_parse_command(spn_cli_parser_t* p) {
  spn_cli_command_usage_t* cmd = &p->cli;
  while (true) {
    if (spn_cli_parser_is_done(p)) {
      break;
    }

    sp_str_t arg = spn_cli_parser_peek(p);

    if (spn_cli_parser_is_opt(p)) {
      if (sp_str_starts_with(arg, sp_str_lit("--"))) {
        // Long option: --name or --name=value
        sp_str_t opt_part = sp_str_sub(arg, 2, arg.len - 2);

        spn_cli_named_opt_t opt = { .name = opt_part };
        sp_str_for(opt_part, it) {
          if (sp_str_at(opt_part, it) == '=') {
            opt.name = sp_str_sub(opt_part, 0, it);
            opt.value = sp_str_sub(opt_part, it + 1, opt_part.len - it - 1);
            opt.has_value = true;
            break;
          }
        }

        // Find matching option
        sp_carr_for(cmd->opts, it) {
          spn_cli_opt_usage_t usage = cmd->opts[it];
          if (!usage.name) break;

          if (sp_str_equal_cstr(opt.name, usage.name)) {
            spn_cli_parser_eat(p);

            sp_str_t value = opt.has_value ? opt.value : spn_cli_parser_peek(p);
            if (!opt.has_value) spn_cli_parser_eat(p);
            spn_cli_assign(usage, value);

            opt.found = true;
            break;
          }
        }

        if (!p->skip_help) {
          if (sp_str_equal_cstr(opt.name, "help")) {
            spn_cli_print_help(p);
            SP_EXIT_SUCCESS();
          }
        }

        if (!opt.found) {
          return spn_cli_parser_err(p, sp_format("Error: unknown option: --{}\n", SP_FMT_STR(opt.name)));
        }
      }
      else if (sp_str_starts_with(arg, sp_str_lit("-"))) {
        // Short option: -b or -bfv
        spn_cli_parser_eat(p);

        spn_cli_str_parser_t ap = {
          .str = sp_str_strip_left(arg, sp_str_lit("-")),
        };

        while (true) {
          if (spn_cli_str_parser_is_done(&ap)) {
            break;
          }

          c8 brief = spn_cli_str_parser_peek(&ap);
          spn_cli_str_parser_eat(&ap);

          sp_carr_for(cmd->opts, i) {
            spn_cli_opt_usage_t opt = cmd->opts[i];
            if (!opt.brief) break;

            if (opt.brief[0] == brief) {
              sp_str_t value;
              if (spn_cli_str_parser_is_done(&ap)) {
                value = spn_cli_parser_peek(p);
                spn_cli_parser_eat(p);
              } else {
                value = spn_cli_str_parser_rest(&ap);
              }
              spn_cli_assign(opt, value);

              ap.found = true;
              break;
            }
          }

          if (!ap.found) {
            return spn_cli_parser_err(p, sp_format("Invalid brief option: {}", SP_FMT_STR(ap.str)));
          }
        }
      }
      else {
        return spn_cli_parser_err(p, sp_format("Invalid option: {}", SP_FMT_STR(arg)));
      }
    }
    else {
      // Positional argument
      if (p->stop_at_non_option) {
        // Stop parsing here - leave remaining args for subcommand
        break;
      }
      p->positionals[p->num_positionals++] = arg;
      spn_cli_parser_eat(p);
    }
  }

  // Bind positionals to expected args
  for (u32 i = 0; i < p->num_positionals && i < SPN_CLI_MAX_ARGS; i++) {
    if (!cmd->args[i].name) break;
    spn_cli_assign_str(cmd->args[i].ptr, p->positionals[i]);
  }

  // Validate required args
  sp_carr_for(cmd->args, it) {
    spn_cli_arg_usage_t arg = cmd->args[it];
    if (!arg.name) break;

    switch (arg.kind) {
      case SPN_CLI_ARG_KIND_REQUIRED: {
        if (p->num_positionals <= it) {
          return spn_cli_parser_err(p, sp_format("Error: missing required argument: {}\n", SP_FMT_CSTR(arg.name)));
        }
        break;
      }
      case SPN_CLI_ARG_KIND_OPTIONAL: {
        break;
      }
    }
  }

  return SPN_OK;
}


spn_cli_command_info_t spn_cli_command_info_from_usage(spn_cli_command_usage_t cmd) {
  spn_cli_command_info_t info = {
    .name = sp_str_from_cstr(cmd.name),
    .usage = sp_str_from_cstr(cmd.usage),
    .summary = sp_str_from_cstr(cmd.summary),
  };

  // Process options
  sp_carr_for(cmd.opts, it) {
    if (!cmd.opts[it].name) break;

    spn_cli_opt_info_t opt = {
      .brief = sp_str_from_cstr(cmd.opts[it].brief),
      .name = sp_str_from_cstr(cmd.opts[it].name),
      .kind = cmd.opts[it].kind,
      .summary = sp_str_from_cstr(cmd.opts[it].summary),
      .placeholder = sp_str_from_cstr(cmd.opts[it].placeholder ? cmd.opts[it].placeholder : ""),
    };
    sp_da_push(info.opts, opt);
  }

  // Process arguments
  sp_carr_for(cmd.args, it) {
    if (!cmd.args[it].name) break;

    spn_cli_arg_info_t arg = {
      .name = sp_str_from_cstr(cmd.args[it].name),
      .kind = cmd.args[it].kind,
      .summary = sp_str_from_cstr(cmd.args[it].summary),
    };
    sp_da_push(info.args, arg);
    sp_da_push(info.brief, arg.name);
  }

  return info;
}

sp_str_t spn_cli_command_usage(spn_cli_command_usage_t cmd) {
  spn_cli_command_info_t info = spn_cli_command_info_from_usage(cmd);

  sp_str_builder_t builder = SP_ZERO_INITIALIZE();

  SP_ASSERT(!sp_str_empty(info.summary));
  sp_str_builder_append_fmt(&builder, "{}", SP_FMT_CSTR(cmd.summary));
  sp_str_builder_new_line(&builder);

  if (!sp_dyn_array_empty(info.opts)) {
    sp_str_builder_new_line(&builder);

    sp_str_builder_append_fmt(&builder, "{:fg brightgreen}", SP_FMT_CSTR("options"));
    sp_str_builder_new_line(&builder);

    sp_tui_begin_table(&spn.tui);
    sp_tui_table_setup_column(&spn.tui, sp_str_lit("Short"));
    sp_tui_table_setup_column(&spn.tui, sp_str_lit("Long"));
    sp_tui_table_setup_column(&spn.tui, sp_str_lit("Type"));
    sp_tui_table_setup_column(&spn.tui, sp_str_lit("Description"));
    sp_tui_table_header_row(&spn.tui);

    sp_dyn_array_for(info.opts, it) {
      spn_cli_opt_info_t opt = info.opts[it];

      // Build short flag display
      sp_str_t short_display;
      if (!sp_str_empty(opt.brief)) {
        sp_str_t short_text = sp_format("-{}", SP_FMT_STR(opt.brief));
        short_display = sp_format("{:fg brightyellow}", SP_FMT_STR(short_text));
      } else {
        short_display = sp_str_lit("");
      }

      // Build long flag display
      sp_str_t long_display;
      if (!sp_str_empty(opt.placeholder)) {
        sp_str_t long_text = sp_format("--{}", SP_FMT_STR(opt.name));
        long_display = sp_format("{:fg brightyellow}={:fg white}", SP_FMT_STR(long_text), SP_FMT_STR(opt.placeholder));
      } else {
        sp_str_t long_text = sp_format("--{}", SP_FMT_STR(opt.name));
        long_display = sp_format("{:fg brightyellow}", SP_FMT_STR(long_text));
      }

      sp_str_t kind_str = sp_format("{:fg brightblack}", SP_FMT_STR(spn_cli_opt_kind_to_str(opt.kind)));

      sp_tui_table_next_row(&spn.tui);
      sp_tui_table_str(&spn.tui, short_display);
      sp_tui_table_str(&spn.tui, long_display);
      sp_tui_table_str(&spn.tui, kind_str);
      sp_tui_table_str(&spn.tui, opt.summary);
    }

    sp_tui_table_set_indent(&spn.tui, 1);
    sp_tui_table_end(&spn.tui);

    sp_str_builder_append(&builder, sp_tui_render(&spn.tui));
  }

  if (!sp_dyn_array_empty(info.args)) {
    sp_str_builder_new_line(&builder);

    sp_str_builder_append_fmt(&builder, "{:fg brightgreen}", SP_FMT_CSTR("arguments"));
    sp_str_builder_new_line(&builder);

    sp_tui_begin_table(&spn.tui);
    sp_tui_table_setup_column(&spn.tui, sp_str_lit("Name"));
    sp_tui_table_setup_column(&spn.tui, sp_str_lit("Type"));
    sp_tui_table_setup_column(&spn.tui, sp_str_lit("Description"));
    sp_tui_table_header_row(&spn.tui);

    sp_dyn_array_for(info.args, it) {
      spn_cli_arg_info_t arg = info.args[it];

      sp_tui_table_next_row(&spn.tui);
      sp_tui_table_fmt(&spn.tui, "{:fg brightyellow}", SP_FMT_STR(arg.name));
      sp_tui_table_str(&spn.tui, sp_str_lit("str"));
      sp_tui_table_str(&spn.tui, arg.summary);
    }

    sp_tui_table_set_indent(&spn.tui, 1);
    sp_tui_table_end(&spn.tui);
    sp_str_t table = sp_tui_render(&spn.tui);

    sp_str_builder_append_fmt(&builder, "{}", SP_FMT_STR(table));
  }

  return sp_str_builder_move(&builder);
}

sp_str_t spn_cli_usage(spn_cli_usage_t* cli) {
  spn_cli_usage_info_t info = SP_ZERO_INITIALIZE();

  sp_carr_for(cli->commands, it) {
    spn_cli_command_usage_t command = cli->commands[it];
    if (!command.name) break;

    spn_cli_command_info_t cmd = spn_cli_command_info_from_usage(command);
    sp_dyn_array_push(info.commands, cmd);
  }

  sp_str_builder_t builder = SP_ZERO_INITIALIZE();

  if (cli->summary) {
    sp_str_builder_append_fmt(&builder, "{}", SP_FMT_CSTR(cli->summary));

    sp_str_builder_new_line(&builder);
    sp_str_builder_new_line(&builder);
  }

  if (cli->usage) {
    sp_str_builder_append_fmt(&builder, "{:fg brightgreen}", SP_FMT_CSTR("usage"));
    sp_str_builder_indent(&builder);
    sp_str_builder_new_line(&builder);
    sp_str_builder_append_fmt(&builder, "{:fg brightcyan}", SP_FMT_CSTR(cli->usage));
    sp_str_builder_dedent(&builder);

    sp_str_builder_new_line(&builder);
    sp_str_builder_new_line(&builder);
  }

  if (!sp_dyn_array_empty(info.commands)) {
    sp_str_builder_append_fmt(&builder, "{:fg brightgreen}", SP_FMT_CSTR("usage"));
    sp_str_builder_new_line(&builder);

    sp_tui_begin_table(&spn.tui);
    sp_tui_table_setup_column(&spn.tui, sp_str_lit("Command"));
    sp_tui_table_setup_column(&spn.tui, sp_str_lit("Arguments"));
    sp_tui_table_setup_column(&spn.tui, sp_str_lit("Description"));
    sp_tui_table_header_row(&spn.tui);

    sp_dyn_array_for(info.commands, it) {
      spn_cli_command_info_t command = info.commands[it];
      sp_str_t args = sp_str_join_n(command.brief, sp_dyn_array_size(command.brief), sp_str_lit(", "));

      sp_tui_table_next_row(&spn.tui);
      sp_tui_table_fmt(&spn.tui, "{:fg brightcyan}", SP_FMT_STR(command.name));
      sp_tui_table_fmt(&spn.tui, "{:fg brightyellow}", SP_FMT_STR(args));
      sp_tui_table_str(&spn.tui, command.summary);
    }

    sp_tui_table_set_indent(&spn.tui, 1);
    sp_tui_table_end(&spn.tui);
    sp_str_t table = sp_tui_render(&spn.tui);

    sp_str_builder_append_fmt(&builder, "{}", SP_FMT_STR(table));
  }

  return sp_str_builder_move(&builder);
}

void spn_cli_run() {
  spn_cli_t* cli = &spn.cli;

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
  else if (sp_cstr_equal("manifest", cli->args[0])) {
    spn_cli_manifest(cli);
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

void spn_cli_assert_num_args(spn_cli_t* cli, u32 n, sp_str_t help) {
  if (cli->num_args != n) {
    sp_log(help);
    SP_EXIT_FAILURE();
  }
}

sp_str_t spn_cli_get_arg(spn_cli_t* cli, u32 n) {
  SP_ASSERT(cli->num_args > n);
  return sp_str_from_cstr(cli->args[n]);
}

void spn_cli_init(spn_cli_t* cli) {
  spn_cli_init_t* cmd = &cli->init;

  // Skip the command name itself (first arg)
  spn_cli_parser_t parser = {
    .argv = (c8**)cli->args + 1,
    .argc = cli->num_args - 1,
    .cli = (spn_cli_command_usage_t) {
      .name = "init",
      .summary = "Initialize a new spn project",
      .opts = {
        { "h", "help", SPN_CLI_OPT_KIND_BOOLEAN, "Print this message", SPN_CLI_NO_PLACEHOLDER, &cli->help },
        { "b", "bare", SPN_CLI_OPT_KIND_BOOLEAN, "Only create a minimal spn.toml", SPN_CLI_NO_PLACEHOLDER, &cmd->bare }
      },
      .args = {}
    }
  };
  spn_cli_parse_command(&parser);

  spn_package_t package = spn_app_new(
    spn.paths.work,
    sp_os_extract_stem(spn.paths.work),
    cmd->bare ? SPN_APP_INIT_BARE : SPN_APP_INIT_NORMAL
  );

  SP_LOG("Initialized project {:fg brightcyan}. Run {:fg brightyellow} to build.", SP_FMT_STR(package.name), SP_FMT_CSTR("spn build"));
}

void spn_cli_list(spn_cli_t* cli) {
  // Skip the command name itself (first arg)
  spn_cli_parser_t parser = {
    .argv = (c8**)cli->args + 1,
    .argc = cli->num_args - 1,
    .cli = (spn_cli_command_usage_t) {
      .name = "list",
      .summary = "List all available packages",
      .opts = {
        { "h", "help", SPN_CLI_OPT_KIND_BOOLEAN, "Print this message", SPN_CLI_NO_PLACEHOLDER, &cli->help }
      },
      .args = {}
    }
  };
  spn_cli_parse_command(&parser);

  sp_tui_begin_table(&spn.tui);
  sp_tui_table_setup_column(&spn.tui, sp_str_lit("Package"));
  sp_tui_table_setup_column(&spn.tui, sp_str_lit("Version"));
  sp_tui_table_setup_column(&spn.tui, sp_str_lit("Repo"));
  sp_tui_table_setup_column(&spn.tui, sp_str_lit("Author"));
  sp_tui_table_header_row(&spn.tui);

  sp_ht_for(app.index, it) {
    sp_str_t path = *sp_ht_it_getp(app.index, it);
    spn_package_t* package = spn_app_find_package(&app, (spn_dep_req_t) {
      .name = sp_os_extract_stem(path),
      .kind = SPN_PACKAGE_KIND_INDEX
    });

    sp_tui_table_next_row(&spn.tui);
    sp_tui_table_fmt(&spn.tui, "{:fg brightcyan}", SP_FMT_STR(package->name));
    sp_tui_table_str(&spn.tui, spn_semver_to_str(package->version));
    sp_tui_table_str(&spn.tui, sp_str_truncate(package->repo, 50, sp_str_lit("...")));
    sp_tui_table_str(&spn.tui, sp_str_truncate(package->author, 30, sp_str_lit("...")));
  }

  sp_tui_table_end(&spn.tui);
  sp_log(sp_tui_render(&spn.tui));
}

void spn_cli_clean(spn_cli_t* cli) {
  SP_LOG("Removing {:fg brightcyan}", SP_FMT_STR(spn.paths.build));
  sp_os_remove_directory(spn.paths.build);
  SP_LOG("Removing {:fg brightcyan}", SP_FMT_STR(spn.paths.store));
  sp_os_remove_directory(spn.paths.store);
}


void spn_cli_copy(spn_cli_t* cli) {
  spn_cli_copy_t* cmd = &cli->copy;

  // Skip the command name itself (first arg)
  spn_cli_parser_t parser = {
    .argv = (c8**)cli->args + 1,
    .argc = cli->num_args - 1,
    .cli = (spn_cli_command_usage_t) {
      .name = "copy",
      .summary = "Copy all project binary dependencies to a directory",
      .opts = {
        { "h", "help", SPN_CLI_OPT_KIND_BOOLEAN, "Print this message", SPN_CLI_NO_PLACEHOLDER, &cli->help }
      },
      .args = {
        { "directory", SPN_CLI_ARG_KIND_REQUIRED, "The destination directory", &cmd->directory }
      }
    }
  };
  spn_cli_parse_command(&parser);

  sp_str_t destination = sp_os_normalize_path(cmd->directory);
  sp_str_t to = sp_os_join_path(spn.paths.work, destination);
  sp_os_create_directory(to);

  sp_ht_for(app.deps, it) {
    spn_dep_context_t* dep = sp_ht_it_getp(app.deps, it);

    sp_dyn_array(sp_os_dir_ent_t) entries = sp_os_scan_directory(dep->paths.lib);
    sp_dyn_array_for(entries, i) {
      sp_os_dir_ent_t* entry = entries + i;
      sp_os_copy_file(
        entry->file_path,
        sp_os_join_path(to, sp_os_extract_file_name(entry->file_path))
      );
    }
  }
}

void spn_cli_ls(spn_cli_t* cli) {
  spn_cli_ls_t* cmd = &cli->ls;

  // Skip the command name itself (first arg)
  spn_cli_parser_t parser = {
    .argv = (c8**)cli->args + 1,
    .argc = cli->num_args - 1,
    .cli = (spn_cli_command_usage_t) {
      .name = "ls",
      .summary = "List files in a package's cache directory",
      .opts = {
        { "h", "help", SPN_CLI_OPT_KIND_BOOLEAN, "Print this message", SPN_CLI_NO_PLACEHOLDER, &cli->help },
        { "d", "dir", SPN_CLI_OPT_KIND_STRING, "Which directory to list (store, include, lib, source, work, vendor)", "DIR", &cmd->dir }
      },
      .args = {
        { "package", SPN_CLI_ARG_KIND_OPTIONAL, "The package to list", &cmd->package }
      }
    }
  };
  spn_cli_parse_command(&parser);

  if (sp_str_valid(cmd->package)) {
    spn_dep_context_t* dep = spn_cli_assert_dep_exists(cmd->package);

    spn_dir_kind_t kind = SPN_DIR_STORE;
    if (sp_str_valid(cmd->dir)) {
      kind = spn_cache_dir_kind_from_str(cmd->dir);
    }

    sp_str_t dir = spn_cache_dir_kind_to_dep_path(dep, kind);
    sp_sh_ls(dir);
  }
  else {
    spn_dir_kind_t kind = SPN_DIR_CACHE;
    if (sp_str_valid(cmd->dir)) {
      kind = spn_cache_dir_kind_from_str(cmd->dir);
    }

    sp_str_t dir = spn_cache_dir_kind_to_path(kind);
    sp_sh_ls(dir);
  }
}

void spn_cli_which(spn_cli_t* cli) {
  spn_cli_which_t* cmd = &cli->which;

  // Skip the command name itself (first arg)
  spn_cli_parser_t parser = {
    .argv = (c8**)cli->args + 1,
    .argc = cli->num_args - 1,
    .cli = (spn_cli_command_usage_t) {
      .name = "which",
      .summary = "Print the cache directory for a package",
      .opts = {
        { "h", "help", SPN_CLI_OPT_KIND_BOOLEAN, "Print this message", SPN_CLI_NO_PLACEHOLDER, &cli->help },
        { "d", "dir", SPN_CLI_OPT_KIND_STRING, "Which directory to show (store, include, lib, source, work, vendor)", "DIR", &cmd->dir }
      },
      .args = {
        { "package", SPN_CLI_ARG_KIND_OPTIONAL, "The package to show path for", &cmd->package }
      }
    }
  };
  spn_cli_parse_command(&parser);

  spn_app_resolve(&app);
  spn_app_prepare_deps(&app);

  if (sp_str_valid(cmd->package)) {
    spn_dep_context_t* dep = spn_cli_assert_dep_exists(cmd->package);

    spn_dir_kind_t kind = SPN_DIR_STORE;
    if (sp_str_valid(cmd->dir)) {
      kind = spn_cache_dir_kind_from_str(cmd->dir);
    }

    sp_str_t dir = spn_cache_dir_kind_to_dep_path(dep, kind);
    printf("%.*s", dir.len, dir.data);

  }
  else {
    spn_dir_kind_t kind = SPN_DIR_CACHE;
    if (sp_str_valid(cmd->dir)) {
      kind = spn_cache_dir_kind_from_str(cmd->dir);
    }

    sp_str_t dir = spn_cache_dir_kind_to_path(kind);
    printf("%.*s", dir.len, dir.data);
  }
}

void spn_cli_manifest(spn_cli_t* cli) {
  spn_cli_manifest_t* cmd = &cli->manifest;

  // Skip the command name itself (first arg)
  spn_cli_parser_t parser = {
    .argv = (c8**)cli->args + 1,
    .argc = cli->num_args - 1,
    .cli = (spn_cli_command_usage_t) {
      .name = "manifest",
      .summary = "Print the manifest contents for a package",
      .opts = {
        { "h", "help", SPN_CLI_OPT_KIND_BOOLEAN, "Print this message", SPN_CLI_NO_PLACEHOLDER, &cli->help }
      },
      .args = {
        { "package", SPN_CLI_ARG_KIND_REQUIRED, "The package to show manifest for", &cmd->package }
      }
    }
  };
  spn_cli_parse_command(&parser);

  spn_dep_context_t* dep = spn_cli_assert_dep_exists(cmd->package);

  sp_str_t path = dep->package->paths.manifest;
  sp_str_t manifest = sp_io_read_file(path);
  if (!sp_str_valid(manifest)) {
    SP_FATAL("Failed to read manifest at {:fg brightyellow}", SP_FMT_STR(path));
  }

  sp_os_log(manifest);
}

spn_dep_req_t spn_dep_req_from_str(sp_str_t str) {
  spn_dep_req_t dep = SP_ZERO_INITIALIZE();
  if (sp_str_starts_with(str, sp_str_lit("file://"))) {
    return (spn_dep_req_t) {
      .kind = SPN_PACKAGE_KIND_FILE,
      .file = sp_str_copy(str)
    };
  }
  else {
    return (spn_dep_req_t) {
      .kind = SPN_PACKAGE_KIND_INDEX,
      .range = spn_semver_range_from_str(str),
    };
  }

  SP_UNREACHABLE_RETURN(SP_ZERO_STRUCT(spn_dep_req_t));
}

sp_str_t spn_dep_req_to_str(spn_dep_req_t dep) {
  switch (dep.kind) {
    case SPN_PACKAGE_KIND_REMOTE:
    case SPN_PACKAGE_KIND_FILE: {
      return dep.file;
    }
    case SPN_PACKAGE_KIND_INDEX: {
      return spn_semver_range_to_str(dep.range);
    }
    case SPN_PACKAGE_KIND_WORKSPACE: {
      SP_BROKEN();
      break;
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

void spn_package_add_dep_request(spn_package_t* package, spn_dep_req_t request) {
  sp_ht_insert(package->deps, request.name, request);
}

void spn_package_add_dep_from_manifest(spn_package_t* package, sp_str_t file_path) {
  SP_ASSERT(package);
  SP_ASSERT(sp_os_does_path_exist(file_path));

  spn_package_t dep = spn_package_load(file_path);

  if (sp_ht_key_exists(package->deps, dep.name)) {
    SP_FATAL("{:fg brightyellow} is already in your project", SP_FMT_STR(dep.name));
  }

  spn_dep_req_t request = {
    .name = sp_str_copy(dep.name),
    .kind = SPN_PACKAGE_KIND_FILE,
    .file = sp_format("{}{}", SP_FMT_CSTR("file://"), SP_FMT_STR(file_path)),
  };
  spn_package_add_dep_request(package, request);
}

void spn_package_add_dep_from_index(spn_package_t* package, sp_str_t name) {
  SP_ASSERT(package);

  if (sp_ht_getp(package->deps, name)) {
    SP_FATAL("{:fg brightyellow} is already in your project", SP_FMT_STR(name));
  }

  spn_dep_req_t request = {
    .name = sp_str_copy(name),
    .kind = SPN_PACKAGE_KIND_INDEX
  };
  spn_package_t* dep = spn_app_find_package(&app, request);
  if (!dep) {
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

  if (sp_dyn_array_empty(dep->versions)) {
    SP_FATAL("{:fg brightcyan} has no known versions", SP_FMT_STR(dep->name));
  }

  spn_semver_parsed_t version = {
    .version = *sp_dyn_array_back(dep->versions),
    .components = { true, true, true }
  };
  request.range = spn_semver_caret_to_range(version);
  spn_package_add_dep_request(package, request);
}

void spn_app_update(sp_str_t name) {
  spn_dep_req_t* request = sp_ht_getp(app.package.deps, name);
  SP_ASSERT(request);

  if (request->kind != SPN_PACKAGE_KIND_INDEX) SP_FATAL("can only update index for now");

  spn_package_t* dep = spn_app_find_package(&app, *request);
  if (!dep) {
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

  if (sp_dyn_array_empty(dep->versions)) {
    SP_FATAL("{:fg brightcyan} has no known versions", SP_FMT_STR(dep->name));
  }

  spn_dep_req_t update = {
    .name = sp_str_copy(dep->name),
    .range = spn_semver_comparison_to_range(SPN_SEMVER_OP_GEQ, *sp_dyn_array_back(dep->versions))
  };
  spn_package_add_dep_request(&app.package, update);

  spn_app_resolve_from_solver(&app);
  spn_app_prepare_deps(&app);
  spn_app_update_lock_file(&app);
}

void spn_cli_add(spn_cli_t* cli) {
  spn_cli_add_t* cmd = &cli->add;

  // Skip the command name itself (first arg)
  spn_cli_parser_t parser = {
    .argv = (c8**)cli->args + 1,
    .argc = cli->num_args - 1,
    .cli = (spn_cli_command_usage_t) {
      .name = "add",
      .summary = "Add a package to the project",
      .opts = {
        { "h", "help", SPN_CLI_OPT_KIND_BOOLEAN, "Print this message", SPN_CLI_NO_PLACEHOLDER, &cli->help }
      },
      .args = {
        { "package", SPN_CLI_ARG_KIND_REQUIRED, "The package to add", &cmd->package }
      }
    }
  };
  spn_cli_parse_command(&parser);

  spn_package_add_dep_from_index(&app.package, cmd->package);
  spn_app_resolve_from_solver(&app);
  spn_app_prepare_deps(&app);

  spn_app_update_lock_file(&app);
  spn_app_write_manifest(&app.package, app.paths.manifest);
}

void spn_cli_update(spn_cli_t* cli) {
  spn_cli_update_t* cmd = &cli->update;

  // Skip the command name itself (first arg)
  spn_cli_parser_t parser = {
    .argv = (c8**)cli->args + 1,
    .argc = cli->num_args - 1,
    .cli = (spn_cli_command_usage_t) {
      .name = "update",
      .summary = "Update a package to the latest version",
      .opts = {
        { "h", "help", SPN_CLI_OPT_KIND_BOOLEAN, "Print this message", SPN_CLI_NO_PLACEHOLDER, &cli->help }
      },
      .args = {
        { "package", SPN_CLI_ARG_KIND_REQUIRED, "The package to update", &cmd->package }
      }
    }
  };
  spn_cli_parse_command(&parser);

  spn_app_update(cmd->package);
}

void spn_cli_tool(spn_cli_t* cli) {
  // Check for help flag manually since this is a command with subcommands
  for (u32 i = 0; i < cli->num_args; i++) {
    if (sp_cstr_equal(cli->args[i], "--help") || sp_cstr_equal(cli->args[i], "-h")) {
      cli->help = true;
      break;
    }
  }

  spn_cli_usage_t usage = {
    .summary = "Run, install, and manage binaries defined by spn packages",
    .commands = {
      {
        .name = "install",
        .args = { { .name = "package", .kind = SPN_CLI_ARG_KIND_REQUIRED, .summary = "The package to install" } },
        .summary = "Install a package's binary targets to the PATH"
      },
      {
        .name = "uninstall",
        .args = { { .name = "package", .kind = SPN_CLI_ARG_KIND_REQUIRED, .summary = "The package to uninstall" } },
        .summary = "Uninstall a package's binary targets to the PATH"
      },
      {
        .name = "run",
        .args = { { .name = "package", .kind = SPN_CLI_ARG_KIND_REQUIRED, .summary = "The package to run" } },
        .summary = "Run a package's binary; if a package exports more than one, run the first"
      },
      {
        .name = "list",
        .summary = "List installed tools"
      },
      {
        .name = "upgrade",
        .args = { { .name = "package", .kind = SPN_CLI_ARG_KIND_REQUIRED, .summary = "The tool to upgrade" } },
        .summary = "Upgrade a tool to the latest version"
      },
    }
  };
  sp_str_t help = spn_cli_usage(&usage);

  if (!cli->num_args || !cli->args[0]) {
    sp_log(help);
    SP_EXIT_FAILURE();
  }

  spn_tool_cmd_t cmd = spn_tool_subcommand_from_str(sp_str_view(cli->args[0]));
  switch (cmd) {
    case SPN_TOOL_INSTALL: {
      spn_cli_command_usage_t usage = {
        .name = "install",
        .summary = "Install a package's binary targets to the PATH",
        .opts = {
          { .brief = "f", .name = "force", .kind = SPN_CLI_OPT_KIND_BOOLEAN, .summary = "Force reinstall even if already installed" },
          { .brief = "v", .name = "version", .kind = SPN_CLI_OPT_KIND_STRING, .summary = "Specific version to install", .placeholder = "VERSION" }
        },
        .args = {
          { "package", SPN_CLI_ARG_KIND_REQUIRED, "The package to install" }
        }
      };
      sp_str_t install_help = spn_cli_command_usage(usage);

      if (cli->help) {
        sp_log(install_help);
        SP_EXIT_SUCCESS();
      }

      spn_cli_assert_num_args(cli, 2, install_help);

      sp_str_t tool = spn_cli_get_arg(cli, 1);
      spn_tool_install(tool);
      break;
    }
    default: {
      break;
    }
  }
  SP_EXIT_SUCCESS();
}

void spn_cli_print(spn_cli_t* cli) {
  spn_cli_print_t* command = &cli->print;

  // Skip the command name itself (first arg)
  spn_cli_parser_t parser = {
    .argv = (c8**)cli->args + 1,
    .argc = cli->num_args - 1,
    .cli = (spn_cli_command_usage_t) {
      .name = "print",
      .summary = "Print or write compiler flags for consuming packages",
      .opts = {
        { "h", "help", SPN_CLI_OPT_KIND_BOOLEAN, "Print this message", SPN_CLI_NO_PLACEHOLDER, &cli->help },
        { "p", "path", SPN_CLI_OPT_KIND_STRING, "Write generated flags to a file", "PATH", &command->path },
        { "c", "compiler", SPN_CLI_OPT_KIND_STRING, "Generate for compiler [*gcc, msvc]", "COMPILER", &command->compiler },
        { "g", "generator", SPN_CLI_OPT_KIND_STRING, "Output format [*raw, shell, make]", "GENERATOR", &command->generator },
        { "o", "output", SPN_CLI_OPT_KIND_STRING, "Output mode: interactive, noninteractive, quiet, none", "MODE", &cli->output }
      },
      .args = {}
    }
  };
  spn_cli_parse_command(&parser);

  if (sp_str_valid(command->path) && !sp_str_valid(command->generator)) {
    SP_FATAL(
      "output path was specified, but no generator. try e.g.:\n  spn print --path {} {:fg yellow}",
      SP_FMT_STR(command->path),
      SP_FMT_CSTR("--generator make")
    );
  }
  if (!sp_str_valid(command->generator)) command->generator = sp_str_lit("");
  if (!sp_str_valid(command->compiler)) command->compiler = sp_str_lit("gcc");

  if (!app.lock.some) {
    SP_FATAL("No lock file found. Run {:fg yellow} first.", SP_FMT_CSTR("spn build"));
  }

  spn_app_resolve(&app);
  spn_app_prepare_deps(&app);

  spn_generator_context_t gen = {
    .kind = spn_gen_kind_from_str(command->generator),
    .compiler = spn_cc_kind_from_str(command->compiler)
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

  if (sp_str_valid(command->path)) {
    sp_str_t destination = sp_os_normalize_path(command->path);
    destination = sp_os_join_path(spn.paths.work, destination);
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

spn_err_t spn_dep_context_build_binary(spn_dep_context_t* build, spn_bin_t bin) {
  spn_dep_context_set_build_state(build, SPN_DEP_BUILD_STATE_BUILDING);
  spn_package_t* package = build->package;

  spn_cc_t cc = spn_cc_new(build);
  spn_cc_set_output_dir(&cc, build->paths.bin);

  sp_dyn_array_for(package->include, it) {
    spn_cc_add_include(&cc, package->include[it]);
  }
  sp_dyn_array_for(package->define, it) {
    spn_cc_add_define(&cc, package->define[it]);
  }

  spn_cc_target_t* target = spn_cc_add_target(&cc, SPN_CC_TARGET_EXECUTABLE, bin.name);
  sp_dyn_array_for(bin.source, it) {
    spn_cc_target_add_source(target, bin.source[it]);
  }
  sp_dyn_array_for(bin.include, it) {
    spn_cc_target_add_include(target, bin.include[it]);
  }
  sp_dyn_array_for(bin.define, it) {
    spn_cc_target_add_define(target, bin.define[it]);
  }

  return spn_cc_run(&cc);
}

spn_err_t spn_dep_context_build_binaries(spn_dep_context_t* build) {
  sp_ht_for(build->package->bin, it) {
    spn_bin_t bin = *sp_ht_it_getp(build->package->bin, it);
    spn_err_t result = spn_dep_context_build_binary(build, bin);

    if (result) {
      spn_dep_context_set_build_state(build, SPN_DEP_BUILD_STATE_FAILED);
      return result;
    }
  }

  return SPN_OK;
}

void spn_cli_build(spn_cli_t* cli) {
  spn_cli_build_t* command = &cli->build;

  // Skip the command name itself (first arg)
  spn_cli_parser_t parser = {
    .argv = (c8**)cli->args + 1,
    .argc = cli->num_args - 1,
    .cli = (spn_cli_command_usage_t) {
      .name = "build",
      .summary = "Build one or more of the project's targets",
      .opts = {
        { "h", "help", SPN_CLI_OPT_KIND_BOOLEAN, "Print this message", SPN_CLI_NO_PLACEHOLDER, &cli->help },
        { "f", "force", SPN_CLI_OPT_KIND_BOOLEAN, "Force build, even if it exists in the store", SPN_CLI_NO_PLACEHOLDER, &command->force },
        { "t", "target", SPN_CLI_OPT_KIND_STRING, "Target to build; if omitted, build all targets", "TARGET", &command->target },
        { "p", "profile", SPN_CLI_OPT_KIND_STRING, "Profile to use; if omitted, use default profile", "PROFILE", &command->profile },
        { "o", "output", SPN_CLI_OPT_KIND_STRING, "Output mode: interactive, noninteractive, quiet, none", "MODE", &cli->output }
      },
      .args = {
        { "target", SPN_CLI_ARG_KIND_OPTIONAL, "Target to build; if omitted, build all targets", &command->target_arg },
        { "profile", SPN_CLI_ARG_KIND_OPTIONAL, "Profile to use; if omitted, default to first defined profile", &command->profile_arg }
      }
    }
  };
  spn_cli_parse_command(&parser);

  sp_opt(spn_bin_t) target = SP_ZERO_INITIALIZE();
  sp_str_t target_name = SP_ZERO_INITIALIZE();
  sp_str_t profile_name = SP_ZERO_INITIALIZE();

  // Check for target via flag first, then positional argument
  if (sp_str_valid(command->target)) {
    target_name = command->target;
  }
  else if (sp_str_valid(command->target_arg)) {
    target_name = command->target_arg;
  }

  // Check for profile via flag first, then positional argument
  if (sp_str_valid(command->profile)) {
    profile_name = command->profile;
  }
  else if (sp_str_valid(command->profile_arg)) {
    profile_name = command->profile_arg;
  }
  
  if (!sp_str_empty(target_name)) {
    if (!sp_ht_key_exists(app.package.bin, target_name)) {
      SP_FATAL("{:fg brightcyan} isn't defined in {:fg brightcyan}", SP_FMT_STR(target_name), SP_FMT_STR(app.paths.manifest));
    }

    sp_opt_set(target, *sp_ht_getp(app.package.bin, target_name));
  }

  // Handle profile selection
  if (!sp_str_empty(profile_name)) {
    if (!sp_ht_key_exists(app.package.profiles, profile_name)) {
      SP_FATAL("{:fg brightcyan} profile isn't defined in {:fg brightcyan}", SP_FMT_STR(profile_name), SP_FMT_STR(app.paths.manifest));
    }
    app.profile = *sp_ht_getp(app.package.profiles, profile_name);
  }

  spn_app_resolve(&app);
  spn_app_prepare_deps(&app);

  spn_tui_mode_t mode = sp_str_valid(spn.cli.output) ?
    spn_output_mode_from_str(spn.cli.output) :
    SPN_OUTPUT_MODE_QUIET;

  sp_ht_for(app.deps, it) {
    spn_dep_context_t* dep = sp_ht_it_getp(app.deps, it);
    dep->force = command->force;
    dep->state = SPN_DEP_BUILD_STATE_IDLE;
    sp_thread_init(&dep->thread, spn_dep_thread_build, dep);
  }

  spn_tui_init(&spn.tui, mode);
  spn_tui_run(&spn.tui);

  spn_dep_context_t build = spn_app_prepare_project(&app);
  switch (target.some) {
    case SP_OPT_SOME: {
      spn_dep_context_build_binary(&build, sp_opt_get(target));
      spn_dep_context_finish(&build);

      break;
    }
    case SP_OPT_NONE: {
      spn_dep_context_build(&build);

      break;
    }
  }

  sp_da(sp_os_dir_ent_t) entries = sp_os_scan_directory(build.paths.bin);
  sp_da_for(entries, it) {
    sp_os_dir_ent_t entry = entries[it];
    sp_os_copy(entry.file_path, sp_os_parent_path(build.paths.store));
  }

  switch (build.state) {
    case SPN_DEP_BUILD_STATE_FAILED: {
      SP_LOG("{:fg red}", SP_FMT_CSTR("FAIL"));
      sp_str_t log = sp_io_read_file(build.paths.log);
      log = sp_str_trim(log);
      sp_os_print(sp_str_view(SP_ANSI_FG_BRIGHT_BLACK));
      sp_os_print(log);
      sp_os_print(sp_str_view(SP_ANSI_RESET));
      sp_os_print(sp_str_view("\n"));
      break;
    }
    default: {
      SP_LOG("{:fg green}", SP_FMT_CSTR("OK"));
      break;
    }
  }

  spn_app_update_lock_file(&app);

  #if 0
  sp_ht_for(app.package.bin, it) {
    spn_bin_t bin = *sp_ht_it_getp(app.package.bin, it);

    if (sp_opt_is_null(bin.profile)) {
      SP_ASSERT(!sp_opt_is_null(app.package.profile));
      sp_opt_set(bin.profile, sp_opt_get(app.package.profile));
    }

    // spn_compile_thread_ctx_t ctx = {
    //   .package = &app.package,
    //   .bin = bin,
    // };
    // sp_thread_init(&ctx.thread, spn_app_thread_build_binary, &ctx);
    // sp_ht_insert(app.threads, bin.name, ctx);
    spn_package_build_binary(&app.package, bin);
  }
#endif

}

s32 main(s32 num_args, const c8** args) {
  spn = SP_ZERO_STRUCT(spn_ctx_t);
  app = SP_ZERO_STRUCT(spn_app_t);
  spn_init(num_args, args);
  spn_cli_run();

  return 0;
}
