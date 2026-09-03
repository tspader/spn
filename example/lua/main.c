#define SP_IMPLEMENTATION
#include "sp.h"

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

static int square(lua_State* L) {
  lua_Integer x = luaL_checkinteger(L, 1);
  lua_pushinteger(L, x * x);
  return 1;
}

s32 run(s32 num_args, const c8** args) {
  const c8* path = num_args > 1 ? args[1] : "main.lua";

  sp_mem_t mem = sp_mem_heap_as_allocator(sp_mem_heap_new());
  sp_str_t script = sp_zero;
  if (sp_io_read_file(mem, sp_cstr_as_str(path), &script)) {
    sp_log_err("{.red} {}", sp_fmt_cstr("could not read"), sp_fmt_cstr(path));
    return 1;
  }

  sp_log("{.bold}", sp_fmt_cstr(path));
  sp_log("{.gray}", sp_fmt_str(sp_str_trim(script)));
  sp_log("");

  lua_State* L = luaL_newstate();
  luaL_openlibs(L);
  lua_register(L, "square", square);

  if (luaL_dofile(L, path) != LUA_OK) {
    sp_log_err("{.red}", sp_fmt_cstr(lua_tostring(L, -1)));
    return 1;
  }

  lua_close(L);
  return 0;
}
SP_MAIN(run)
