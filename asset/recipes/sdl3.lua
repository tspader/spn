local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'libsdl-org/SDL',
  lib = 'SDL3',
  kinds = { 'shared', 'static' },
  build = function(builder)
    builder:cmake({
      install = true,
      defines = {
        { 'SDL_SHARED', builder.kind == spn.build_kind.shared },
        { 'SDL_STATIC', builder.kind == spn.build_kind.static },
        { 'SDL_TESTS', false },
        { 'SDL_EXAMPLES', false },
      }
    })
  end
})

return recipe
