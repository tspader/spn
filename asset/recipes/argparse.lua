local spn = require('spn')

local config = spn.recipes.single_header({
  git = 'tspader/argparse',
  header = 'argparse.h'
})
return config
