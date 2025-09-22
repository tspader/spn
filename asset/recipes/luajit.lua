local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'LuaJIT/LuaJIT',
  kinds = { 'static', 'shared' },
  lib = 'luajit',
  build = function(builder)
    -- mike.......
    builder:sh({
      command = 'rsync',
      args = {'-a', '--exclude=.git', builder.paths.source .. '/', builder.paths.work .. '/'}
    })

    builder:make({
      target = 'amalg'
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
