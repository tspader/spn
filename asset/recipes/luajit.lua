local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'LuaJIT/LuaJIT',
  kinds = { 'static', 'shared' },
  libs = { 'luajit' },
  options = {

  },
  build = function(builder)
    -- mike.......
    builder:copy({
      { builder:source('*'), builder:work() }
    })

    local make = {
      target = 'amalg',
      macos = {
        env = {
          MACOSX_DEPLOYMENT_TARGET = '13.0'
        },
        variables = {
          LUAJIT_SO = 'libluajit.dylib',
          TARGET_DYLIBPATH = '@rpath/libluajit.dylib'
        }
      }
    }

    if builder.platform == 'macos' then
      make.env = {
        MACOSX_DEPLOYMENT_TARGET = '13.0'
      }
      make.variables = {
        LUAJIT_SO = 'libluajit.dylib',
        TARGET_DYLIBPATH = '@rpath/libluajit.dylib'
      }
    end

    builder:make(make)

    builder:copy({
      { builder:source('src/lua.h'), builder:include() },
      { builder:source('src/luaconf.h'), builder:include() },
      { builder:source('src/lualib.h'), builder:include() },
      { builder:source('src/lauxlib.h'), builder:include() },
      { builder:work('src/libluajit*'), builder:lib() },
    })
  end
})

return recipe
