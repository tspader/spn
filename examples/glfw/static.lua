local project = {
  name = 'glfw3-static',
  system_deps = {
    'GL',
  },
  deps = {
    glfw = {
      kind = 'static'
    },
  }
}

return project
