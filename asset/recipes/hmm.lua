local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'HandmadeMath/Handmade-Math',
  kinds = { 'source' },
  build = function(builder)
    builder:copy({
      { builder:source('HandmadeMath.h'), builder:include() },
    })
  end,
})

return recipe
