local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'rxi/microui',
  lib = 'microui',
  kinds = { 'source' },
  include = {
    vendor = true,
  },
  build = function(builder)
    builder:copy({
      { builder:source('src/microui.h'), builder:include() },
      { builder:source('src/microui.c'), builder:vendor() },
    })
  end,
})

return recipe
