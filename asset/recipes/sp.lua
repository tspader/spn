local spn = require('spn')

local config = spn.recipes.single_header({
  git = 'tspader/sp',
  lib = 'fuck',
  header = 'sp.h'
})
return config
