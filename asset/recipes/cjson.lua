local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'DaveGamble/cJSON',
  lib = 'cjson',
  kinds = { 'shared', 'source' },
  build = function(builder)
    builder:copy({
      { builder:source('cJSON.h'), builder:include() },
    })

    if builder.kind == 'source' then
      builder:copy({
        { builder:source('cJSON.c'), builder:vendor() },
      })
    else
      builder:cc({
        compiler = 'cc',
        shared = true,
        source = { 'cJSON.c' },
        output = 'libcjson.so',
      })

      builder:copy({
        { builder:work('libcjson.so'), builder:lib() },
      })
    end
  end,
})

return recipe
