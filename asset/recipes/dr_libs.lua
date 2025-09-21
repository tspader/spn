local spn = require('spn')

local config = spn.recipes.basic({
  git = 'mackron/dr_libs',
  build = function (builder)
    builder:copy({
      { builder:source('*.h'), builder:include() }
    })
  end
})
return config
