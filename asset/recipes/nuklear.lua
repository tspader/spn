local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'Immediate-Mode-UI/Nuklear',
  kinds = { 'source' },
  build = function(builder)
    builder:copy({
      { builder:source('nuklear.h'), builder:include() },
      { builder:source('demo'), builder:include() },
    })
  end,
})

return recipe
