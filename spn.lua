local project = {
  name = 'spn',
  deps = {
    sp = {},
    argparse = {},
    luajit = {
      kind = 'static'
    }
  }
}

return project
