local spn = require('spn')

local config = spn.recipes.single_header({
  git = 'tspader/toml',
  header = 'toml.h'
})
return config
