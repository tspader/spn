local project = {
  name = 'glfw',
  system_deps = {
    '-Framework OpenGL',
  },
  deps = {
    glfw = {
      kind = 'static'
    },
  }
}

return project
