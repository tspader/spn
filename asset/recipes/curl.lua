local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'curl/curl',
  libs = { 'curl' },
  kinds = { 'shared', 'static' },
  build = function(builder)
    builder:cmake({
      defines = {
        { 'BUILD_TESTING', false },
        { 'CURL_DISABLE_TESTS', true },
        { 'CURL_DISABLE_EXAMPLES', true },
      },
      install = true,
    })

  end,
})

return recipe
