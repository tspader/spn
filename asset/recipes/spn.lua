local spn = require('spn')

local recipe = spn.recipes.single_header({
  git = 'tspader/spn',
  header = 'source/spn.h'
})
return recipe
