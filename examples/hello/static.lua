local project = {
  name = 'hello-static',
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
