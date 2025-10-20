local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'libgit2/libgit2',
  libs = { 'git2' },
  kinds = { 'shared', 'static' },
  build = function(builder)
    builder:cmake({
      defines = {
        { 'BUILD_EXAMPLES', false },
        { 'BUILD_FUZZER', false },
        { 'REGEX_BACKEND', 'builtin' },
        { 'USE_GSSAPI', false },
      },
      install = true
    })
  end,
})

return recipe
