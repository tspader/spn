local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'LuaJIT/LuaJIT',
  lib = 'luajit',
  copy = {
    [spn.dir.include] = {
      [spn.dir.source] = {
        'src/lua.h',
        'src/luaconf.h',
        'src/lualib.h',
        'src/lauxlib.h',
      },
    },
    [spn.dir.lib] = {
      [spn.dir.work] = {
        'src/libluajit.dylib',
        'src/libluajit.so',
        'src/libluajit.a',
      }
    }
  },
  build = function(builder)
    builder:sh({
      command = 'rsync',
      args = {'-a', '--exclude=.git', builder.paths.source .. '/', builder.paths.work .. '/'}
    })

    builder:make({
      target = 'amalg'
    })
  end
})

return recipe
