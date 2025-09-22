local spn = require('spn')

local config = spn.recipes.basic({
  git = 'mackron/dr_libs',
  kinds = { 'source' },
  build = function (builder)
    builder:copy({
      { builder:source('*.h'), builder:include() }
    })
  end
})
return config
