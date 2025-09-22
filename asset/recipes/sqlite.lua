local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'sqlite/sqlite',
  kinds = { 'shared', 'static' },
  lib = 'sqlite3',
  build = function(builder)
    builder:sh({
      command = spn.join_path(builder.paths.source, 'configure'),
    })
    builder:make()
    builder:copy({
      { builder:work('sqlite3.h'), builder:include() },
      { builder:work('libsqlite3*'), builder:lib() },
    })
  end
})

return recipe
