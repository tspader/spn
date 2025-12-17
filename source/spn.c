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

#define SP_MAIN
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
  //#include <link.h>
  #include <unistd.h>
  #include <string.h>
  #include <pty.h>
  #include <sys/wait.h>
#endif

#include "spn.h"
#include "graph.h"

#define SPN_VERSION "1.0.0"
#define SPN_COMMIT "00c0fa98"


#define SPN_ERR(X) \
  X(SPN_OK, "ok") \
  X(SPN_ERROR, "error")

typedef s32 spn_err_t;
#define SPN_OK 0
#define SPN_ERROR 1


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

#define sp_rb_push(rb, value) sp_ring_buffer_push(&(rb), &(value))
#define sp_rb_for(rb, it) sp_ring_buffer_for(rb, it)

#define sp_try(expr) do { s32 _sp_result = (expr); if (_sp_result) return _sp_result; } while (0)
#define sp_try_as(expr, err) do { s32 _sp_result = (expr); if (_sp_result) return err; } while (0)

#define sp_ht_collect_keys(ht, da) \
  do { \
    sp_ht_for_kv((ht), __it) { \
      sp_dyn_array_push((da), *__it.key); \
    } \
  } while(0)

#define sp_ht_get_key_index_mt(__HT, __PTR) sp_ht_get_key_index_fn((void**)&((__HT)->data), (void*)&((__PTR)), (__HT)->info)

#define sp_ht_get_mt(__HT, __PTR, __INDEX)\
    (\
        (__HT) == SP_NULLPTR ? SP_NULLPTR :\
        ((__INDEX) = sp_ht_get_key_index_mt(__HT, __PTR), \
        ((__INDEX) != SP_HT_INVALID_INDEX ? &(__HT)->data[(__INDEX)].val : NULL)) \
    )

sp_str_t sp_color_to_tui_rgb(sp_color_t color);
sp_str_t sp_color_to_tui_rgb_f(u8 r, u8 g, u8 b);

sp_str_t sp_str_pad_ex(sp_str_t str, u32 n, c8 c) {
  if (str.len >= n) return sp_str_copy(str);

  sp_str_builder_t builder = SP_ZERO_INITIALIZE();

  sp_str_builder_append(&builder, str);
  for (u32 it = str.len; it < n; it++) {
    sp_str_builder_append_c8(&builder, c);
  }

  return sp_str_builder_move(&builder);
}

sp_str_t sp_str_repeat(c8 c, u32 len) {
  if (!len) return SP_ZERO_STRUCT(sp_str_t);

  c8* buffer = (c8*)sp_alloc(len);
  sp_mem_fill(buffer, len, &c, sizeof(c8));
  return sp_str(buffer, len);
}

void sp_scratch_log() {
  sp_mem_arena_t* arena = sp_mem_get_scratch_arena();
  SP_LOG("{}", SP_FMT_U32(arena->bytes_used));
}

void strip_ansi(char* buf, ssize_t* len) {
    char* out = buf;
    char* in = buf;
    char* end = buf + *len;

    while (in < end) {
        if (*in == '\033' && in + 1 < end && *(in + 1) == '[') {
            // Skip ESC[...m sequences
            in += 2;
            while (in < end && !(*in >= 'A' && *in <= 'z')) in++;
            if (in < end) in++;  // skip the final letter
        } else {
            *out++ = *in++;
        }
    }
    *len = out - buf;
}

#ifdef SP_LINUX
// pty-wrap: run a command with stdout/stderr connected to a pty
// This forces line-buffering so output is flushed before crashes
s32 spn_pty_wrap(s32 num_args, const c8** args) {
  if (num_args < 3) {
    fprintf(stderr, "usage: spn --pty-wrap <command> [args...]\n");
    return 1;
  }

  int master;
  pid_t pid = forkpty(&master, NULL, NULL, NULL);

  if (pid < 0) {
    perror("forkpty");
    return 1;
  }

  if (pid == 0) {
    // Child: stdout/stderr are now the slave side of the pty
    // libc will line-buffer because isatty(1) == true
    execvp(args[2], (char* const*)&args[2]);
    _exit(127);
  }

  // Parent: copy master -> stdout
  char buf[4096];
  ssize_t n;
  while ((n = read(master, buf, sizeof(buf))) > 0) {
    strip_ansi(buf, &n);
    write(STDOUT_FILENO, buf, n);
  }

  int status;
  waitpid(pid, &status, 0);
  close(master);

  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  if (WIFSIGNALED(status)) {
    return 128 + WTERMSIG(status);
  }
  return 1;
}
#endif


/////////////
// SPINNER //
/////////////
#define SPN_SPINNER_TRAIL_LEN  6
#define SPN_SPINNER_CYCLE_MS   500.0f
#define SPN_SPINNER_HOLD_START_MS 500.0f
#define SPN_SPINNER_HOLD_END_MS   100.0f

#define SPN_SPINNER_ACTIVE   "\u25A0"
#define SPN_SPINNER_INACTIVE "\u2B1D"

typedef struct spn_spinner_t {
  sp_interp_t interp;
  bool        forward;
  bool        render_forward;  // direction for trail rendering
  f32         hold_ms;
  f32         hold_total_ms;   // total hold time for fade calculation
  f32         value;
  u32         width;
  sp_color_t  color;
} spn_spinner_t;

void spn_spinner_init(spn_spinner_t* s, sp_color_t color) {
  s->interp = sp_interp_build(0.0f, 1.0f, SPN_SPINNER_CYCLE_MS);
  s->forward = true;
  s->render_forward = true;
  s->hold_ms = 0;
  s->hold_total_ms = 0;
  s->value = 0;
  s->width = 8;
  s->color = color;
}

void spn_spinner_update(spn_spinner_t* s, f32 dt_ms) {
  if (s->hold_ms > 0) {
    s->hold_ms -= dt_ms;
    // Once hold expires, flip render direction
    if (s->hold_ms <= 0) {
      s->render_forward = s->forward;
    }
    return;
  }

  if (sp_interp_update(&s->interp, dt_ms)) {
    s->forward = !s->forward;
    s->hold_total_ms = s->forward ? SPN_SPINNER_HOLD_START_MS : SPN_SPINNER_HOLD_END_MS;
    s->hold_ms = s->hold_total_ms;
    s->interp = sp_interp_build(s->forward ? 0.0f : 1.0f,
                                 s->forward ? 1.0f : 0.0f,
                                 SPN_SPINNER_CYCLE_MS);
    // render_forward stays the same until hold expires
  }
  s->value = sp_interp_ease_inout(&s->interp);
}

sp_str_t spn_spinner_render(spn_spinner_t* s) {
  sp_str_builder_t builder = SP_ZERO_INITIALIZE();
  f32 active_pos = s->value * (s->width - 1);

  // Calculate trail fade during hold period
  f32 trail_fade = 1.0f;
  if (s->hold_ms > 0 && s->hold_total_ms > 0) {
    // Fade trail out over the hold period
    trail_fade = s->hold_ms / s->hold_total_ms;
  }

  for (u32 i = 0; i < s->width; i++) {
    f32 pos = (f32)i;
    f32 abs_dist = active_pos - pos;
    if (abs_dist < 0) abs_dist = -abs_dist;

    // Use render_forward for trail direction (delays flip until hold ends)
    bool is_behind = s->render_forward ? (pos < active_pos) : (pos > active_pos);

    sp_color_t color = SP_ZERO_INITIALIZE();
    const c8* glyph;


    if (abs_dist < 1.0f) {
      // Head position
      color = s->color;
      glyph = SPN_SPINNER_ACTIVE;
    }
    else if (is_behind && abs_dist < (f32)SPN_SPINNER_TRAIL_LEN) {
      // Trail - fade based on distance AND hold fade
      f32 alpha = 1.0f;
      for (s32 j = 0; j < (s32)abs_dist; j++) {
        alpha *= 0.65f;
      }
      alpha *= trail_fade;  // Additional fade during hold
      // Clamp to inactive brightness minimum
      f32 inactive = 0.2f;
      alpha = inactive + alpha * (1.0f - inactive);

      sp_color_t hsv = sp_color_rgb_to_hsv(s->color);
      hsv.v *= alpha;

      color = sp_color_hsv_to_rgb(hsv);
      glyph = SPN_SPINNER_ACTIVE;
    }
    else {
      // Inactive
      sp_color_t hsv = sp_color_rgb_to_hsv(s->color);
      hsv.v *= 0.2f;
      color = sp_color_hsv_to_rgb(hsv);
      glyph = SPN_SPINNER_INACTIVE;
    }

    sp_str_builder_append_fmt(&builder, "{}{}",
      SP_FMT_STR(sp_color_to_tui_rgb(color)),
      SP_FMT_CSTR(glyph)
    );
  }

  sp_str_builder_append_cstr(&builder, SP_ANSI_RESET);
  return sp_str_builder_move(&builder);
}


sp_str_t sp_str_map_kernel_colorize(sp_str_map_context_t* context) {
  sp_str_t id = *(sp_str_t*)context->user_data;
  sp_str_t ansi = sp_format_color_id_to_ansi_fg(id);
  return sp_format("{}{}{}", SP_FMT_STR(ansi), SP_FMT_STR(context->str), SP_FMT_CSTR(SP_ANSI_RESET));
}

sp_str_t sp_os_get_bin_path() {
  sp_str_t path = sp_os_get_env_var(SP_LIT("HOME"));
  SP_ASSERT(!sp_str_empty(path));

  return sp_fs_join_path(path, sp_str_lit(".local/bin"));
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
  toml_table_t*     lib;
  toml_table_t*       src;
  toml_table_t*       a;
  toml_table_t*       so;
  toml_array_t*   bin;
  toml_array_t*   test;
  toml_table_t*   deps;
  toml_array_t*   profile;
  toml_array_t*   registry;
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


///////////////
// GENERATOR //
///////////////
typedef enum {
  SPN_GEN_KIND_RAW,
  SPN_GEN_KIND_SHELL,
  SPN_GEN_KIND_MAKE,
  SPN_GEN_KIND_CMAKE,
  SPN_GEN_KIND_PKGCONFIG,
} spn_generator_kind_t;

typedef enum {
  SPN_GEN_NONE,
  SPN_GEN_INCLUDE,
  SPN_GEN_LIB_INCLUDE,
  SPN_GEN_LIBS,
  SPN_GEN_SYSTEM_LIBS,
  SPN_GEN_RPATH,
  SPN_GEN_DEFINE,
  SPN_GEN_ALL,
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
  u8 padding [4];
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
  sp_ht(spn_pkg_linkage_t, bool) enabled;
  sp_str_t name;
} spn_lib_t;

#define SPN_VISIBILITY_KIND(X) \
  X(SPN_VISIBILITY_PUBLIC, "public") \
  X(SPN_VISIBILITY_TEST, "test")

typedef enum {
  SPN_VISIBILITY_KIND(SP_X_NAMED_ENUM_DEFINE)
} spn_visibility_t;

#define SPN_PACKAGE_KIND(X) \
  X(SPN_PACKAGE_KIND_NONE, "none") \
  X(SPN_PACKAGE_KIND_WORKSPACE, "workspace") \
  X(SPN_PACKAGE_KIND_FILE, "file") \
  X(SPN_PACKAGE_KIND_REMOTE, "remote") \
  X(SPN_PACKAGE_KIND_INDEX, "index")

typedef enum {
  SPN_PACKAGE_KIND(SP_X_NAMED_ENUM_DEFINE)
} spn_package_kind_t;


typedef struct {
  spn_visibility_t visibility;
  sp_str_t name;
  sp_da(sp_str_t) source;
  sp_da(sp_str_t) include;
  sp_da(sp_str_t) define;
} spn_target_t;

typedef enum {
  SPN_PROFILE_BUILTIN,
  SPN_PROFILE_USER,
} spn_profile_kind_t;

typedef struct {
  sp_str_t name;
  spn_pkg_linkage_t linkage;
  spn_libc_kind_t libc;
  spn_c_standard_t standard;
  spn_build_mode_t mode;
  spn_profile_kind_t kind;
  struct {
    spn_cc_kind_t kind;
    sp_str_t exe;
  } cc;
} spn_profile_t;

typedef struct {
  sp_str_t name;
  spn_package_kind_t kind;
  spn_visibility_t visibility;
  union {
    spn_semver_range_t range;
    sp_str_t file;
  };
} spn_pkg_req_t;

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
  spn_visibility_t visibility;
  spn_package_kind_t kind;
  sp_da(sp_str_t) deps;
  sp_da(sp_str_t) dependents;
} spn_lock_entry_t;

typedef struct {
  spn_semver_t version;
  sp_str_t commit;
  sp_ht(sp_str_t, spn_lock_entry_t) entries;
  sp_ht(sp_str_t, bool) system_deps;
} spn_lock_file_t;

typedef struct {
  sp_str_t name;
  sp_str_t location;
  spn_package_kind_t kind;
} spn_registry_t;

sp_str_t spn_registry_get_path(spn_registry_t* registry);

typedef sp_ht(sp_str_t, spn_pkg_req_t) spn_pkg_dep_reqs_t;


struct spn_pkg {
  sp_str_t name;
  sp_str_t repo;
  sp_str_t author;
  sp_str_t maintainer;
  spn_semver_t version;
  spn_lib_t lib;
  sp_ht(sp_str_t, spn_target_t) binaries;
  sp_ht(sp_str_t, spn_target_t) tests;
  spn_pkg_dep_reqs_t deps;
  sp_ht(sp_str_t, spn_dep_option_t) options;
  sp_ht(sp_str_t, spn_dep_options_t) config;
  sp_ht(spn_semver_t, spn_metadata_t) metadata;
  sp_da(spn_semver_t) versions;
  sp_da(spn_registry_t) registries;
  sp_da(sp_str_t) include;
  sp_da(sp_str_t) define;
  sp_da(sp_str_t) system_deps;
  sp_ht(sp_str_t, spn_profile_t) profiles;
  spn_package_kind_t kind;

  spn_build_fn_t on_configure;
  spn_build_fn_t on_build;
  spn_build_fn_t on_package;

  spn_package_paths_t paths;
};

spn_pkg_t     spn_pkg_new(sp_str_t path);
spn_pkg_t     spn_pkg_load(sp_str_t path);
spn_pkg_t     spn_pkg_from_default(sp_str_t path, sp_str_t name);
spn_pkg_t     spn_pkg_from_index(sp_str_t path);
spn_pkg_t     spn_pkg_from_manifest(sp_str_t path);
void          spn_pkg_add_dep(spn_pkg_t* package, spn_pkg_req_t request);
void          spn_pkg_add_dep_from_index(spn_pkg_t* package, sp_str_t name, spn_visibility_t visibility);
void          spn_pkg_init(spn_pkg_t* package);
void          spn_pkg_set_index(spn_pkg_t* package, sp_str_t path);
void          spn_pkg_set_manifest(spn_pkg_t* package, sp_str_t path);
void          spn_pkg_add_version(spn_pkg_t* package, spn_semver_t version, sp_str_t commit);
sp_str_t      spn_pkg_get_url(spn_pkg_t* build);



typedef struct {
  sp_str_t name;
  struct {
    bool public;
    bool test;
  } disabled;
} spn_target_filter_t;

bool spn_target_filter_pass(spn_target_filter_t* filter, spn_target_t* target);
bool spn_target_filter_pass_visibility(spn_target_filter_t* filter, spn_visibility_t visibility);



typedef struct {
  spn_bg_id_t build;
  spn_bg_id_t bin;
} spn_bin_build_nodes_t;

typedef struct {
  spn_bg_id_t fetch;
  spn_bg_id_t repo;
  spn_bg_id_t build;
  spn_bg_id_t stamp;
} spn_pkg_build_nodes_t;

typedef struct {
  u64 compile;
  u64 build;
  u64 package;
  u64 total;
} spn_build_time_t;

typedef struct spn_builder spn_builder_t;

typedef struct {
  sp_str_t name;
  spn_pkg_t* package;
  spn_builder_t* builder;
  struct {
    sp_str_t source;
    sp_str_t store;
    sp_str_t work;
  } paths;
} spn_build_ctx_config_t;

struct spn_build_ctx {
  sp_str_t name;
  spn_builder_t* builder;
  spn_pkg_t* package;
  spn_profile_t profile;

  struct {
    sp_str_t source;
    sp_str_t work;
    sp_str_t store;
    sp_str_t stamp;
    sp_str_t include;
    sp_str_t lib;
    sp_str_t bin;
    sp_str_t vendor;
    struct {
      sp_str_t build;
      sp_str_t test;
    } logs;
  } paths;

  spn_build_time_t time;
  sp_da(sp_ps_config_t) commands;
  sp_ps_t ps;
  struct {
    sp_io_stream_t build;
    sp_io_stream_t test;
  } logs;
};

struct spn_bin_ctx {
  spn_build_ctx_t ctx;
  spn_target_t* bin;
};

struct spn_pkg_ctx {
  spn_build_ctx_t ctx;
  spn_pkg_linkage_t linkage;
  spn_metadata_t metadata;
  spn_dep_options_t options;
  sp_hash_t build_id;
  sp_str_t message;
  bool force;

  spn_dep_state_t state;
  sp_thread_t thread;
  sp_mutex_t mutex;
  sp_str_t error;
  spn_pkg_build_nodes_t nodes;
};

struct spn_builder {
  spn_target_filter_t filter;
  spn_profile_t profile;
  sp_da(spn_target_t*) targets;
  struct {
    sp_ht(sp_str_t, spn_bin_ctx_t) bins;
    sp_ht(sp_str_t, spn_pkg_ctx_t) deps;
    spn_build_ctx_t package;
  } contexts;
  spn_build_graph_t graph;
  spn_bg_dirty_t* dirty;
  spn_bg_executor_t* executor;
  sp_mutex_t mutex;
};

spn_builder_t* spn_builder_new();
void           spn_builder_init(spn_builder_t* build);
spn_pkg_ctx_t* spn_builder_find_pkg_ctx(spn_builder_t* build, sp_str_t name);


spn_generator_kind_t   spn_gen_kind_from_str(sp_str_t str);
spn_gen_entry_kind_t   spn_gen_entry_from_str(sp_str_t str);
spn_cc_kind_t          spn_cc_kind_from_str(sp_str_t str);
spn_package_kind_t     spn_registry_kind_from_str(sp_str_t str);
sp_str_t               spn_gen_format_entry_for_compiler(sp_str_t entry, spn_gen_entry_kind_t kind, spn_cc_kind_t compiler);
sp_str_t               spn_cc_kind_to_executable(spn_cc_kind_t compiler);
sp_str_t               spn_cc_c_standard_to_switch(spn_c_standard_t standard);
sp_str_t               spn_cc_lib_kind_to_switch(spn_pkg_linkage_t kind);
sp_str_t               spn_cc_build_mode_to_switch(spn_build_mode_t mode);
sp_dyn_array(sp_str_t) spn_gen_build_entry_for_dep(spn_pkg_ctx_t* dep, spn_gen_entry_kind_t kind, spn_cc_kind_t c);
sp_str_t               spn_gen_build_entries_for_dep(spn_pkg_ctx_t* dep, spn_cc_kind_t c);
sp_str_t               spn_gen_build_entries_for_all(spn_gen_entry_kind_t kind, spn_cc_kind_t c);
sp_str_t               spn_print_system_deps_only(spn_cc_kind_t compiler);
sp_str_t               spn_print_deps_only(spn_gen_entry_kind_t kind, spn_cc_kind_t compiler);
spn_c_standard_t       spn_c_standard_from_str(sp_str_t str);
sp_str_t               spn_c_standard_to_str(spn_c_standard_t standard);
sp_str_t               spn_opt_cstr_required(const c8* value);
sp_str_t               spn_opt_cstr_optional(const c8* value, const c8* fallback);

spn_dir_kind_t    spn_cache_dir_kind_from_str(sp_str_t str);
sp_str_t          spn_cache_dir_kind_to_dep_path(spn_pkg_ctx_t* dep, spn_dir_kind_t kind);
spn_pkg_linkage_t spn_pkg_linkage_from_str(sp_str_t str);
sp_os_lib_kind_t  spn_pkg_linkage_to_sp_os_lib_kind(spn_pkg_linkage_t kind);
sp_str_t          spn_pkg_linkage_to_str(spn_pkg_linkage_t kind);
void              spn_dep_context_set_build_state(spn_pkg_ctx_t* dep, spn_dep_state_t state);
void              spn_dep_context_set_build_error(spn_pkg_ctx_t* dep, sp_str_t error);

void              spn_build_ctx_init(spn_build_ctx_t* ctx, spn_build_ctx_config_t config);
spn_err_t         spn_build_ctx_compile(spn_build_ctx_t* build);
spn_err_t         spn_build_ctx_run_script(spn_build_ctx_t* build);
void              spn_build_ctx_prepare_io(spn_build_ctx_t* build);
sp_ps_output_t    spn_build_ctx_subprocess(spn_build_ctx_t* build, sp_ps_config_t config);
spn_err_t         spn_bin_build_run(spn_bin_ctx_t* build);
spn_err_t         spn_pkg_build_run(spn_pkg_ctx_t* dep);
spn_err_t         spn_pkg_build_sync_remote(spn_pkg_ctx_t* dep);
spn_err_t         spn_pkg_build_sync_local(spn_pkg_ctx_t* dep);
spn_err_t         spn_pkg_build_resolve_commit(spn_pkg_ctx_t* dep);
void              spn_pkg_build_stamp(spn_pkg_ctx_t* build);
sp_str_t          spn_pkg_build_get_target_path(spn_pkg_ctx_t* build, spn_target_t* target);
bool              spn_pkg_build_is_stamped(spn_pkg_ctx_t* context);
sp_str_t          spn_pkg_build_get_include_dir(spn_pkg_ctx_t* pkg);
sp_str_t          spn_pkg_build_get_lib_dir(spn_pkg_ctx_t* pkg);
sp_str_t          spn_pkg_build_get_lib(spn_pkg_ctx_t* pkg);
sp_str_t          spn_pkg_build_get_rpath(spn_pkg_ctx_t* pkg);

spn_lock_file_t   spn_load_lock_file(sp_str_t path);
spn_pkg_req_t     spn_pkg_req_from_str(sp_str_t str);
sp_str_t          spn_pkg_req_to_str(spn_pkg_req_t request);


#define SPN_RESOLVE_STRATEGY(X) \
  X(SPN_RESOLVE_STRATEGY_LOCK_FILE, "lock") \
  X(SPN_RESOLVE_STRATEGY_SOLVER, "solver")

typedef enum {
  SPN_RESOLVE_STRATEGY(SP_X_NAMED_ENUM_DEFINE)
} spn_resolve_strategy_t;

typedef struct {
  sp_opt(u32) low;
  sp_opt(u32) high;
  spn_pkg_req_t source;
} spn_dep_version_range_t;

typedef struct {
  sp_str_t name;
  spn_package_kind_t kind;
  spn_semver_t version;
} spn_resolved_pkg_t;

typedef struct {
  sp_ht(sp_str_t, sp_da(spn_dep_version_range_t)) ranges;
  sp_ht(sp_str_t, bool) visited;
  sp_da(sp_str_t) system_deps;
  sp_ht(sp_str_t, spn_resolved_pkg_t) resolved;
  int versions;
} spn_resolver_t;



#define SPN_BUILD_EVENT(X) \
  X(SPN_BUILD_EVENT_FETCH, "fetch") \
  X(SPN_BUILD_EVENT_RESOLVE, "resolve") \
  X(SPN_BUILD_EVENT_SYNC, "sync") \
  X(SPN_BUILD_EVENT_CHECKOUT, "checkout") \
  X(SPN_BUILD_EVENT_BUILD, "build") \
  X(SPN_BUILD_EVENT_DEP_BUILD, "build") \
  X(SPN_BUILD_EVENT_DEP_BUILD_PASSED, "ok") \
  X(SPN_BUILD_EVENT_DEP_BUILD_FAILED, "failed") \
  X(SPN_BUILD_EVENT_TARGET_BUILD, "build") \
  X(SPN_BUILD_EVENT_TARGET_BUILD_PASSED, "ok") \
  X(SPN_BUILD_EVENT_TARGET_BUILD_FAILED, "failed") \
  X(SPN_BUILD_EVENT_BUILD_PASSED, "done") \
  X(SPN_BUILD_EVENT_PACKAGE, "package") \
  X(SPN_BUILD_EVENT_CANCEL, "cancel") \
  X(SPN_BUILD_EVENT_COMPILE, "compile") \
  X(SPN_BUILD_EVENT_TEST_RUN, "run") \
  X(SPN_BUILD_EVENT_TEST_PASSED, "passed") \
  X(SPN_BUILD_EVENT_TEST_FAILED, "failed")

typedef enum {
  SPN_BUILD_EVENT(SP_X_NAMED_ENUM_DEFINE)
} spn_build_event_kind_t;

typedef struct {
  spn_build_event_kind_t kind;
  spn_build_ctx_t* ctx;

  union {
    struct { sp_ps_config_t ps; } compile;
    struct { sp_str_t commit; spn_semver_t version; sp_str_t message; } checkout;
    struct { u64 time; } done;
    struct { spn_resolve_strategy_t strategy; } resolve;
  };
} spn_build_event_t;

typedef struct {
  sp_rb(spn_build_event_t) buffer;
  sp_mutex_t mutex;
  sp_cv_t condition;
} spn_event_buffer_t;

spn_event_buffer_t*      spn_event_buffer_new();
void                     spn_event_buffer_push(spn_event_buffer_t* evs, spn_build_ctx_t* ctx, spn_build_event_kind_t k);
void                     spn_event_buffer_push_ex(spn_event_buffer_t* evs, spn_build_ctx_t* ctx, spn_build_event_t e);
sp_da(spn_build_event_t) spn_event_buffer_drain(spn_event_buffer_t* events);
spn_build_event_t        spn_build_event_make(spn_build_ctx_t* ctx, spn_build_event_kind_t kind);
sp_str_t                 spn_build_event_kind_to_str(spn_build_event_kind_t);



////////////
// CONFIG //
////////////
typedef struct {
  sp_str_t dir;
  sp_str_t   lock;
  sp_str_t   build;
  sp_str_t     profile;
} spn_app_paths_t;

struct spn_config {
  sp_str_t spn;
  sp_da(spn_registry_t) registries;
};


///////////
// TOOLS //
///////////
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



///////////////
// GENERATOR //
///////////////
typedef struct {
  spn_gen_entry_kind_t kind;
  spn_cc_kind_t compiler;
} spn_gen_format_context_t;

sp_str_t spn_generator_format_entry_kernel(sp_str_map_context_t* context);



/////////
// TCC //
/////////
typedef TCCState spn_tcc_t;

spn_tcc_t* spn_tcc_new(spn_build_ctx_t* ctx);
spn_err_t  spn_tcc_add_file(spn_tcc_t* tcc, sp_str_t file_path);
spn_err_t  spn_tcc_register(spn_tcc_t* tcc);
void       spn_tcc_error(void* opaque, const char* message);
void       spn_tcc_list_fn(void* opaque, const char* name, const void* value);



/////////
// TUI //
/////////
#define SPN_TUI_NUM_OPTIONS 3
#define SP_TUI_PRINT(command) sp_io_write_cstr(&spn.logger.err, command);

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
  SP_TUI_TABLE_SETUP,
  SP_TUI_TABLE_BUILDING,
} sp_tui_table_state_t;

typedef struct {
  u32 row;
  u32 col;
} sp_tui_cursor_t;

typedef struct {
  sp_str_t name;
  u32 min_width;
} sp_tui_column_t;

typedef struct {
  sp_da(sp_tui_column_t) cols;
  sp_da(sp_da(sp_str_t)) rows;
  sp_tui_cursor_t cursor;
  sp_tui_table_state_t state;
  u32 columns;
  u32 indent;
} sp_tui_table_t;

typedef struct {
  spn_tui_mode_t mode;
  u32 num_deps;
  u32 width;
  sp_ht(sp_str_t, spn_dep_state_t) state;
  spn_builder_t* build;
  spn_spinner_t spinner;

  struct {
    u32 num_done;
    u32 num_total;
    u32 max_name;
  } info;

  struct {
    sp_tm_timer_t timer;
    u64 accumulated;
  } frame;

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

#define SP_TUI_COLOR(r, g, b) "\033[38;2;" SP_MACRO_STR(r) ";" SP_MACRO_STR(g) ";" SP_MACRO_STR(b) "m"
#define SP_TUI_INDIAN_RED       SP_TUI_COLOR(180, 101, 111)
#define SP_TUI_TYRIAN_PURPLE    SP_TUI_COLOR(95,  26,  55)
#define SP_TUI_CARDINAL         SP_TUI_COLOR(194, 37,  50)
#define SP_TUI_CELADON          SP_TUI_COLOR(183, 227, 204)
#define SP_TUI_SPRING_GREEN     SP_TUI_COLOR(89,  255, 160)
#define SP_TUI_MINDARO          SP_TUI_COLOR(188, 231, 132)
#define SP_TUI_LIGHT_GREEN      SP_TUI_COLOR(161, 239, 139)
#define SP_TUI_ZOMP             SP_TUI_COLOR(99,  160, 136)
#define SP_TUI_MIDNIGHT_GREEN   SP_TUI_COLOR(25,  83,  95)
#define SP_TUI_PRUSSIAN_BLUE    SP_TUI_COLOR(16,  43,  63)
#define SP_TUI_ORANGE           SP_TUI_COLOR(249, 166, 32)
#define SP_TUI_SUNGLOW          SP_TUI_COLOR(255, 209, 102)
#define SP_TUI_SELECTIVE_YELLOW SP_TUI_COLOR(250, 188, 42)
#define SP_TUI_GUNMETAL         SP_TUI_COLOR(43,  61,  65)
#define SP_TUI_PAYNES_GRAY      SP_TUI_COLOR(76,  95,  107)
#define SP_TUI_CADET_GRAY       SP_TUI_COLOR(131, 160, 160)
#define SP_TUI_CHARCOAL         SP_TUI_COLOR(64,  67,  78)
#define SP_TUI_COOL_GRAY        SP_TUI_COLOR(140, 148, 173)
#define SP_TUI_CREAM            SP_TUI_COLOR(245, 255, 198)
#define SP_TUI_MISTY_ROSE       SP_TUI_COLOR(255, 227, 227)
#define SP_TUI_TAUPE            SP_TUI_COLOR(68,  53,  39)
#define SP_TUI_DARK_GREEN       SP_TUI_COLOR(4,   27,  21)
#define SP_TUI_RICH_BLACK       SP_TUI_COLOR(4,   10,  15)
#define SP_TUI_WHITE            SP_TUI_COLOR(255, 255, 255)


void     sp_tui_print(sp_str_t str);
void     sp_tui_up(u32 n);
void     sp_tui_down(u32 n);
void     sp_tui_clear_line();
void     sp_tui_show_cursor();
void     sp_tui_hide_cursor();
void     sp_tui_home();
void     sp_tui_flush();
void     sp_tui_checkpoint(spn_tui_t* tui);
void     sp_tui_restore(spn_tui_t* tui);
void     sp_tui_setup_raw_mode(spn_tui_t* tui);
void     sp_tui_begin_table(sp_tui_table_t* table);
void     sp_tui_table_setup_column(sp_tui_table_t* table, sp_str_t name);
void     sp_tui_table_setup_column_ex(sp_tui_table_t* table, sp_str_t name, u32 min_width);
void     sp_tui_table_header_row(sp_tui_table_t* table);
void     sp_tui_table_next_row(sp_tui_table_t* table);
void     sp_tui_table_column(sp_tui_table_t* table, u32 n);
void     sp_tui_table_column_named(sp_tui_table_t* table, sp_str_t name);
void     sp_tui_table_fmt(sp_tui_table_t* table, const c8* fmt, ...);
void     sp_tui_table_str(sp_tui_table_t* table, sp_str_t str);
void     sp_tui_table_set_indent(sp_tui_table_t* table, u32 indent);
void     sp_tui_table_end(sp_tui_table_t* table);
sp_str_t sp_tui_table_render(sp_tui_table_t* table);
void     spn_tui_init(spn_tui_t* tui, spn_tui_mode_t mode, spn_builder_t* build);


/////////
// CLI //
/////////
#define SPN_CLI_COMMAND(X) \
  X(SPN_CLI_INIT, "init") \
  X(SPN_CLI_ADD, "add") \
  X(SPN_CLI_BUILD, "build") \
  X(SPN_CLI_TEST, "test") \
  X(SPN_CLI_CLEAN, "clean") \
  X(SPN_CLI_GENERATE, "generate") \
  X(SPN_CLI_COPY, "copy") \
  X(SPN_CLI_UPDATE, "update") \
  X(SPN_CLI_LIST, "list") \
  X(SPN_CLI_WHICH, "which") \
  X(SPN_CLI_LS, "ls") \
  X(SPN_CLI_MANIFEST, "manifest") \
  X(SPN_CLI_TOOL, "tool")

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

#define SPN_CLI_IMPLEMENTATION
#define SPN_CLI_HELP
#include "cli.h"

typedef struct {
  sp_str_t package;
  bool test;
} spn_cli_add_t;

typedef struct {
  sp_str_t package;
} spn_cli_update_t;

typedef struct {
  union {
    sp_str_t package;
    sp_str_t dir;
  };
  bool force;
  sp_str_t version;
} spn_cli_tool_install_t;

typedef struct {
  sp_str_t package;
  sp_str_t command;
} spn_cli_tool_run_t;

typedef struct {
  spn_tool_cmd_t subcommand;
  union {
    spn_cli_tool_install_t install;
    spn_cli_tool_run_t run;
  };
} spn_cli_tool_t;

typedef struct {
  bool bare;
} spn_cli_init_t;

typedef struct {
  bool force;
  bool tests;
  sp_str_t target;
  sp_str_t profile;
} spn_cli_build_t;

typedef struct {
  sp_str_t target;
  sp_str_t profile;
} spn_cli_test_t;

typedef struct {
  sp_str_t generator;
  sp_str_t compiler;
  sp_str_t path;
} spn_cli_generate_t;

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

struct spn_cli {
  u32 num_args;
  const c8** args;
  sp_str_t project_dir;
  sp_str_t project_file;
  sp_str_t output;
  bool help;

  spn_cli_command_usage_t cmd;
  spn_cli_add_t add;
  spn_cli_update_t update;
  spn_cli_tool_t tool;
  spn_cli_init_t init;
  spn_cli_generate_t generate;
  spn_cli_build_t build;
  spn_cli_test_t test;
  spn_cli_ls_t ls;
  spn_cli_which_t which;
  spn_cli_manifest_t manifest;
  spn_cli_copy_t copy;
};

spn_cli_result_t spn_cli_clean(spn_cli_t* cli);
spn_cli_result_t spn_cli_build(spn_cli_t* cli);
spn_cli_result_t spn_cli_test(spn_cli_t* cli);
spn_cli_result_t spn_cli_generate(spn_cli_t* cli);
spn_cli_result_t spn_cli_copy(spn_cli_t* cli);
spn_cli_result_t spn_cli_init(spn_cli_t* cli);
spn_cli_result_t spn_cli_add(spn_cli_t* cli);
spn_cli_result_t spn_cli_update(spn_cli_t* cli);
spn_cli_result_t spn_cli_tool(spn_cli_t* cli);
spn_cli_result_t spn_cli_tool_install(spn_cli_t* cli);
spn_cli_result_t spn_cli_tool_uninstall(spn_cli_t* cli);
spn_cli_result_t spn_cli_tool_run(spn_cli_t* cli);
spn_cli_result_t spn_cli_list(spn_cli_t* cli);
spn_cli_result_t spn_cli_ls(spn_cli_t* cli);
spn_cli_result_t spn_cli_which(spn_cli_t* cli);
spn_cli_result_t spn_cli_manifest(spn_cli_t* cli);
spn_cli_result_t spn_cli_root(spn_cli_t* cli);
spn_cli_result_t spn_cli_help(spn_cli_parser_t* p);

#define SPN_CLI_UNIMPLEMENTED() SP_LOG("unimplemented"); return SPN_CLI_ERR;

/////////
// APP //
/////////
typedef enum {
  SPN_APP_STATE_INIT,
  SPN_APP_STATE_BUILDING,
  SPN_APP_STATE_PREPARE_RUN,
  SPN_APP_STATE_RUNNING,
} spn_app_state_t;

typedef struct {
  spn_target_filter_t filter;
  spn_profile_t profile;
  bool force;
  struct { bool tests; bool build; } tasks;
} spn_app_config_t;

typedef struct {
  spn_app_paths_t paths;
  spn_pkg_t package;
  sp_opt(spn_lock_file_t) lock;
  spn_resolver_t resolver;
  spn_builder_t build;
  spn_app_state_t state;

  spn_app_config_t config;

  sp_da(sp_str_t) search;
  sp_ht(sp_str_t, sp_str_t) registry;
  sp_ht(sp_str_t, spn_pkg_t) cache;
} spn_app_t;

typedef enum {
  SPN_APP_INIT_NORMAL,
  SPN_APP_INIT_BARE,
} spn_app_init_mode_t;

spn_app_t app;

spn_app_t      spn_app_new();
void           spn_app_load(spn_app_t* app, sp_str_t manifest_path);
void           spn_app_resolve(spn_app_t* app);
void           spn_app_prepare_build(spn_app_t* app);
void           spn_app_resolve_from_solver(spn_app_t* app);
void           spn_app_update_lock_file(spn_app_t* app);
void           spn_app_resolve_from_lock_file(spn_app_t* app);
void           spn_app_write_manifest(spn_pkg_t* package, sp_str_t path);
spn_pkg_t*     spn_app_find_package(spn_app_t* app, sp_str_t name);
spn_pkg_t*     spn_app_find_package_from_request(spn_app_t* app, spn_pkg_req_t dep);
spn_pkg_t*     spn_app_ensure_package(spn_app_t* app, spn_pkg_req_t dep);
s32            spn_app_thread_build_binary(void* user_data);
void           spn_app_add_package_constraints(spn_app_t* app, spn_pkg_t* package);
void           spn_app_bail_on_missing_package(spn_app_t* app, sp_str_t name);
spn_app_t      spn_app_init_and_write(sp_str_t path, sp_str_t name, spn_app_init_mode_t mode);
void           spn_app_update_dep(spn_app_t* app, sp_str_t name);
void           spn_resolver_init(spn_resolver_t* r);
void           spn_tool_ensure_manifest();
void           spn_tool_list();
void           spn_tool_run(sp_str_t package_name, sp_da(sp_str_t) args);
void           spn_tool_upgrade(sp_str_t package_name);
sp_str_t       spn_get_tool_path(spn_target_t* bin);

typedef struct {
  spn_cli_t cli;
  struct {
    spn_tools_paths_t tools;
    sp_str_t cwd;
    sp_str_t project;
    sp_str_t   manifest;
    sp_str_t executable;
    sp_str_t config_dir;     // $HOME/.config/spn
    sp_str_t   config;       // $HOME/.config/spn/spn.toml
    sp_str_t bin;            // $HOME/.local/bin
    sp_str_t storage;        // $HOME/.loca/share/spn
    sp_str_t   runtime;      // $HOME/.loca/share/spn/runtime
    sp_str_t   log;          // $HOME/.loca/share/spn/log
    sp_str_t     invocation; // $HOME/.loca/share/spn/log/$ISO_TIMESTAMP.log
    sp_str_t   spn;
    sp_str_t     include;
    sp_str_t     index;
    sp_str_t   cache;
    sp_str_t     build;
    sp_str_t     store;
    sp_str_t     source;
  } paths;
  spn_tui_t tui;
  sp_atomic_s32 control;
  sp_str_t tcc_error;
  spn_config_t config;
  spn_registry_t registry;
  spn_event_buffer_t* events;
  sp_app_t* sp;
  s32 num_args;
  const c8** args;

  struct {
    sp_io_stream_t out;
    sp_io_stream_t err;
  } logger;
} spn_ctx_t;

spn_ctx_t spn;
sp_app_result_t spn_init(sp_app_t* app);
sp_app_result_t spn_poll(sp_app_t* app);
sp_app_result_t spn_update(sp_app_t* app);
void spn_ctx_log(const c8* fmt, ...);
void spn_ctx_warn(const c8* fmt, ...);
void spn_ctx_error(const c8* fmt, ...);
void spn_ctx_tui(const c8* fmt, ...);



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
  sp_da(sp_str_t) libs;
  sp_da(sp_str_t) lib_dirs;
  sp_da(sp_str_t) rpath;
  sp_da(sp_io_stream_t) embed;

  spn_cc_target_kind_t kind;
  union {
    spn_cc_executable_t exe;
    spn_cc_shared_lib_t shared_lib;
    spn_cc_static_lib_t static_lib;
  };
} spn_cc_target_t;

struct spn_cc {
  spn_build_ctx_t* build;
  sp_da(sp_str_t) include;
  sp_da(sp_str_t) define;
  sp_str_t dir;
  sp_da(spn_cc_target_t) targets;
  sp_ps_config_t config;
};

struct spn_make {
  spn_pkg_ctx_t* build;
  sp_str_t target;
};

struct spn_autoconf {
  spn_pkg_ctx_t* build;
  sp_da(sp_str_t) flags;
};

typedef struct {
  sp_str_t name;
  sp_str_t value;
} spn_cmake_define_t;

struct spn_cmake {
  spn_pkg_ctx_t* build;
  spn_cmake_gen_t generator;
  sp_da(spn_cmake_define_t) defines;
  sp_da(sp_str_t) args;
};

spn_cc_t         spn_cc_new(spn_build_ctx_t* build);
void             spn_cc_add_include_rel(spn_cc_t* cc, sp_str_t dir);
void             spn_cc_add_define(spn_cc_t* cc, sp_str_t var);
void             spn_cc_set_output_dir(spn_cc_t* cc, sp_str_t dir);
spn_cc_target_t* spn_cc_add_target(spn_cc_t* cc, spn_cc_target_kind_t kind, sp_str_t name);
void             spn_cc_target_add_source(spn_cc_target_t* cc, sp_str_t file_path);
void             spn_cc_target_add_define(spn_cc_target_t* cc, sp_str_t var);
void             spn_cc_target_add_include_rel(spn_cc_target_t* cc, sp_str_t dir);
void             spn_cc_target_add_include_abs(spn_cc_target_t* cc, sp_str_t dir);
void             spn_cc_target_add_lib(spn_cc_target_t* cc, sp_str_t lib);
void             spn_cc_target_add_lib_dir(spn_cc_target_t* cc, sp_str_t dir);
void             spn_cc_target_add_rpath(spn_cc_target_t* cc, sp_str_t dir);
void             spn_cc_target_embed_file(spn_cc_target_t* cc, sp_str_t file_path);
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
  APPLY(spn_cmake) \
  APPLY(spn_cmake_new) \
  APPLY(spn_cmake_set_generator) \
  APPLY(spn_cmake_add_define) \
  APPLY(spn_cmake_add_arg) \
  APPLY(spn_cmake_configure) \
  APPLY(spn_cmake_build) \
  APPLY(spn_cmake_install) \
  APPLY(spn_cmake_run) \
  APPLY(spn_log) \
  APPLY(spn_get_libc) \
  APPLY(spn_get_target) \
  APPLY(spn_copy)

#define SPN_DEFINE_LIB_ENTRY(SYM) { .symbol = SP_MACRO_STR(SYM), .fn = SYM },

spn_lib_fn_t spn_lib [] = {
  SPN_LIB_ENTRIES(SPN_DEFINE_LIB_ENTRY)
};

void spn_make(spn_pkg_ctx_t* build) {
  spn_make_t* make = spn_make_new(build);
  spn_make_run(make);
}

spn_make_t* spn_make_new(spn_pkg_ctx_t* dep) {
  spn_make_t* make = SP_ALLOC(spn_make_t);
  make->build = dep;
  return make;
}

void spn_make_add_target(spn_make_t* make, const c8* target) {
  make->target = sp_str_from_cstr(target);
}

void spn_make_run(spn_make_t* make) {
  spn_pkg_ctx_t* build = make->build;

  sp_ps_config_t ps = SP_ZERO_INITIALIZE();
  ps.command = SP_LIT("make");
  sp_ps_config_add_arg(&ps, SP_LIT("--quiet"));
  sp_ps_config_add_arg(&ps, SP_LIT("--directory"));
  sp_ps_config_add_arg(&ps, build->ctx.paths.work);
  if (!sp_str_empty(make->target)) {
    sp_ps_config_add_arg(&ps, make->target);
  }

  spn_build_ctx_subprocess(&build->ctx, ps);
}

void spn_autoconf(spn_pkg_ctx_t* build) {
  spn_autoconf_t* autoconf = spn_autoconf_new(build);
  spn_autoconf_run(autoconf);
}

spn_autoconf_t* spn_autoconf_new(spn_pkg_ctx_t* dep) {
  spn_autoconf_t* autoconf = SP_ALLOC(spn_autoconf_t);
  autoconf->build = dep;
  return autoconf;
}

void spn_autoconf_run(spn_autoconf_t* autoconf) {
  spn_pkg_ctx_t* build = autoconf->build;

  sp_ps_config_t config = {
    .command = sp_fs_join_path(build->ctx.paths.source, SP_LIT("configure")),
    .args = {
      sp_format("--prefix={}", SP_FMT_STR(build->ctx.paths.store)),
      build->linkage == SPN_LIB_KIND_SHARED ?
        SP_LIT("--enable-shared") :
        SP_LIT("--disable-shared"),
      build->linkage == SPN_LIB_KIND_STATIC ?
        SP_LIT("--enable-static") :
        SP_LIT("--disable-static"),
    },
  };

  sp_da_for(autoconf->flags, it) {
    sp_ps_config_add_arg(&config, autoconf->flags[it]);
  }

  sp_ps_output_t result = spn_build_ctx_subprocess(&build->ctx, config);
}

void spn_autoconf_add_flag(spn_autoconf_t* autoconf, const c8* flag) {
  sp_da_push(autoconf->flags, sp_str_from_cstr(flag));
}

sp_str_t spn_cmake_gen_to_str(spn_cmake_gen_t gen) {
  switch (gen) {
    case SPN_CMAKE_GEN_DEFAULT:        return (sp_str_t){0};
    case SPN_CMAKE_GEN_UNIX_MAKEFILES: return sp_str_lit("Unix Makefiles");
    case SPN_CMAKE_GEN_NINJA:          return sp_str_lit("Ninja");
    case SPN_CMAKE_GEN_XCODE:          return sp_str_lit("Xcode");
    case SPN_CMAKE_GEN_MSVC:           return sp_str_lit("Visual Studio 17 2022");
    case SPN_CMAKE_GEN_MINGW:          return sp_str_lit("MinGW Makefiles");
  }
  return (sp_str_t){0};
}

void spn_cmake(spn_pkg_ctx_t* build) {
  spn_cmake_t* cmake = spn_cmake_new(build);
  spn_cmake_run(cmake);
}

spn_cmake_t* spn_cmake_new(spn_pkg_ctx_t* build) {
  spn_cmake_t* cmake = SP_ALLOC(spn_cmake_t);
  cmake->build = build;
  cmake->generator = SPN_CMAKE_GEN_DEFAULT;
  return cmake;
}

void spn_cmake_set_generator(spn_cmake_t* cmake, spn_cmake_gen_t gen) {
  cmake->generator = gen;
}

static sp_str_t spn_cmake_format_define(sp_str_t name, sp_str_t value) {
  return sp_format("-D{}={}", SP_FMT_STR(name), SP_FMT_STR(value));
}

void spn_cmake_add_define(spn_cmake_t* cmake, const c8* name, const c8* value) {
  spn_cmake_define_t def = {
    .name = sp_str_from_cstr(name),
    .value = sp_str_from_cstr(value),
  };
  sp_da_push(cmake->defines, def);
}

void spn_cmake_add_arg(spn_cmake_t* cmake, const c8* arg) {
  sp_da_push(cmake->args, sp_str_from_cstr(arg));
}

void spn_cmake_configure(spn_cmake_t* cmake) {
  spn_pkg_ctx_t* build = cmake->build;

  sp_ps_config_t config = {
    .command = SP_LIT("cmake"),
    .args = {
      SP_LIT("-S"), build->ctx.paths.source,
      SP_LIT("-B"), build->ctx.paths.work,
    }
  };

  if (cmake->generator != SPN_CMAKE_GEN_DEFAULT) {
    sp_ps_config_add_arg(&config, SP_LIT("-G"));
    sp_ps_config_add_arg(&config, spn_cmake_gen_to_str(cmake->generator));
  }

  sp_ps_config_add_arg(&config, spn_cmake_format_define(
    SP_LIT("CMAKE_INSTALL_PREFIX"),
    build->ctx.paths.store)
  );

  sp_ps_config_add_arg(&config, spn_cmake_format_define(
    SP_LIT("BUILD_SHARED_LIBS"),
    build->linkage == SPN_LIB_KIND_SHARED ? SP_LIT("ON") : SP_LIT("OFF"))
  );

  sp_ps_config_add_arg(&config, spn_cmake_format_define(
    SP_LIT("CMAKE_BUILD_TYPE"),
    build->ctx.profile.mode == SPN_DEP_BUILD_MODE_RELEASE ? SP_LIT("Release") : SP_LIT("Debug"))
  );

  sp_da_for(cmake->defines, it) {
    spn_cmake_define_t define = cmake->defines[it];
    sp_ps_config_add_arg(&config, spn_cmake_format_define(define.name, define.value));
  }

  sp_da_for(cmake->args, it) {
    sp_ps_config_add_arg(&config, cmake->args[it]);
  }

  spn_build_ctx_subprocess(&build->ctx, config);
}

void spn_cmake_build(spn_cmake_t* cmake) {
  spn_pkg_ctx_t* build = cmake->build;

  spn_build_ctx_subprocess(&build->ctx, (sp_ps_config_t) {
    .command = SP_LIT("cmake"),
    .args = {
      SP_LIT("--build"),
      build->ctx.paths.work
    }
  });
}

void spn_cmake_install(spn_cmake_t* cmake) {
  spn_pkg_ctx_t* build = cmake->build;

  spn_build_ctx_subprocess(&build->ctx, (sp_ps_config_t) {
    .command = SP_LIT("cmake"),
    .args = {
      SP_LIT("--install"),
      build->ctx.paths.work
    }
  });
}

void spn_cmake_run(spn_cmake_t* cmake) {
  spn_cmake_configure(cmake);
  spn_cmake_build(cmake);
}

spn_cc_t spn_cc_new(spn_build_ctx_t* build) {
  return (spn_cc_t) {
    .build = build,
  };
}

void spn_cc_add_include_rel(spn_cc_t* cc, sp_str_t dir) {
  sp_da_push(cc->include, sp_fs_join_path(app.paths.dir, dir));
}

void spn_cc_add_define(spn_cc_t* cc, sp_str_t var) {
  sp_da_push(cc->define, var);
}

void spn_cc_target_add_source(spn_cc_target_t* target, sp_str_t file_path) {
  sp_da_push(target->source, sp_fs_join_path(app.paths.dir, file_path));
}

void spn_cc_target_add_include_rel(spn_cc_target_t* target, sp_str_t dir) {
  spn_cc_target_add_include_abs(target, sp_fs_join_path(app.paths.dir, dir));
}

void spn_cc_target_add_include_abs(spn_cc_target_t* target, sp_str_t dir) {
  // @spader probably, this is bad. shouldn't be able to include arbitrary files out-of-tree
  sp_da_push(target->include, dir);
}

void spn_cc_target_add_define(spn_cc_target_t* target, sp_str_t var) {
  sp_da_push(target->define, sp_str_copy(var));
}

void spn_cc_target_add_lib(spn_cc_target_t* target, sp_str_t lib) {
  sp_da_push(target->libs, sp_str_copy(lib));
}

void spn_cc_target_add_lib_dir(spn_cc_target_t* target, sp_str_t dir) {
  sp_da_push(target->lib_dirs, sp_str_copy(dir));
}

void spn_cc_target_add_rpath(spn_cc_target_t* target, sp_str_t dir) {
  sp_da_push(target->rpath, sp_str_copy(dir));
}

void spn_cc_target_embed_file(spn_cc_target_t* cc, sp_str_t file_path) {
  //sp_io_from_file(file_path, sp_io_mode_t mode)
}

// @lib
// manifests for third party packages should still use [[lib]], so everything
// would flow through this code path. the pkg_ctx version won't exist
void spn_cc_target_add_library(spn_cc_target_t* target, spn_build_ctx_t* ctx) {

}

void spn_cc_target_add_dep(spn_cc_target_t* target, spn_pkg_ctx_t* pkg) {
  spn_cc_target_add_include_abs(target, spn_pkg_build_get_include_dir(pkg));

  switch (pkg->linkage) {
    case SPN_LIB_KIND_STATIC: {
      spn_cc_target_add_lib(target, spn_pkg_build_get_lib(pkg));
      break;
    }
    case SPN_LIB_KIND_SHARED: {
      spn_cc_target_add_rpath(target, spn_pkg_build_get_rpath(pkg));
      spn_cc_target_add_lib(target, spn_pkg_build_get_lib(pkg));
      break;
    }
    case SPN_LIB_KIND_SOURCE:
    case SPN_LIB_KIND_NONE: {
      break;
    }
  }
}

void spn_cc_set_output_dir(spn_cc_t* cc, sp_str_t dir) {
  cc->dir = sp_str_copy(dir);
}

spn_cc_target_t* spn_cc_add_target(spn_cc_t* cc, spn_cc_target_kind_t kind, sp_str_t name) {
  spn_cc_target_t target = {
    .name = sp_str_copy(name),
    .kind = kind
  };
  sp_da_push(cc->targets, target);
  return sp_da_back(cc->targets);
}

void spn_cc_ps_add_includes(sp_ps_config_t* ps, sp_da(sp_str_t) includes, spn_cc_kind_t compiler) {
  sp_da_for(includes, j) {
    sp_ps_config_add_arg(ps, spn_gen_format_entry_for_compiler(
      includes[j],
      SPN_GEN_INCLUDE,
      compiler
    ));
  }
}

void spn_cc_ps_add_defines(sp_ps_config_t* ps, sp_da(sp_str_t) defines, spn_cc_kind_t compiler) {
  sp_da_for(defines, j) {
    sp_ps_config_add_arg(ps, spn_gen_format_entry_for_compiler(
      defines[j],
      SPN_GEN_DEFINE,
      compiler
    ));
  }
}

void spn_cc_ps_add_lib_dirs(sp_ps_config_t* ps, sp_da(sp_str_t) lib_dirs, spn_cc_kind_t compiler) {
  sp_da_for(lib_dirs, j) {
    sp_ps_config_add_arg(ps, spn_gen_format_entry_for_compiler(
      lib_dirs[j],
      SPN_GEN_LIB_INCLUDE,
      compiler
    ));
  }
}

void spn_cc_ps_add_libs(sp_ps_config_t* ps, sp_da(sp_str_t) libs, spn_cc_kind_t compiler) {
  sp_da_for(libs, j) {
    sp_ps_config_add_arg(ps, spn_gen_format_entry_for_compiler(
      libs[j],
      SPN_GEN_LIBS,
      compiler
    ));
  }
}

void spn_cc_ps_add_rpaths(sp_ps_config_t* ps, sp_da(sp_str_t) rpaths, spn_cc_kind_t compiler) {
  sp_da_for(rpaths, j) {
    sp_ps_config_add_arg(ps, spn_gen_format_entry_for_compiler(
      rpaths[j],
      SPN_GEN_RPATH,
      compiler
    ));
  }
}

void spn_cc_ps_add_system_deps(sp_ps_config_t* ps, sp_da(sp_str_t) system_deps, spn_cc_kind_t compiler) {
  sp_da_for(system_deps, j) {
    sp_ps_config_add_arg(ps, spn_gen_format_entry_for_compiler(
      system_deps[j],
      SPN_GEN_SYSTEM_LIBS,
      compiler
    ));
  }
}

spn_err_t spn_cc_run(spn_cc_t* cc) {
  spn_profile_t* profile = &cc->build->profile;
  spn_cc_kind_t compiler = profile->cc.kind;

  sp_ps_config_t common = SP_ZERO_INITIALIZE();
  common.command = sp_str_copy(profile->cc.exe);

  spn_cc_ps_add_includes(&common, cc->include, compiler);
  spn_cc_ps_add_defines(&common, cc->define, compiler);
  spn_cc_ps_add_system_deps(&common, app.resolver.system_deps, compiler);
  sp_ps_config_add_arg(&common, spn_cc_c_standard_to_switch(profile->standard));
  sp_ps_config_add_arg(&common, spn_cc_build_mode_to_switch(profile->mode));
  sp_ps_config_add_arg(&common, spn_cc_lib_kind_to_switch(profile->linkage));

  sp_da_for(cc->targets, it) {
    spn_cc_target_t target = cc->targets[it];

    switch (target.kind) {
      case SPN_CC_TARGET_EXECUTABLE: {
        sp_ps_config_t process = sp_ps_config_copy(&common);

        sp_da_for(target.source, j) {
          sp_ps_config_add_arg(&process, target.source[j]);
        }

        spn_cc_ps_add_includes(&process, target.include, compiler);
        spn_cc_ps_add_defines(&process, target.define, compiler);
        spn_cc_ps_add_lib_dirs(&process, target.lib_dirs, compiler);
        spn_cc_ps_add_libs(&process, target.libs, compiler);
        spn_cc_ps_add_rpaths(&process, target.rpath, compiler);
        sp_ps_config_add_arg(&process, sp_str_lit("-o"));
        sp_ps_config_add_arg(&process, sp_fs_join_path(cc->build->paths.bin, target.name));

        spn_event_buffer_push_ex(spn.events, cc->build, (spn_build_event_t) {
          .kind = SPN_BUILD_EVENT_COMPILE,
          .compile = {
            .ps = sp_ps_config_copy(&process)
          },
        });

        sp_ps_output_t result = spn_build_ctx_subprocess(cc->build, process);
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


spn_bin_ctx_t* spn_get_target(spn_build_ctx_t* ctx, const c8* name) {
  return sp_ht_getp(ctx->builder->contexts.bins, sp_str_view(name));
}

void spn_log(spn_build_ctx_t* ctx, const c8* message) {
  sp_io_stream_t* io = &ctx->logs.build;
  sp_io_write_line(io, sp_tm_epoch_to_iso8601(sp_tm_now_epoch()));
  sp_io_write_line(io, sp_str_view(message));
}

spn_libc_kind_t spn_get_libc(spn_build_ctx_t* ctx) {
  return ctx->profile.libc;
}

void spn_dep_set_s64(spn_pkg_ctx_t* dep, const c8* name, s64 value) {
  spn_dep_option_t option = {
    .kind = SPN_DEP_OPTION_KIND_S64,
    .name = sp_str_from_cstr(name),
    .s = value
  };
  sp_ht_insert(dep->options, option.name, option);
}

void spn_copy(spn_pkg_ctx_t* dep, spn_dir_kind_t from_kind, const c8* from_path, spn_dir_kind_t to_kind, const c8* to_path) {
  sp_str_t from = sp_fs_join_path(spn_cache_dir_kind_to_dep_path(dep, from_kind), sp_str_view(from_path));
  sp_str_t to = sp_fs_join_path(spn_cache_dir_kind_to_dep_path(dep, to_kind), sp_str_view(to_path));
  sp_fs_copy(from, to);

}


/////////
// TUI //
/////////
void sp_tui_print(sp_str_t str) {
  sp_io_write_str(&spn.logger.err, str);
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
  fflush(stderr);
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
sp_str_t spn_cli_opt_kind_to_str(spn_cli_opt_kind_t kind) {
  switch (kind) {
    SPN_CLI_OPT_KIND(SP_X_NAMED_ENUM_CASE_TO_STRING_LOWER)
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_cli_arg_kind_t spn_cli_arg_kind_from_str(sp_str_t str) {
  SPN_CLI_ARG_KIND(SP_X_NAMED_ENUM_STR_TO_ENUM)
  SP_UNREACHABLE_RETURN(SPN_CLI_ARG_KIND_REQUIRED);
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

sp_str_t spn_build_event_kind_to_str(spn_build_event_kind_t kind) {
  switch (kind) {
    SPN_BUILD_EVENT(SP_X_NAMED_ENUM_CASE_TO_STRING_LOWER)
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

sp_str_t spn_bin_kind_to_str(spn_visibility_t kind) {
  switch (kind) {
    SPN_VISIBILITY_KIND(SP_X_NAMED_ENUM_CASE_TO_STRING_LOWER)
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_resolve_strategy_t spn_resolve_strategy_from_str(sp_str_t str) {
  SPN_RESOLVE_STRATEGY(SP_X_NAMED_ENUM_STR_TO_ENUM)
  SP_UNREACHABLE_RETURN(SPN_RESOLVE_STRATEGY_SOLVER);
}

sp_str_t spn_resolve_strategy_to_str(spn_resolve_strategy_t strategy) {
  switch (strategy) {
    SPN_RESOLVE_STRATEGY(SP_X_NAMED_ENUM_CASE_TO_STRING_LOWER)
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_visibility_t spn_visibility_from_str(sp_str_t str) {
  SPN_VISIBILITY_KIND(SP_X_NAMED_ENUM_STR_TO_ENUM)
  SP_UNREACHABLE_RETURN(SPN_VISIBILITY_PUBLIC);
}

sp_str_t spn_visibility_to_str(spn_visibility_t visibility) {
  switch (visibility) {
    SPN_VISIBILITY_KIND(SP_X_NAMED_ENUM_CASE_TO_STRING_LOWER)
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

sp_str_t spn_package_kind_to_str(spn_package_kind_t kind) {
  switch (kind) {
    SPN_PACKAGE_KIND(SP_X_NAMED_ENUM_CASE_TO_STRING_LOWER)
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_package_kind_t spn_package_kind_from_str(sp_str_t str) {
  SPN_PACKAGE_KIND(SP_X_NAMED_ENUM_STR_TO_ENUM)
  SP_UNREACHABLE_RETURN(SPN_PACKAGE_KIND_NONE);
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
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_build_mode_t spn_dep_build_mode_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, "debug"))   return SPN_DEP_BUILD_MODE_DEBUG;
  else if (sp_str_equal_cstr(str, "release")) return SPN_DEP_BUILD_MODE_RELEASE;

  SP_FATAL("Unknown build mode {:fg brightyellow}; options are [debug, release]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_DEP_BUILD_MODE_DEBUG);
}

sp_str_t spn_dep_build_mode_to_str(spn_build_mode_t mode) {
  switch (mode) {
    case SPN_DEP_BUILD_MODE_DEBUG:   return sp_str_lit("debug");
    case SPN_DEP_BUILD_MODE_RELEASE: return sp_str_lit("release");
    default: SP_UNREACHABLE_RETURN(sp_str_lit(""));
  }
}

spn_pkg_linkage_t spn_lib_kind_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, "shared")) return SPN_LIB_KIND_SHARED;
  else if (sp_str_equal_cstr(str, "static")) return SPN_LIB_KIND_STATIC;
  else if (sp_str_equal_cstr(str, "source")) return SPN_LIB_KIND_SOURCE;
  else if (sp_str_equal_cstr(str, "none"))   return SPN_LIB_KIND_NONE;

  SP_FATAL("Unknown build kind {:fg brightyellow}; options are [shared, static, source]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_LIB_KIND_SHARED);
}

sp_str_t spn_pkg_linkage_to_str(spn_pkg_linkage_t kind) {
  switch (kind) {
    case SPN_LIB_KIND_SHARED: return sp_str_lit("shared");
    case SPN_LIB_KIND_STATIC: return sp_str_lit("static");
    case SPN_LIB_KIND_SOURCE: return sp_str_lit("source");
    default: SP_UNREACHABLE_RETURN(sp_str_lit(""));
  }
}

sp_str_t spn_cc_lib_kind_to_switch(spn_pkg_linkage_t kind) {
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

sp_str_t spn_cc_build_mode_to_switch(spn_build_mode_t mode) {
  switch (mode) {
    case SPN_DEP_BUILD_MODE_DEBUG: return sp_str_lit("-g");
    case SPN_DEP_BUILD_MODE_RELEASE:
    default: return sp_str_lit("");
  }
}

sp_str_t spn_cc_kind_to_executable(spn_cc_kind_t compiler) {
  switch (compiler) {
    case SPN_CC_TCC:      return sp_str_lit("tcc");
    case SPN_CC_GCC:      return sp_str_lit("gcc");
    case SPN_CC_CLANG:      return sp_str_lit("clang");
    case SPN_CC_MUSL_GCC: return sp_str_lit("musl-gcc");
    case SPN_CC_CUSTOM:   SP_FALLTHROUGH();
    case SPN_CC_NONE:     return sp_str_lit("gcc");
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

sp_os_lib_kind_t spn_lib_kind_to_sp_os_lib_kind(spn_pkg_linkage_t kind) {
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
  if      (sp_str_equal_cstr(str, ""))            return SPN_GEN_ALL;
  else if (sp_str_equal_cstr(str, "include"))     return SPN_GEN_INCLUDE;
  else if (sp_str_equal_cstr(str, "lib-include")) return SPN_GEN_LIB_INCLUDE;
  else if (sp_str_equal_cstr(str, "libs"))        return SPN_GEN_LIBS;
  else if (sp_str_equal_cstr(str, "system-libs")) return SPN_GEN_SYSTEM_LIBS;

  SP_FATAL("Unknown flag {:fg brightyellow}; options are [include, lib-include, libs, system-libs]", SP_FMT_QUOTED_STR(str));
  SP_UNREACHABLE_RETURN(SPN_GEN_ALL);
}

spn_cc_kind_t spn_cc_kind_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, ""))    return SPN_CC_NONE;
  else if (sp_str_equal_cstr(str, "tcc")) return SPN_CC_TCC;
  else if (sp_str_equal_cstr(str, "gcc")) return SPN_CC_GCC;
  else if (sp_str_equal_cstr(str, "clang")) return SPN_CC_CLANG;
  else if (sp_str_equal_cstr(str, "musl-gcc")) return SPN_CC_MUSL_GCC;

  spn_ctx_warn("Unknown compiler {:fg brightyellow}; we'll assume a gcc command line when generating switches", SP_FMT_STR(str));
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
  SP_UNREACHABLE_RETURN(SPN_PACKAGE_KIND_NONE);
}

spn_generator_kind_t spn_gen_kind_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, ""))           return SPN_GEN_KIND_RAW;
  else if (sp_str_equal_cstr(str, "shell"))      return SPN_GEN_KIND_SHELL;
  else if (sp_str_equal_cstr(str, "make"))       return SPN_GEN_KIND_MAKE;
  else if (sp_str_equal_cstr(str, "cmake"))      return SPN_GEN_KIND_CMAKE;
  else if (sp_str_equal_cstr(str, "pkgconfig"))  return SPN_GEN_KIND_PKGCONFIG;

  SP_FATAL("Unknown generator {:fg brightyellow}; options are [[empty], shell, make, cmake, pkgconfig]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_GEN_KIND_RAW);
}

sp_str_t spn_cache_dir_kind_to_path(spn_dir_kind_t kind) {
  switch (kind) {
    case SPN_DIR_CACHE:   return spn.paths.cache;
    case SPN_DIR_STORE:   return spn.paths.store;
    case SPN_DIR_SOURCE:  return spn.paths.source;
    case SPN_DIR_WORK:    return spn.paths.cwd;
    default: SP_UNREACHABLE_RETURN(sp_str_lit(""));
  }
}

sp_str_t spn_cache_dir_kind_to_dep_path(spn_pkg_ctx_t* dep, spn_dir_kind_t kind) {
  switch (kind) {
    case SPN_DIR_STORE:   return dep->ctx.paths.store;
    case SPN_DIR_INCLUDE: return dep->ctx.paths.include;
    case SPN_DIR_LIB:     return dep->ctx.paths.lib;
    case SPN_DIR_VENDOR:  return dep->ctx.paths.vendor;
    case SPN_DIR_SOURCE:  return dep->ctx.paths.source;
    case SPN_DIR_WORK:    return dep->ctx.paths.work;
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
    case SPN_CC_CLANG:
    case SPN_CC_GCC: {
      switch (kind) {
        case SPN_GEN_INCLUDE:     return sp_format("-I{}",          SP_FMT_STR(entry));
        case SPN_GEN_LIB_INCLUDE: return sp_format("-L{}",          SP_FMT_STR(entry));
        case SPN_GEN_LIBS:        return sp_format("{}",            SP_FMT_STR(entry));
        case SPN_GEN_SYSTEM_LIBS: return sp_format("-l{}",          SP_FMT_STR(entry));
        case SPN_GEN_RPATH:       return sp_format("-Wl,-rpath,{}", SP_FMT_STR(entry));
        case SPN_GEN_DEFINE:      return sp_format("-D{}",          SP_FMT_STR(entry));
        default: SP_UNREACHABLE_RETURN(sp_str_lit(""));
      }
    }
    default: SP_UNREACHABLE_RETURN(sp_str_lit(""));
  }
}

sp_str_t spn_pkg_build_get_include_dir(spn_pkg_ctx_t* pkg) {
  return pkg->ctx.paths.include;
}

sp_str_t spn_pkg_build_get_lib_dir(spn_pkg_ctx_t* pkg) {
  switch (pkg->linkage) {
    case SPN_LIB_KIND_SHARED: {
      return pkg->ctx.paths.lib;
      break;
    }
    case SPN_LIB_KIND_NONE:
    case SPN_LIB_KIND_STATIC:
    case SPN_LIB_KIND_SOURCE: {
      return SP_ZERO_STRUCT(sp_str_t);
    }
  }
  SP_UNREACHABLE_RETURN(SP_ZERO_STRUCT(sp_str_t));
}

sp_str_t spn_pkg_build_get_rpath(spn_pkg_ctx_t* pkg) {
  return spn_pkg_build_get_lib_dir(pkg);
}

sp_str_t spn_pkg_build_get_lib(spn_pkg_ctx_t* pkg) {
  switch (pkg->linkage) {
    case SPN_LIB_KIND_SHARED:
    case SPN_LIB_KIND_STATIC: {
      sp_os_lib_kind_t kind = spn_lib_kind_to_sp_os_lib_kind(pkg->linkage);
      sp_str_t lib = pkg->ctx.package->lib.name;
      lib = sp_os_lib_to_file_name(lib, kind);
      lib = sp_fs_join_path(pkg->ctx.paths.lib, lib);
      return lib;
    }
    case SPN_LIB_KIND_NONE:
    case SPN_LIB_KIND_SOURCE: {
      return SP_ZERO_STRUCT(sp_str_t);
    }
  }
  SP_UNREACHABLE_RETURN(SP_ZERO_STRUCT(sp_str_t));
}

sp_dyn_array(sp_str_t) spn_gen_build_entry_for_dep(spn_pkg_ctx_t* dep, spn_gen_entry_kind_t kind, spn_cc_kind_t compiler) {
  sp_dyn_array(sp_str_t) entries = SP_NULLPTR;

  switch (kind) {
    case SPN_GEN_INCLUDE: {
      sp_dyn_array_push(entries, dep->ctx.paths.include);
      break;
    }
    case SPN_GEN_RPATH:
      switch (dep->linkage) {
        case SPN_LIB_KIND_SHARED: {
          sp_dyn_array_push(entries, dep->ctx.paths.lib);
          break;
        }
        case SPN_LIB_KIND_NONE:
        case SPN_LIB_KIND_STATIC:
        case SPN_LIB_KIND_SOURCE: {
          return entries;
        }
      }

      break;
    case SPN_GEN_LIB_INCLUDE:  {
      switch (dep->linkage) {
        case SPN_LIB_KIND_SHARED: {
          sp_dyn_array_push(entries, dep->ctx.paths.lib);
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
    case SPN_GEN_LIBS: {
      switch (dep->linkage) {
        case SPN_LIB_KIND_NONE:
        case SPN_LIB_KIND_SHARED:
        case SPN_LIB_KIND_STATIC: {
          sp_os_lib_kind_t kind = spn_lib_kind_to_sp_os_lib_kind(dep->linkage);
          sp_str_t lib = dep->ctx.package->lib.name;
          lib = sp_os_lib_to_file_name(lib, kind);
          lib = sp_fs_join_path(dep->ctx.paths.lib, lib);
          sp_dyn_array_push(entries, lib);
          break;
        }
        case SPN_LIB_KIND_SOURCE: {
          return entries;
        }
      }
      break;
    }
    case SPN_GEN_SYSTEM_LIBS: {
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

sp_str_t spn_gen_build_entries_for_dep(spn_pkg_ctx_t* dep, spn_cc_kind_t compiler) {
  spn_gen_entry_kind_t kinds [] = { SPN_GEN_INCLUDE, SPN_GEN_LIB_INCLUDE, SPN_GEN_LIBS };

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

  sp_ht_for_kv(app.build.contexts.deps, it) {
    spn_pkg_ctx_t* dep = it.val;
    sp_dyn_array(sp_str_t) dep_entries = spn_gen_build_entry_for_dep(dep, kind, compiler);
    sp_str_t dep_flags = sp_str_join_n(dep_entries, sp_dyn_array_size(dep_entries), sp_str_lit(" "));
    if (dep_flags.len > 0) {
      sp_dyn_array_push(entries, dep_flags);
    }
  }

  return sp_str_join_n(entries, sp_dyn_array_size(entries), sp_str_lit(" "));
}


void sp_sh_ls(sp_str_t path) {
  if (!sp_fs_exists(path)) {
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
    if (sp_fs_is_on_path(sp_str_view(tools[i].command))) {
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
      SP_LOG("{}", SP_FMT_STR(sp_str_trim(result.out)));
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
  if (!sp_fs_exists(repo)) return SPN_ERROR;

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





//////////
// TOML //
//////////
toml_table_t* spn_toml_parse(sp_str_t path) {
  if (!sp_fs_exists(path)) return SP_NULLPTR;

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
spn_tcc_t* spn_tcc_new(spn_build_ctx_t* ctx) {
  spn_tcc_t* tcc = tcc_new();
  tcc_set_error_func(tcc, ctx, spn_tcc_error);
  tcc_set_lib_path(tcc, sp_str_to_cstr(spn.paths.runtime));
  tcc_set_output_type(tcc, TCC_OUTPUT_MEMORY);
  sp_try_as(tcc_add_include_path(tcc, sp_str_to_cstr(spn.paths.include)), SP_NULLPTR);
  tcc_define_symbol(tcc, "SPN", "");
  sp_try_as(spn_tcc_register(tcc), SP_NULLPTR);
  return tcc;
}

spn_err_t spn_tcc_register(spn_tcc_t* tcc) {
  sp_carr_for(spn_lib, it) {
    sp_try_as(tcc_add_symbol(tcc, spn_lib[it].symbol, spn_lib[it].fn), SPN_ERROR);
  }
  return SPN_OK;
}

spn_err_t spn_tcc_add_file(spn_tcc_t* tcc, sp_str_t file_path) {
  sp_try_as(tcc_add_file(tcc, sp_str_to_cstr(file_path)), SPN_ERROR);
  return SPN_OK;
}

void spn_tcc_error(void* user_data, const char* message) {
  spn_build_ctx_t* ctx = (spn_build_ctx_t*)user_data;
  sp_io_write_cstr(&ctx->logs.build, message);
  sp_io_write_new_line(&ctx->logs.build);
}

void spn_tcc_list_fn(void* opaque, const char* name, const void* value) {
  sp_da(sp_str_t) syms = (sp_da(sp_str_t))opaque;
  sp_dyn_array_push(syms, sp_str_from_cstr(name));
}


/////////
// TUI //
/////////
sp_str_t spn_tui_name_to_color(sp_str_t str);

sp_str_t sp_color_to_tui_rgb(sp_color_t c) {
  return sp_color_to_tui_rgb_f(c.r * 255, c.g * 255, c.b * 255);
}

sp_str_t sp_color_to_tui_rgb_f(u8 r, u8 g, u8 b) {
  return sp_format("\033[38;2;{};{};{}m", SP_FMT_U32(r), SP_FMT_U32(g), SP_FMT_U32(b));
}

sp_str_t spn_tui_color_name(sp_str_t name) {
  return sp_format("{}{}{}",
    SP_FMT_STR(spn_tui_name_to_color(name)),
    SP_FMT_STR(name),
    SP_FMT_CSTR(SP_ANSI_RESET)
  );
}

sp_str_t spn_tui_decorate_name(sp_str_t name, u32 padded_len, c8 pad) {
  sp_str_builder_t b = SP_ZERO_INITIALIZE();
  sp_str_builder_append(&b, spn_tui_name_to_color(name));
  sp_str_builder_append_cstr(&b, "\u2590 ");
  sp_str_builder_append(&b, name);
  sp_str_builder_append_cstr(&b, SP_ANSI_RESET);

  if (padded_len > name.len) {
    sp_str_builder_append(&b, sp_str_repeat(pad, padded_len - name.len));
  }

  return sp_str_builder_move(&b);
}

sp_str_t spn_tui_name_to_color(sp_str_t str) {
  // bucket index -> hash of string that claimed it
  static sp_ht(u32, sp_hash_t) buckets = SP_NULLPTR;

  sp_hash_t hash = sp_hash_str(str);
  u32 lo = (u32)hash;
  u32 hi = hash >> 32;

  // 12 mostly perceptually distinct hue buckets, avoiding red/green
  static const f32 bucket_hues[] = {
    30, 40, 50, 60,     // oranges/yellows
    160, 180,           // cyans
    200, 220, 240,      // blue
    250, 280, 310, 340  // purple
  };
  u32 original_bucket = lo % sp_carr_len(bucket_hues);
  u32 bucket = original_bucket;
  while (sp_ht_key_exists(buckets, bucket)) {
    sp_hash_t* claimed = sp_ht_getp(buckets, bucket);
    if (claimed && *claimed == hash) break;
    bucket = (bucket + 1) % sp_carr_len(bucket_hues);
    if (bucket == original_bucket) break;
  }
  sp_ht_insert(buckets, bucket, hash);

  sp_color_t hsv = {
    .h = bucket_hues[bucket], //+ (f32)((hi >> 16) % 20) - 10, // jitter a little bit within buckets
    .s = 40.0f,
    .v = 75.f
  };
  sp_color_t rgb = sp_color_hsv_to_rgb(hsv);
  u8 r = (u8)(rgb.r * 255.0f);
  u8 g = (u8)(rgb.g * 255.0f);
  u8 b = (u8)(rgb.b * 255.0f);
  //return sp_format("{}", SP_FMT_U32(bucket));
  return sp_color_to_tui_rgb_f(r, g, b);
}

// sp_str_t spn_tui_name_to_color(sp_str_t str) {
//   sp_hash_t hash = sp_hash_str(str);
//   u32 truncated_hash = (u32)(hash ^ (hash >> 32));
//
//   // generate a hue, but avoid red and green
//   f32 hue = (f32)(((u64)truncated_hash * 360) >> 32);
//   if (hue >= 90 && hue <= 135) hue -= 45;
//   if (hue >= 340) hue -= 20;
//   if (hue <= 25) hue += 25;
//
//   f32 value = (f32)(((u64)(u32)(hash >> 32) * 50) >> 32) + 50;
//
//   sp_color_t hsv = { .h = hue, .s = 40.0f, .v = value, .a = 1.0f };
//   sp_color_t rgb = sp_color_hsv_to_rgb(hsv);
//
//   u8 r = (u8)(rgb.r * 255.0f);
//   u8 g = (u8)(rgb.g * 255.0f);
//   u8 b = (u8)(rgb.b * 255.0f);
//
//   return sp_color_to_tui_rgb_f(r, g, b);
// }

sp_str_t spn_tui_render_log(sp_io_stream_t* io, sp_str_t name) {
  sp_str_builder_t builder = SP_ZERO_INITIALIZE();
  sp_str_builder_append_fmt(&builder, "{}", SP_FMT_STR(spn_tui_color_name(name)));
  sp_str_builder_new_line(&builder);

  sp_str_t trailer = sp_str_lit("CUM CUM!!! CUM!!!!!!");
  sp_io_seek(io, 0, SP_IO_SEEK_SET);
  s64 size = SP_MIN(sp_io_size(io), 1024);
  c8* buffer = (c8*)sp_alloc(size);
  sp_io_read(io, buffer, size);
  sp_io_close(io);

  sp_str_t log = sp_str_truncate(sp_str(buffer, size), 1023, sp_str_lit("\n...truncated"));

  sp_str_builder_append_cstr(&builder, SP_ANSI_FG_BRIGHT_BLACK);
  sp_str_builder_append_fmt(&builder, "{}", SP_FMT_STR(log));
  sp_str_builder_append_cstr(&builder, SP_ANSI_RESET);

  if (buffer[size - 1] != '\n') {
    sp_str_builder_new_line(&builder);
  }

  return sp_str_builder_move(&builder);
}

sp_str_t spn_tui_render_build_event(spn_build_event_t* event) {
  sp_str_builder_t builder = SP_ZERO_INITIALIZE();

  // event kind
  switch (event->kind) {
    case SPN_BUILD_EVENT_TEST_PASSED:
    case SPN_BUILD_EVENT_TARGET_BUILD_PASSED:
    case SPN_BUILD_EVENT_DEP_BUILD_PASSED:
    case SPN_BUILD_EVENT_BUILD_PASSED: {
      sp_str_builder_append_fmt(&builder,
        "{}{:fg green :pad 8}{} ",
        SP_FMT_CSTR(SP_ANSI_BOLD),
        SP_FMT_STR(spn_build_event_kind_to_str(event->kind)),
        SP_FMT_CSTR(SP_ANSI_RESET)
      );
      break;
    }
    case SPN_BUILD_EVENT_TEST_FAILED:
    case SPN_BUILD_EVENT_DEP_BUILD_FAILED:
    case SPN_BUILD_EVENT_TARGET_BUILD_FAILED: {
      sp_str_builder_append_fmt(&builder,
        "{:fg red :pad 8} ",
        SP_FMT_STR(spn_build_event_kind_to_str(event->kind))
      );
      break;
    }
    default: {
      sp_str_builder_append_fmt(&builder,
        "{:fg brightblack :pad 8} ",
        SP_FMT_STR(spn_build_event_kind_to_str(event->kind))
      );
      break;
    }
  }

  // package
  sp_str_builder_append(&builder, spn_tui_decorate_name(event->ctx->name, spn.tui.info.max_name, ' '));
  sp_str_builder_append_c8(&builder, ' ');

  // extras
  switch (event->kind) {
    case SPN_BUILD_EVENT_SYNC: {
      sp_str_builder_append_fmt(&builder,
        "{:fg brightblack} ",
        SP_FMT_STR(spn_pkg_get_url(event->ctx->package))
      );
      break;
    }
    case SPN_BUILD_EVENT_CHECKOUT: {
      sp_str_builder_append_fmt(&builder,
        "{} {:fg brightblack} {}{}{}",
        SP_FMT_STR(spn_semver_to_str(event->checkout.version)),
        SP_FMT_STR(sp_str_truncate(event->checkout.commit, 8, SP_ZERO_STRUCT(sp_str_t))),
        SP_FMT_CSTR(SP_ANSI_ITALIC),
        SP_FMT_STR(sp_str_truncate(event->checkout.message, 32, sp_str_lit("..."))),
        SP_FMT_CSTR(SP_ANSI_RESET)
      );
      break;
    }
    case SPN_BUILD_EVENT_RESOLVE: {
      switch (event->resolve.strategy) {
        case SPN_RESOLVE_STRATEGY_SOLVER: {
          sp_str_builder_append_fmt(&builder, "{:fg brightblack}", SP_FMT_CSTR("solver"));
          break;
        }
        case SPN_RESOLVE_STRATEGY_LOCK_FILE: {
          sp_str_builder_append_fmt(&builder, "{:fg brightblack}", SP_FMT_CSTR("lock"));
          break;
        }
      }
      break;
    }
    case SPN_BUILD_EVENT_COMPILE: {
      break;
    }
    case SPN_BUILD_EVENT_TEST_FAILED: {
      sp_str_builder_append_fmt(&builder, "returned code {}", SP_FMT_S32(1));
      break;
    }
    case SPN_BUILD_EVENT_DEP_BUILD_PASSED: {
      sp_str_builder_append_fmt(&builder,
        "built in {:fg brightcyan}s",
        SP_FMT_F32(sp_tm_ns_to_s_f(event->ctx->time.total))
      );
      break;

    }
    case SPN_BUILD_EVENT_BUILD_PASSED: {
      sp_str_builder_append_fmt(&builder,
        "Built profile {:fg brightcyan} in {:fg brightcyan}s",
        SP_FMT_STR(event->ctx->profile.name),
        SP_FMT_F32(sp_tm_ns_to_s_f(event->done.time))
      );
      break;
    }
    default: {
      break;
    }
  }

  return sp_str_builder_move(&builder);
}

void spn_tui_init(spn_tui_t* tui, spn_tui_mode_t mode, spn_builder_t* build) {
  tui->build = build;
  tui->mode = mode;
  tui->info.num_total = sp_ht_size(build->dirty->commands);

  sp_ht_for_kv(build->contexts.deps, it) {
    tui->info.max_name = SP_MAX(tui->info.max_name, it.key->len);
  }

  sp_ht_for_kv(build->contexts.bins, it) {
    tui->info.max_name = SP_MAX(tui->info.max_name, it.key->len);
  }

  spn_spinner_init(&tui->spinner, sp_color_rgb_255(99,  160, 136));

  switch (tui->mode) {
    case SPN_OUTPUT_MODE_INTERACTIVE: {
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

void sp_tui_begin_table(sp_tui_table_t* table) {
  SP_ASSERT(table->state == SP_TUI_TABLE_NONE);

  table->cols = SP_NULLPTR;
  table->rows = SP_NULLPTR;
  table->cursor = (sp_tui_cursor_t) { .row = 0, .col = 0 };
  table->state = SP_TUI_TABLE_SETUP;
  table->columns = 0;
  table->indent = 0;
}

void sp_tui_table_setup_column(sp_tui_table_t* table, sp_str_t name) {
  sp_tui_table_setup_column_ex(table, name, 0);
}

void sp_tui_table_setup_column_ex(sp_tui_table_t* table, sp_str_t name, u32 min_width) {
  SP_ASSERT(table->state == SP_TUI_TABLE_SETUP);
  sp_tui_column_t col = { .name = name, .min_width = min_width };
  sp_dyn_array_push(table->cols, col);
  table->columns++;
}

void sp_tui_table_header_row(sp_tui_table_t* table) {
  SP_ASSERT(table->state == SP_TUI_TABLE_SETUP);
  SP_ASSERT(table->columns > 0);
  table->state = SP_TUI_TABLE_BUILDING;
  table->cursor.row = 0;
  table->cursor.col = 0;
}

void sp_tui_table_next_row(sp_tui_table_t* table) {
  SP_ASSERT(table->state == SP_TUI_TABLE_BUILDING);

  // Set cursor to the next row index (which is current array size)
  table->cursor.row = sp_dyn_array_size(table->rows);

  // Add new empty row
  sp_da(sp_str_t) new_row = SP_NULLPTR;
  sp_dyn_array_push(table->rows, new_row);

  table->cursor.col = 0;
}

void sp_tui_table_column(sp_tui_table_t* table, u32 n) {
  SP_ASSERT(table->state == SP_TUI_TABLE_BUILDING);
  SP_ASSERT(n < table->columns);
  table->cursor.col = n;
}

void sp_tui_table_column_named(sp_tui_table_t* table, sp_str_t name) {
  SP_ASSERT(table->state == SP_TUI_TABLE_BUILDING);

  sp_dyn_array_for(table->cols, i) {
    if (sp_str_equal(table->cols[i].name, name)) {
      table->cursor.col = i;
      return;
    }
  }

  SP_ASSERT(false && "Column name not found");
}

void sp_tui_table_str(sp_tui_table_t* table, sp_str_t str) {
  SP_ASSERT(table->state == SP_TUI_TABLE_BUILDING);
  SP_ASSERT(table->cursor.col < table->columns);
  SP_ASSERT(table->cursor.row < sp_dyn_array_size(table->rows));

  // Get current row (was created in sp_tui_table_next_row)
  sp_da(sp_str_t)* row = &table->rows[table->cursor.row];

  // Ensure row has enough cells
  while (sp_dyn_array_size(*row) <= table->cursor.col) {
    sp_dyn_array_push(*row, sp_str_lit(""));
  }

  // Set the cell
  (*row)[table->cursor.col] = str;
  table->cursor.col++;
}

void sp_tui_table_fmt(sp_tui_table_t* table, const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  sp_str_t str = sp_format_v(SP_CSTR(fmt), args);
  va_end(args);

  sp_tui_table_str(table, str);
}

void sp_tui_table_set_indent(sp_tui_table_t* table, u32 indent) {
  table->indent = indent;
}

void sp_tui_table_end(sp_tui_table_t* table) {
  SP_ASSERT(table->state == SP_TUI_TABLE_BUILDING);
  table->state = SP_TUI_TABLE_NONE;
}

static void sp_tui_apply_indent(sp_str_builder_t* builder, u32 indent) {
  for (u32 i = 0; i < indent; i++) {
    sp_str_builder_append_c8(builder, ' ');
    sp_str_builder_append_c8(builder, ' ');
  }
}

sp_str_t sp_tui_table_render(sp_tui_table_t* table) {
  SP_ASSERT(table->state == SP_TUI_TABLE_NONE);

  if (table->columns == 0) {
    return sp_str_lit("");
  }

  // Calculate column widths based on visual width (excluding ANSI codes)
  sp_da(u32) widths = SP_NULLPTR;
  for (u32 col = 0; col < table->columns; col++) {
    u32 max_width = table->cols[col].min_width;

    sp_dyn_array_for(table->rows, row_idx) {
      sp_da(sp_str_t)* row = &table->rows[row_idx];
      if (col < sp_dyn_array_size(*row)) {
        max_width = SP_MAX(max_width, sp_str_visual_len((*row)[col]));
      }
    }

    sp_dyn_array_push(widths, max_width);
  }

  // Build output
  sp_str_builder_t builder = SP_ZERO_INITIALIZE();

  // Render rows
  sp_dyn_array_for(table->rows, row_idx) {
    sp_da(sp_str_t)* row = &table->rows[row_idx];

    // Apply indentation for each row
    sp_tui_apply_indent(&builder, table->indent);

    for (u32 col = 0; col < table->columns; col++) {
      sp_str_t cell = (col < sp_dyn_array_size(*row)) ? (*row)[col] : sp_str_lit("");
      sp_str_t padded = sp_str_visual_pad(cell, widths[col]);

      sp_str_builder_append_fmt(&builder, "{}", SP_FMT_STR(padded));

      if (col < table->columns - 1) {
        sp_str_builder_append_c8(&builder, ' ');
      }
    }
    sp_str_builder_new_line(&builder);
  }

  return sp_str_builder_move(&builder);
}

void spn_lock_file_init(spn_lock_file_t* lock) {
  sp_ht_set_fns(lock->entries, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(lock->system_deps, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
}

spn_lock_file_t spn_build_lock_file() {
  spn_lock_file_t lock = SP_ZERO_INITIALIZE();
  spn_lock_file_init(&lock);

  sp_ht_for_kv(app.resolver.resolved, it) {
    spn_resolved_pkg_t* resolved = it.val;
    spn_pkg_ctx_t* dep = sp_ht_getp(app.build.contexts.deps, resolved->name);
    spn_pkg_t* package = dep->ctx.package;

    spn_pkg_req_t* direct_req = sp_ht_getp(app.package.deps, resolved->name);
    bool is_explicit = direct_req != SP_NULLPTR;

    spn_lock_entry_t entry = {
      .name = sp_str_copy(resolved->name),
      .version = dep->metadata.version,
      .commit = sp_str_copy(dep->metadata.commit),
      .import_kind = is_explicit ? SPN_DEP_IMPORT_KIND_EXPLICIT : SPN_DEP_IMPORT_KIND_TRANSITIVE,
      .visibility = is_explicit ? direct_req->visibility : SPN_VISIBILITY_PUBLIC,
      .kind = resolved->kind,
    };

    sp_da_for(package->system_deps, i) {
      sp_ht_insert(lock.system_deps, sp_str_copy(package->system_deps[i]), true);
    }

    sp_ht_for_kv(package->deps, n) {
      sp_dyn_array_push(entry.deps, sp_str_copy(n.val->name));
    }

    sp_ht_insert(lock.entries, entry.name, entry);
  }

  // Now that everyone has a node, go back and add the reverse references
  sp_ht_for_kv(lock.entries, it) {
    sp_dyn_array_for(it.val->deps, n) {
      spn_lock_entry_t* dep = sp_ht_getp(lock.entries, it.val->deps[n]);
      if (dep) sp_dyn_array_push(dep->dependents, sp_str_copy(it.val->name));
    }
  }

  return lock;
}

spn_lock_file_t spn_lock_file_load(sp_str_t path) {
  spn_lock_file_t lock = SP_ZERO_INITIALIZE();
  spn_lock_file_init(&lock);

  SP_ASSERT(sp_fs_exists(path));

  toml_table_t* root = spn_toml_parse(path);
  SP_ASSERT(root);

  toml_table_t* pkg_table = toml_table_table(root, "package");
  if (pkg_table) {
    sp_da(sp_str_t) sys_deps = spn_toml_arr_to_str_arr(toml_table_array(pkg_table, "system_deps"));
    sp_da_for(sys_deps, i) {
      sp_ht_insert(lock.system_deps, sys_deps[i], true);
    }
  }

  toml_array_t* deps = toml_table_array(root, "dep");
  if (!deps) return lock;

  spn_toml_arr_for(deps, it) {
    toml_table_t* pkg = toml_array_table(deps, it);
    SP_ASSERT(pkg);

    spn_lock_entry_t entry = {
      .name = spn_toml_str(pkg, "name"),
      .version = spn_semver_from_str(spn_toml_str(pkg, "version")),
      .commit = spn_toml_str(pkg, "commit"),
      .kind = spn_package_kind_from_str(spn_toml_str(pkg, "kind")),
      .visibility = spn_visibility_from_str(spn_toml_str(pkg, "visibility")),
      .deps = spn_toml_arr_to_str_arr(toml_table_array(pkg, "deps")),
    };
    sp_ht_insert(lock.entries, entry.name, entry);
  }

  sp_ht_for_kv(lock.entries, it) {
    sp_dyn_array_for(it.val->deps, n) {
      spn_lock_entry_t* dep = sp_ht_getp(lock.entries, it.val->deps[n]);
      if (dep) sp_dyn_array_push(dep->dependents, it.val->name);
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

bool spn_pkg_build_is_stamped(spn_pkg_ctx_t* context) {
  return sp_fs_exists(context->ctx.paths.stamp);
}

bool sp_cmp_kernel_env_var(void* va, void* vb) {
  sp_env_var_t* a = (sp_env_var_t*)a;
  sp_env_var_t* b = (sp_env_var_t*)b;
  if (!sp_str_equal(a->key, b->key)) return false;
  return sp_str_equal(a->value, b->value);
}

bool sp_zcmp_kernel_env_var(void* va) {
  sp_env_var_t* a = (sp_env_var_t*)a;
  return !sp_str_valid(a->key) && !sp_str_valid(a->value);
}

sp_ps_output_t spn_build_ctx_subprocess(spn_build_ctx_t* build, sp_ps_config_t config) {
  config.io = (sp_ps_io_config_t) {
    .in = { .mode = SP_PS_IO_MODE_NULL },
    .out = { .mode = SP_PS_IO_MODE_EXISTING, .stream = build->logs.build },
    .err = { .mode = SP_PS_IO_MODE_REDIRECT }
  };
  config.cwd = build->paths.work;


  u32 it = 0;
  for (; it < sp_carr_len(config.env.extra); it++) {
    if (!sp_str_valid(config.env.extra[it].key)) {
      break;
    }
  }
  SP_ASSERT(it != sp_carr_len(config.env.extra));

  config.env.extra[it] = (sp_env_var_t) {
    .key = sp_str_lit("CC"),
    .value = build->profile.cc.exe
  };

  sp_da_push(build->commands, sp_ps_config_copy(&config));

  sp_ps_t ps = sp_ps_create(config);
  return sp_ps_output(&ps);
}

void spn_dep_context_set_build_state(spn_pkg_ctx_t* dep, spn_dep_state_t state) {
  sp_mutex_lock(&dep->mutex);
  dep->state = state;
  sp_mutex_unlock(&dep->mutex);
}

void spn_dep_context_set_build_error(spn_pkg_ctx_t* dep, sp_str_t error) {
  sp_mutex_lock(&dep->mutex);
  dep->state = SPN_DEP_BUILD_STATE_FAILED;
  dep->error = sp_str_copy(error);
  sp_mutex_unlock(&dep->mutex);
}


sp_str_t spn_pkg_build_get_target_path(spn_pkg_ctx_t* build, spn_target_t* bin) {
  return sp_fs_join_path(build->ctx.paths.bin, bin->name);
}

sp_str_t spn_get_tool_path(spn_target_t* bin) {
  return sp_fs_join_path(spn.paths.bin, bin->name);
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
      range.high.version = (spn_semver_t){SP_LIMIT_U32_MAX, SP_LIMIT_U32_MAX, SP_LIMIT_U32_MAX};
      break;
    }
    case SPN_SEMVER_OP_GT: {
      range.low.op = SPN_SEMVER_OP_GT;
      range.low.version = version;
      range.high.op = SPN_SEMVER_OP_LEQ;
      range.high.version = (spn_semver_t){SP_LIMIT_U32_MAX, SP_LIMIT_U32_MAX, SP_LIMIT_U32_MAX};
      break;
    }
    case SPN_SEMVER_OP_LEQ: {
      range.low.op = SPN_SEMVER_OP_GEQ;
      range.low.version = SP_ZERO_STRUCT(spn_semver_t);
      range.high.op = SPN_SEMVER_OP_LEQ;
      range.high.version = version;
      break;
    }
    case SPN_SEMVER_OP_LT: {
      range.low.op = SPN_SEMVER_OP_GEQ;
      range.low.version = SP_ZERO_STRUCT(spn_semver_t);
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
      SP_UNREACHABLE_RETURN(false);
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

void spn_pkg_init(spn_pkg_t* package) {
  sp_ht_set_fns(package->binaries, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(package->tests, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(package->deps, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(package->options, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(package->config, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(package->profiles, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
}

void spn_pkg_set_index(spn_pkg_t* package, sp_str_t path) {
  package->kind = SPN_PACKAGE_KIND_INDEX;
  package->paths.dir = sp_str_copy(path);
  package->paths.manifest = sp_fs_join_path(package->paths.dir, sp_str_lit("spn.toml"));
  package->paths.metadata = sp_fs_join_path(package->paths.dir, sp_str_lit("metadata.toml"));
  package->paths.script = sp_fs_join_path(package->paths.dir, sp_str_lit("spn.c"));
}

void spn_pkg_set_manifest(spn_pkg_t* package, sp_str_t path) {
  package->kind = SPN_PACKAGE_KIND_FILE;
  package->paths.dir = sp_fs_parent_path(path);
  package->paths.manifest = sp_str_copy(path);
  package->paths.metadata = sp_fs_join_path(package->paths.dir, sp_str_lit("metadata.toml"));
  package->paths.script = sp_fs_join_path(package->paths.dir, sp_str_lit("spn.c"));
}

void spn_pkg_add_version(spn_pkg_t* package, spn_semver_t version, sp_str_t commit) {
  spn_metadata_t metadata = {
    .version = version,
    .commit = sp_str_copy(commit)
  };

  sp_ht_insert(package->metadata, version, metadata);
  sp_dyn_array_push(package->versions, version);
}

sp_str_t spn_pkg_get_url(spn_pkg_t* pkg) {
  return sp_format("https://github.com/{}.git", SP_FMT_STR(pkg->repo));
}

spn_pkg_t spn_pkg_new(sp_str_t name) {
  spn_pkg_t package = SP_ZERO_INITIALIZE();
  spn_pkg_init(&package);
  package.name = sp_str_copy(name);
  return package;
}

spn_pkg_t spn_pkg_from_bare_default(sp_str_t path, sp_str_t name) {
  spn_pkg_t package = spn_pkg_new(name);
  spn_pkg_set_manifest(&package, sp_fs_join_path(path, sp_str_lit("spn.toml")));
  package.version = (spn_semver_t) { 0, 1, 0 };
  sp_dyn_array_push(package.versions, package.version);
  return package;
}

spn_pkg_t spn_pkg_from_default(sp_str_t path, sp_str_t name) {
  spn_pkg_t package = spn_pkg_new(name);
  spn_pkg_set_manifest(&package, sp_fs_join_path(path, sp_str_lit("spn.toml")));
  spn_pkg_add_dep_from_index(&package, sp_str_lit("sp"), SPN_VISIBILITY_PUBLIC);

  package.version = (spn_semver_t) { 0, 1, 0 };
  sp_dyn_array_push(package.versions, package.version);

  spn_target_t bin = {
    .name = sp_str_copy(package.name),
  };
  sp_dyn_array_push(bin.source, sp_str_lit("main.c"));
  sp_ht_insert(package.binaries, bin.name, bin);

  return package;
}

spn_pkg_t spn_pkg_from_index(sp_str_t path) {
  sp_str_t manifest = sp_fs_join_path(path, sp_str_lit("spn.toml"));
  SP_ASSERT(sp_fs_exists(manifest));

  spn_pkg_t package = spn_pkg_load(manifest);
  spn_pkg_set_index(&package, path);

  toml_table_t* metadata = spn_toml_parse(package.paths.metadata);
  if (metadata) {
    toml_array_t* versions = toml_table_array(metadata, "versions");

    const c8* key = SP_NULLPTR;
    spn_toml_arr_for(versions, it) {
      toml_table_t* entry = toml_array_table(versions, it);

      spn_semver_t version = spn_semver_from_str(spn_toml_str(entry, "version"));
      spn_pkg_add_version(&package, version, spn_toml_str(entry, "commit"));
    }

    sp_dyn_array_sort(package.versions, spn_semver_sort_kernel);
  }

  return package;
}

spn_pkg_t spn_pkg_from_manifest(sp_str_t manifest) {
  SP_ASSERT(sp_fs_exists(manifest));

  spn_pkg_t package = spn_pkg_load(manifest);
  spn_pkg_set_manifest(&package, manifest);
  return package;
}

spn_target_t spn_pkg_load_bin(toml_table_t* toml) {
  spn_target_t bin = SP_ZERO_INITIALIZE();

  if (!toml) return bin;

  bin.name = spn_toml_str(toml, "name");
  bin.source = spn_toml_arr_to_str_arr(toml_table_array(toml, "source"));
  bin.include = spn_toml_arr_to_str_arr(toml_table_array(toml, "include"));
  bin.define = spn_toml_arr_to_str_arr(toml_table_array(toml, "define"));

  toml_value_t kind = toml_table_string(toml, "kind");
  bin.visibility = kind.ok ?
    spn_visibility_from_str(sp_str_view(kind.u.s)) :
    SPN_VISIBILITY_PUBLIC;

  return bin;
}

void spn_pkg_load_deps(toml_table_t* toml, spn_pkg_t* package, spn_visibility_t visibility) {
  if (!toml) return;

  const c8* key = SP_NULLPTR;
  spn_toml_for(toml, n, key) {
    spn_pkg_req_t dep = spn_pkg_req_from_str(spn_toml_str(toml, key));
    dep.name = sp_str_from_cstr(key);
    dep.visibility = visibility;

    sp_ht_insert(package->deps, dep.name, dep);
  }
}

spn_pkg_t spn_pkg_load(sp_str_t manifest_path) {
  spn_pkg_t package = SP_ZERO_INITIALIZE();
  spn_pkg_init(&package);

  spn_toml_package_t toml = SP_ZERO_INITIALIZE();
  toml.manifest = spn_toml_parse(manifest_path);
  toml.package = toml_table_table(toml.manifest, "package");
  toml.lib = toml_table_table(toml.manifest, "lib");
  toml.bin = toml_table_array(toml.manifest, "bin");
  toml.test = toml_table_array(toml.manifest, "test");
  toml.profile = toml_table_array(toml.manifest, "profile");
  toml.registry = toml_table_array(toml.manifest, "registry");
  toml.deps = toml_table_table(toml.manifest, "deps");
  toml.options = toml_table_table(toml.manifest, "options");
  toml.config = toml_table_table(toml.manifest, "config");

  package.name = spn_toml_str(toml.package, "name");
  package.repo = spn_toml_str_opt(toml.package, "repo", "");
  package.author = spn_toml_str_opt(toml.package, "author", "");
  package.maintainer = spn_toml_str_opt(toml.package, "maintainer", "");

  sp_str_t commit = spn_toml_str_opt(toml.package, "commit", "");
  package.version = spn_semver_from_str(spn_toml_str(toml.package, "version"));
  spn_pkg_add_version(&package, package.version, commit);

  if (toml.package) {
    toml_array_t* include = toml_table_array(toml.package, "include");
    if (include) {
      package.include = spn_toml_arr_to_str_arr(include);
    }

    toml_array_t* define = toml_table_array(toml.package, "define");
    if (define) {
      package.define = spn_toml_arr_to_str_arr(define);
    }

    toml_array_t* system_deps = toml_table_array(toml.package, "system_deps");
    if (system_deps) {
      package.system_deps = spn_toml_arr_to_str_arr(system_deps);
    }
  }

  if (toml.lib) {
    toml_array_t* kinds = toml_table_array(toml.lib, "kinds");
    spn_toml_arr_for(kinds, it) {
      toml_value_t value = toml_array_string(kinds, it);
      SP_ASSERT(value.ok);

      spn_pkg_linkage_t kind = spn_lib_kind_from_str(sp_str_view(value.u.s));
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
      spn_target_t bin = spn_pkg_load_bin(toml_array_table(toml.bin, n));
      sp_ht_insert(package.binaries, bin.name, bin);
    }
  }

  if (toml.test) {
    spn_toml_arr_for(toml.test, n) {
      spn_target_t test = spn_pkg_load_bin(toml_array_table(toml.test, n));
      test.visibility = SPN_VISIBILITY_TEST;
      sp_ht_insert(package.tests, test.name, test);
    }
  }

  if (toml.deps) {
    spn_pkg_load_deps(toml_table_table(toml.deps, "package"), &package, SPN_VISIBILITY_PUBLIC);
    spn_pkg_load_deps(toml_table_table(toml.deps, "test"), &package, SPN_VISIBILITY_TEST);
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

spn_pkg_req_t spn_pkg_req_from_str(sp_str_t str) {
  spn_pkg_req_t dep = SP_ZERO_INITIALIZE();
  if (sp_str_starts_with(str, sp_str_lit("file://"))) {
    return (spn_pkg_req_t) {
      .kind = SPN_PACKAGE_KIND_FILE,
      .file = sp_str_copy(str)
    };
  }
  else {
    return (spn_pkg_req_t) {
      .kind = SPN_PACKAGE_KIND_INDEX,
      .range = spn_semver_range_from_str(str),
    };
  }

  SP_UNREACHABLE_RETURN(SP_ZERO_STRUCT(spn_pkg_req_t));
}

sp_str_t spn_pkg_req_to_str(spn_pkg_req_t dep) {
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
    case SPN_PACKAGE_KIND_NONE: {
      SP_UNREACHABLE_RETURN(sp_str_lit(""));
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

void spn_pkg_add_dep(spn_pkg_t* package, spn_pkg_req_t request) {
  sp_ht_insert(package->deps, request.name, request);
}

void spn_pkg_set_dep_from_index(spn_pkg_t* package, sp_str_t name, spn_visibility_t visibility) {
  SP_ASSERT(package);

  spn_pkg_req_t request = {
    .name = sp_str_copy(name),
    .kind = SPN_PACKAGE_KIND_INDEX,
    .visibility = visibility
  };

  spn_pkg_t* dep = spn_app_ensure_package(&app, request);
  if (!dep) {
    spn_app_bail_on_missing_package(&app, name);
  }

  if (sp_dyn_array_empty(dep->versions)) {
    SP_FATAL("{:fg brightcyan} has no known versions", SP_FMT_STR(dep->name));
  }

  spn_semver_parsed_t version = {
    .version = *sp_dyn_array_back(dep->versions),
    .components = { true, true, true }
  };
  request.range = spn_semver_caret_to_range(version);
  spn_pkg_add_dep(package, request);
}

void spn_pkg_add_dep_from_index(spn_pkg_t* package, sp_str_t name, spn_visibility_t visibility) {
  if (sp_ht_getp(package->deps, name)) {
    SP_FATAL("{:fg brightyellow} is already in your project", SP_FMT_STR(name));
  }
  spn_pkg_set_dep_from_index(package, name, visibility);
}

void spn_resolver_init(spn_resolver_t* resolver) {
  sp_ht_set_fns(resolver->ranges, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(resolver->resolved, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(resolver->visited, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
}

void spn_app_bail_on_missing_package(spn_app_t* app, sp_str_t name) {
  sp_str_t prefix = sp_str_lit("  > ");
  sp_str_t color = sp_str_lit("brightcyan");

  sp_da(sp_str_t) search = app->search;
  search = sp_str_map(search, sp_dyn_array_size(search), &color, sp_str_map_kernel_colorize);
  search = sp_str_map(search, sp_dyn_array_size(search), &prefix, sp_str_map_kernel_prepend);

  SP_FATAL(
    "Could not find {:fg yellow} on search path: \n{}",
    SP_FMT_STR(name),
    SP_FMT_STR(sp_str_join_n(search, sp_dyn_array_size(search), sp_str_lit("\n")))
  );
}

void spn_app_add_dep_constraints_2(spn_app_t* app, spn_pkg_req_t request) {
  SP_ASSERT(app);
  SP_ASSERT(!sp_str_empty(request.name));

  spn_resolver_t* resolver = &app->resolver;

  spn_pkg_t* dep = spn_app_ensure_package(app, request);

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

      sp_dyn_array_for(dep->versions, n) {
        spn_semver_t version = dep->versions[n];

        if (!range.low.some) {
          if (spn_semver_satisfies(version, low, request.range.low.op)) {
            sp_opt_set(range.low, n);
          }
        }

        if (spn_semver_satisfies(version, high, request.range.high.op)) {
          sp_opt_set(range.high, n);
        }
      }

      break;
    }
    case SPN_PACKAGE_KIND_REMOTE:
    case SPN_PACKAGE_KIND_WORKSPACE: {
      SP_BROKEN();
      break;
    }
    case SPN_PACKAGE_KIND_NONE: {
      SP_UNREACHABLE_CASE();
    }
  }

  sp_dyn_array_push(*ranges, range);
}

void spn_app_add_dep_constraints(spn_app_t* app, spn_pkg_dep_reqs_t* deps) {
  SP_ASSERT(app);
  SP_ASSERT(deps);

  spn_resolver_t* resolver = &app->resolver;

  sp_ht_for_kv(*deps, it) {
    spn_pkg_req_t request = *it.val;
    spn_pkg_t* dep = spn_app_ensure_package(app, request);

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

        sp_dyn_array_for(dep->versions, n) {
          spn_semver_t version = dep->versions[n];

          if (!range.low.some) {
            if (spn_semver_satisfies(version, low, request.range.low.op)) {
              sp_opt_set(range.low, n);
            }
          }

          if (spn_semver_satisfies(version, high, request.range.high.op)) {
            sp_opt_set(range.high, n);
          }
        }

        break;
      }
      case SPN_PACKAGE_KIND_REMOTE:
      case SPN_PACKAGE_KIND_WORKSPACE: {
        SP_BROKEN();
        break;
      }
      case SPN_PACKAGE_KIND_NONE: {
        SP_UNREACHABLE_CASE();
      }
    }

    sp_dyn_array_push(*ranges, range);
  }
}

void spn_resolver_add_request(spn_app_t* app, spn_pkg_req_t request) {
}

void spn_app_add_package_constraints(spn_app_t* app, spn_pkg_t* package) {
  spn_resolver_t* resolver = &app->resolver;

  if (sp_ht_key_exists(resolver->visited, package->name)) {
    // @spader keep a stack to provide a real error message
    SP_FATAL("{:fg brightcyan} transitively includes itself", SP_FMT_STR(package->name));
  }

  // system deps
  sp_dyn_array_for(package->system_deps, i) {
    sp_str_t sys_dep = package->system_deps[i];
    bool found = false;
    sp_dyn_array_for(resolver->system_deps, j) {
      if (sp_str_equal(resolver->system_deps[j], sys_dep)) { found = true; break; }
    }
    if (!found) sp_dyn_array_push(resolver->system_deps, sys_dep);
  }

  // prevent circular deps by marking this dep until we're done with the subtree
  sp_ht_insert(resolver->visited, package->name, true);

  sp_ht_for_kv(package->deps, it) {
    spn_pkg_req_t request = *it.val;
    SP_ASSERT(!sp_str_empty(request.name));
    spn_pkg_t* dep = spn_app_ensure_package(app, request);
    SP_ASSERT(dep);

    // recurse
    spn_app_add_package_constraints(app, dep);

    // add the dependency itself
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

        sp_dyn_array_for(dep->versions, n) {
          spn_semver_t version = dep->versions[n];

          if (!range.low.some) {
            if (spn_semver_satisfies(version, low, request.range.low.op)) {
              sp_opt_set(range.low, n);
            }
          }

          if (spn_semver_satisfies(version, high, request.range.high.op)) {
            sp_opt_set(range.high, n);
          }
        }

        break;
      }
      case SPN_PACKAGE_KIND_REMOTE:
      case SPN_PACKAGE_KIND_WORKSPACE: {
        SP_BROKEN();
        break;
      }
      case SPN_PACKAGE_KIND_NONE: {
        SP_UNREACHABLE_CASE();
      }
    }

    sp_dyn_array_push(*ranges, range);
  }

  sp_ht_erase(resolver->visited, package->name);
}

void spn_app_resolve_from_lock_file(spn_app_t* app) {
  spn_resolver_init(&app->resolver);
  SP_ASSERT(app->lock.some);

  spn_lock_file_t* lock = &app->lock.value;
  sp_ht_for_kv(lock->entries, it) {
    spn_lock_entry_t* entry = it.val;

    spn_pkg_req_t request = {
      .name = *it.key,
      .kind = entry->kind,
      .visibility = entry->visibility,
    };

    if (request.kind == SPN_PACKAGE_KIND_INDEX) {
      request.range = (spn_semver_range_t) {
        .low = { .version = entry->version, .op = SPN_SEMVER_OP_EQ },
        .high = { .version = entry->version, .op = SPN_SEMVER_OP_EQ },
        .mod = SPN_SEMVER_MOD_CMP
      };
    }

    spn_app_ensure_package(app, request);

    sp_ht_insert(app->resolver.resolved, entry->name, ((spn_resolved_pkg_t) {
      .name = *it.key,
      .kind = request.kind,
      .version = entry->version
    }));
  }

  sp_ht_for_kv(lock->system_deps, it) {
    sp_da_push(app->resolver.system_deps, *it.key);
  }
}

void spn_app_resolve_from_solver(spn_app_t* app) {
  spn_resolver_init(&app->resolver);
  spn_app_add_package_constraints(app, &app->package);

  sp_ht_for_kv(app->resolver.ranges, it) {
    sp_str_t name = *it.key;
    sp_da(spn_dep_version_range_t) ranges = *it.val;
    SP_ASSERT(sp_dyn_array_size(ranges));

    spn_pkg_req_t req_low, req_high = SP_ZERO_INITIALIZE();
    u32 low = 0, high = SP_LIMIT_U32_MAX;
    sp_dyn_array_for(ranges, n) {
      spn_dep_version_range_t range = ranges[n];
      SP_ASSERT(range.low.some);
      SP_ASSERT(range.high.some);

      if (sp_opt_get(range.low) >= low) {
        low = sp_opt_get(range.low);
        req_low = range.source;
      }
      if (sp_opt_get(range.high) <= high) {
        high = sp_opt_get(range.high);
        req_high = range.source;
      }
    }

    if (low > high) {
      sp_str_builder_t builder = SP_ZERO_INITIALIZE();
      sp_str_builder_append_fmt(&builder, "{:fg brightcyan} cannot be resolved:", SP_FMT_STR(name));
      sp_str_builder_indent(&builder);
      sp_str_builder_new_line(&builder);
      sp_str_builder_append_fmt(&builder, "{:fg brightcyan} requires {:fg brightred}", SP_FMT_STR(req_low.name), SP_FMT_STR(spn_semver_range_to_str(req_low.range)));
      sp_str_builder_new_line(&builder);
      sp_str_builder_append_fmt(&builder, "{:fg brightcyan} requires {:fg brightred}", SP_FMT_STR(req_high.name), SP_FMT_STR(spn_semver_range_to_str(req_high.range)));

      SP_FATAL("{}", SP_FMT_STR(sp_str_builder_move(&builder)));
    }


    spn_pkg_t* dep = spn_app_ensure_package(app, req_high);
    sp_ht_insert(app->resolver.resolved, name, ((spn_resolved_pkg_t) {
      .name = name,
      .version = dep->versions[high],
      .kind = req_high.kind,
    }));
  }
}

void spn_app_resolve(spn_app_t* app) {
  switch (app->lock.some) {
    case SP_OPT_SOME: {
      spn_event_buffer_push_ex(spn.events, &app->build.contexts.package, (spn_build_event_t) {
        .kind = SPN_BUILD_EVENT_RESOLVE,
        .resolve = {
          .strategy = SPN_RESOLVE_STRATEGY_LOCK_FILE
        }
      });

      spn_app_resolve_from_lock_file(app);
      break;
    }
    case SP_OPT_NONE: {
      spn_event_buffer_push_ex(spn.events, &app->build.contexts.package, (spn_build_event_t) {
        .kind = SPN_BUILD_EVENT_RESOLVE,
        .resolve = {
          .strategy = SPN_RESOLVE_STRATEGY_SOLVER
        }
      });

      spn_app_resolve_from_solver(app);
      break;
    }
  }
}

void spn_build_ctx_prepare_io(spn_build_ctx_t* build) {
  build->paths.include = sp_fs_join_path(build->paths.store, SP_LIT("include"));
  build->paths.bin = sp_fs_join_path(build->paths.store, SP_LIT("bin"));
  build->paths.lib = sp_fs_join_path(build->paths.store, SP_LIT("lib"));
  build->paths.vendor = sp_fs_join_path(build->paths.store, SP_LIT("vendor"));
  build->paths.stamp = sp_fs_join_path(build->paths.store, SP_LIT("spn.stamp"));
  build->paths.logs.build = sp_fs_join_path(build->paths.work, sp_format("{}.build.log", SP_FMT_STR(build->name)));
  build->paths.logs.test = sp_fs_join_path(build->paths.work, sp_format("{}.test.log", SP_FMT_STR(build->name)));

  sp_fs_create_dir(build->paths.work);
  sp_fs_create_dir(build->paths.store);
  sp_fs_create_dir(build->paths.bin);
  sp_fs_create_dir(build->paths.include);
  sp_fs_create_dir(build->paths.lib);
  sp_fs_create_dir(build->paths.vendor);

  sp_fs_create_file(build->paths.logs.build);
  build->logs.build = sp_io_from_file(build->paths.logs.build, SP_IO_MODE_READ | SP_IO_MODE_WRITE);
}

spn_err_t spn_pkg_build_run(spn_pkg_ctx_t* build) {
  //SP_ASSERT(!spn_dep_context_is_build_stamped(build));

  sp_tm_timer_t timer = sp_tm_start_timer();
  sp_try(spn_build_ctx_run_script(&build->ctx));
  spn_pkg_build_stamp(build);
  build->ctx.time.total = sp_tm_read_timer(&timer);

  return SPN_OK;
}

spn_err_t spn_pkg_build_sync_remote(spn_pkg_ctx_t* build) {
  if (!sp_fs_exists(build->ctx.paths.source)) {
    spn_dep_context_set_build_state(build, SPN_DEP_BUILD_STATE_CLONING);

    sp_str_t url = spn_pkg_get_url(build->ctx.package);
    if (spn_git_clone(url, build->ctx.paths.source)) {
      spn_dep_context_set_build_error(build, sp_format(
        "Failed to clone {:fg brightcyan}",
        SP_FMT_STR(build->ctx.package->name)
      ));

      return SPN_ERROR;
    }

    SP_ASSERT(sp_fs_is_dir(build->ctx.paths.source));
  }
  else {
    spn_dep_context_set_build_state(build, SPN_DEP_BUILD_STATE_FETCHING);
    if (spn_git_fetch(build->ctx.paths.source)) {
      spn_dep_context_set_build_error(build, sp_format(
        "Failed to fetch {:fg brightcyan}",
        SP_FMT_STR(build->ctx.package->name)
      ));

      return SPN_ERROR;
    }
  }

  return SPN_OK;
}

spn_err_t spn_pkg_build_resolve_commit(spn_pkg_ctx_t* build) {
  spn_dep_context_set_build_state(build, SPN_DEP_BUILD_STATE_RESOLVING);

  sp_str_t message = spn_git_get_commit_message(build->ctx.paths.source, build->metadata.commit);
  message = sp_str_truncate(message, 32, SP_LIT("..."));
  message = sp_str_replace_c8(message, '\n', ' ');
  message = sp_str_replace_c8(message, '{', '['); // @spader @hack
  message = sp_str_replace_c8(message, '}', ']');
  message = sp_str_pad(message, 32);

  sp_mutex_lock(&build->mutex);
  build->message = message;
  sp_mutex_unlock(&build->mutex);

  return SPN_OK;
}

spn_err_t spn_pkg_build_sync_local(spn_pkg_ctx_t* dep) {
  return spn_git_checkout(dep->ctx.paths.source, dep->metadata.commit);
}

void spn_pkg_build_stamp(spn_pkg_ctx_t* dep) {
  sp_io_stream_t io = sp_io_from_file(dep->ctx.paths.stamp, SP_IO_MODE_WRITE);

  spn_toml_writer_t writer = spn_toml_writer_new();
  spn_toml_begin_table_cstr(&writer, "package");
  spn_toml_append_str_cstr(&writer, "name", dep->ctx.package->name);
  spn_toml_append_str_cstr(&writer, "version", spn_semver_to_str(dep->metadata.version));
  spn_toml_append_str_cstr(&writer, "commit", dep->metadata.commit);
  spn_toml_append_str_cstr(&writer, "source", dep->ctx.paths.source);
  spn_toml_append_str_cstr(&writer, "work", dep->ctx.paths.work);
  spn_toml_end_table(&writer);

  spn_toml_begin_table_cstr(&writer, "profile");
  spn_toml_append_str_cstr(&writer, "name", dep->ctx.profile.name);
  spn_toml_append_str_cstr(&writer, "cc", dep->ctx.profile.cc.exe);
  spn_toml_append_str_cstr(&writer, "linkage", spn_pkg_linkage_to_str(dep->ctx.profile.linkage));
  spn_toml_append_str_cstr(&writer, "libc", spn_libc_kind_to_str(dep->ctx.profile.libc));
  spn_toml_append_str_cstr(&writer, "mode", spn_dep_build_mode_to_str(dep->ctx.profile.mode));
  spn_toml_append_str_cstr(&writer, "standard", spn_c_standard_to_str(dep->ctx.profile.standard));
  spn_toml_end_table(&writer);

  spn_toml_begin_array_cstr(&writer, "command");
  sp_da_for(dep->ctx.commands, it) {
    sp_ps_config_t command = dep->ctx.commands[it];
    spn_toml_append_array_table(&writer);
    spn_toml_append_str(&writer, sp_str_lit("command"), command.command);

    // Add arguments as an array if there are any
    u32 num_args = 0;
    for (; num_args < sp_carr_len(command.args); num_args++) {
      if (!sp_str_valid(command.args[num_args])) {
        break;
      }
    }
    if (num_args) {
      spn_toml_append_str_carr_cstr(&writer, "args", command.args, num_args);
    }


    bool has_env = !sp_ht_empty(command.env.env.vars);
    bool has_extra = sp_str_valid(command.env.extra[0].key);
    if (has_env || has_extra) {
      spn_toml_begin_table_cstr(&writer, "env");

      sp_ht_for_kv(command.env.env.vars, it) {
        spn_toml_append_str(&writer, *it.key, *it.val);
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

spn_build_ctx_t spn_build_ctx_make(spn_build_ctx_config_t config) {
  spn_build_ctx_t ctx = SP_ZERO_INITIALIZE();
  spn_build_ctx_init(&ctx, config);
  return ctx;
}

void spn_build_ctx_init(spn_build_ctx_t* ctx, spn_build_ctx_config_t config) {
  ctx->name = sp_str_copy(config.name);
  ctx->profile = config.builder->profile;
  ctx->package = config.package;
  ctx->builder = config.builder;
  ctx->paths.source = config.paths.source;
  ctx->paths.store = config.paths.store;
  ctx->paths.work = config.paths.work;
  spn_build_ctx_prepare_io(ctx);
}

spn_err_t spn_build_ctx_compile(spn_build_ctx_t* ctx) {
  if (!sp_fs_exists(ctx->package->paths.script)) {
    return SPN_ERROR;
  }

  spn_tcc_t* tcc = spn_tcc_new(ctx);
  sp_try(spn_tcc_add_file(tcc, ctx->package->paths.script));
  sp_try_as(tcc_set_options(tcc, "-nostdlib"), SPN_ERROR);
  sp_try_as(tcc_relocate(tcc), SPN_ERROR);
  ctx->package->on_package = tcc_get_symbol(tcc, "package");
  ctx->package->on_build = tcc_get_symbol(tcc, "build");

  return SPN_OK;
}

spn_err_t spn_build_ctx_run_script(spn_build_ctx_t* build) {
  sp_tm_timer_t timer = sp_tm_start_timer();

  sp_try(spn_build_ctx_compile(build));
  build->time.compile = sp_tm_lap_timer(&timer);

  if (build->package->on_build) {
    build->package->on_build(build);
    build->time.build = sp_tm_lap_timer(&timer);
  }

  if (build->package->on_package) {
    build->package->on_package(build);
    build->time.package = sp_tm_lap_timer(&timer);
  }

  return SPN_OK;
}

s32 spn_executor_sync_repo(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_ctx_t* build = (spn_pkg_ctx_t*)user_data;

  spn_event_buffer_push(spn.events, &build->ctx, SPN_BUILD_EVENT_SYNC);
  spn_pkg_build_sync_remote(build);

  spn_pkg_build_resolve_commit(build);

  return SPN_OK;
}

s32 spn_executor_run_pkg_build(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_ctx_t* build = (spn_pkg_ctx_t*)user_data;

  // @spader @allocator
  // you need to give the build context an allocator; technically, this is fine, since the default
  // malloc wrapper is stateless. but really, this memory would get allocated with some TLS allocator
  // and potentially junked before the main thread can read it. and also unfreeable
  spn_event_buffer_push_ex(spn.events, &build->ctx, (spn_build_event_t) {
    .kind = SPN_BUILD_EVENT_CHECKOUT,
    .checkout = {
      .commit = sp_str_copy(build->metadata.commit),
      .version = build->metadata.version,
      .message = sp_str_copy(build->message)
    }
  });
  spn_pkg_build_sync_local(build);

  spn_event_buffer_push(spn.events, &build->ctx, SPN_BUILD_EVENT_DEP_BUILD);
  //return 69;

  spn_err_t err = spn_pkg_build_run(build);
  switch (err) {
    //case SPN_OK:    { spn_event_buffer_push(spn.events, &build->ctx, SPN_BUILD_EVENT_DEP_BUILD_PASSED); break; }
    case SPN_OK:    { break; }
    case SPN_ERROR: { spn_event_buffer_push(spn.events, &build->ctx, SPN_BUILD_EVENT_DEP_BUILD_FAILED); break; }
  }

  return err;
}

s32 spn_executor_bin(spn_bg_cmd_t* cmd, void* user_data) {
  spn_bin_ctx_t* build = (spn_bin_ctx_t*)user_data;
  spn_event_buffer_push(spn.events, &build->ctx, SPN_BUILD_EVENT_TARGET_BUILD);

  spn_err_t err = spn_bin_build_run(build);
  switch (err) {
    case SPN_OK:    { spn_event_buffer_push(spn.events, &build->ctx, SPN_BUILD_EVENT_TARGET_BUILD_PASSED); break; }
    case SPN_ERROR: { spn_event_buffer_push(spn.events, &build->ctx, SPN_BUILD_EVENT_TARGET_BUILD_FAILED); break; }
  }

  return err;
}

bool spn_target_filter_pass_visibility(spn_target_filter_t* filter, spn_visibility_t visibility) {
  switch (visibility) {
    case SPN_VISIBILITY_PUBLIC: return !filter->disabled.public;
    case SPN_VISIBILITY_TEST: return !filter->disabled.test;
  }
}

bool spn_target_filter_pass(spn_target_filter_t* filter, spn_target_t* bin) {
  if (!sp_str_empty(filter->name)) {
    return sp_str_equal(filter->name, bin->name);
  }

  return spn_target_filter_pass_visibility(filter, bin->visibility);
}

bool spn_match_visibility(spn_visibility_t target, spn_visibility_t dep) {
  switch (target) {
    case SPN_VISIBILITY_PUBLIC: {
      switch (dep) {
        case SPN_VISIBILITY_PUBLIC: return true;
        case SPN_VISIBILITY_TEST: return false;
      }
    }
    case SPN_VISIBILITY_TEST: {
      switch (dep) {
        case SPN_VISIBILITY_PUBLIC: return true;
        case SPN_VISIBILITY_TEST: return true;
      }
    }
  }
  SP_UNREACHABLE_RETURN(false);
}

spn_builder_t* spn_builder_new() {
  spn_builder_t* build = SP_ALLOC(spn_builder_t);
  spn_builder_init(build);
  return build;
}

void spn_builder_init(spn_builder_t* build) {
  sp_ht_set_fns(build->contexts.bins, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(build->contexts.deps, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_mutex_init(&build->mutex, SP_MUTEX_PLAIN);
}


spn_pkg_ctx_t* spn_builder_find_pkg_ctx(spn_builder_t* build, sp_str_t name) {
  sp_mutex_lock(&build->mutex);
  spn_pkg_ctx_t* pkg = sp_ht_getp(build->contexts.deps, name);
  sp_mutex_unlock(&build->mutex);

  return pkg;
}

sp_str_t spn_compiler_to_str(spn_cc_kind_t compiler) {
  return sp_str_lit("clang");
}

spn_build_event_t spn_build_event_make(spn_build_ctx_t* ctx, spn_build_event_kind_t kind) {
  return (spn_build_event_t) {
    .ctx = ctx,
    .kind = kind
  };
}

spn_event_buffer_t* spn_event_buffer_new() {
  spn_event_buffer_t* events = SP_ALLOC(spn_event_buffer_t);
  sp_ring_buffer_init(&events->buffer, 64, sizeof(spn_build_event_t));
  return events;
}

void spn_event_buffer_push(spn_event_buffer_t* events, spn_build_ctx_t* ctx, spn_build_event_kind_t kind) {
  spn_event_buffer_push_ex(events, ctx, spn_build_event_make(ctx, kind));
}

void spn_event_buffer_push_ex(spn_event_buffer_t* events, spn_build_ctx_t* ctx, spn_build_event_t event) {
  event.ctx = ctx;

  sp_mutex_lock(&events->mutex);
  sp_rb_push(events->buffer, event);
  sp_mutex_unlock(&events->mutex);
}

sp_da(spn_build_event_t) spn_event_buffer_drain(spn_event_buffer_t* events) {
  sp_mutex_lock(&events->mutex);

  sp_da(spn_build_event_t) result = SP_NULLPTR;
  sp_rb_for(events->buffer, it) {
    spn_build_event_t* event = sp_rb_it_getp(&it, spn_build_event_t);
    sp_da_push(result, *event);
  }

  sp_ring_buffer_clear(&events->buffer);
  sp_mutex_unlock(&events->mutex);

  return result;
}

spn_err_t spn_bin_build_run(spn_bin_ctx_t* build) {
  spn_pkg_t* package = build->ctx.package;
  spn_target_t* bin = build->bin;
  spn_builder_t* builder = build->ctx.builder;

  SP_ASSERT(!sp_da_empty(bin->source));

  spn_cc_t cc = spn_cc_new(&build->ctx);
  spn_cc_set_output_dir(&cc, build->ctx.paths.bin);

  sp_da_for(package->include, it) {
    spn_cc_add_include_rel(&cc, package->include[it]);
  }
  sp_da_for(package->define, it) {
    spn_cc_add_define(&cc, package->define[it]);
  }

  spn_cc_target_t* target = spn_cc_add_target(&cc, SPN_CC_TARGET_EXECUTABLE, bin->name);
  sp_da_for(bin->source, it) {
    spn_cc_target_add_source(target, bin->source[it]);
  }
  sp_da_for(bin->include, it) {
    spn_cc_target_add_include_rel(target, bin->include[it]);
  }
  sp_da_for(bin->define, it) {
    spn_cc_target_add_define(target, bin->define[it]);
  }

  sp_ht_for_kv(package->deps, it) {
    if (spn_match_visibility(bin->visibility, it.val->visibility)) {
      spn_cc_target_add_dep(target, spn_builder_find_pkg_ctx(builder, *it.key));
    }
  }

  return spn_cc_run(&cc);
}

/////////
// APP //
/////////

void spn_app_prepare_build(spn_app_t* app) {
  app->build = (spn_builder_t) {
    .profile = app->config.profile,
    .filter = app->config.filter
  };
  spn_builder_init(&app->build);

  app->paths.build = sp_fs_join_path(app->package.paths.dir, sp_str_lit("build"));
  app->paths.profile = sp_fs_join_path(app->paths.build, app->build.profile.name);

  // FILTER
  sp_ht_for_kv(app->package.binaries, it) {
    spn_target_t* target = it.val;
    if (spn_target_filter_pass(&app->build.filter, target)) {
      sp_da_push(app->build.targets, target);
    }
  }

  sp_ht_for_kv(app->package.tests, it) {
    spn_target_t* target = it.val;
    if (spn_target_filter_pass(&app->build.filter, target)) {
      sp_da_push(app->build.targets, target);
    }
  }

  // PACKAGE
  app->build.contexts.package = spn_build_ctx_make((spn_build_ctx_config_t) {
    .name = app->package.name,
    .package = &app->package,
    .builder = &app->build,
    .paths = {
      .store = sp_fs_join_path(app->paths.profile, sp_str_lit("store")),
      .work = sp_fs_join_path(app->paths.profile, sp_str_lit("work")),
      .source = sp_str_copy(app->package.paths.dir),
    }
  });

  // BINARIES
  sp_da_for(app->build.targets, it) {
    spn_target_t* target = app->build.targets[it];
    spn_bin_ctx_t b = {
      .bin = target,
      .ctx = spn_build_ctx_make((spn_build_ctx_config_t) {
        .name = target->name,
        .package = &app->package,
        .builder = &app->build,
        .paths = {
          .store = sp_fs_join_path(app->paths.profile, sp_str_lit("store")),
          .work = sp_fs_join_path(app->paths.profile, sp_str_lit("work")),
          .source = sp_str_copy(app->package.paths.dir),
        }
      })
    };
    sp_ht_insert(app->build.contexts.bins, b.ctx.name, b);
  }

  // DEPS
  sp_ht_for_kv(app->resolver.resolved, it) {
    sp_str_t name = *it.key;
    spn_resolved_pkg_t resolved = *it.val;
    spn_pkg_t* package = spn_app_find_package(app, name);
    SP_ASSERT(package);

    spn_metadata_t* metadata = sp_ht_getp(package->metadata, resolved.version);
    SP_ASSERT(metadata);

    spn_pkg_linkage_t linkage = SPN_LIB_KIND_NONE;

    spn_dep_options_t* options = sp_ht_getp(app->package.config, name);
    if (options) {
      spn_dep_option_t* kind = sp_ht_getp(*options, sp_str_lit("kind"));
      if (kind) {
        linkage = spn_lib_kind_from_str(kind->str);
      }
    }

    if (!linkage) {
      spn_pkg_linkage_t kinds [] = {
        SPN_LIB_KIND_SOURCE, SPN_LIB_KIND_STATIC, SPN_LIB_KIND_SHARED
      };
      SP_CARR_FOR(kinds, it) {
        if (sp_ht_getp(package->lib.enabled, kinds[it])) {
          linkage = kinds[it];
        }
      }
    }

    sp_dyn_array(sp_hash_t) hashes = SP_NULLPTR;
    sp_dyn_array_push(hashes, sp_hash_str(metadata->commit));
    sp_dyn_array_push(hashes, sp_hash_str(app->build.profile.cc.exe));
    sp_dyn_array_push(hashes, app->build.profile.cc.kind);
    sp_dyn_array_push(hashes, app->build.profile.libc);
    sp_dyn_array_push(hashes, app->build.profile.mode);
    sp_dyn_array_push(hashes, linkage);
    sp_dyn_array_push(hashes, metadata->version.major);
    sp_dyn_array_push(hashes, metadata->version.minor);
    sp_dyn_array_push(hashes, metadata->version.patch);
    sp_hash_t build_id = sp_hash_combine(hashes, sp_dyn_array_size(hashes));
    sp_str_t build_str = sp_format("{}", SP_FMT_SHORT_HASH(build_id));

    spn_pkg_ctx_t dep = {
      .metadata = *metadata,
      .build_id = build_id,
      .linkage = linkage
    };

    switch (resolved.kind) {
      case SPN_PACKAGE_KIND_INDEX: {
        dep.ctx = spn_build_ctx_make((spn_build_ctx_config_t) {
          .name = package->name,
          .package = package,
          .builder = &app->build,
          .paths = {
            .work = sp_fs_join_path(sp_fs_join_path(spn.paths.build, package->name), build_str),
            .store = sp_fs_join_path(sp_fs_join_path(spn.paths.store, package->name), build_str),
            .source = sp_fs_join_path(spn.paths.source, package->name),
          }
        });
        break;
      }
      case SPN_PACKAGE_KIND_FILE:
      case SPN_PACKAGE_KIND_WORKSPACE:
      case SPN_PACKAGE_KIND_REMOTE:
      case SPN_PACKAGE_KIND_NONE: {
        SP_UNREACHABLE_CASE();
      }
    }

    sp_ht_insert(app->build.contexts.deps, name, dep);
  }

  // GRAPH: DEPS
  sp_ht_for_kv(app->build.contexts.deps, it) {
    spn_pkg_ctx_t* dep = it.val;
    spn_pkg_build_nodes_t nodes = {
      .fetch = spn_bg_add_fn(&app->build.graph, spn_executor_sync_repo, dep),
      .repo = spn_bg_add_file(&app->build.graph, dep->ctx.paths.source),
      .build = spn_bg_add_fn(&app->build.graph, spn_executor_run_pkg_build, dep),
      .stamp = spn_bg_add_file(&app->build.graph, dep->ctx.paths.stamp),
    };

    spn_bg_tag_command(&app->build.graph, nodes.fetch, sp_format("{} (sync)", SP_FMT_STR(dep->ctx.name)));
    spn_bg_tag_command(&app->build.graph, nodes.build, sp_format("{} (build)", SP_FMT_STR(dep->ctx.name)));

    spn_bg_cmd_add_output(&app->build.graph, nodes.fetch, nodes.repo);
    spn_bg_cmd_add_input(&app->build.graph, nodes.build, nodes.repo);
    spn_bg_cmd_add_output(&app->build.graph, nodes.build, nodes.stamp);

    dep->nodes = nodes;
  }

  // GRAPH: MANIFEST
  spn_bg_id_t manifest = spn_bg_add_file(&app->build.graph, spn.paths.manifest);

  // GRAPH: BINARIES
  sp_ht_for_kv(app->build.contexts.bins, it) {
    spn_build_ctx_t* ctx = &it.val->ctx;
    spn_target_t* bin = it.val->bin;
    sp_str_t file_path = sp_fs_join_path(ctx->paths.bin, ctx->name);

    spn_bin_build_nodes_t nodes = {
      .build = spn_bg_add_fn(&app->build.graph, spn_executor_bin, it.val),
      .bin = spn_bg_add_file(&app->build.graph, file_path),
    };

    spn_bg_tag_command(&app->build.graph, nodes.build, sp_format("[{}] build", SP_FMT_STR(ctx->name)));

    spn_bg_cmd_add_output(&app->build.graph, nodes.build, nodes.bin);

    sp_ht_for_kv(app->build.contexts.deps, j) {
      spn_bg_cmd_add_input(&app->build.graph, nodes.build, j.val->nodes.stamp);
    }

    spn_bg_cmd_add_input(&app->build.graph, nodes.build, manifest);

    sp_da_for(bin->source, it) {
      spn_bg_id_t node = spn_bg_add_file(&app->build.graph, sp_fs_join_path(app->paths.dir, bin->source[it]));
      spn_bg_cmd_add_input(&app->build.graph, nodes.build, node);
    }

    sp_da_for(app->package.include, it) {
      sp_da(sp_os_dir_ent_t) files = sp_fs_collect(app->package.include[it]);
      sp_da_for(files, n) {
        spn_bg_id_t node = spn_bg_add_file(&app->build.graph, sp_fs_join_path(app->paths.dir, files[n].file_path));
        spn_bg_cmd_add_input(&app->build.graph, nodes.build, node);
      }
    }

    sp_da_for(bin->include, it) {
      sp_da(sp_os_dir_ent_t) files = sp_fs_collect(bin->include[it]);
      sp_da_for(files, n) {
        spn_bg_id_t node = spn_bg_add_file(&app->build.graph, sp_fs_join_path(app->paths.dir, files[n].file_path));
        spn_bg_cmd_add_input(&app->build.graph, nodes.build, node);
      }
    }

  }
}


void spn_app_update_lock_file(spn_app_t* app) {
  spn_lock_file_t lock = spn_build_lock_file();

  // Add top-level package's system_deps to lock
  sp_da_for(app->package.system_deps, i) {
    sp_ht_insert(lock.system_deps, sp_str_copy(app->package.system_deps[i]), true);
  }

  sp_da(sp_str_t) keys = SP_NULLPTR;
  sp_ht_collect_keys(lock.entries, keys);
  sp_dyn_array_sort(keys, sp_str_sort_kernel_alphabetical);

  spn_toml_writer_t toml = spn_toml_writer_new();

  spn_toml_begin_table_cstr(&toml, "spn");
  spn_toml_append_str_cstr(&toml, "version", sp_str_lit(SPN_VERSION));
  spn_toml_append_str_cstr(&toml, "commit", sp_str_lit(SPN_COMMIT));
  spn_toml_end_table(&toml);

  // Write [package] table with system_deps
  if (sp_ht_size(lock.system_deps)) {
    spn_toml_begin_table_cstr(&toml, "package");
    sp_da(sp_str_t) sys_deps = SP_NULLPTR;
    sp_ht_collect_keys(lock.system_deps, sys_deps);
    sp_dyn_array_sort(sys_deps, sp_str_sort_kernel_alphabetical);
    spn_toml_append_str_array_cstr(&toml, "system_deps", sys_deps);
    spn_toml_end_table(&toml);
  }

  spn_toml_begin_array_cstr(&toml, "dep");
  sp_dyn_array_for(keys, it) {
    spn_lock_entry_t* entry = sp_ht_getp(lock.entries, keys[it]);

    spn_toml_append_array_table(&toml);
    spn_toml_append_str_cstr(&toml, "name", entry->name);
    spn_toml_append_str_cstr(&toml, "version", spn_semver_to_str(entry->version));
    spn_toml_append_str_cstr(&toml, "commit", entry->commit);
    spn_toml_append_str_cstr(&toml, "kind", spn_package_kind_to_str(entry->kind));
    spn_toml_append_str_cstr(&toml, "visibility", spn_visibility_to_str(entry->visibility));

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

void spn_app_write_manifest(spn_pkg_t* package, sp_str_t path) {
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
  if (!sp_dyn_array_empty(package->system_deps)) {
    spn_toml_append_str_array_cstr(&toml, "system_deps", package->system_deps);
  }
  if (!sp_dyn_array_empty(package->define)) {
    spn_toml_append_str_array_cstr(&toml, "define", package->define);
  }
  spn_toml_end_table(&toml);

  if (sp_ht_size(package->deps)) {
    // Write package deps
    bool has_package_deps = false;
    sp_ht_for_kv(package->deps, it) {
      if (it.val->visibility != SPN_VISIBILITY_TEST) {
        has_package_deps = true;
        break;
      }
    }
    if (has_package_deps) {
      spn_toml_begin_table_cstr(&toml, "deps.package");
      sp_ht_for_kv(package->deps, it) {
        if (it.val->visibility != SPN_VISIBILITY_TEST) {
          spn_toml_append_str(&toml, *it.key, spn_pkg_req_to_str(*it.val));
        }
      }
      spn_toml_end_table(&toml);
    }

    // Write test deps
    bool has_test_deps = false;
    sp_ht_for_kv(package->deps, it) {
      if (it.val->visibility == SPN_VISIBILITY_TEST) {
        has_test_deps = true;
        break;
      }
    }
    if (has_test_deps) {
      spn_toml_begin_table_cstr(&toml, "deps.test");
      sp_ht_for_kv(package->deps, it) {
        if (it.val->visibility == SPN_VISIBILITY_TEST) {
          spn_toml_append_str(&toml, *it.key, spn_pkg_req_to_str(*it.val));
        }
      }
      spn_toml_end_table(&toml);
    }
  }

  if (sp_ht_size(package->profiles)) {
    spn_toml_begin_array_cstr(&toml, "profile");
    sp_ht_for_kv(package->profiles, it) {
      if (it.val->kind != SPN_PROFILE_BUILTIN) {
        spn_toml_append_array_table(&toml);
        spn_toml_append_str_cstr(&toml, "name", it.val->name);
        spn_toml_append_str_cstr(&toml, "cc", it.val->cc.exe);
        spn_toml_append_str_cstr(&toml, "linkage", spn_pkg_linkage_to_str(it.val->linkage));
        spn_toml_append_str_cstr(&toml, "libc", spn_libc_kind_to_str(it.val->libc));
        spn_toml_append_str_cstr(&toml, "standard", spn_c_standard_to_str(it.val->standard));
        spn_toml_append_str_cstr(&toml, "mode", spn_dep_build_mode_to_str(it.val->mode));
      }
    }
    spn_toml_end_array(&toml);
  }

  if (sp_ht_size(package->lib.enabled)) {
    spn_toml_begin_table_cstr(&toml, "lib");
    sp_da(sp_str_t) kinds = SP_NULLPTR;
    sp_ht_for_kv(package->lib.enabled, it) {
      if (*it.val) {
        sp_dyn_array_push(kinds, spn_pkg_linkage_to_str(*it.key));
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

  if (sp_ht_size(package->binaries)) {
    spn_toml_begin_array_cstr(&toml, "bin");
    sp_ht_for_kv(package->binaries, it) {
      spn_toml_append_array_table(&toml);
      spn_toml_append_str_cstr(&toml, "name", it.val->name);

      if (it.val->visibility != SPN_VISIBILITY_PUBLIC) {
        spn_toml_append_str_cstr(&toml, "kind", spn_bin_kind_to_str(it.val->visibility));
      }
      if (sp_dyn_array_size(it.val->source)) {
        spn_toml_append_str_array_cstr(&toml, "source", it.val->source);
      }
      if (sp_dyn_array_size(it.val->include)) {
        spn_toml_append_str_array_cstr(&toml, "include", it.val->include);
      }
      if (sp_dyn_array_size(it.val->define)) {
        spn_toml_append_str_array_cstr(&toml, "define", it.val->define);
      }
    }
    spn_toml_end_array(&toml);
  }

  if (sp_ht_size(package->tests)) {
    spn_toml_begin_array_cstr(&toml, "test");
    sp_ht_for_kv(package->tests, it) {
      spn_toml_append_array_table(&toml);
      spn_toml_append_str_cstr(&toml, "name", it.val->name);

      if (sp_dyn_array_size(it.val->source)) {
        spn_toml_append_str_array_cstr(&toml, "source", it.val->source);
      }
      if (sp_dyn_array_size(it.val->include)) {
        spn_toml_append_str_array_cstr(&toml, "include", it.val->include);
      }
      if (sp_dyn_array_size(it.val->define)) {
        spn_toml_append_str_array_cstr(&toml, "define", it.val->define);
      }
    }
    spn_toml_end_array(&toml);
  }

  if (sp_ht_size(package->options)) {
    spn_toml_begin_table_cstr(&toml, "options");
    sp_ht_for_kv(package->options, it) {
      spn_toml_append_option(&toml, *it.key, *it.val);
    }
    spn_toml_end_table(&toml);
  }

  if (sp_ht_size(package->config)) {
    spn_toml_begin_table_cstr(&toml, "config");

    sp_ht_for_kv(package->config, it) {
      spn_toml_begin_table(&toml, *it.key);
      sp_ht_for_kv(*it.val, n) {
        spn_toml_append_option(&toml, *n.key, *n.val);
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
      return sp_fs_join_path(app.paths.dir, registry->location);
    }
    case SPN_PACKAGE_KIND_INDEX: {
      return sp_str_copy(registry->location);
    }
    case SPN_PACKAGE_KIND_FILE:
    case SPN_PACKAGE_KIND_REMOTE: {
      SP_UNREACHABLE();
    }
    case SPN_PACKAGE_KIND_NONE: {
      SP_UNREACHABLE_RETURN(sp_str_lit(""));
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_pkg_t* spn_app_find_package(spn_app_t* app, sp_str_t name) {
  spn_pkg_t* package = sp_ht_getp(app->cache, name);
  return package;
}

spn_pkg_t* spn_app_find_package_from_request(spn_app_t* app, spn_pkg_req_t request) {
  spn_pkg_t* package = spn_app_find_package(app, request.name);
  if (package->kind != request.kind) {
    return SP_NULLPTR;
  }

  return package;
}

spn_pkg_t* spn_app_ensure_package(spn_app_t* app, spn_pkg_req_t request) {
  sp_str_t name = request.name;

  if (!sp_ht_key_exists(app->cache, name)) {
    switch (request.kind) {
      case SPN_PACKAGE_KIND_FILE: {
        sp_str_t prefix = sp_str_lit("file://");
        sp_str_t manifest = {
          .data = request.file.data + prefix.len,
          .len = request.file.len - prefix.len
        };
        spn_pkg_t package = spn_pkg_from_manifest(manifest);
        sp_ht_insert(app->cache, name, package);

        break;
      }
      case SPN_PACKAGE_KIND_INDEX: {
        sp_str_t* path = sp_ht_getp(app->registry, name);
        if (!path) {
          spn_app_bail_on_missing_package(app, name);
        }

        spn_pkg_t package = spn_pkg_from_index(*path);
        sp_ht_insert(app->cache, name, package);

        break;
      }
      case SPN_PACKAGE_KIND_REMOTE:
      case SPN_PACKAGE_KIND_WORKSPACE: {
        SP_FATAL("unimplemented find_package");
        break;
      }
      case SPN_PACKAGE_KIND_NONE: {
        SP_UNREACHABLE_RETURN(SP_NULLPTR);
      }
    }
  }

  return spn_app_find_package_from_request(app, request);
}

spn_app_t spn_app_new() {
  spn_app_t app = SP_ZERO_INITIALIZE();

  sp_ht_set_fns(app.cache, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(app.build.contexts.deps, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(app.registry, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(app.build.contexts.bins, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(app.build.contexts.deps, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);

  return app;
}

spn_app_t spn_app_init_and_write(sp_str_t path, sp_str_t name, spn_app_init_mode_t mode) {
  sp_str_t paths [] = {
    sp_fs_join_path(path, sp_str_lit("spn.toml")),
    sp_fs_join_path(path, sp_str_lit("spn.c")),
  };
  sp_carr_for(paths, it) {
    if (sp_fs_exists(paths[it])) {
      SP_FATAL("{:fg brightcyan} already exists; bailing", SP_FMT_STR(paths[it]));
    }
  }

  spn_app_t app = spn_app_new();
  switch (mode) {
    case SPN_APP_INIT_NORMAL: {
      app.package = spn_pkg_from_default(path, name);

      sp_str_t main = sp_fs_join_path(path, sp_str_lit("main.c"));
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

      spn_app_write_manifest(&app.package, app.package.paths.manifest);

      break;
    }
    case SPN_APP_INIT_BARE: {
      app.package = spn_pkg_from_bare_default(path, name);
      spn_app_write_manifest(&app.package, app.package.paths.manifest);

      break;
    }
  }

  return app;
}

void spn_app_load(spn_app_t* app, sp_str_t manifest_path) {
  // Load the top level package
  if (sp_fs_exists(manifest_path)) {
    app->package = spn_pkg_from_manifest(manifest_path);
  }

  app->paths.dir = app->package.paths.dir;
  app->paths.lock = sp_fs_join_path(app->paths.dir, SP_LIT("spn.lock"));

  // Now that we know all the registries, discover all packages
  sp_dyn_array_push(app->search, spn_registry_get_path(&spn.registry));

  sp_dyn_array_for(spn.config.registries, it) {
    spn_registry_t* registry = &spn.config.registries[it];
    sp_dyn_array_push(app->search, spn_registry_get_path(registry));
  }

  sp_dyn_array_for(app->package.registries, it) {
    spn_registry_t* registry = &app->package.registries[it];
    sp_dyn_array_push(app->search, spn_registry_get_path(registry));
  }

  sp_dyn_array_for(app->search, i) {
    sp_str_t path = app->search[i];
    if (!sp_fs_exists(path)) continue;
    if (!sp_fs_is_dir(path)) {
      SP_FATAL(
        "{:fg brightcyan} is on the search path, but it's not a directory",
        SP_FMT_STR(path)
      );
    }

    sp_da(sp_os_dir_ent_t) entries = sp_fs_collect(path);
    sp_dyn_array_for(entries, i) {
      sp_os_dir_ent_t entry = entries[i];
      sp_str_t stem = sp_fs_get_stem(entry.file_path);
      sp_ht_insert(app->registry, stem, entry.file_path);
    }
  }

  // Load the lock file
  if (sp_fs_exists(app->paths.lock)) {
    sp_opt_set(app->lock, spn_lock_file_load(app->paths.lock));
  }

  // apply any defaults
  if (sp_ht_empty(app->package.profiles)) {
    spn_profile_t debug = {
      .name = sp_str_lit("debug"),
      .cc = { SPN_CC_GCC, sp_str_lit("gcc") },
      .linkage = SPN_LIB_KIND_SHARED,
      .libc = SPN_LIBC_GNU,
      .standard = SPN_C11,
      .mode = SPN_DEP_BUILD_MODE_DEBUG,
      .kind = SPN_PROFILE_BUILTIN,
    };
    sp_ht_insert(app->package.profiles, debug.name, debug);

    spn_profile_t profiles [] = {
      {
        .name = sp_str_lit("debug"),
        .linkage  = SPN_LIB_KIND_SHARED,
        .libc     = SPN_LIBC_GNU,
        .standard = SPN_C11,
        .mode     = SPN_DEP_BUILD_MODE_DEBUG,
        .kind     = SPN_PROFILE_BUILTIN,
        .cc = {
          .kind = SPN_CC_GCC,
          .exe = sp_str_lit("gcc")
        },
      },
      {
        .name     = sp_str_lit("release"),
        .linkage  = SPN_LIB_KIND_SHARED,
        .libc     = SPN_LIBC_GNU,
        .standard = SPN_C11,
        .mode     = SPN_DEP_BUILD_MODE_RELEASE,
        .kind     = SPN_PROFILE_BUILTIN,
        .cc = {
          .kind = SPN_CC_GCC,
          .exe = sp_str_lit("gcc")
        },
      }
    };
    sp_carr_for(profiles, it) {
      sp_ht_insert(app->package.profiles, profiles[it].name, profiles[it]);
    }
  }
}

void spn_ctx_log(const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  sp_str_t str = sp_format_v(SP_CSTR(fmt), args);
  va_end(args);

  sp_io_write_line(&spn.logger.out, str);
}

void spn_ctx_warn(const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  sp_str_t str = sp_format_v(SP_CSTR(fmt), args);
  va_end(args);

  sp_io_write_line(&spn.logger.err, str);
}

void spn_ctx_error(const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  sp_str_t str = sp_format_v(SP_CSTR(fmt), args);
  va_end(args);

  sp_io_write_line(&spn.logger.err, str);
}

void spn_ctx_tui(const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  sp_str_t str = sp_format_v(SP_CSTR(fmt), args);
  va_end(args);

  sp_io_write_line(&spn.logger.err, str);
}

#ifdef SP_POSIX
void spn_signal_handler(s32 kind) {
  switch (kind) {
    case SIGINT: {
      printf("sigint\n");
      sp_atomic_s32_set(&spn.sp->shutdown, 1);
      sp_io_write_new_line(&spn.logger.out);
      sp_io_write_new_line(&spn.logger.err);
      break;
    }
    default: {
      break;
    }
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



/////////
// APP //
/////////
sp_app_result_t spn_init(sp_app_t* sp) {
  spn.sp = sp;

  spn_install_signal_handlers();
  spn.logger.out = sp_io_from_file_handle(STDOUT_FILENO, SP_IO_FILE_CLOSE_MODE_NONE);
  spn.logger.err = sp_io_from_file_handle(STDERR_FILENO, SP_IO_FILE_CLOSE_MODE_NONE);

  spn.events = spn_event_buffer_new();

  sp_atomic_s32_set(&spn.control, 0);

  spn.paths.cwd = sp_fs_get_cwd();
  spn.paths.bin = sp_os_get_bin_path();
  spn.paths.storage = sp_fs_join_path(sp_fs_get_storage_path(), sp_str_lit("spn"));
  spn.paths.tools.dir = sp_fs_join_path(spn.paths.storage, sp_str_lit("tools"));
  spn.paths.tools.manifest = sp_fs_join_path(spn.paths.tools.dir, sp_str_lit("spn.toml"));
  spn.paths.tools.lock = sp_fs_join_path(spn.paths.storage, sp_str_lit("spn.lock"));

  // Config
  spn.paths.config_dir = sp_fs_join_path(sp_fs_get_config_path(), SP_LIT("spn"));
  spn.paths.config = sp_fs_join_path(spn.paths.config_dir, SP_LIT("spn.toml"));

  if (sp_fs_exists(spn.paths.config)) {
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
    spn.paths.spn = sp_fs_join_path(spn.paths.storage, sp_str_lit("spn"));
  }

  spn.paths.index = sp_fs_join_path(spn.paths.spn, sp_str_lit("packages"));
  spn.paths.include = sp_fs_join_path(spn.paths.spn, sp_str_lit("include"));

  if (!sp_fs_exists(spn.paths.spn)) {
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
  spn.paths.runtime = sp_fs_join_path(spn.paths.storage, SP_LIT("runtime"));
  spn.paths.log = sp_fs_join_path(spn.paths.storage, SP_LIT("log"));
  spn.paths.cache = sp_fs_join_path(spn.paths.storage, SP_LIT("cache"));
  spn.paths.source = sp_fs_join_path(spn.paths.cache, SP_LIT("source"));
  spn.paths.build = sp_fs_join_path(spn.paths.cache, SP_LIT("build"));
  spn.paths.store = sp_fs_join_path(spn.paths.cache, SP_LIT("store"));

  sp_fs_create_dir(spn.paths.log);
  sp_fs_create_dir(spn.paths.cache);
  sp_fs_create_dir(spn.paths.source);
  sp_fs_create_dir(spn.paths.build);
  sp_fs_create_dir(spn.paths.store);
  sp_fs_create_dir(spn.paths.bin);
  sp_fs_create_dir(spn.paths.tools.dir);

  sp_tm_epoch_t now = sp_tm_now_epoch();
  spn.paths.invocation = sp_fs_join_path(spn.paths.log, sp_tm_epoch_to_iso8601(now));
  sp_fs_create_file(spn.paths.invocation);

  spn_cli_t* cli = &spn.cli;
  spn.cli.cmd = (spn_cli_command_usage_t) {
    .name = "spn",
    .summary = "A package manager and build tool for modern C",
    .opts = {
      { "h", "help", SPN_CLI_OPT_KIND_BOOLEAN, "Print help message", SPN_CLI_NO_PLACEHOLDER, &cli->help },
      { "C", "project-dir", SPN_CLI_OPT_KIND_STRING, "Specify the directory containing project file", "DIR", &cli->project_dir },
      { "f", "file", SPN_CLI_OPT_KIND_STRING, "Specify the project file path", "FILE", &cli->project_file },
      { "o", "output", SPN_CLI_OPT_KIND_STRING, "Output mode: interactive, noninteractive, quiet, none", "MODE", &cli->output }
    },
    .commands = (spn_cli_command_usage_t[]) {
      {
        .name = "init",
        .opts = {
          {
            .brief = "b",
            .name = "bare",
            .kind = SPN_CLI_OPT_KIND_BOOLEAN,
            .summary = "Create minimal project without sp dependency or main.c",
            .ptr = &cli->init.bare
          }
        },
        .summary = "Initialize a project in the current directory",
        .handler = spn_cli_init
      },
      {
        .name = "add",
        .args = {
          {
            .name = "package",
            .kind = SPN_CLI_ARG_KIND_REQUIRED,
            .summary = "The package to add",
            .ptr = &spn.cli.add.package
          }
        },
        .opts = {
          {
            .brief = "t",
            .name = "test",
            .kind = SPN_CLI_OPT_KIND_BOOLEAN,
            .summary = "Add as a test dependency",
            .ptr = &spn.cli.add.test
          }
        },
        .summary = "Add the latest version of a package to the project",
        .handler = spn_cli_add
      },

      {
        .name = "build",
        .opts = {
          {
            .brief = "f",
            .name = "force",
            .kind = SPN_CLI_OPT_KIND_BOOLEAN,
            .summary = "Force build, even if it exists in store",
            .ptr = &spn.cli.build.force
          },
          {
            .brief = "p",
            .name = "profile",
            .kind = SPN_CLI_OPT_KIND_STRING,
            .summary = "Profile to use for building",
            .placeholder = "PROFILE",
            .ptr = &spn.cli.build.profile
          },
          {
            .brief = "t",
            .name = "target",
            .kind = SPN_CLI_OPT_KIND_STRING,
            .summary = "Target to build",
            .placeholder = "TARGET",
            .ptr = &spn.cli.build.target
          },
          {
            .name = "tests",
            .kind = SPN_CLI_OPT_KIND_BOOLEAN,
            .summary = "Include test targets",
            .ptr = &spn.cli.build.tests
          }
        },
        .summary = "Build the project, including dependencies, from source",
        .handler = spn_cli_build
      },

      {
        .name = "test",
        .opts = {
          {
            .brief = "p",
            .name = "profile",
            .kind = SPN_CLI_OPT_KIND_STRING,
            .summary = "Profile to use for building",
            .placeholder = "PROFILE",
            .ptr = &spn.cli.test.profile
          },
          {
            .brief = "t",
            .name = "target",
            .kind = SPN_CLI_OPT_KIND_STRING,
            .summary = "Test target to run",
            .placeholder = "TARGET",
            .ptr = &spn.cli.test.target
          }
        },
        .summary = "Build and run tests",
        .handler = spn_cli_test
      },

      {
        .name = "generate",
        .opts = {
          {
            .brief = "g",
            .name = "generator",
            .kind = SPN_CLI_OPT_KIND_STRING,
            .summary = "Generator type (raw, shell, make)",
            .placeholder = "TYPE",
            .ptr = &spn.cli.generate.generator
          },
          {
            .brief = "c",
            .name = "compiler",
            .kind = SPN_CLI_OPT_KIND_STRING,
            .summary = "Compiler to format flags for (gcc, clang, tcc)",
            .placeholder = "COMPILER",
            .ptr = &spn.cli.generate.compiler
          },
          {
            .brief = "p",
            .name = "path",
            .kind = SPN_CLI_OPT_KIND_STRING,
            .summary = "Output directory for generated file",
            .placeholder = "PATH",
            .ptr = &spn.cli.generate.path
          }
        },
        .summary = "Generate build system files with dependency flags",
        .handler = spn_cli_generate
      },
      {
        .name = "link",
        .args = {
          {
            .name = "kind",
            .kind = SPN_CLI_ARG_KIND_REQUIRED,
            .summary = "The link kind",
            .ptr = &spn.cli.copy.directory
          }
        },
        .summary = "Link or copy the binary outputs of your dependencies",
        .handler = spn_cli_copy
      },
      {
        .name = "update",
        .args = {
          {
            .name = "package",
            .kind = SPN_CLI_ARG_KIND_REQUIRED,
            .summary = "The package to update",
            .ptr = &spn.cli.update.package
          }
        },
        .summary = "Update an existing package to the latest version in the project",
        .handler = spn_cli_update
      },
      {
        .name = "list",
        .summary = "List all known packages in all registries",
        .handler = spn_cli_list
      },
      {
        .name = "which",
        .opts = {
          {
            .brief = "d",
            .name = "dir",
            .kind = SPN_CLI_OPT_KIND_STRING,
            .summary = "Which directory to show (store, include, lib, source, work, vendor)",
            .placeholder = "DIR",
            .ptr = &cli->which.dir
          }
        },
        .args = {
          {
            .name = "package",
            .kind = SPN_CLI_ARG_KIND_OPTIONAL,
            .summary = "The package to show path for",
            .ptr = &cli->which.package
          }
        },
        .summary = "Print the absolute path of a cache dir for a package",
        .handler = spn_cli_which
      },
      {
        .name = "ls",
        .opts = {
          {
            .brief = "d",
            .name = "dir",
            .kind = SPN_CLI_OPT_KIND_STRING,
            .summary = "Which directory to list (store, include, lib, source, work, vendor)",
            .placeholder = "DIR",
            .ptr = &cli->ls.dir
          }
        },
        .args = {
          {
            .name = "package",
            .kind = SPN_CLI_ARG_KIND_OPTIONAL,
            .summary = "The package to list",
            .ptr = &cli->ls.package
          }
        },
        .summary = "Run ls against a cache dir for a package (e.g. to see build output)",
        .handler = spn_cli_ls
      },
      {
        .name = "manifest",
        .args = {
          {
            .name = "package",
            .kind = SPN_CLI_ARG_KIND_REQUIRED,
            .summary = "The package name",
            .ptr = &cli->manifest.package
          }
        },
        .summary = "Print the full manifest source for a package",
        .handler = spn_cli_manifest
      },
      {
        .name = "tool",
        .summary = "Run, install, and manage binaries defined by spn packages",
        .commands = (spn_cli_command_usage_t[]) {
          {
            .name = "install",
            .opts = {
              {
                .brief = "f",
                .name = "force",
                .kind = SPN_CLI_OPT_KIND_BOOLEAN,
                .summary = "Force reinstall even if already installed",
                .ptr = &cli->tool.install.force
              },
              {
                .brief = "v",
                .name = "version",
                .kind = SPN_CLI_OPT_KIND_STRING,
                .summary = "Version to install",
                .placeholder = "VERSION",
                .ptr = &cli->tool.install.version
              }
            },
            .args = {
              {
                .name = "package",
                .kind = SPN_CLI_ARG_KIND_REQUIRED,
                .summary = "The package to install",
                .ptr = &cli->tool.install.package
              }
            },
            .summary = "Install a package's binary targets to the PATH",
            .handler = spn_cli_tool_install
          },
          {
            .name = "uninstall",
            .opts = {
              {
                .brief = "f",
                .name = "force",
                .kind = SPN_CLI_OPT_KIND_BOOLEAN,
                .summary = "Force removal even if not installed by spn",
                .ptr = &cli->tool.install.force
              }
            },
            .args = {
              {
                .name = "package",
                .kind = SPN_CLI_ARG_KIND_REQUIRED,
                .summary = "The package to uninstall",
                .ptr = &cli->tool.install.package
              }
            },
            .summary = "Uninstall a package's binary targets from the PATH",
            .handler = spn_cli_tool_uninstall
          },
          {
            .name = "run",
            .args = {
              {
                .name = "package",
                .kind = SPN_CLI_ARG_KIND_REQUIRED,
                .summary = "The package to run",
                .ptr = &cli->tool.run.package
              },
              {
                .name = "command",
                .kind = SPN_CLI_ARG_KIND_OPTIONAL,
                .summary = "The command to run",
                .ptr = &cli->tool.run.command
              }
            },
            .summary = "Run a binary from a package",
            .handler = spn_cli_tool_run
          },
          SPN_CLI_ARGS_DONE,
        },
        .handler = spn_cli_tool
      },
      {
        .name = "clean",
        .summary = "Remove build cache and store directories",
        .handler = spn_cli_clean
      },
      SPN_CLI_ARGS_DONE,
    },
    .handler = spn_cli_root
  };

  spn_cli_parser_t parser = {
    .args = spn.args + 1,
    .num_args = spn.num_args - 1,
    .cmd = &cli->cmd
  };

  switch (spn_cli_parse(&parser)) {
    case SPN_CLI_CONTINUE: break;
    case SPN_CLI_DONE: spn_cli_help(&parser); return SP_APP_QUIT;
    case SPN_CLI_ERR:
      if (sp_str_valid(parser.err)) sp_io_write_line(&spn.logger.err, parser.err);
      spn_cli_help(&parser);
      return SP_APP_ERR;
  }


  if (sp_str_valid(cli->project_dir)) {
    spn.paths.project = sp_fs_canonicalize_path(cli->project_dir);
  }
  else {
    spn.paths.project = sp_str_copy(spn.paths.cwd);
  }
  spn.paths.manifest = sp_fs_join_path(spn.paths.project, sp_str_lit("spn.toml"));

  app = spn_app_new();
  spn_app_load(&app, spn.paths.manifest);

  spn_cli_result_t result = spn_cli_dispatch(&parser, cli);
  switch (result) {
    case SPN_CLI_CONTINUE: {
      return SP_APP_CONTINUE;
    }
    case SPN_CLI_DONE: {
      return SP_APP_QUIT;
    }
    case SPN_CLI_ERR: {
      return SP_APP_ERR;
    }
    default: {
      SP_UNREACHABLE_CASE();
    }
  }
}

sp_app_result_t spn_poll(sp_app_t* sp) {
  spn_app_t* app = (spn_app_t*)sp->user_data;

  switch (app->state) {
    case SPN_APP_STATE_BUILDING: {
      // @hack, properly init this
      spn_builder_t* build = &app->build;
      spn_bg_executor_t* ex = build->executor;

      sp_da(spn_build_event_t) events = spn_event_buffer_drain(spn.events);

      sp_mutex_lock(&ex->mutex);
      spn.tui.info.num_done = sp_ht_size(ex->completed);
      sp_mutex_unlock(&ex->mutex);

      sp_da_for(events, it) {
        spn_build_event_t* event = &events[it];

        switch (event->kind) {
          case SPN_BUILD_EVENT_COMPILE: {
            sp_da(sp_str_t) args = event->compile.ps.dyn_args;
            sp_str_t msg = sp_str_join_n(args, sp_da_size(args), sp_str_lit(" "));
            sp_io_write_line(&event->ctx->logs.build, msg);
            break;
          }
          case SPN_BUILD_EVENT_RESOLVE: {
            sp_io_write_line(&event->ctx->logs.build, sp_str_lit("resolved"));
            // sp_ht_for_kv(app->resolver.resolved, it) {
              // sp_io_write_line(&event->ctx->logs.build, sp_format("{}: {}",
              //                 SP_FMT_STR(*it.key), SP_FMT_STR(spn_semver_to_str(it.val->version))));
            // }
            break;
          }
          default: {
            sp_tui_home();
            sp_tui_clear_line();
            sp_io_write_line(&spn.logger.err, spn_tui_render_build_event(event));
            break;
          }
        }

      }
      return SP_APP_CONTINUE;
    }
    case SPN_APP_STATE_INIT:
    case SPN_APP_STATE_PREPARE_RUN:
    case SPN_APP_STATE_RUNNING: {
      return SP_APP_CONTINUE;
    }
  }
}

sp_app_result_t spn_update(sp_app_t* sp) {
  spn_app_t* app = (spn_app_t*)sp->user_data;
  spn_builder_t* build = &app->build;

  switch (app->state) {
    // INIT
    case SPN_APP_STATE_INIT: {
      SP_ASSERT(app->config.tasks.build || app->config.tasks.tests);

      if (app->config.tasks.build) {
        spn_app_resolve(app);
        spn_app_prepare_build(app);

        spn_build_ctx_compile(&build->contexts.package);
        spn_build_ctx_run_script(&build->contexts.package);

        build->dirty = app->config.force ?
          spn_bg_compute_forced_dirty(&build->graph) :
          spn_bg_compute_dirty(&build->graph);

        build->executor = spn_bg_executor_new(&build->graph, build->dirty, (spn_bg_executor_config_t) {
          .num_threads = 3,
          .enable_logging = false
        });

        spn_bg_executor_run(app->build.executor);

        spn_tui_init(&spn.tui, SPN_OUTPUT_MODE_INTERACTIVE, &app->build);

        app->state = SPN_APP_STATE_BUILDING;
        return SP_APP_CONTINUE;
      }
      else if (app->config.tasks.tests){
        app->state = SPN_APP_STATE_PREPARE_RUN;
        return SP_APP_CONTINUE;
      }

      SP_UNREACHABLE_RETURN(SP_APP_ERR);
    }
    // BUILDING
    case SPN_APP_STATE_BUILDING: {
      spn_spinner_update(&spn.tui.spinner, sp_tm_ns_to_ms_f(sp->frame.target));

      // sp_tui_home();
      // sp_tui_clear_line();
      // sp_tui_print(sp_format("[{}/{}] {}",
      //   SP_FMT_U32(spn.tui.info.num_done),
      //   SP_FMT_U32(spn.tui.info.num_total),
      //   SP_FMT_STR(spn_spinner_render(&spn.tui.spinner))
      // ));
      // sp_tui_flush();

      spn_builder_t* build = &app->build;
      spn_bg_executor_t* ex = spn.tui.build->executor;
      if (sp_atomic_s32_get(&ex->shutdown)) {
        spn_bg_executor_join(ex);
        spn_poll(sp);

        sp_tui_home();
        sp_tui_clear_line();

        sp_opt(spn_bg_exec_error_t) error = SP_ZERO_INITIALIZE();
        if (sp_da_size(ex->errors)) {
          sp_opt_set(error, ex->errors[0]);
        }

        switch (error.some) {
          case SP_OPT_SOME: {
            spn_bg_cmd_t* cmd = spn_bg_find_command(&spn.tui.build->graph, error.value.cmd_id);
            spn_build_ctx_t* ctx = (spn_build_ctx_t*)cmd->fn.user_data;
            sp_io_write_str(&spn.logger.err, spn_tui_render_log(&ctx->logs.build, ctx->name));
            return SP_APP_ERR;
          }
          case SP_OPT_NONE: {
            if (app->config.tasks.tests) {
              app->state = SPN_APP_STATE_PREPARE_RUN;
              return SP_APP_CONTINUE;
            }

            if (!app->lock.some) {
              spn_app_update_lock_file(app);
            }

            spn_build_event_t event = spn_build_event_make(&build->contexts.package, SPN_BUILD_EVENT_BUILD_PASSED);
            event.done.time = build->executor->elapsed;
            sp_io_write_line(&spn.logger.err, spn_tui_render_build_event(&event));

            return SP_APP_QUIT;
          }
        }

        SP_UNREACHABLE();
      }

      return SP_APP_CONTINUE;
    }
    // PREPARE_RUN
    case SPN_APP_STATE_PREPARE_RUN: {
      // spn_log("");
      // spn_log("running {} tests", SP_FMT_U32(sp_da_size(build->bins)));

      bool ok = true;
      sp_ht(sp_str_t, s32) tests = SP_NULLPTR;

      sp_ht_for_kv(build->contexts.bins, it) {
        spn_build_ctx_t* ctx = &it.val->ctx;
        spn_target_t* bin = it.val->bin;

        if (!spn_target_filter_pass(&build->filter, bin)) {
          continue;
        }

        sp_fs_create_file(ctx->paths.logs.test);
        ctx->logs.test = sp_io_from_file(ctx->paths.logs.test, SP_IO_MODE_WRITE | SP_IO_MODE_READ);

        spn_build_event_t event = spn_build_event_make(ctx, SPN_BUILD_EVENT_TEST_RUN);
        sp_io_write_line(&spn.logger.err, spn_tui_render_build_event(&event));

        // run it
        sp_ps_t process = sp_ps_create((sp_ps_config_t) {
          .command = sp_fs_join_path(ctx->paths.bin, bin->name),
          .io = {
            .in = { .mode = SP_PS_IO_MODE_NULL },
            .out = { .mode = SP_PS_IO_MODE_EXISTING, .stream = it.val->ctx.logs.test },
            .err = { .mode = SP_PS_IO_MODE_REDIRECT }
          },
          .cwd = ctx->paths.work,
        });
        sp_ps_output_t result = sp_ps_output(&process);
        sp_ht_insert(tests, bin->name, result.status.exit_code);

        switch (result.status.exit_code) {
          case SP_ERR_OK: {
            spn_build_event_t event = spn_build_event_make(ctx, SPN_BUILD_EVENT_TEST_PASSED);
            sp_io_write_line(&spn.logger.err, spn_tui_render_build_event(&event));

            //spn_tui("{:fg green}", SP_FMT_CSTR("OK"));
            break;
          }
          default: {
            spn_build_event_t event = spn_build_event_make(ctx, SPN_BUILD_EVENT_TEST_FAILED);
            sp_io_write_line(&spn.logger.err, spn_tui_render_build_event(&event));

            // SP_LOG("{:fg red} (returned code {:fg brightblack})",
            //   SP_FMT_CSTR("FAIL"),
            //   SP_FMT_S32(result.status.exit_code)
            // );
            ok = false;
            break;
          }
        }
      }

      if (!app->lock.some) {
        spn_app_update_lock_file(app);
      }

      if (ok) {
        SP_LOG("{:fg green}", SP_FMT_CSTR("OK"));
      }
      else {
        sp_ht_for_kv(tests, it) {
          spn_bin_ctx_t* target = sp_ht_getp(build->contexts.bins, *it.key);
          spn_build_ctx_t* ctx = &target->ctx;

          if (*it.val) {
            sp_io_write_new_line(&spn.logger.err);
            sp_io_write_str(&spn.logger.err, spn_tui_render_log(&ctx->logs.test, ctx->name));
          }
        }
      }

      return SP_APP_QUIT;
    }
    // RUNNING
    case SPN_APP_STATE_RUNNING: {
      SP_UNREACHABLE();
      return SP_APP_QUIT;
    }
  }
}

sp_app_result_t spn_deinit(sp_app_t* app) {
  switch (spn.tui.mode) {
    case SPN_OUTPUT_MODE_INTERACTIVE: {
      sp_tui_restore(&spn.tui);
      sp_tui_show_cursor();
      sp_tui_home();
      sp_tui_flush();
      break;
    }
    case SPN_OUTPUT_MODE_NONINTERACTIVE: {
      break;
    }
    case SPN_OUTPUT_MODE_QUIET: {
      break;
    }
    case SPN_OUTPUT_MODE_NONE: {
      break;
    }
  }
  return SP_APP_QUIT;
}

sp_app_config_t sp_main(s32 num_args, const c8** args) {
  spn = (spn_ctx_t) {
    .num_args = num_args,
    .args = args
  };
  app = SP_ZERO_STRUCT(spn_app_t);

  return (sp_app_config_t) {
    .user_data = &app,
    .on_init = spn_init,
    .on_poll = spn_poll,
    .on_update = spn_update,
    .on_deinit = spn_deinit,
    .fps = 144,
  };
}

/////////
// CLI //
/////////
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

    sp_tui_begin_table(&spn.tui.table);
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Short"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Long"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Type"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Description"));
    sp_tui_table_header_row(&spn.tui.table);

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

      sp_tui_table_next_row(&spn.tui.table);
      sp_tui_table_str(&spn.tui.table, short_display);
      sp_tui_table_str(&spn.tui.table, long_display);
      sp_tui_table_str(&spn.tui.table, kind_str);
      sp_tui_table_str(&spn.tui.table, opt.summary);
    }

    sp_tui_table_set_indent(&spn.tui.table, 1);
    sp_tui_table_end(&spn.tui.table);

    sp_str_builder_append(&builder, sp_tui_table_render(&spn.tui.table));
  }

  if (!sp_dyn_array_empty(info.args)) {
    sp_str_builder_new_line(&builder);

    sp_str_builder_append_fmt(&builder, "{:fg brightgreen}", SP_FMT_CSTR("arguments"));
    sp_str_builder_new_line(&builder);

    sp_tui_begin_table(&spn.tui.table);
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Name"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Type"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Description"));
    sp_tui_table_header_row(&spn.tui.table);

    sp_dyn_array_for(info.args, it) {
      spn_cli_arg_info_t arg = info.args[it];

      sp_tui_table_next_row(&spn.tui.table);
      sp_tui_table_fmt(&spn.tui.table, "{:fg brightyellow}", SP_FMT_STR(arg.name));
      sp_tui_table_str(&spn.tui.table, sp_str_lit("str"));
      sp_tui_table_str(&spn.tui.table, arg.summary);
    }

    sp_tui_table_set_indent(&spn.tui.table, 1);
    sp_tui_table_end(&spn.tui.table);
    sp_str_t table = sp_tui_table_render(&spn.tui.table);

    sp_str_builder_append_fmt(&builder, "{}", SP_FMT_STR(table));
  }

  return sp_str_builder_move(&builder);
}

sp_str_t spn_cli_usage(spn_cli_command_usage_t* cmd) {
  spn_cli_command_info_t cmd_info = spn_cli_command_info_from_usage(*cmd);
  spn_cli_usage_info_t info = SP_ZERO_INITIALIZE();

  // Collect subcommands
  if (cmd->commands) {
    for (spn_cli_command_usage_t* sub = cmd->commands; sub->name; sub++) {
      spn_cli_command_info_t sub_info = spn_cli_command_info_from_usage(*sub);
      sp_dyn_array_push(info.commands, sub_info);
    }
  }

  sp_str_builder_t builder = SP_ZERO_INITIALIZE();

  if (cmd->summary) {
    sp_str_builder_append_fmt(&builder, "{}", SP_FMT_CSTR(cmd->summary));
    sp_str_builder_new_line(&builder);
    sp_str_builder_new_line(&builder);
  }

  if (cmd->usage) {
    sp_str_builder_append_fmt(&builder, "{:fg brightgreen}", SP_FMT_CSTR("usage"));
    sp_str_builder_indent(&builder);
    sp_str_builder_new_line(&builder);
    sp_str_builder_append_fmt(&builder, "{:fg brightcyan}", SP_FMT_CSTR(cmd->usage));
    sp_str_builder_dedent(&builder);
    sp_str_builder_new_line(&builder);
    sp_str_builder_new_line(&builder);
  }

  // Render opts (from the command itself)
  if (!sp_dyn_array_empty(cmd_info.opts)) {
    sp_str_builder_append_fmt(&builder, "{:fg brightgreen}", SP_FMT_CSTR("options"));
    sp_str_builder_new_line(&builder);

    sp_tui_begin_table(&spn.tui.table);
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Short"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Long"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Type"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Description"));
    sp_tui_table_header_row(&spn.tui.table);

    sp_dyn_array_for(cmd_info.opts, it) {
      spn_cli_opt_info_t opt = cmd_info.opts[it];

      sp_str_t short_display;
      if (!sp_str_empty(opt.brief)) {
        sp_str_t short_text = sp_format("-{}", SP_FMT_STR(opt.brief));
        short_display = sp_format("{:fg brightyellow}", SP_FMT_STR(short_text));
      } else {
        short_display = sp_str_lit("");
      }

      sp_str_t long_display;
      if (!sp_str_empty(opt.placeholder)) {
        sp_str_t long_text = sp_format("--{}", SP_FMT_STR(opt.name));
        long_display = sp_format("{:fg brightyellow}={:fg white}", SP_FMT_STR(long_text), SP_FMT_STR(opt.placeholder));
      } else {
        sp_str_t long_text = sp_format("--{}", SP_FMT_STR(opt.name));
        long_display = sp_format("{:fg brightyellow}", SP_FMT_STR(long_text));
      }

      sp_str_t kind_str = sp_format("{:fg brightblack}", SP_FMT_STR(spn_cli_opt_kind_to_str(opt.kind)));

      sp_tui_table_next_row(&spn.tui.table);
      sp_tui_table_str(&spn.tui.table, short_display);
      sp_tui_table_str(&spn.tui.table, long_display);
      sp_tui_table_str(&spn.tui.table, kind_str);
      sp_tui_table_str(&spn.tui.table, opt.summary);
    }

    sp_tui_table_set_indent(&spn.tui.table, 1);
    sp_tui_table_end(&spn.tui.table);
    sp_str_builder_append(&builder, sp_tui_table_render(&spn.tui.table));
    sp_str_builder_new_line(&builder);
  }

  // Render subcommands
  if (!sp_dyn_array_empty(info.commands)) {
    sp_str_builder_append_fmt(&builder, "{:fg brightgreen}", SP_FMT_CSTR("commands"));
    sp_str_builder_new_line(&builder);

    sp_tui_begin_table(&spn.tui.table);
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Command"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Arguments"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Description"));
    sp_tui_table_header_row(&spn.tui.table);

    sp_dyn_array_for(info.commands, it) {
      spn_cli_command_info_t command = info.commands[it];
      sp_str_t args = sp_str_join_n(command.brief, sp_dyn_array_size(command.brief), sp_str_lit(", "));

      sp_tui_table_next_row(&spn.tui.table);
      sp_tui_table_fmt(&spn.tui.table, "{:fg brightcyan}", SP_FMT_STR(command.name));
      sp_tui_table_fmt(&spn.tui.table, "{:fg brightyellow}", SP_FMT_STR(args));
      sp_tui_table_str(&spn.tui.table, command.summary);
    }

    sp_tui_table_set_indent(&spn.tui.table, 1);
    sp_tui_table_end(&spn.tui.table);
    sp_str_builder_append(&builder, sp_tui_table_render(&spn.tui.table));
  }

  return sp_str_builder_move(&builder);
}

spn_cli_result_t spn_cli_help(spn_cli_parser_t* p) {
  if (!p->resolved) {
    sp_log(spn_cli_command_usage(*p->cmd));
    return SPN_CLI_DONE;
  }
  else if (!p->resolved->commands) {
    sp_log(spn_cli_command_usage(*p->resolved));
    return SPN_CLI_DONE;
  }
  else {
    sp_log(spn_cli_command_usage(*p->resolved->commands));
    return SPN_CLI_DONE;
  }
  return SPN_CLI_DONE;
}

spn_err_t spn_cli_set_profile(spn_app_t* app, sp_str_t profile_name) {
  if (sp_str_empty(profile_name)) {
    SP_ASSERT(!sp_ht_empty(app->package.profiles));

    sp_ht_for_kv(app->package.profiles, it) {
      app->config.profile = *it.val;
      return SPN_OK;
    }
    SP_UNREACHABLE_RETURN(SPN_ERROR);
  }


  if (!sp_ht_key_exists(app->package.profiles, profile_name)) {
    spn_ctx_error("{:fg brightcyan} profile isn't defined in {:fg brightcyan}",
      SP_FMT_STR(profile_name),
      SP_FMT_STR(app->package.paths.manifest)
    );
    return SPN_ERROR;
  }

  app->config.profile = *sp_ht_getp(app->package.profiles, profile_name);
  return SPN_OK;
}



/////////
// CLI //
/////////
spn_pkg_ctx_t* spn_cli_assert_dep_exists(sp_str_t name) {
  spn_pkg_ctx_t* dep = sp_ht_getp(app.build.contexts.deps, name);
  SP_ASSERT_FMT(dep, "{:fg brightyellow} is not in this project", SP_FMT_STR(name));
  return dep;
}

spn_cli_result_t spn_cli_init(spn_cli_t* cli) {
  spn_cli_init_t* cmd = &cli->init;

  spn_app_t app = spn_app_init_and_write(
    spn.paths.cwd,
    sp_fs_get_stem(spn.paths.cwd),
    cmd->bare ? SPN_APP_INIT_BARE : SPN_APP_INIT_NORMAL
  );

  SP_LOG("Initialized project {:fg brightcyan}. Run {:fg brightyellow} to build.", SP_FMT_STR(app.package.name), SP_FMT_CSTR("spn build"));
  return SPN_CLI_DONE;
}

spn_cli_result_t spn_cli_root(spn_cli_t* cli) {
  sp_str_t help = spn_cli_usage(&cli->cmd);
  sp_log(help);
  return SPN_CLI_DONE;
}

spn_cli_result_t spn_cli_list(spn_cli_t* cli) {
  sp_tui_begin_table(&spn.tui.table);
  sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Package"));
  sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Version"));
  sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Repo"));
  sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Author"));
  sp_tui_table_header_row(&spn.tui.table);

  sp_ht_for_kv(app.registry, it) {
    spn_pkg_t* package = spn_app_ensure_package(&app, (spn_pkg_req_t) {
      .name = sp_fs_get_stem(*it.val),
      .kind = SPN_PACKAGE_KIND_INDEX
    });

    sp_tui_table_next_row(&spn.tui.table);
    sp_tui_table_fmt(&spn.tui.table, "{:fg brightcyan}", SP_FMT_STR(package->name));
    sp_tui_table_str(&spn.tui.table, spn_semver_to_str(package->version));
    sp_tui_table_str(&spn.tui.table, sp_str_truncate(package->repo, 50, sp_str_lit("...")));
    sp_tui_table_str(&spn.tui.table, sp_str_truncate(package->author, 30, sp_str_lit("...")));
  }

  sp_tui_table_end(&spn.tui.table);
  sp_log(sp_tui_table_render(&spn.tui.table));
  return SPN_CLI_DONE;
}

spn_cli_result_t spn_cli_clean(spn_cli_t* cli) {
  SP_LOG("Removing {:fg brightcyan}", SP_FMT_STR(spn.paths.build));
  sp_fs_remove_dir(spn.paths.build);
  SP_LOG("Removing {:fg brightcyan}", SP_FMT_STR(spn.paths.store));
  sp_fs_remove_dir(spn.paths.store);
  return SPN_CLI_DONE;
}


spn_cli_result_t spn_cli_copy(spn_cli_t* cli) {
  spn_cli_copy_t* cmd = &cli->copy;

  sp_str_t destination = sp_fs_normalize_path(cmd->directory);
  sp_str_t to = sp_fs_join_path(spn.paths.cwd, destination);
  sp_fs_create_dir(to);

  sp_ht_for_kv(app.build.contexts.deps, it) {
    sp_dyn_array(sp_os_dir_ent_t) entries = sp_fs_collect(it.val->ctx.paths.lib);
    sp_dyn_array_for(entries, i) {
      sp_os_dir_ent_t* entry = entries + i;
      sp_fs_copy_file(
        entry->file_path,
        sp_fs_join_path(to, sp_fs_get_name(entry->file_path))
      );
    }
  }
  return SPN_CLI_DONE;
}

spn_cli_result_t spn_cli_ls(spn_cli_t* cli) {
  spn_cli_ls_t* cmd = &cli->ls;

  spn_app_resolve(&app);
  spn_app_prepare_build(&app);

  if (sp_str_valid(cmd->package)) {
    spn_pkg_ctx_t* dep = spn_cli_assert_dep_exists(cmd->package);

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
  return SPN_CLI_DONE;
}

spn_cli_result_t spn_cli_which(spn_cli_t* cli) {
  spn_cli_which_t* cmd = &cli->which;

  spn_app_resolve(&app);
  spn_app_prepare_build(&app);

  if (sp_str_valid(cmd->package)) {
    spn_pkg_ctx_t* dep = spn_cli_assert_dep_exists(cmd->package);

    spn_dir_kind_t kind = SPN_DIR_STORE;
    if (sp_str_valid(cmd->dir)) {
      kind = spn_cache_dir_kind_from_str(cmd->dir);
    }

    spn_ctx_log("{}", SP_FMT_STR(spn_cache_dir_kind_to_dep_path(dep, kind)));
  }
  else {
    spn_dir_kind_t kind = SPN_DIR_CACHE;
    if (sp_str_valid(cmd->dir)) {
      kind = spn_cache_dir_kind_from_str(cmd->dir);
    }

    spn_ctx_log("{}", SP_FMT_STR(spn_cache_dir_kind_to_path(kind)));
  }
  return SPN_CLI_DONE;
}

spn_cli_result_t spn_cli_manifest(spn_cli_t* cli) {
  spn_cli_manifest_t* cmd = &cli->manifest;

  spn_app_resolve(&app);
  spn_app_prepare_build(&app);

  spn_pkg_ctx_t* dep = spn_cli_assert_dep_exists(cmd->package);

  sp_str_t path = dep->ctx.package->paths.manifest;
  sp_str_t manifest = sp_io_read_file(path);
  if (!sp_str_valid(manifest)) {
    SP_FATAL("Failed to read manifest at {:fg brightyellow}", SP_FMT_STR(path));
  }

  sp_log(manifest);
  return SPN_CLI_DONE;
}

spn_cli_result_t spn_cli_add(spn_cli_t* cli) {
  spn_cli_add_t* cmd = &cli->add;

  spn_visibility_t visibility = cmd->test ? SPN_VISIBILITY_TEST : SPN_VISIBILITY_PUBLIC;
  spn_pkg_add_dep_from_index(&app.package, cmd->package, visibility);
  spn_app_resolve_from_solver(&app);
  spn_app_prepare_build(&app);
  spn_app_update_lock_file(&app);
  spn_app_write_manifest(&app.package, app.package.paths.manifest);
  return SPN_CLI_DONE;
}

spn_cli_result_t spn_cli_update(spn_cli_t* cli) {
  spn_cli_update_t* cmd = &cli->update;

  spn_pkg_req_t* existing = sp_ht_getp(app.package.deps, cmd->package);
  if (!existing) {
    SP_FATAL("package {:fg brightcyan} is not a dependency", SP_FMT_STR(cmd->package));
  }

  spn_pkg_set_dep_from_index(&app.package, cmd->package, existing->visibility);
  spn_app_resolve_from_solver(&app);
  spn_app_prepare_build(&app);
  spn_app_update_lock_file(&app);
  spn_app_write_manifest(&app.package, app.package.paths.manifest);
  return SPN_CLI_DONE;
}

spn_cli_result_t spn_cli_tool_install(spn_cli_t* cli) {
  SPN_CLI_UNIMPLEMENTED();
}

spn_cli_result_t spn_cli_tool_uninstall(spn_cli_t* cli) {
  SPN_CLI_UNIMPLEMENTED();
}

spn_cli_result_t spn_cli_tool_run(spn_cli_t* cli) {
  SPN_CLI_UNIMPLEMENTED();
}

spn_cli_result_t spn_cli_tool(spn_cli_t* cli) {
  SPN_CLI_UNIMPLEMENTED();
}

spn_cli_result_t spn_cli_generate(spn_cli_t* cli) {
  spn_cli_generate_t* command = &cli->generate;

  if (sp_str_valid(command->path) && !sp_str_valid(command->generator)) {
    SP_FATAL(
      "output path was specified, but no generator. try e.g.:\n  spn generate --path {} {:fg yellow}",
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
  spn_app_prepare_build(&app);

  spn_generator_context_t gen = {
    .kind = spn_gen_kind_from_str(command->generator),
    .compiler = spn_cc_kind_from_str(command->compiler)
  };
  gen.include = spn_gen_build_entries_for_all(SPN_GEN_INCLUDE, gen.compiler);
  gen.lib_include = spn_gen_build_entries_for_all(SPN_GEN_LIB_INCLUDE, gen.compiler);
  gen.libs = spn_gen_build_entries_for_all(SPN_GEN_LIBS, gen.compiler);
  gen.rpath = spn_gen_build_entries_for_all(SPN_GEN_RPATH, gen.compiler);

  spn_gen_format_context_t fmt = {
    .kind = SPN_GEN_SYSTEM_LIBS,
    .compiler = gen.compiler
  };
  sp_dyn_array(sp_str_t) entries = sp_str_map(app.resolver.system_deps, sp_dyn_array_size(app.resolver.system_deps), &fmt, spn_generator_format_entry_kernel);
  gen.system_libs = sp_str_join_n(entries, sp_dyn_array_size(entries), sp_str_lit(" "));

  switch (gen.kind) {
    case SPN_GEN_KIND_RAW: {
      gen.file_name = SP_LIT("spn.txt");
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
        "export SPN_INCLUDES=\"{}\"\n"
        "export SPN_LIB_INCLUDES=\"{}\"\n"
        "export SPN_LIBS=\"{}\"\n"
        "export SPN_SYSTEM_LIBS=\"{}\"\n"
        "export SPN_RPATH=\"{}\"\n"
        "export SPN_FLAGS=\"$SPN_INCLUDES $SPN_LIB_INCLUDES $SPN_LIBS $SPN_SYSTEM_LIBS $SPN_RPATH\"\n";
      gen.output = sp_format(template,
        SP_FMT_STR(gen.include),
        SP_FMT_STR(gen.lib_include),
        SP_FMT_STR(gen.libs),
        SP_FMT_STR(gen.system_libs),
        SP_FMT_STR(gen.rpath)
      );
      break;
    }

    case SPN_GEN_KIND_MAKE: {
      gen.file_name = SP_LIT("spn.mk");
      const c8* template =
        "SPN_INCLUDES := {}\n"
        "SPN_LIB_INCLUDES := {}\n"
        "SPN_LIBS := {}\n"
        "SPN_SYSTEM_LIBS := {}\n"
        "SPN_RPATH := {}\n"
        "SPN_FLAGS := $(SPN_INCLUDES) $(SPN_LIB_INCLUDES) $(SPN_LIBS) $(SPN_SYSTEM_LIBS) $(SPN_RPATH)\n";
      gen.output = sp_format(template,
        SP_FMT_STR(gen.include),
        SP_FMT_STR(gen.lib_include),
        SP_FMT_STR(gen.libs),
        SP_FMT_STR(gen.system_libs),
        SP_FMT_STR(gen.rpath)
      );
      break;
    }

    case SPN_GEN_KIND_CMAKE: {
      gen.file_name = SP_LIT("spn.cmake");
      const c8* template =
        "set(SPN_INCLUDES \"{}\")\n"
        "set(SPN_LIB_INCLUDES \"{}\")\n"
        "set(SPN_LIBS \"{}\")\n"
        "set(SPN_SYSTEM_LIBS \"{}\")\n"
        "set(SPN_RPATH \"{}\")\n"
        "set(SPN_FLAGS \"$";
      sp_str_t template_end = sp_str_lit(
        "{SPN_INCLUDES} $"
        "{SPN_LIB_INCLUDES} $"
        "{SPN_LIBS} $"
        "{SPN_SYSTEM_LIBS} $"
        "{SPN_RPATH}\")\n");
      sp_str_t formatted = sp_format(template,
        SP_FMT_STR(gen.include),
        SP_FMT_STR(gen.lib_include),
        SP_FMT_STR(gen.libs),
        SP_FMT_STR(gen.system_libs),
        SP_FMT_STR(gen.rpath)
      );
      gen.output = sp_str_concat(formatted, template_end);
      break;
    }

    case SPN_GEN_KIND_PKGCONFIG: {
      gen.file_name = SP_LIT("spn.pc");
      const c8* template =
        "Name: {}\n"
        "Description: spn-managed dependencies for {}\n"
        "Version: {}.{}.{}\n"
        "Cflags: {} {}\n"
        "Libs: {} {} {}\n";
      gen.output = sp_format(template,
        SP_FMT_STR(app.package.name),
        SP_FMT_STR(app.package.name),
        SP_FMT_U32(app.package.version.major),
        SP_FMT_U32(app.package.version.minor),
        SP_FMT_U32(app.package.version.patch),
        SP_FMT_STR(gen.include),
        SP_FMT_STR(gen.lib_include),
        SP_FMT_STR(gen.libs),
        SP_FMT_STR(gen.system_libs),
        SP_FMT_STR(gen.rpath)
      );
      break;
    }

    default: {
      SP_UNREACHABLE();
    }
  }

  if (sp_str_valid(command->path)) {
    sp_str_t destination = sp_fs_normalize_path(command->path);
    destination = sp_fs_join_path(spn.paths.cwd, destination);
    sp_fs_create_dir(destination);

    sp_str_t file_path = sp_fs_join_path(destination, gen.file_name);
    sp_io_stream_t file = sp_io_from_file(file_path, SP_IO_MODE_WRITE);
    if (sp_io_write_str(&file, gen.output) != gen.output.len) {
      SP_FATAL("Failed to write {}", SP_FMT_STR(file_path));
    }
    sp_io_close(&file);

    spn_ctx_log("Generated {:fg brightcyan}", SP_FMT_STR(file_path));
  }
  else {
    sp_log(gen.output);
  }
  return SPN_CLI_DONE;
}

spn_cli_result_t spn_cli_test(spn_cli_t* cli) {
  spn_cli_test_t* command = &cli->test;

  app.config = (spn_app_config_t) {
    .tasks = {
      .build = true,
      .tests = true,
    },
    .filter = (spn_target_filter_t) {
      .name = command->target,
      .disabled = {
        .public = true
      }
    }
  };
  sp_try_as(spn_cli_set_profile(&app, command->profile), SPN_CLI_ERR);

  return SPN_CLI_CONTINUE;
}

spn_cli_result_t spn_cli_build(spn_cli_t* cli) {
  spn_cli_build_t* command = &cli->build;

  app.config = (spn_app_config_t) {
    .force = command->force,
    .tasks = {
      .build = true
    },
    .filter = (spn_target_filter_t) {
      .name = command->target,
      .disabled = {
        .test = sp_str_empty(command->target) && !command->tests
      }
    },
  };

  sp_try_as(spn_cli_set_profile(&app, command->profile), SPN_CLI_ERR);

  return SPN_CLI_CONTINUE;
}
