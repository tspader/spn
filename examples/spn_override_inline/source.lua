local spn = require('spn')

local project = {
  name = 'spn_inline_override',
  recipe_dirs = { './recipes' },
  recipes = {
    stb = spn.recipes.basic({
      git = 'nothings/stb',
      kinds = { 'source' },
      build = function(builder)
        builder:copy({
          { builder:source('stb_sprintf.h'), builder:include('foo_sprintf.h')}
        })
      end
    }),
  },
  deps = {
    stb = {},
  },
}

return project
