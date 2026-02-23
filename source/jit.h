#ifndef SPN_JIT_H
#define SPN_JIT_H

typedef enum {
  JIT_NOACTION = 0,
  JIT_REGISTER_FN,
  JIT_UNREGISTER_FN
} jit_actions_t;

typedef struct jit_code_entry {
  struct jit_code_entry* next_entry;
  struct jit_code_entry* prev_entry;
  const char* symfile_addr;
  uint64_t symfile_size;
} spn_jit_entry_t;

typedef struct jit_descriptor {
  uint32_t version;
  uint32_t action_flag;
  struct jit_code_entry* relevant_entry;
  struct jit_code_entry* first_entry;
} spn_jit_desc_t;

void register_jit_code(const char *elf_data, size_t elf_size);

#endif
