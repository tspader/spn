local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'libsdl-org/SDL',
  lib = 'SDL3',
  copy = {
    [spn.dir.include] = {
      [spn.dir.source] = {
        'include/SDL3'
      }
    },
    [spn.dir.lib] = {
      [spn.dir.work] = {
        'libSDL3.so',
        'libSDL3.dylib',
        'libSDL3.a',
      }
    }
  },
  build = function(builder)
    builder:cmake({
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
