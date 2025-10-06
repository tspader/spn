local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'tree-sitter/tree-sitter',
  libs = { 'tree-sitter' },
  kinds = { 'static' },
  build = function(builder)
    local make = {
      variables = {
        PREFIX = builder.paths.store
      },
      dir = builder.paths.source
    }
    builder:make(make)

    make.target = 'install'
    builder:make(make)
  end,
})

return recipe
