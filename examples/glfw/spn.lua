local project = {
  name = 'glfw',
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
