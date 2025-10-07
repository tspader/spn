local project = {
  name = 'raylib-static',
  system_deps = {
    'm'
  },
  deps = {
    sp = {},
    raylib = {
      kind = 'static'
    },
  },
}

return project
