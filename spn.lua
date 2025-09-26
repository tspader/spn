local project = {
  name = 'spn',
  deps = {
    sp = {},
    sdl3 = {
      kind = 'static'
    },
    argparse = {},
    luajit = {
      kind = 'static'
    }
  },
}

return project
