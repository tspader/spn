local spn = require('spn')

local recipe = spn.recipes.single_header({
  git = 'nothings/stb',
  header = 'stb_ds.h', -- i.e. not stb_sprintf.h
})

return recipe
