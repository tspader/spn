local spn = require('spn')

local config = spn.recipes.single_header({
  git = 'fungos/cr',
  header = 'cr/cr.h'
})
return config
