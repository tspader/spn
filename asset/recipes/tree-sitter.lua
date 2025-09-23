local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'tree-sitter/tree-sitter',
  lib = 'tree-sitter',
  kinds = { 'static' },
  build = function(builder)
    builder:sh({
      command = 'make',
      args = {
        '-C', builder.paths.source,
        '-j8',
        string.format('PREFIX=%s', builder.paths.store),
      },
    })

    builder:sh({
      command = 'make',
      args = {
        '-C', builder.paths.source,
        string.format('PREFIX=%s', builder.paths.store),
        'install',
      },
    })
  end,
})

return recipe
