local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'antirez/sds',
  libs = { 'sds' },
  kinds = { 'source' },
  include = {
    vendor = true,
  },
  build = function(builder)
    builder:copy({
      { builder:source('sds.h'), builder:include() },
      { builder:source('sdsalloc.h'), builder:include() },
    })

    builder:copy({
      { builder:source('sds.c'), builder:vendor() },
    })
  end,
})

return recipe
