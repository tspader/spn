local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'raysan5/raylib',
  libs = { 'raylib' },
  kinds = { 'shared', 'static' },
  build = function(builder)
    local shared = builder.kind == 'shared'
    builder:cmake({
      defines = {
        { 'BUILD_SHARED_LIBS', shared },
        { 'BUILD_EXAMPLES', false },
        { 'BUILD_GAMES', false },
      },
      install = true,
    })

    builder:copy({
      { builder:source('src/extras'), builder:include() },
      { builder:source('examples'), builder:vendor() },
    })
  end,
})

return recipe
