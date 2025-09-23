local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'nothings/stb',
  kinds = { 'source' },
  build = function(builder)
    builder:copy({
      { builder:source('*.h'), builder:include() },
    })
  end,
})

return recipe
