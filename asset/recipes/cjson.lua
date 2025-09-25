local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'DaveGamble/cJSON',
  libs = { 'cjson' },
  kinds = { 'source' },
  include = {
    vendor = true,
  },
  build = function(builder)
    builder:copy({
      { builder:source('cJSON.h'), builder:include() },
    })

    builder:copy({
      { builder:source('cJSON.c'), builder:vendor() },
    })
  end,
})

return recipe
