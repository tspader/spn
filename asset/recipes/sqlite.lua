local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'sqlite/sqlite',
  lib = 'sqlite3',
  copy = {
    [spn.dir.include] = {
      [spn.dir.work] = {
        'sqlite3.h'
      },
    },
    [spn.dir.lib] = {
      [spn.dir.work] = {
        'libsqlite3.dylib',
        'libsqlite3.so',
        'libsqlite3.a',
      }
    }
  },
  build = function(builder)
    builder:sh({
      command = spn.join_path(builder.paths.source, 'configure'),
    })
    builder:make()
  end
})

return recipe
