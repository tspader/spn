local ffi = require('ffi')
local iterator = require('iterator')
local dbg = require('debugger')
local c = require('c')
local sp = c.sp

local module = {}
module.__index = module

-------------
-- BUILDER --
-------------
function module.new(dep)
  local self = setmetatable({}, module)
  self.dep = dep
  self.name = dep.info.name:cstr()
  self.kind = c.spn.dep.build_kind_to_str(dep.spec.kind):cstr()
  self.paths = {
    recipe = dep.info.paths.recipe:cstr(),
    source = dep.paths.source:cstr(),
    work = dep.paths.work:cstr(),
    store = dep.paths.store:cstr(),
    lib = dep.paths.lib:cstr(),
    include = dep.paths.include:cstr(),
    vendor = dep.paths.vendor:cstr(),
  }
  return self
end

function module:build()
  self.recipe = dofile(self.paths.recipe)

  if self.recipe.build then
    self.recipe.build(self)
  end

  if self.recipe.copy then
    for to_id, entries in iterator.pairs(self.recipe.copy) do
      local store = sp.str.from_cstr(self.paths[to_id])

      for from_id, files in iterator.pairs(entries) do
        local source = sp.str.from_cstr(self.paths[from_id])

        for file_path in iterator.values(files) do
          local from = sp.os.join_path(source, sp.str.from_cstr(file_path))

          local target_name = sp.os.extract_file_name(sp.str.from_cstr(file_path))
          local to = sp.os.join_path(store, target_name)

          if sp.os.is_directory(from) then
            sp.os.copy_directory(from, to)
          elseif sp.os.is_regular_file(from) then
            sp.os.copy_file(from, to)
          end
        end
      end
    end
  end

end

----------------
-- PUBLIC API --
----------------
---@param config spn_lua_sh_config_t
function module:sh(config)
  local context = ffi.new('spn_sh_process_context_t')
  context.command = sp.str.from_cstr(config.command)
  context.work = self.dep.paths.work
  context.shell = self.dep.sh

  if config.args then
    local args_array = ffi.new('sp_str_t* [1]')
    for arg in iterator.values(config.args) do
      local str_arg = sp.str.from_cstr(arg)
      args_array[0] = sp.dyn_array.push(args_array[0], str_arg)
    end

    context.args = args_array[0]
  end

  c.spn.sh.run(context)
  c.spn.sh.wait(context)

  if context.result.return_code ~= 0 then
    dbg()
  end
end

---@param config spn_lua_sh_config_t
function module:sh_proxy(config)
  local shell = config.command
  for arg in iterator.values(config.args) do
    shell = string.format('%s %s', shell, arg)
  end

  return self:sh({
    command = 'sh',
    args = {
      '-c',
      shell
    }
  })
end

---@param config spn_lua_make_config_t
function module:make(config)
  config = config or {}

  local sh = {
    command = 'make',
    args = {
      '--quiet',
      '--directory', self.paths.work
    }
  }

  if config.makefile then
    table.insert(sh.args, '--makefile')
    table.insert(sh.args, config.makefile)
  end

  if config.target then
    table.insert(sh.args, config.target)
  end

  self:sh(sh)
end

---@param config spn_lua_cmake_config_t
function module:cmake(config)
  config = config or {}
  config.defines = config.defines or {}
  config.parallel = (config.parallel == nil) and true or config.parallel
  config.install = (config.install == nil) and false or config.install

  -- Generate
  local sh = {
    command = 'cmake',
    args = {
      '-S', self.paths.source,
      '-B', self.paths.work,
    }
  }

  for define in iterator.values(config.defines) do
    local name = define[1]
    local value = define[2] and 'ON' or 'OFF'
    table.insert(sh.args, string.format('-D%s=%s', name, value))
  end

  self:sh_proxy(sh)

  -- Build
  sh = {
    command = 'cmake',
    args = {
      '--build', self.paths.work,
    }
  }
  if config.parallel then
    table.insert(sh.args, '--parallel')
  end

  self:sh_proxy(sh)

  -- Install
  if config.install then
    self:sh_proxy({
      command = 'cmake',
      args = {
        '--install', self.paths.work,
        '--prefix', self.paths.store
      }
    })
  end
end

function module:path(base, subpath)
  return {
    base = base,
    subpath = subpath,
    absolute = sp.os.join_path(sp.str.from_cstr(base), sp.str.from_cstr(subpath)):cstr()
  }
end

function module:source(subpath)
  return self:path(self.paths.source, subpath)
end

function module:work(subpath)
  return self:path(self.paths.work, subpath)
end

function module:store(subpath)
  return self:path(self.paths.store, subpath)
end

function module:include(subpath)
  return self:path(self.paths.include, subpath)
end

function module:lib(subpath)
  return self:path(self.paths.lib, subpath)
end

function module:vendor(subpath)
  return self:path(self.paths.vendor, subpath)
end

function module:copy(config)
  for entry in iterator.values(config) do
    local from = entry[1]
    local to = entry[2]
    sp.os.copy(sp.str.from_cstr(from.absolute), sp.str.from_cstr(to.absolute))
  end
end

return module
