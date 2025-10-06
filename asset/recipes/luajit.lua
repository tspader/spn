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

    builder:make({
      target = 'amalg',
      env = {
        MACOSX_DEPLOYMENT_TARGET = '13.0'
      }
    })

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
