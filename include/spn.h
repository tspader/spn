#ifndef SPN_SPN_H
#define SPN_SPN_H

#ifndef SP_SP_H
  #include <stdint.h>
  // #include <stdlib.h>
  // #include <string.h>

  typedef int8_t   s8;
  typedef int16_t  s16;
  typedef int32_t  s32;
  typedef int64_t  s64;
  typedef uint8_t  u8;
  typedef uint16_t u16;
  typedef uint32_t u32;
  typedef uint64_t u64;
  typedef float    f32;
  typedef double   f64;
  typedef char     c8;
#endif

typedef enum {
  SPN_ABI_NONE,
  SPN_ABI_GNU,
  SPN_ABI_MUSL,
  SPN_ABI_MINGW,
  SPN_ABI_MSVC,
} spn_abi_t;

typedef enum {
  SPN_OS_NONE,
  SPN_OS_WINDOWS,
  SPN_OS_LINUX,
  SPN_OS_MACOS,
  SPN_OS_WASI,
} spn_os_t;

typedef enum {
  SPN_ARCH_NONE,
  SPN_ARCH_X64,
  SPN_ARCH_ARM64,
  SPN_ARCH_WASM32,
} spn_arch_t;

typedef struct {
  spn_arch_t arch;
  spn_os_t os;
  spn_abi_t abi;
} spn_triple_t;

typedef struct {
  u16 major;
  u16 minor;
} spn_os_version_t;

typedef enum {
  SPN_BUILD_MODE_NONE,
  SPN_BUILD_MODE_DEBUG,
  SPN_BUILD_MODE_RELEASE,
} spn_build_mode_t;

typedef enum {
  SPN_OPT_LEVEL_NONE,
  SPN_OPT_LEVEL_0,
  SPN_OPT_LEVEL_1,
  SPN_OPT_LEVEL_2,
  SPN_OPT_LEVEL_3,
  SPN_OPT_LEVEL_S,
  SPN_OPT_LEVEL_Z,
} spn_opt_level_t;

typedef enum {
  SPN_SANITIZER_NONE      = 0,
  SPN_SANITIZER_ADDRESS   = 1 << 0,
  SPN_SANITIZER_THREAD    = 1 << 1,
  SPN_SANITIZER_UNDEFINED = 1 << 2,
  SPN_SANITIZER_MEMORY    = 1 << 3,
  SPN_SANITIZER_LEAK      = 1 << 4,
} spn_sanitizer_t;

typedef u32 spn_sanitizer_set_t;

typedef enum {
  SPN_LIB_KIND_NONE,
  SPN_LIB_KIND_SHARED,
  SPN_LIB_KIND_STATIC,
  SPN_LIB_KIND_SOURCE,
  SPN_LIB_KIND_OBJECT,
} spn_linkage_t;

typedef enum {
  SPN_WIN_SUBSYSTEM_NONE,
  SPN_WIN_SUBSYSTEM_CONSOLE,
  SPN_WIN_SUBSYSTEM_WINDOWS,
} spn_win_subsystem_t;

typedef enum {
  SPN_LIBC_GNU = 0,
  SPN_LIBC_MUSL = 1,
  SPN_LIBC_COSMOPOLITAN = 2,
  SPN_LIBC_CUSTOM = 3,
} spn_libc_kind_t;

typedef enum {
  SPN_DIR_NONE = 0,
  SPN_DIR_CACHE = 1,
  SPN_DIR_STORE = 2,
  SPN_DIR_INCLUDE = 3,
  SPN_DIR_VENDOR = 4,
  SPN_DIR_LIB = 5,
  SPN_DIR_SOURCE = 6,
  SPN_DIR_WORK = 7,
  SPN_DIR_PROJECT = 8,
} spn_dir_t;

typedef enum {
  SPN_C_STANDARD_NONE,
  SPN_C11,
  SPN_C99,
  SPN_C89,
} spn_c_standard_t;

typedef enum {
  SPN_CXX_STANDARD_NONE,
  SPN_CXX11,
  SPN_CXX14,
  SPN_CXX17,
  SPN_CXX20,
  SPN_CXX23,
} spn_cxx_standard_t;

typedef enum {
  SPN_LANG_C,
  SPN_LANG_CXX,
} spn_lang_t;

typedef enum {
  SPN_CC_NONE,
  SPN_CC_GCC,
  SPN_CC_CLANG,
  SPN_CC_MUSL_GCC,
  SPN_CC_TCC,
  SPN_CC_COSMOCC,
  SPN_CC_ZIG,
  SPN_CC_CUSTOM,
} spn_cc_kind_t;

typedef enum {
  SPN_CMAKE_GEN_DEFAULT,
  SPN_CMAKE_GEN_UNIX_MAKEFILES,
  SPN_CMAKE_GEN_NINJA,
  SPN_CMAKE_GEN_XCODE,
  SPN_CMAKE_GEN_MSVC,
  SPN_CMAKE_GEN_MINGW,
} spn_cmake_gen_t;

typedef enum {
  SPN_OK = 0,
  SPN_ERROR = 1,
  SPN_ERR_MANIFEST_PARSE,
  SPN_ERR_MANIFEST_FIELD,
  SPN_ERR_MANIFEST_ISSUES,
  SPN_ERR_NO_MANIFEST,
  SPN_ERR_NOT_GIT_REPO,
  SPN_ERR_GIT,
  SPN_ERR_VERSION_EXISTS,
  SPN_ERR_BUILD_GRAPH,
  SPN_ERR_TOOLCHAIN_UNKNOWN,
  SPN_ERR_TOOLCHAIN_TARGET,
  SPN_ERR_TOOLCHAIN_HOST,
  SPN_ERR_TOOLCHAIN_FETCH,
  SPN_ERR_TOOLCHAIN_NO_SHA,
  SPN_ERR_TOOLCHAIN_SHA,
  SPN_ERR_TOOLCHAIN_EXTRACT,
  SPN_ERR_TOOLCHAIN_NO_CXX,
  SPN_ERR_TOML_MISSING,
  SPN_ERR_TOML_TYPE,
  SPN_ERR_TOML_PARSE,
  SPN_CODEGEN_ERR_EXPECTED_BOOL,
  SPN_CODEGEN_ERR_EXPECTED_STR,
  SPN_CODEGEN_ERR_EXPECTED_OBJECT,
  SPN_CODEGEN_ERR_MISSING_KEY,
  SPN_CODEGEN_ERR_DUPLICATE_KEY,
  SPN_CODEGEN_ERR_PARSE,
  SPN_CODEGEN_ERR_FILE_MISSING,
  SPN_CODEGEN_ERR_INVALID,
  SPN_ERR_WASM_INIT_FAILED,
  SPN_ERR_WASM_REGISTER_FAILED,
  SPN_ERR_WASM_MODULE_LOAD_FAILED,
  SPN_ERR_WASM_MODULE_INSTANCE_FAILED,
  SPN_ERR_WASM_CTX_FAILED,
  SPN_ERR_WASM_MODULE_CALL_FAILED,
  SPN_ERR_WASM_READ_FAILED,
  SPN_ERR_WASM_THREAD_ENV_FAILED,
  SPN_ERR_WASM_SCRIPT_ERROR,
  SPN_ERR_WASM_NO_SCRIPT,
  SPN_ERR_WASM_EXPORT_NOT_FOUND,
  SPN_ERR_PROFILE_INVALID,
  SPN_ERR_PROFILE_UNDEFINED,
  SPN_ERR_FLAG_INVALID,
  SPN_ERR_SANITIZER_UNSUPPORTED,
  SPN_ERR_SANITIZER_STATIC,
  SPN_ERR_COMPILER_FEATURE_UNSUPPORTED,
  SPN_ERR_FS_REMOVE,
  SPN_ERR_FS_READ,
  SPN_ERR_FS_WRITE,
  SPN_ERR_INDEX_UNKNOWN,
  SPN_ERR_INDEX_SYNC,
  SPN_ERR_INDEX_PINNED,
  SPN_ERR_INDEX_PUBLISH_PROTOCOL,
  SPN_ERR_PUBLISH_PUSH,
  SPN_ERR_PUBLISH_DIRTY,
  SPN_ERR_PUBLISH_UNPUSHED,
  SPN_ERR_PKG_UNKNOWN,
  SPN_ERR_PKG_NO_MATCH,
  SPN_ERR_MANIFEST_EDIT,
  SPN_ERR_DAG_GLOB,
  SPN_ERR_DAG_OUTPUT_NAME,
  SPN_ERR_DAG_DUPLICATE_OUTPUT,
  SPN_ERR_DAG_STAT,
  SPN_ERR_DAG_HASH,
  SPN_ERR_DAG_MISSING_INPUT,
  SPN_ERR_DAG_MISSING_OUTPUT,
  SPN_ERR_DAG_SCRATCH,
  SPN_ERR_DAG_ACTION,
  SPN_ERR_DAG_STORE_READ,
  SPN_ERR_DAG_STORE_WRITE,
  SPN_ERR_DAG_STORE_MISSING,
  SPN_ERR_DAG_TREE,
  SPN_ERR_DAG_STALLED,
} spn_err_t;

typedef struct spn              spn_t;
typedef struct spn_config       spn_config_t;
typedef struct spn_pkg          spn_pkg_t;
typedef struct spn_target       spn_target_t;
typedef struct spn_profile      spn_profile_t;
typedef struct spn_index        spn_index_t;
typedef struct spn_cmake        spn_cmake_t;
typedef struct spn_make         spn_make_t;
typedef struct spn_autoconf     spn_autoconf_t;
typedef struct spn_node_t       spn_node_t;
typedef struct spn_node_ctx_t   spn_node_ctx_t;

#ifdef __wasm32__
#define SPN_API __attribute__((import_module("env")))
#define SPN_EXPORT __attribute__((used, visibility("default")))

#else
  #define SPN_API
  #define SPN_EXPORT
#endif

typedef spn_err_t (*spn_configure_fn_t) (spn_t*, spn_config_t*);
typedef s32       (*spn_node_fn_t)      (spn_t*, spn_node_ctx_t*);


#define SP_EMBED_DEFAULT_SYMBOL SP_NULLPTR
#define SP_EMBED_DEFAULT_DATA_T SP_NULLPTR
#define SP_EMBED_DEFAULT_SIZE_T SP_NULLPTR

spn_target_t* spn_get_target(spn_t* spn, const c8* name);
const spn_t*  spn_get_dep(const spn_t* spn, const c8* name);
const c8*     spn_get_dir(const spn_t* spn, spn_dir_t dir);
const c8*     spn_get_subdir(const spn_t* spn, spn_dir_t base, const c8* path);
void          spn_write_file(spn_t* spn, const c8* path, const c8* content);
s32           spn_copy(spn_t* spn, spn_dir_t from, const c8* from_path, spn_dir_t to, const c8* to_path);
void          spn_log(spn_t* spn, const c8* message);

void          spn_fs_copy(const c8* from, const c8* to);
void          spn_fs_copy_glob(const c8* glob, const c8* dir);
void          spn_fs_create_dir(const c8* path);
void          spn_fs_cat_ex(const c8* path, const c8* a0, const c8* a1, const c8* a2, const c8* a3);
void          spn_io_write(const c8* path, const c8* contents);
const c8*     spn_fmt_ex(const c8* fmt, const c8* a0, const c8* a1, const c8* a2, const c8* a3);

#define spn_fmt_select(fmt, a0, a1, a2, a3, ...) spn_fmt_ex(fmt, a0, a1, a2, a3)
#define spn_fmt(fmt, ...) spn_fmt_select(fmt, ##__VA_ARGS__, 0, 0, 0, 0, 0)
#define spn_fs_cat_select(path, a0, a1, a2, a3, ...) spn_fs_cat_ex(path, a0, a1, a2, a3)
#define spn_fs_cat(path, ...) spn_fs_cat_select(path, ##__VA_ARGS__, 0, 0, 0, 0, 0)

spn_target_t* spn_add_exe(spn_config_t* config, const c8* name);
spn_target_t* spn_add_lib(spn_config_t* config, const c8* name, spn_linkage_t kind);
spn_target_t* spn_add_test(spn_config_t* config, const c8* name);
void          spn_add_include(spn_config_t* config, const c8* path);
void          spn_add_define(spn_config_t* config, const c8* define);
void          spn_add_system_dep(spn_config_t* config, const c8* dep);
spn_node_t*   spn_add_node(spn_config_t* config, const c8* tag);
void          spn_node_add_input(spn_node_t* node, const c8* input);
void          spn_node_add_output(spn_node_t* node, const c8* output);
void          spn_node_link(spn_node_t* from, spn_node_t* to);
void          spn_node_set_fn(spn_node_t* node, const c8* fn);
void          spn_node_set_user_data(spn_node_t* node, void* user_data);
void*         spn_node_ctx_get_user_data(spn_node_ctx_t* ctx);

void          spn_target_add_source(spn_target_t* target, const c8* source);
void          spn_target_add_include(spn_target_t* target, const c8* include);
void          spn_target_add_define(spn_target_t* target, const c8* define);
void          spn_target_add_flag(spn_target_t* target, const c8* flag);
void          spn_target_set_linked(spn_target_t* target, s32 linked);
void          spn_target_embed_file(spn_target_t* target, const c8* file);
void          spn_target_embed_file_ex(spn_target_t* target, const c8* file, const c8* symbol, const c8* data_type, const c8* size_type);
void          spn_target_embed_mem(spn_target_t* target, const c8* symbol, const u8* buffer, u64 buffer_size);
void          spn_target_embed_mem_ex(spn_target_t* target, const c8* symbol, const u8* buffer, u64 buffer_size, const c8* data_type, const c8* size_type);
void          spn_target_embed_dir(spn_target_t* target, const c8* dir);
void          spn_target_embed_dir_ex(spn_target_t* target, const c8* dir, const c8* dest, const c8* data_type, const c8* size_type);

spn_profile_t*   spn_get_profile(spn_t* spn);
spn_libc_kind_t  spn_profile_get_libc(spn_profile_t* profile);
spn_linkage_t    spn_profile_get_linkage(spn_profile_t* profile);
spn_c_standard_t spn_profile_get_standard(spn_profile_t* profile);
spn_build_mode_t spn_profile_get_mode(spn_profile_t* profile);
spn_opt_level_t  spn_profile_get_opt(spn_profile_t* profile);
spn_sanitizer_set_t spn_profile_get_sanitizers(spn_profile_t* profile);

s32             spn_make(spn_t* spn);
spn_make_t*     spn_make_new(spn_t* spn);
void            spn_make_add_target(spn_make_t* make, const c8* target);
s32             spn_make_run(spn_make_t* make);
s32             spn_autoconf(spn_t* spn);
spn_autoconf_t* spn_autoconf_new(spn_t* spn);
void            spn_autoconf_add_flag(spn_autoconf_t* autoconf, const c8* flag);
s32             spn_autoconf_run(spn_autoconf_t* autoconf);
s32             spn_cmake(spn_t* spn);
spn_cmake_t*    spn_cmake_new(spn_t* spn);
void            spn_cmake_set_generator(spn_cmake_t* cmake, spn_cmake_gen_t gen);
void            spn_cmake_add_define(spn_cmake_t* cmake, const c8* name, const c8* value);
void            spn_cmake_add_arg(spn_cmake_t* cmake, const c8* arg);
s32             spn_cmake_configure(spn_cmake_t* cmake);
s32             spn_cmake_build(spn_cmake_t* cmake);
s32             spn_cmake_install(spn_cmake_t* cmake);
s32             spn_cmake_run(spn_cmake_t* cmake);

#endif
