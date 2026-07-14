#ifndef SP_SIM_H
#define SP_SIM_H

#include "sp.h"

typedef struct sp_sim_inode sp_sim_inode_t;
typedef struct sp_sim_fd sp_sim_fd_t;

typedef struct {
  sp_mem_t mem;
  sp_ht(sp_str_t, sp_sim_inode_t*) nodes;
  sp_da(sp_sim_fd_t) fds;
  sp_sys_timespec_t clock;
  u64 ids;
  u64 syscalls;
  const sp_sys_vtable_t* prev;
} sp_sim_t;

void sp_sim_init(sp_sim_t* sim, sp_mem_t mem);
void sp_sim_install(sp_sim_t* sim);
void sp_sim_remove(sp_sim_t* sim);

bool sp_sim_touch(sp_sim_t* sim, sp_str_t path);
bool sp_sim_stealth_write(sp_sim_t* sim, sp_str_t path, sp_str_t bytes);

#endif
