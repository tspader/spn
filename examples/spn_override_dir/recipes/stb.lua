local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'nothings/stb',
  kinds = { 'source' },
  build = function(builder)
    builder:copy({
      { builder:source('stb_sprintf.h'), builder:include('foo_sprintf.h')}
    })
  end
})

return recipe
