local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'floooh/sokol',
  kinds = { 'source' },
  build = function(builder)
    builder:copy({
      { builder:source('*.h'), builder:include() },
      { builder:source('util'), builder:include() },
    })
  end,
})

return recipe
