local inspect = require('inspect')
local ffi = require('ffi')
local utils = require('utils')
local c = require('c')
local sp = c.sp
local serpent = require('serpent')
local sp_lua_stack_t = require('stack')
local spn_lua_dep_builder_t = require('build')

-------------
-- PROJECT --
-------------
---@class spn_lua_dep_include_t
---@field include boolean
---@field vendor boolean
---@field store boolean

---@class spn_lua_dep_t
---@field kind? string
---@field include? spn_lua_dep_include_t
---@field options table


-----------
-- BUILD --
-----------
---@class spn_lua_dep_paths_t
---@field recipe string
---@field source string
---@field work string
---@field store string
---@field lib string
---@field include string
---@field vendor string

---@class spn_lua_dep_build_t
---@field dep spn_lua_dep_t
---@field paths spn_lua_dep_paths_t

---@class spn_lua_cc_config_t
---@field compiler string
---@field output string
---@field shared string|nil
---@field source string[]|nil
---@field include string[]|nil

---@class spn_lua_make_config_t
---@field makefile string|nil
---@field target string|nil
---@field targets string[]|nil
---@field cwd string|nil
---@field jobs number|string|nil
---@field variables table<string, string>|nil

---@class spn_lua_cmake_config_t
---@field defines string[]|nil
---@field install boolean|nil
---@field parallel boolean|nil

---@class spn_lua_sh_config_t
---@field command string
---@field args? string[]
---@field env? string[]
---@field cwd string|nil

------------
-- RECIPE --
------------
---@class spn_lua_recipe_t
---@field git string
---@field libs string[]
---@field kinds string[]
---@field include spn_lua_dep_include_t
---@field branch string
---@field options table
---@field build fun(spn_lua_dep_build_t): nil
---@field configure fun(table): nil

---@class spn_lua_recipe_config_t
---@field git string
---@field kinds? string[]
---@field include? spn_lua_dep_include_t
---@field libs? string[]
---@field branch? string
---@field options? table
---@field build fun(spn_lua_dep_build_t): nil | nil
---@field configure? fun(table): nil

---@class spn_lua_single_header_config_t : spn_lua_recipe_config_t
---@field header string


------------
-- MODULE --
------------
local spn = {
  recipes = {},
  lock_file = {
    deps = {}
  },
  project = {
    name = '',
    deps = {},
  },
  config = {},

  iterator = require('iterator'),
  dir = {
    source = 'source',
    work = 'work',
    include = 'include',
    lib = 'lib',
    vendor = 'vendor',
  },
  join_path = function(la, lb)
    return sp.os.join_path(sp.str.from_cstr(la), sp.str.from_cstr(lb)):cstr()
  end
}


----------
-- INIT --
----------
function spn.load()
  local app = c.app

  local loader = loadfile(app.paths.user_config:cstr())
  if loader then
    local ok, result = pcall(loader)
    if not ok then
      error(string.format('failed to evaluate user config %s: %s', app.paths.user_config:cstr(), result))
    end

    if type(result) == 'table' then
      spn.config = result
    else
      error(string.format('user config did not return a table (%s)', app.paths.user_config:cstr()))
    end
  end

  local chunk, _ = loadfile(app.paths.project.config:cstr())
  if chunk then
    local _, project = pcall(chunk)
    spn.project = project
  end
  spn.project = dofile(app.paths.project.config:cstr())

  local chunk, _ = loadfile(app.paths.project.lock:cstr())
  if chunk then
    local _, lock_file = pcall(chunk)
    spn.lock_file = lock_file
  end

  -- Build the recipe search paths, then load all recipes we find in them
  local recipe_dirs = {}

  table.insert(recipe_dirs, app.paths.recipes)
  for dir in spn.iterator.values(spn.project.recipe_dirs or {}) do
    table.insert(recipe_dirs, sp.os.join_path(app.paths.project.dir, sp.str.from_cstr(dir)))
  end

  for dir in spn.iterator.values(recipe_dirs) do
    if not sp.os.does_path_exist(dir) then return end
    if not sp.os.is_directory(dir) then return end

    local entries = sp.os.scan_directory(dir)
    for index = 0, entries.count - 1 do
      local entry = entries.data[index]
      local extension = sp.os.extract_extension(entry.file_name)

      if sp.str.equal_cstr(extension, "lua") then
        local recipe = dofile(entry.file_path:cstr())
        recipe.name = sp.os.extract_stem(entry.file_name):cstr()
        recipe.file_path = entry.file_path:cstr()

        spn.recipes[recipe.name] = recipe
      end
    end
  end

  if spn.project.recipes then
    for name, recipe in pairs(spn.project.recipes) do
      recipe.name = name
      recipe.file_path = app.paths.project.config:cstr()

      spn.recipes[name] = recipe
    end
  end

  -- Configure the deps that the project uses with the project's options
  for name, spec in pairs(spn.project.deps) do
    local recipe = spn.recipes[name]
    recipe.spec = {
      kind = spec.kind or recipe.kinds[1],
      include = utils.merge(recipe.include, spec.include),
      options = utils.merge(recipe.options, spec.options),
    }
    recipe:configure()
  end

  spn.project.matrix = spn.project.matrix or {
    {
      name = 'debug',
      mode = 'debug',
    },
    {
      name = 'release',
      mode = 'release',
    },
  }
  for matrix in spn.iterator.values(spn.project.matrix) do
    matrix.mode = matrix.mode or 'debug'
  end
end

function spn.parse()
  local app = c.app

  -- User config
  if spn.config.spn then
    app.config.paths.spn = sp.str.from_cstr(spn.config.spn)
  end

  if spn.config.pull_recipes then
    app.config.pull_recipes = spn.config.pull_recipes
  end

  if spn.config.pull_deps then
    app.config.pull_deps = spn.config.pull_deps
  end

  -- Project file
  app.project.name = sp.str.from_cstr(spn.project.name)

  for config in spn.iterator.values(spn.project.matrix) do
    local matrix = ffi.new('spn_build_matrix_t')
    matrix.name = sp.str.from_cstr(config.name)
    matrix.mode = c.spn.dep.build_mode_from_str(sp.str.from_cstr(config.mode))
    matrix.hash = sp.hash.combine({ matrix.mode })

    app.project.matrices = sp.dyn_array.push(app.project.matrices, matrix)
  end

  if spn.project.system_deps then
    for dep_name in spn.iterator.values(spn.project.system_deps) do
      app.project.system_deps = sp.dyn_array.push(app.project.system_deps, sp.str.from_cstr(dep_name))
    end
  end

  -- Dependencies
  for name in spn.iterator.keys(spn.project.deps) do
    local recipe = spn.recipes[name]

    local dep = ffi.new('spn_dep_info_t')
    dep.name = sp.str.from_cstr(recipe.name)
    dep.git = sp.str.from_cstr(recipe.git)
    dep.branch = sp.str.from_cstr(recipe.branch)
    dep.paths.source = sp.os.join_path(app.paths.source, dep.name)
    dep.paths.recipe = sp.str.from_cstr(recipe.file_path)

    for lib in spn.iterator.values(recipe.libs) do
      dep.libs = sp.dyn_array.push(dep.libs, sp.str.from_cstr(lib))
    end

    app.deps[0] = sp.dyn_array.push(app.deps[0], dep)
  end

  for name in spn.iterator.keys(spn.project.deps) do
    local recipe = spn.recipes[name]

    local dep = ffi.new('spn_dep_spec_t')
    dep.info = c.spn.dep.find(sp.str.from_cstr(name))

    local lock = spn.lock_file.deps[name]
    if lock then
      dep.lock = sp.str.from_cstr(lock.commit)
    end

    dep.kind = c.spn.dep.build_kind_from_str(sp.str.from_cstr(recipe.spec.kind))

    dep.include.include = recipe.spec.include.include
    dep.include.vendor = recipe.spec.include.vendor
    dep.include.store = recipe.spec.include.store

    -- Hash name and everything in the options table
    local values = {}
    table.insert(values, name)
    table.insert(values, recipe.spec.kind)

    local stack = sp_lua_stack_t:new()
    stack:push(recipe.spec.options)

    while not stack:is_empty() do
      local t = stack:pop()

      for key, value in pairs(t) do
        if type(value) == 'table' then
          stack:push(value)
        else
          table.insert(values, string.format('%s:%s', key, value))
        end
      end
    end

    dep.hash = sp.hash.combine(values)

    -- Push it to an array in C
    app.project.deps = sp.dyn_array.push(app.project.deps, dep)
  end
end

function spn.context(app)
  c.load(app)
end

function spn.init(app)
  spn.context(app)
  spn.load()
  spn.parse()
end

function spn.build(app, dep)
  spn.context(app)
  spn.load()

  dep = ffi.cast('spn_dep_build_context_t*', dep)
  local recipe = spn.recipes[dep.info.name:cstr()]
  local builder = spn_lua_dep_builder_t.new(dep, recipe)
  builder:build()
end

function spn.lock()
  local app = c.app
  local lock_array = app.lock[0]

  local num_deps = 0
  for _ in pairs(spn.project.deps) do
    num_deps = num_deps + 1
  end

  local lockfile = {
    deps = {}
  }

  for i = 0, num_deps - 1 do
    local entry = lock_array[i]

    local name = ffi.string(entry.name.data, entry.name.len)
    local commit = ffi.string(entry.commit.data, entry.commit.len)
    local build_id = ffi.string(entry.build_id.data, entry.build_id.len)

    lockfile.deps[name] = {
      name = name,
      commit = commit,
      build_id = build_id
    }
  end

  local serialized = serpent.block(lockfile, {comment = false, sortkeys = true})

  local path = app.paths.project.lock:cstr()
  local file = io.open(path, 'w')
  if not file then
    return false
  end

  file:write("return " .. serialized)
  file:close()

  return true
end

function spn.copy(to)
  to = sp.str.from_cstr(to)

  for dep in spn.iterator.values(spn.project.deps) do
    local entries = sp.os.scan_directory(sp.str.from_cstr(dep.paths.lib))

    for j = 0, entries.count - 1 do
      local entry = entries.data[j]
      sp.os.copy_file(
        entry.file_path,
        sp.os.join_path(sp.str.from_cstr(to), sp.os.extract_file_name(entry.file_path))
      )
    end
  end
end

---@param config spn_lua_recipe_config_t
---@return spn_lua_recipe_t
local basic = function(config)
  local recipe = {
    git = '',
    libs = {},
    kinds = { 'shared', 'static', 'source' },
    include = {
      include = true,
      vendor = false,
      store = false
    },
    branch = 'HEAD',
    options = {},
    build = function() end,
    configure = function() end,
  }

  recipe.git = config.git
  recipe.libs = config.libs or recipe.libs
  recipe.kinds = config.kinds or recipe.kinds
  recipe.branch = config.branch or recipe.branch
  recipe.options = config.options or recipe.options
  recipe.build = config.build or recipe.build
  recipe.configure = config.configure or recipe.configure
  if config.include then
    recipe.include = {
      include = utils.ternary(config.include.include, recipe.include.include),
      vendor = utils.ternary(config.include.vendor, recipe.include.vendor),
      store = utils.ternary(config.include.store, recipe.include.store),
    }
  end
  return recipe
end


---@param config spn_lua_single_header_config_t
---@return spn_lua_recipe_t
local single_header = function(config)
  local recipe = basic(config)
  recipe.kinds = { 'source' }
  recipe.build = function(builder)
    builder:copy({
      { builder:source(config.header), builder:include() }
    })
  end
  return recipe
end

local module = {
  recipes = {
    single_header = single_header,
    basic = basic,
  },
  dir = spn.dir,
  join_path = spn.join_path,
  iterator = spn.iterator,
  build_kind = {
    shared = 'shared',
    static = 'static',
    source = 'source'
  },
  utils = {
    merge = utils.merge,
  },
  internal = spn
}

return module
