#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

int main() {
  lua_State* state = luaL_newstate();
  if (!state) return 1;
  luaL_openlibs(state);
  if (luaL_dostring(state, "return 40 + 2")) return 2;
  int value = (int)lua_tointeger(state, -1);
  lua_close(state);
  return value == 42 ? 0 : 3;
}
