local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'antirez/sds',
  lib = 'sds',
  kinds = { 'shared', 'source' },
  build = function(builder)
    builder:copy({
      { builder:source('sds.h'), builder:include() },
    })

    if builder.kind == 'source' then
      builder:copy({
        { builder:source('sds.c'), builder:vendor() },
      })
    else
      builder:cc({
        compiler = 'cc',
        shared = true,
        source = { 'sds.c' },
        output = 'libsds.so',
      })

      builder:copy({
        { builder:work('libsds.so'), builder:lib() },
      })
    end
  end,
})

return recipe
