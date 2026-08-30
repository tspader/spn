#include "install/install.h"

#if defined(SP_WIN32)
static spn_install_reg_t probe_registry_value(sp_mem_t mem, HKEY key, DWORD type, DWORD size, sp_str_t* out) {
  spn_install_reg_t kind = SPN_INSTALL_REG_NONE;
  switch (type) {
    case REG_SZ: kind = SPN_INSTALL_REG_SZ; break;
    case REG_EXPAND_SZ: kind = SPN_INSTALL_REG_EXPAND; break;
    default: return SPN_INSTALL_REG_OTHER;
  }
  if (size < sizeof(u16)) {
    return kind;
  }

  u16* buffer = (u16*)sp_mem_allocator_alloc(mem, size);
  if (RegQueryValueExW(key, L"Path", SP_NULLPTR, &type, (BYTE*)buffer, &size) != ERROR_SUCCESS) {
    return SPN_INSTALL_REG_OTHER;
  }
  u32 len = (u32)(size / sizeof(u16));
  while (len && !buffer[len - 1]) {
    len--;
  }
  sp_str_t value = sp_zero;
  if (sp_wtf16_to_wtf8(mem, (sp_wide_str_t) { .data = buffer, .len = len }, &value)) {
    return SPN_INSTALL_REG_OTHER;
  }
  *out = value;
  return kind;
}

static void probe_registry(sp_mem_t mem, spn_install_facts_t* facts) {
  HKEY key = SP_NULLPTR;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_QUERY_VALUE, &key)) {
    return;
  }

  DWORD type = 0;
  DWORD size = 0;
  if (RegQueryValueExW(key, L"Path", SP_NULLPTR, &type, SP_NULLPTR, &size) == ERROR_SUCCESS) {
    facts->registry.kind = probe_registry_value(mem, key, type, size, &facts->registry.path);
  }
  RegCloseKey(key);
}
#endif

spn_install_facts_t spn_install_probe(sp_mem_t mem, spn_install_layout_t* layout) {
  spn_install_facts_t facts = sp_zero;
  facts.exe = sp_fs_get_exe_path(mem);

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch_for(mem);
  sp_for(it, layout->num_rc) {
    facts.rc[it].exists = sp_fs_is_file(layout->rc[it].path);
    if (facts.rc[it].exists) {
      sp_str_t content = sp_zero;
      if (!sp_io_read_file(scratch.mem, layout->rc[it].path, &content)) {
        facts.rc[it].has_line = sp_str_contains(content, layout->rc_line);
      }
    }
  }
  sp_mem_end_scratch(scratch);

  sp_da_for(layout->shadows, it) {
    if (sp_fs_is_target_file(layout->shadows[it])) {
      facts.shadow = layout->shadows[it];
      break;
    }
  }

#if defined(SP_WIN32)
  probe_registry(mem, &facts);
#endif
  return facts;
}
