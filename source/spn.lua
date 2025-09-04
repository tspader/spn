local ffi = require('ffi')

local spn = {
  recipes = {},
  internal = {},
}

function spn.log(fmt, ...)
  print(string.format(fmt, ...))
end

function spn.recipes.single_header(options)
  return options
end

function spn.internal.read(recipe)
end

function spn.internal.configure()
  local app = spn.internal.app
  local file_path = app.paths.user_config
  local config = loadfile(ffi.string(file_path.data, file_path.len))
  if config then
    print('found a config')
    print(config())
  else
    print('no config')
  end

end

function spn.internal.init(app)
  print(app)
  spn.internal.app = app
  header = ffi.cdef([[
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
    // TUI //
    /////////
    typedef struct {
      u32 std_in;
    } sp_tui_checkpoint_t;

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
    bool sp_tui_read_char(c8* input_char);
    void spn_tui_update_noninteractive(spn_tui_t* tui);
    void spn_tui_update_interactive(spn_tui_t* tui);
    void spn_tui_update(spn_tui_t* tui);
    void spn_tui_read(spn_tui_t* tui);
    void spn_tui_init(spn_tui_t* tui, spn_tui_state_t);
    void spn_tui_cleanup(spn_tui_t* tui);

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

    typedef lua_State* sp_lua_context_t;





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
      toml_table_t* table;
    } spn_dep_parse_entry_t;

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

    typedef spn_lock_entry_t* spn_lock_file_t;

    struct spn_build_context_t {
      spn_dep_context_t* deps;
    };

    spn_dep_info_t*        spn_dep_find(sp_str_t name);
    bool                   spn_dep_is_binary(spn_dep_info_t* dep);
    bool                   spn_dep_state_is_terminal(spn_dep_context_t* dep);
    s32                    spn_dep_sort_kernel_alphabetical(const void* a, const void* b);
    sp_str_t               spn_dep_read_url(sp_str_t file_path);
    sp_str_t*              spn_dep_read_libs(sp_str_t file_path);
    sp_str_t               spn_dep_option_env_name(spn_dep_option_t* option);
    sp_str_t               spn_dep_build_state_to_str(spn_dep_build_state_t state);
    spn_lock_entry_t*      spn_dep_context_get_lock_entry(spn_dep_context_t* dep);
    void                   spn_dep_context_prepare(spn_dep_context_t* context);
    void                   spn_dep_context_add_options(spn_dep_context_t* context, toml_table_t* toml);
    spn_dep_context_t      spn_dep_context_from_default_profile(sp_str_t name);
    void                   spn_dep_context_set_env(spn_dep_context_t* context, sp_str_t name, sp_str_t value);
    void                   spn_dep_context_set_build_state(spn_dep_context_t* dep, spn_dep_build_state_t state);
    void                   spn_dep_context_set_build_error(spn_dep_context_t* dep, sp_str_t error);
    spn_dep_context_t*     spn_dep_context_find(spn_build_context_t* build, sp_str_t name);
    sp_str_t               spn_print(spn_dep_context_t* dep, spn_cli_print_kind_t kind, spn_cli_compiler_t c);
    sp_str_t               spn_print_one(spn_dep_context_t* dep, spn_cli_print_kind_t kind, spn_cli_compiler_t c);
    sp_str_t               spn_print_all(spn_dep_context_t* dep, spn_cli_compiler_t c);
    spn_build_context_t    spn_build_context_from_default_profile();

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


    /////////
    // APP //
    /////////
    typedef struct {
      spn_cli_t cli;
      spn_paths_t paths;
      spn_targets_t targets;
      spn_project_t project;
      spn_config_t toml_config;
      spn_build_context_t build;
      spn_tui_t tui;
      spn_dep_info_t deps;
      spn_lock_entry_t lock;
      SDL_AtomicInt control;

      spn_lua_config_t config;
      sp_lua_t lua;
    } spn_app_t;

    void sp_os_log(sp_str_t message);
  ]])
end

return spn
