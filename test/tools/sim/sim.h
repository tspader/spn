#ifndef SP_SIM_H
#define SP_SIM_H

#include "sp.h"

typedef struct sp_sim_inode sp_sim_inode_t;
typedef struct sp_sim_fd sp_sim_fd_t;

typedef struct {
  sp_str_t path;
  u64 sys;
} sp_sim_event_t;

typedef enum {
  SP_SIM_FAULT_NONE,
  SP_SIM_FAULT_RANDOM,
  SP_SIM_FAULT_AT,
  SP_SIM_FAULT_FROM,
} sp_sim_fault_kind_t;

typedef struct {
  sp_mem_t mem;
  sp_ht(sp_str_t, sp_sim_inode_t*) nodes;
  sp_ht(sp_str_t, sp_sim_inode_t*) shadow;
  sp_da(sp_sim_fd_t) fds;
  sp_da(sp_sim_event_t) events;
  sp_sys_timespec_t clock;
  u64 granularity;
  u64 autotick;
  u64 ids;
  u64 syscalls;
  sp_sim_fault_kind_t fault_kind;
  u64 fault_state;
  u64 fault_den;
  u64 fault_next;
  u64 fail_points;
  u64 faults;
  sp_da(u64) fault_log;
  u64 crash_at;
  bool crashed;
  const sp_sys_vtable_t* prev;
} sp_sim_t;

void sp_sim_init(sp_sim_t* sim, sp_mem_t mem);
void sp_sim_install(sp_sim_t* sim);
void sp_sim_remove(sp_sim_t* sim);

void sp_sim_advance(sp_sim_t* sim, u64 ns);
bool sp_sim_touch(sp_sim_t* sim, sp_str_t path);
bool sp_sim_stealth_write(sp_sim_t* sim, sp_str_t path, sp_str_t bytes);

void sp_sim_fault_eio(sp_sim_t* sim, u64 seed, u64 denominator);
void sp_sim_fault_at(sp_sim_t* sim, u64 nth);
void sp_sim_fault_from(sp_sim_t* sim, u64 nth);
void sp_sim_fault_crash(sp_sim_t* sim, u64 after);
void sp_sim_fault_clear(sp_sim_t* sim);
bool sp_sim_crash_restore(sp_sim_t* sim);

#endif
