local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'TinyCC/tinycc',
  kinds = { 'static' },
  libs = { 'tcc' },
  build = function(builder)
    builder:configure()
    builder:make()
    builder:make({
      target = 'install'
    })
    builder:copy({
      { builder:source('examples'), builder:vendor()}
    })
  end,
})

return recipe
