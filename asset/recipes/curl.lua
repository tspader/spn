local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'curl/curl',
  lib = 'curl',
  kinds = { 'shared', 'static' },
  build = function(builder)
    local shared = builder.kind == 'shared'
    builder:cmake({
      defines = {
        { 'BUILD_SHARED_LIBS', shared },
        { 'BUILD_TESTING', false },
        { 'CURL_DISABLE_TESTS', true },
        { 'CURL_DISABLE_EXAMPLES', true },
      },
      install = true,
    })

  end,
})

return recipe
