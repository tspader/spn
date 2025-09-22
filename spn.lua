local project = {
  name = 'spn',
  deps = {
    sp = {},
    sdl3 = {},
    argparse = {},
    luajit = {
      kind = 'shared'
    }
  },
}

return project
