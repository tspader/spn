local spn = require('spn')

local project = {
  name = 'test_inline_recipe',
  recipes = {
    testlib = spn.recipes.single_header({
      git = 'nothings/stb',
      header = 'stb_sprintf.h',
    })
  },
  deps = {
    testlib = {},
  },
}

return project
