# overview
- sp.h is a single-header C standard library replacement
- the first ~100 lines is a reference guide with searchable tags for each module

# rules
- Never comment any code, under any circumstances. Code with comments will be rejected outright.
- Never use `malloc`, `calloc`, or `realloc`; use `sp_alloc` (which zero initializes)
- Unless explicitly interfacing with an existing C API, never use `const char*`; use `sp_str_t` (pointer + length)
- Never use `strcmp`, `strlen`, or any `string.h` functions with `sp_str_t`; use `sp_str_*`
- Never use `strcmp`, `strlen`, or any `string.h` functions with `const char*`; use `sp_cstr_*`
- Always use `SP_ZERO_INITIALIZE()`. When you need a type, use `SP_ZERO_STRUCT(T)`
- Always use `sp_da(T)` and `sp_ht(T)` for dynamic arrays and hash maps (`sp_da_*` and `sp_ht_*`)
- Always use `sp_da_for(arr, it)` and `sp_ht_for(ht, it)` to iterate sp_da and sp_ht
- Never check `str.len > 0`; always use `!sp_str_empty(str)`
- Always use C99 designated initializers for struct literals when possible
- Always use short literal types (`s32`, `u8`, `c8`, `const c8*`)
- Never use `printf` family; always use `SP_LOG()`
- Always use `sp_carr_for()` when iterating a C array
- Always explicitly handle all enum cases in a switch statement; do not use `default`
  - `default` is only acceptable when there are many cases, but only a few are handled differently
- Prefer to use `for` macros when possible
    - Use `sp_for(it, n)` instead of `for (int it = 0; it < n; it++)`
    - Use `sp_for_range(it, low, high)` instead of `for (int it = low; it < high; it++)`

- Never use `NULL`; always use `SP_NULL` or `SP_NULLPTR` (identical, just semantic aliases)
- Never use `make`; the included Makefile is strictly for debugging. Instead, use `spn`

## searching
Always use the following pattern when searching for code in `sp.h`:
- Grep for `@modules` in `sp.h`, including ~50 lines of context. This will list every module.
- For each module you may need:
  - Grep for `@$(module)` for the location of the module in the header
  - Read from that location until you reach the end of the module
    - Usually, less than 100 lines
  - Add any functions you need more info on to a Todo
- For each function in your list:
  - If you have an LSP:
    - Use LSP tooling to read the implementation
  - If you do not have an LSP:
    - Grep for `$(return) $(fn)` to find the implementation; e.g. `sp_tls_rt_t* sp_tls_rt_get`
    - Read the implementation

## patterns
### Comments
Never comment your code. Ever. Code with comments will be rejected outright.

### Initialization
```c
// Always zero-initialize structs
sp_str_builder_t builder = SP_ZERO_INITIALIZE();
sp_dynamic_array_t arr = SP_ZERO_INITIALIZE();
```

### String Handling
```c
// Create strings
sp_str_t literal = sp_str_lit("hello");     // Compile-time string literal
sp_str_t view = sp_str_view(some_char_ptr); // Runtime C string (calculates length)
sp_str_t copy = sp_str_from_cstr("hello");  // Allocates and copies
const char* cstr = sp_str_to_cstr(str);
```

### Dynamic Arrays (stb-style)
```c
sp_da(int) numbers = SP_NULLPTR;
sp_da_push(numbers, 42);
sp_da_push(numbers, 100);

sp_da_for(numbers, i) {
  SP_LOG("numbers[{}] = {}", SP_FMT_U32(i), SP_FMT_S32(numbers[i]));
}

u32 count = sp_da_size(numbers);
u32 capacity = sp_da_capacity(numbers);
```

### Hash Tables (stb-style)
```c
struct {
  sp_ht(s32, s32) scalar;
  sp_str_ht(s32) str;
  sp_cstr_ht(s32) cstr;
} ht = SP_ZERO_INITIALIZE();

// no initialization needed
sp_ht_insert(ht.scalar, 69, 420);
bool exists = sp_ht_key_exists(ht.scalar, 69);
s32* value = sp_ht_getp(ht.scalar, 69);

// use sp_str_ht_insert(), not sp_ht_insert(); everything else is the same
sp_str_ht_insert(ht.str, sp_str_lit("answer"), 42);
sp_cstr_ht_insert(ht.str, "answer", 42);

sp_ht_for(ht.str, it) {
  sp_str_t key = *sp_ht_it_getkp(map, it);
  s32 value = *sp_ht_it_getp(map, it);
}

sp_ht_free(ht.scalar)
```

### Formatting and Logging
```c
// Type-safe formatting with color support
SP_LOG(
  "Processing {:fg cyan} with {} {}",
  SP_FMT_STR(name),
  SP_FMT_U32(count),
  SP_FMT_CSTR("items")
);

sp_str_t msg = sp_format("Result: {}", SP_FMT_S32(42));

// Colors: :fg, :bg, :color
// Colors: black, red, green, yellow, blue, magenta, cyan, white
// Add 'bright' prefix for bright variants
```

### Switch Statements
```c
// prefer switch statements to if/else chains
// always use braces
// always handle all cases
switch (state) {
  case STATE_IDLE: {
    break;
  }
  case STATE_RUNNING: {
    break;
  }
}
```

### Error Handling
```c
// Return an enum for recoverable errors (consumer app may have their own error type)
sp_err_t load_config(sp_str_t path, config_t* config) {
  if (!sp_os_does_path_exist(path)) {
    SP_LOG("Config not found: {}", SP_FMT_STR(path));
    return SP_ERR_WHATEVER;
  }

  return SP_ERR_OK;
}

void process_array(int* arr, u32 size) {
  if (!arr) return;
}
```
