local project = {
  name = 'sdl3-static',
  system_deps = {
    'm'
  },
  deps = {
    sdl3 = {
      kind = 'static'
    },
  },
}

return project
