local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'jedisct1/libsodium',
  lib = 'sodium',
  kinds = { 'shared', 'static' },
  build = function(builder)
    local shared = builder.kind == 'shared'
    local source = builder.paths.source
    local prefix = builder.paths.store
    local shared_flag = shared and '--enable-shared --disable-static' or '--disable-shared --enable-static'

    builder:sh({
      command = 'sh',
      args = {
        '-c',
        string.format('cd %s && ./autogen.sh -s', source),
      },
    })

    builder:sh({
      command = 'sh',
      args = {
        '-c',
        string.format('cd %s && ./configure --prefix=%s %s', source, prefix, shared_flag),
      },
    })

    builder:sh({
      command = 'sh',
      args = {
        '-c',
        string.format('cd %s && make -j8', source),
      },
    })

    builder:sh({
      command = 'sh',
      args = {
        '-c',
        string.format('cd %s && make install', source),
      },
    })
  end,
})

return recipe
