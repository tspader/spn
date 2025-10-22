local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'sqlite/sqlite',
  kinds = { 'shared', 'static' },
  libs = { 'sqlite3' },
  build = function(builder)
    builder:configure({
      args = {
        '--disable-tcl'
      }
    })
    builder:make()
    builder:make({
      target = 'install'
    })
  end
})

return recipe
