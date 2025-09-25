local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'tree-sitter/tree-sitter',
  libs = { 'tree-sitter' },
  kinds = { 'static' },
  build = function(builder)
    builder:make({
      directory = builder.paths.source,
      jobs = 8,
      variables = {
        PREFIX = builder.paths.store,
      },
    })

    builder:make({
      directory = builder.paths.source,
      variables = {
        PREFIX = builder.paths.store,
      },
      targets = { 'install' },
    })
  end,
})

return recipe
