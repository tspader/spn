#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

int main(int num_args, const char** args) {
  lua_State* l = luaL_newstate();
  luaL_openlibs(l);
  luaL_dostring(l, "print('hello, world!')");
}

