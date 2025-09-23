local spn = require('spn')

local recipe = spn.recipes.single_header({
  git = 'HandmadeMath/Handmade-Math',
  header = 'HandmadeMath.h',
})

return recipe
