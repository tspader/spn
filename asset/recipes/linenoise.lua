local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'antirez/linenoise',
  lib = 'linenoise',
  kinds = { 'source' },
  include = {
    vendor = true,
  },
  build = function(builder)
    builder:copy({
      { builder:source('linenoise.h'), builder:include() },
    })

    builder:copy({
      { builder:source('linenoise.c'), builder:vendor() },
    })
  end,
})

return recipe
