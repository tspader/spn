local spn = require('spn')

local config = spn.recipes.single_header({
  git = 'tspader/sp',
  header = 'sp.h'
})
return config
