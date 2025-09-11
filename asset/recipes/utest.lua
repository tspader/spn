local spn = require('spn')

local config = spn.recipes.single_header({
  git = 'sheredom/utest.h',
  header = 'utest.h'
})
return config
