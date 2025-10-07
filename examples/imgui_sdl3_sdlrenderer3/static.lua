local project = {
  name = 'imgui-static',
  deps = {
    imgui = {
      kind = 'static'
    },
    sdl3 = {
      kind = 'static'
    }
  },
}

return project
