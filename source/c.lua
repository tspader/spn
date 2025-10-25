local iterator = require('iterator')

local module = {
  sp = {
    os = {},
    str = {},
    cstr = {},
    hash = {},
  },
  spn = {
    dep = {},
    sh = {},
  },
  sdl = {},
  -- app = nil
}

function module.load(app)
  local ffi = require('ffi')

  ffi.cdef([[
    typedef int8_t           s8;
    typedef int16_t          s16;
    typedef int32_t          s32;
    typedef int64_t          s64;
    typedef uint8_t          u8;
    typedef uint16_t         u16;
    typedef uint32_t         u32;
    typedef uint64_t         u64;
    typedef float            f32;
    typedef double           f64;
    typedef char             c8;
    typedef wchar_t          c16;
    typedef size_t           sp_size_t;
    typedef u64              sp_hash_t;
    typedef void*            sp_opaque_ptr;
    typedef void*            sp_thread_t;
    typedef void*            sp_mutex_t;
    typedef struct lua_State lua_State;
    typedef lua_State*       sp_lua_context_t;

    typedef struct {
      u32 len;
      const c8* data;
      u32 capacity;
    } sp_str_t;

    typedef enum {
      SP_TERNARY_FALSE,
      SP_TERNARY_TRUE,
      SP_TERNARY_NULL,
    } sp_ternary_t;

    typedef struct {
      s32 count;
    } sp_lua_pop_t;

    typedef struct {
      sp_lua_context_t state;
      sp_lua_pop_t pop;
    } sp_lua_t;

    typedef enum {
      SP_OS_FILE_ATTR_NONE = 0,
      SP_OS_FILE_ATTR_REGULAR_FILE = 1,
      SP_OS_FILE_ATTR_DIRECTORY = 2,
    } sp_os_file_attr_t;

    typedef struct {
      sp_str_t file_path;
      sp_str_t file_name;
      sp_os_file_attr_t attributes;
    } sp_os_directory_entry_t;

    typedef struct {
      sp_os_directory_entry_t* data;
      u32 count;
    } sp_os_directory_entry_list_t;
    typedef sp_os_directory_entry_list_t sp_os_dirs_t;

    typedef enum {
      SP_ALLOCATOR_MODE_ALLOC,
      SP_ALLOCATOR_MODE_FREE,
      SP_ALLOCATOR_MODE_RESIZE,
    } sp_allocator_mode_t;

    typedef void* (*sp_alloc_fn_t)(void*, sp_allocator_mode_t, u32, void*);

    typedef struct sp_allocator_t {
      sp_alloc_fn_t on_alloc;
      void* user_data;
    } sp_allocator_t;

    typedef enum {
      SP_IO_SEEK_SET,
      SP_IO_SEEK_CUR,
      SP_IO_SEEK_END,
    } sp_io_whence_t;

    typedef enum {
      SP_IO_MODE_READ   = 1 << 0,
      SP_IO_MODE_WRITE  = 1 << 1,
      SP_IO_MODE_APPEND = 1 << 2,
    } sp_io_mode_t;

    typedef enum {
      SP_IO_FILE_CLOSE_MODE_NONE,
      SP_IO_FILE_CLOSE_MODE_AUTO,
    } sp_io_file_close_mode_t;

    typedef struct sp_io_stream_t sp_io_stream_t;

    typedef s64  (*sp_io_size_fn )(sp_io_stream_t*);
    typedef s64  (*sp_io_seek_fn )(sp_io_stream_t*, s64, sp_io_whence_t);
    typedef u64  (*sp_io_read_fn )(sp_io_stream_t*, void* , u64);
    typedef u64  (*sp_io_write_fn)(sp_io_stream_t*, const void*, u64);
    typedef void (*sp_io_close_fn)(sp_io_stream_t*);

    typedef struct {
      sp_io_size_fn  size;
      sp_io_seek_fn  seek;
      sp_io_read_fn  read;
      sp_io_write_fn write;
      sp_io_close_fn close;
    } sp_io_callbacks_t;

    typedef struct {
      s32 fd;
      sp_io_file_close_mode_t close_mode;
    } sp_io_file_data_t;

    typedef struct {
      u8* base;
      u8* here;
      u8* stop;
    } sp_io_memory_data_t;

    struct sp_io_stream_t {
      sp_io_callbacks_t callbacks;
      union {
        sp_io_file_data_t file;
        sp_io_memory_data_t memory;
      };
      sp_allocator_t allocator;
    };


    typedef enum {
      SP_PS_IO_FILENO_NONE,
      SP_PS_IO_FILENO_STDIN = 0,
      SP_PS_IO_FILENO_STDOUT = 1,
      SP_PS_IO_FILENO_STDERR = 2,
    } sp_ps_io_file_number_t;

    typedef enum {
      SP_ENV_EXPORT_OVERWRITE_DUPES,
      SP_ENV_EXPORT_SKIP_DUPES,
    } sp_env_export_overwrite_t;

    typedef enum {
      SP_PS_IO_MODE_INHERIT,
      SP_PS_IO_MODE_NULL,
      SP_PS_IO_MODE_CREATE,
      SP_PS_IO_MODE_EXISTING,
      SP_PS_IO_MODE_REDIRECT,
    } sp_ps_io_mode_t;

    typedef enum {
      SP_PS_IO_NONBLOCKING,
      SP_PS_IO_BLOCKING,
    } sp_ps_io_blocking_t;

    typedef enum {
      SP_PS_ENV_INHERIT,
      SP_PS_ENV_CLEAN,
      SP_PS_ENV_EXISTING,
    } sp_ps_env_mode_t;

    typedef enum {
      SP_PS_STATE_RUNNING,
      SP_PS_STATE_DONE
    } sp_ps_state_t;

    typedef struct {
      sp_str_t key;
      sp_str_t value;
    } sp_env_var_t;

    typedef void* sp_env_table_t;

    typedef struct {
      sp_env_table_t vars;
    } sp_env_t;

    typedef struct {
      sp_ps_io_mode_t mode;
      sp_io_stream_t stream;
      sp_ps_io_blocking_t block;
    } sp_ps_io_stream_config_t;

    typedef struct {
      sp_ps_io_stream_config_t in;
      sp_ps_io_stream_config_t out;
      sp_ps_io_stream_config_t err;
    } sp_ps_io_config_t;

    typedef sp_ps_io_config_t sp_ps_io_t;

    typedef struct {
      sp_ps_env_mode_t mode;
      sp_env_t env;
      sp_env_var_t extra [16];
    } sp_ps_env_config_t;

    typedef struct {
      sp_str_t command;
      sp_str_t args [8];
      sp_str_t cwd;
      sp_ps_env_config_t env;
      sp_ps_io_config_t io;
    } sp_ps_config_t;

    typedef struct {
      c8** argv;
      c8** envp;
      sp_ps_env_mode_t env_mode;
    } sp_ps_platform_t;

    typedef struct {
      s32 pid;
      sp_ps_io_t io;
      sp_ps_platform_t platform;
      sp_allocator_t allocator;
    } sp_ps_t;
  ]])

  ffi.cdef([[
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
      SPN_DIR_STORE,
      SPN_DIR_INCLUDE,
      SPN_DIR_VENDOR,
    } spn_cli_dir_kind_t;

    typedef struct {
      const c8* package;
      const c8* kind;
    } spn_cli_dir_t;

    typedef enum {
      SPN_PRINT_NONE,
      SPN_PRINT_INCLUDE,
      SPN_PRINT_LIB_INCLUDE,
      SPN_PRINT_LIBS,
      SPN_PRINT_ALL,
    } spn_cli_print_kind_t;

    typedef enum {
      SPN_PRINT_COMPILER_NONE,
      SPN_PRINT_COMPILER_GCC,
    } spn_cli_compiler_t;

    typedef struct {
      const c8* kind;
      const c8* package;
      const c8* compiler;
    } spn_cli_print_t;

    typedef struct {
      u32 num_args;
      const c8** args;
      const c8* project_directory;
      bool no_interactive;

      spn_cli_add_t add;
      spn_cli_init_t init;
      spn_cli_list_t list;
      spn_cli_print_t flags;
      spn_cli_dir_t dir;
    } spn_cli_t;

    //////////////////
    // DEPENDENCIES //
    //////////////////
    typedef enum {
      SPN_DEP_BUILD_MODE_DEBUG,
      SPN_DEP_BUILD_MODE_RELEASE,
    } spn_dep_build_mode_t;

    typedef enum {
      SPN_DEP_BUILD_KIND_SHARED,
      SPN_DEP_BUILD_KIND_STATIC,
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
      SPN_DEP_BUILD_STATE_AWAITING_CONFIRMATION,
      SPN_DEP_BUILD_STATE_CHECKING_OUT,
      SPN_DEP_BUILD_STATE_PREPARING,
      SPN_DEP_BUILD_STATE_BUILDING,
      SPN_DEP_BUILD_STATE_DONE,
      SPN_DEP_BUILD_STATE_CANCELED,
      SPN_DEP_BUILD_STATE_FAILED
    } spn_dep_build_state_t;

    typedef struct {
      sp_str_t name;
      spn_dep_build_mode_t mode;
      sp_hash_t hash;
    } spn_build_matrix_t;

    typedef struct {
      spn_dep_build_state_t build;
      spn_dep_repo_state_t repo;
    } spn_dep_state_t;

    typedef struct {
      sp_str_t source;
      sp_str_t recipe;
    } spn_dep_paths_t;

    typedef struct {
      sp_str_t log;
      sp_str_t stamp;
      sp_str_t source;
      sp_str_t work;
      sp_str_t store;
      sp_str_t include;
      sp_str_t lib;
      sp_str_t vendor;
    } spn_dep_build_paths_t;

    typedef struct {
      sp_str_t name;
      sp_str_t git;
      sp_str_t branch;
      sp_str_t* libs;
      spn_dep_paths_t paths;
    } spn_dep_info_t;

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
      spn_build_matrix_t* matrix;
      sp_str_t build_id;
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

      sp_ps_config_t cfg;
      sp_ps_t ps;
      sp_io_stream_t log;

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

    typedef spn_lock_entry_t* spn_lock_file_t;

    typedef struct {
      spn_dep_build_context_t* deps;
      spn_build_matrix_t* matrix;
    } spn_build_context_t;

    /////////
    // APP //
    /////////
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
      sp_str_t name;
      sp_str_t* system_deps;
      spn_dep_spec_t* deps;
      spn_build_matrix_t* matrices;
    } spn_project_t;

    typedef struct {
      sp_str_t build;
      sp_str_t clone;
      sp_str_t libs;
      sp_str_t url;
    } spn_targets_t;

    typedef struct {
      struct {
        sp_str_t spn;
      } paths;

      sp_str_t output;
    } spn_lua_config_t;

typedef struct {
  spn_cli_t* cli;
  spn_paths_t* paths;
  spn_project_t* project;
  spn_build_context_t* build;
  spn_dep_info_t** deps;
  spn_lock_entry_t** lock;
  spn_lua_config_t* config;

  sp_hash_t            (*sp_hash_str)(sp_str_t str);
  sp_hash_t            (*sp_hash_combine)(sp_hash_t* hashes, u32 num_hashes);
  void*                (*sp_alloc)(u32 n);
  c8*                  (*sp_cstr_copy)(const c8* str);
  sp_str_t             (*sp_str_copy)(sp_str_t str);
  bool                 (*sp_str_equal_cstr)(sp_str_t str, const c8* cstr);
  sp_str_t             (*sp_str_from_cstr)(const c8* cstr);
  c8*                  (*sp_str_to_cstr)(sp_str_t str);
  void                 (*sp_os_copy)(sp_str_t from, sp_str_t to);
  void                 (*sp_os_copy_file)(sp_str_t from, sp_str_t to);
  void                 (*sp_os_copy_directory)(sp_str_t from, sp_str_t to);
  sp_str_t             (*sp_os_extract_extension)(sp_str_t path);
  sp_str_t             (*sp_os_extract_stem)(sp_str_t path);
  sp_str_t             (*sp_os_extract_file_name)(sp_str_t path);
  sp_str_t             (*sp_os_join_path)(sp_str_t a, sp_str_t b);
  bool                 (*sp_os_does_path_exist)(sp_str_t a);
  void                 (*sp_os_log)(sp_str_t message);
  sp_os_dirs_t         (*sp_os_scan_directory)(sp_str_t path);
  bool                 (*sp_os_is_directory)(sp_str_t path);
  bool                 (*sp_os_is_regular_file)(sp_str_t path);
  sp_str_t             (*sp_os_platform_name)(void);
  void                 (*sp_dyn_array_push_f)(void** arr, void* val, u32 val_len);
  spn_dep_info_t*      (*spn_dep_find)(sp_str_t name);
  sp_str_t             (*spn_git_fetch)(sp_str_t repo);
  u32                  (*spn_git_num_updates)(sp_str_t repo, sp_str_t from, sp_str_t to);
  void                 (*spn_git_checkout)(sp_str_t repo, sp_str_t commit);
  sp_str_t             (*spn_git_get_remote_url)(sp_str_t repo_path);
  sp_str_t             (*spn_git_get_commit)(sp_str_t repo_path, sp_str_t id);
  sp_str_t             (*spn_git_get_commit_message)(sp_str_t repo_path, sp_str_t id);
  sp_str_t             (*sp_str_truncate)(sp_str_t str, u32 n, sp_str_t trailer);
  spn_dep_build_kind_t (*spn_dep_build_kind_from_str)(sp_str_t str);
  sp_str_t             (*spn_dep_build_mode_to_str)(spn_dep_build_mode_t);
  spn_dep_build_mode_t (*spn_dep_build_mode_from_str)(sp_str_t);
} spn_lua_context_t;
  ]])

  module.app = ffi.cast('spn_lua_context_t*', app)

  -- sp.h
  for name, namespace in pairs(module.sp) do
    setmetatable(namespace, {
      __index = function(_, key)
        key = string.format("sp_%s_%s", name, key)
        return module.app[key]
      end
    })
  end

  module.sp.dyn_array = {
    push = function(array, value)
      local arr_ptr = ffi.new('void* [1]')
      arr_ptr[0] = array

      module.app.sp_dyn_array_push_f(
        ffi.cast('void**', arr_ptr),
        ffi.cast('void*',  value),
        ffi.sizeof(value)
      )
      return arr_ptr[0]
    end
  }

  module.sp.ternary = function(value)
    if value == nil then
      return ffi.C.SP_TERNARY_NULL
    elseif value then
      return ffi.C.SP_TERNARY_TRUE
    else
      return ffi.C.SP_TERNARY_FALSE
    end
  end


  -- spn
  for name, namespace in pairs(module.spn) do
    setmetatable(namespace, {
      __index = function(_, key)
        key = string.format("spn_%s_%s", name, key)
        return module.app[key]
      end
    })
  end

  module.sp.hash.combine = function(values)
    table.sort(values)

    local hashes = ffi.new('sp_hash_t* [1]')
    local hash = ffi.new('sp_hash_t [1]')
    for value in iterator.values(values) do
      local hashable = tostring(value)
      hashable = module.sp.str.from_cstr(hashable)
      hash[0] = module.sp.hash.str(hashable)
      hashes[0] = module.sp.dyn_array.push(hashes[0], hash)
    end

    return module.app.sp_hash_combine(hashes[0], #values)
  end

  module.spn.build_mode = {
    debug = ffi.C.SPN_DEP_BUILD_MODE_DEBUG,
    release = ffi.C.SPN_DEP_BUILD_MODE_RELEASE,
  }

  module.spn.string = ffi.metatype('sp_str_t', {
    __tostring = function(self) return ffi.string(self.data, self.len) end,
    __len = function(self) return self.len end,
    __eq = function(a, b) return tostring(a) == tostring(b) end,
    __index = function(self, key)
      if key == 'cstr' then
        return function() return ffi.string(self.data, self.len) end
      else
        return function(_, ...)
          if self.len == 0 then
            return ''
          end

          local str = ffi.string(self.data, self.len)
          return str[key](str, ...) end
      end
    end,
  })
end

return module
