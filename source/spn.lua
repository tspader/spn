local ffi = require('ffi')

local spn = {
  recipes = {},
  internal = {
    recipes = {}
  },
}

function spn.log(fmt, ...)
  print(string.format(fmt, ...))
end

function spn.recipes.default()
  return {
    files = {
      include = {},
      bin = {},
      vendor = {}
    }
  }
end

function spn.recipes.single_header(config)
  local recipe = spn.recipes.default()
  recipe.git = config.git
  table.insert(recipe.files.include, config.header)
  return recipe
end

function spn.internal.read(recipe)
end

function spn.internal.init(app)
  app = ffi.cast('spn_lua_context_t*', app)
  spn.internal.app = spp

  local config = loadfile(tostring(app.paths.user_config))
  if not config then
    return
  end

  ok, config = pcall(config)
  if config.spn then
    app.config.paths.spn = sp.str.from_cstr(config.spn)
  end

  local entries = sp.os.scan_directory(app.paths.recipes)
  for index = 0, entries.count do
    local entry = entries.data[index]
    local extension = sp.os.extract_extension(entry.file_name)
    if sp.str.equal_cstr(extension, "lua") then
      local name = sp.os.extract_stem(entry.file_name)
      local recipe = dofile(entry.file_path:cstr())
      spn.internal.recipes[name:cstr()] = recipe

      local dep = ffi.new('spn_dep_info_t')
      dep.name = name
      dep.url = sp.str.from_cstr(recipe.git)
      dep.paths.source = sp.os.join_path(app.paths.source, name)
    end
  end
end

function spn.internal.ffi()
  ffi.cdef([[
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
    typedef wchar_t  c16;
    typedef size_t   sp_size_t;

    typedef struct SDL_Process SDL_Process;
    typedef u32 SDL_PropertiesID;
    typedef void* sp_opaque_ptr;
    typedef struct toml_table_t toml_table_t;
    typedef struct spn_dep_context_t spn_dep_context_t;
    typedef struct lua_State lua_State;

    typedef struct {
      u32 len;
      const c8* data;
    } sp_str_t;

    typedef struct {
      sp_str_t source;
      sp_str_t recipe;
    } spn_dep_paths_t;

    typedef struct {
      sp_str_t name;
      sp_str_t url;
      spn_dep_paths_t paths;
      sp_str_t* libs;
    } spn_dep_info_t;


    typedef struct {
      sp_str_t output;
      s32 return_code;
    } spn_sh_process_result_t;

    typedef struct {
      SDL_Process* process;
      spn_sh_process_result_t result;
    } spn_sh_process_context_t;

    /////////
    // GIT //
    /////////
    sp_str_t spn_git_fetch(sp_str_t repo);
    u32      spn_git_num_updates(sp_str_t repo, sp_str_t from, sp_str_t to);
    void     spn_git_checkout(sp_str_t repo, sp_str_t commit);
    sp_str_t spn_git_get_remote_url(sp_str_t repo_path);
    sp_str_t spn_git_get_commit(sp_str_t repo_path, sp_str_t id);
    sp_str_t spn_git_get_commit_message(sp_str_t repo_path, sp_str_t id);
    sp_str_t sp_str_truncate(sp_str_t str, u32 n, sp_str_t trailer);


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

    spn_cli_dir_kind_t spn_dir_kind_from_cstr(sp_str_t str);

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

    spn_cli_print_kind_t spn_print_kind_from_cstr(sp_str_t str);
    spn_cli_compiler_t spn_print_compiler_from_cstr(sp_str_t str);

    typedef struct {
      u32 num_args;
      const c8** args;
      const c8* project_directory;
      bool no_interactive;
      bool lock;
      spn_cli_add_t add;
      spn_cli_init_t init;
      spn_cli_list_t list;
      spn_cli_print_t flags;
      spn_cli_dir_t dir;
    } spn_cli_t;

    void spn_cli_command_add(spn_cli_t* cli);
    void spn_cli_command_init(spn_cli_t* cli);
    void spn_cli_command_list(spn_cli_t* cli);
    void spn_cli_command_nuke(spn_cli_t* cli);
    void spn_cli_command_clean(spn_cli_t* cli);
    void spn_cli_command_print(spn_cli_t* cli);
    void spn_cli_command_build(spn_cli_t* cli);
    void spn_cli_command_dir(spn_cli_t* cli);

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
      sp_str_t build;
      sp_str_t store;
      sp_str_t include;
      sp_str_t bin;
      sp_str_t vendor;
    } spn_dep_build_paths_t;

    typedef struct {
      sp_str_t key;
      sp_str_t value;
    } spn_dep_option_t;

    typedef struct {
      sp_str_t name;
      sp_str_t url;
      spn_dep_paths_t paths;
      sp_str_t* libs;
    } spn_dep_info_t;

    typedef sp_str_t spn_dep_id_t;

    typedef struct {
      u32 delta;
    } spn_dep_update_info_t;

    typedef struct {
      sp_str_t name;
      sp_str_t url;
      sp_str_t commit;
      sp_str_t build_id;
    } spn_lock_entry_t;

    typedef struct spn_dep_context_t spn_dep_context_t;
    typedef spn_lock_entry_t* spn_lock_file_t;

    typedef struct {
      spn_dep_context_t* deps;
    } spn_build_context_t;

    spn_dep_info_t* spn_dep_find(sp_str_t name);
    sp_str_t        spn_dep_read_url(sp_str_t file_path);
    sp_str_t*       spn_dep_read_libs(sp_str_t file_path);
    sp_str_t        spn_dep_option_env_name(spn_dep_option_t* option);
    sp_str_t        spn_dep_build_state_to_str(spn_dep_build_state_t state);

    /////////
    // APP //
    /////////
    typedef struct {
      toml_table_t* toml;
      bool auto_pull_recipes;
      bool auto_pull_deps;
      sp_str_t cache_override;
      sp_str_t recipe_override;
      bool builtin_recipes_enabled;
    } spn_config_t;

    void spn_config_read(spn_config_t* config, sp_str_t path);
    void spn_config_read_from_string(spn_config_t* config, sp_str_t toml_content);

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
      spn_dep_id_t* dependencies;
      toml_table_t* toml;
    } spn_project_t;

    typedef struct {
      struct {
        sp_str_t spn;
      } paths;
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
  ]])

  ffi.cdef([[
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

    c8*                          sp_cstr_copy(const c8* str);
    bool                         sp_str_equal_cstr(sp_str_t str, const c8* cstr);
    sp_str_t                     sp_str_from_cstr(const c8* cstr);
    sp_str_t                     sp_os_extract_extension(sp_str_t path);
    sp_str_t                     sp_os_extract_stem(sp_str_t path);
    sp_str_t                     sp_os_join_path(sp_str_t a, sp_str_t b);
    void                         sp_os_log(sp_str_t message);
    sp_os_directory_entry_list_t sp_os_scan_directory(sp_str_t path);
  ]])

  sp = {
    os = {},
    str = {},
    cstr = {}
  }
  for module_name, module in pairs(sp) do
    setmetatable(module, {
      __index = function(_, key)
        key = string.format("sp_%s_%s", module_name, key)
        return ffi.C[key]
      end
    })
  end

  spn.string = ffi.metatype('sp_str_t', {
    __tostring = function(self) return ffi.string(self.data, self.len) end,
    __len = function(self) return self.len end,
    __eq = function(a, b) return tostring(a) == tostring(b) end,
    __index = function(self, key)
      if key == 'cstr' then
        return function() return ffi.string(self.data, self.len) end
      else
        return function(_, ...)
          local str = ffi.string(self.data, self.len)
          return str[key](str, ...) end
      end
    end,
  })
end

return spn
