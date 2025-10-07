return {
  name = 'examples',
  deps = {
    sp = {},
    argparse = {}
  },
  targets = {
    main = {
      inputs = { 'main.c' }
    }
  }
}
