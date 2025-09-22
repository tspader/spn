local project = {
  name = 'hello',
  deps = {
    luajit = {
      include = {
        include = false,
        vendor = true,
        store = true,
      },
      kind = 'shared'
    },
  },
}

return project
