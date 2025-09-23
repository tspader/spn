local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'bellard/quickjs',
  lib = 'quickjs',
  kinds = { 'static' },
  build = function(builder)
    builder:make({
      directory = builder.paths.source,
      targets = { 'libquickjs.a' },
    })

    builder:copy({
      { builder:source('quickjs.h'), builder:include() },
      { builder:source('quickjs-libc.h'), builder:include() },
      { builder:source('libquickjs.a'), builder:lib() },
    })
  end,
})

return recipe
