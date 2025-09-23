local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'antirez/linenoise',
  lib = 'linenoise',
  kinds = { 'shared', 'source' },
  build = function(builder)
    builder:copy({
      { builder:source('linenoise.h'), builder:include() },
    })

    if builder.kind == 'source' then
      builder:copy({
        { builder:source('linenoise.c'), builder:vendor() },
      })
    else
      builder:cc({
        compiler = 'cc',
        shared = true,
        source = { 'linenoise.c' },
        output = 'liblinenoise.so',
      })

      builder:copy({
        { builder:work('liblinenoise.so'), builder:lib() },
      })
    end
  end,
})

return recipe
