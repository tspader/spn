local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'libsdl-org/SDL',
  lib = 'SDL3',
  build = function(builder)
    builder:cmake({
      install = true,
      defines = {
        { 'SDL_SHARED', true },
        { 'SDL_STATIC', false },
        { 'SDL_TESTS', false },
        { 'SDL_EXAMPLES', false },
      }
    })
  end
})

return recipe
