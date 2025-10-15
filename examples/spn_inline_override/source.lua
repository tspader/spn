local spn = require('spn')

local project = {
  name = 'spn_inline_override',
  recipe_dirs = { './recipes' },
  recipes = {
    stb_sp = spn.recipes.single_header({
      git = 'nothings/stb',
      header = 'stb_sprintf.h',
    })
  },
  deps = {
    stb_sp = {},
  },
}

return project
