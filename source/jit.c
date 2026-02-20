#include "jit.h"

void register_jit_code(const char *elf_data, size_t elf_size) {
  // struct jit_code_entry *entry = sp_alloc_type(struct jit_code_entry);
  // entry->symfile_addr = elf_data;
  // entry->symfile_size = elf_size;
  //
  // entry->next_entry = __jit_debug_descriptor.first_entry;
  // entry->prev_entry = NULL;
  // if (entry->next_entry)
  //   entry->next_entry->prev_entry = entry;
  //
  // __jit_debug_descriptor.first_entry = entry;
  // __jit_debug_descriptor.relevant_entry = entry;
  // __jit_debug_descriptor.action_flag = JIT_REGISTER_FN;
  //
  // __jit_debug_register_code();
}
