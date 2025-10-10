
local spn = require('spn')

local config = spn.recipes.single_header({
  git = 'graphitemaster/incbin',
  header = 'incbin.h'
})
return config
