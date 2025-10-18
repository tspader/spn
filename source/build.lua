local ffi = require('ffi')
local iterator = require('iterator')
local utils = require('utils')
local c = require('c')
local sp = c.sp

local module = {}
module.__index = module

-------------
-- BUILDER --
-------------
function module.new(dep, recipe)
  local self = setmetatable({}, module)
  self.recipe = recipe
  self.options = recipe.spec.options
  self.kind = recipe.spec.kind

  self.dep = dep
  self.mode = c.spn.dep.build_mode_to_str(dep.matrix.mode):cstr()
  self.name = dep.info.name:cstr()
  self.paths = {
    recipe = dep.info.paths.recipe:cstr(),
    source = dep.paths.source:cstr(),
    work = dep.paths.work:cstr(),
    store = dep.paths.store:cstr(),
    lib = dep.paths.lib:cstr(),
    include = dep.paths.include:cstr(),
    vendor = dep.paths.vendor:cstr(),
  }

  self.platform = c.sp.os.platform_name():cstr()

  -- @spader
  -- Half baked. I only invoke the compiler directly to build ImGui as a DLL, and I don't want to
  -- do that almost ever. The only thing I can see is if we wanna do cross compilation, just stick
  -- your compiler and cflags in the build matrix and spn passes it along when it builds.
  --
  -- but for now...yeah, just leaving this here
  if self.platform == 'macos' then
    self.tools = {
      cc = 'clang',
    }
  elseif self.platform == 'linux' then
    self.tools = {
      cc = 'gcc',
    }
  else
    self.tools = {
      cc = 'gcc',
    }
  end

  return self
end

function module:build()
  self.recipe.build(self)
end

----------------
-- PUBLIC API --
----------------
---@param config spn_lua_sh_config_t
function module:sh(config)
  config = config or {}
  config = utils.merge(config, config[self.platform] or {})
  config.cwd = config.cwd or self.paths.work
  config.args = config.args or {}
  config.env = config.env or {}

  local context = ffi.new('spn_sh_process_context_t')
  context.command = sp.str.from_cstr(config.command)
  context.work = sp.str.from_cstr(config.cwd)
  context.shell = self.dep.sh

  for arg in iterator.values(config.args) do
    c.spn.sh.add_arg(context, c.sp.str.from_cstr(arg))
  end

  for name, value in iterator.pairs(config.env) do
    c.spn.sh.add_env(context, c.sp.str.from_cstr(name), c.sp.str.from_cstr(value))
  end

  c.spn.sh.run(context)
  c.spn.sh.wait(context)

  if context.result.return_code ~= 0 then
    error(string.format('failed with return code %d', context.result.return_code))
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

---@param config spn_lua_cc_config_t
function module:cc(config)
  config = config or {}
  config = utils.merge(config, config[self.platform] or {})

  local sh = {
    command = self.tools.cc,
    args = {
      '-o', self.paths.work .. '/' .. config.output
    }
  }

  for source in iterator.values(config.source) do
    table.insert(sh.args, self.paths.source .. '/' ..  source)
  end

  if config.shared then
    table.insert(sh.args, '-shared')
    table.insert(sh.args, '-fPIC')
  end

  if config.include then
    for include in iterator.values(config.include) do
      local path = self.paths.source .. '/' .. include
      table.insert(sh.args, string.format('-I%s', path))
    end
  end

  self:sh(sh)
end


---@param config spn_lua_make_config_t
function module:make(config)
  config = config or {}
  config.jobs = config.jobs or 4
  config = utils.merge(config, config[self.platform] or {})

  local sh = {
    command = 'make',
    args = {
      '--quiet',
      '--directory', config.dir or self.paths.work
    },
    env = config.env
  }

  if config.makefile then
    table.insert(sh.args, '--makefile')
    table.insert(sh.args, config.makefile)
  end

  if config.jobs then
    table.insert(sh.args, string.format('--jobs=%s', config.jobs))
  end

  if config.variables then
    for name, value in iterator.pairs(config.variables) do
      table.insert(sh.args, string.format('%s=%s', name, value))
    end
  end

  if config.targets then
    for target in iterator.values(config.targets) do
      table.insert(sh.args, target)
    end
  elseif config.target then
    table.insert(sh.args, config.target)
  end

  self:sh(sh)
end

---@param config spn_lua_cmake_config_t
function module:cmake(config)
  config = config or {}
  config = utils.merge(config, config[self.platform] or {})
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

  local build_type = self.mode == 'debug' and 'Debug' or 'Release'
  table.insert(sh.args, string.format('-DCMAKE_BUILD_TYPE=%s', build_type))
  table.insert(sh.args, string.format('-DCMAKE_DEBUG_POSTFIX='))

  local shared = self.kind == 'shared'
  table.insert(config.defines, { 'BUILD_SHARED_LIBS', shared })

  for define in iterator.values(config.defines) do
    local name = define[1]
    local value = define[2]
    if type(value) == 'bool' then
      value = value and 'ON' or 'OFF'
    end
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

function module:configure(config)
  config = config or {}

  local sh = {
    command = self:source('configure').absolute,
    args = {
      string.format('--prefix=%s', self.paths.store),
      self.kind == 'shared' and '--enable-shared' or '--disable-shared',
      self.kind == 'shared' and '--disable-static' or '--enable-static',
    },
    cwd = self.paths.work
  }

  if config.args then
    for arg in iterator.values(config.args) do
      table.insert(sh.args, arg)
    end
  end

  self:sh(sh)
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
