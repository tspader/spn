local spn = require('spn')

local project = {
  name = 'test_inline_recipe',
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
