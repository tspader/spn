local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'jedisct1/libsodium',
  libs = { 'sodium' },
  kinds = { 'shared', 'static' },
  build = function(builder)
    builder:sh({
      command = builder:source('autogen.sh').absolute,
      args = { '-s' },
      cwd = builder.paths.source,
    })

    builder:configure()
    builder:make()
    builder:make({ target = 'install' })
  end,
})

return recipe
