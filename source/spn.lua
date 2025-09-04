local ffi = require('ffi')

local spn = {
  recipes = {},
  internal = {},
  bar = 'bar is from the master table'
}

function spn.log(fmt, ...)
  print(string.format(fmt, ...))
end

function spn.recipes.single_header(options)
  return options
end

function spn.internal.read(recipe)
  print('reading from lua')
  local str = ffi.new('sp_str_t', #recipe, recipe)
  spn.log('%s: %s', recipe, 'bazzz')
end

function spn.internal.init()
  print('hello big one')
  spn.bar = 'bar is from init()'

  header = ffi.cdef([[
    typedef int8_t   s8;
    typedef int16_t  s16;
    typedef int32_t  s32;
    typedef int64_t  s64;
    typedef uint8_t  u8;
    typedef uint16_t u16;
    typedef uint32_t u32;
    typedef uint64_t u64;
    typedef float    f32;
    typedef double   f64;
    typedef char     c8;
    typedef wchar_t  c16;
    typedef size_t   sp_size_t;

    typedef struct SDL_Process SDL_Process;
    typedef u32 SDL_PropertiesID;
    typedef void* sp_opaque_ptr;

    typedef struct {
      u32 len;
      const c8* data;
    } sp_str_t;

    typedef struct {
      sp_str_t source;
      sp_str_t recipe;
    } spn_dep_paths_t;

    typedef struct {
      sp_str_t name;
      sp_str_t url;
      spn_dep_paths_t paths;
      sp_str_t* libs;
    } spn_dep_info_t;

    void sp_os_log(sp_str_t message);

    unsigned int foo(unsigned int bar);
  ]])

  local ffi = require('ffi')
  local bar = ffi.C.foo(2000)
  print('in lua, bar is: ' .. tostring(bar))
  spn.internal.read('quz')
end

return spn


