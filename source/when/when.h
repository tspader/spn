#ifndef SPN_WHEN_WHEN_H
#define SPN_WHEN_WHEN_H

#include "sp.h"
#include "spn.h"
#include "when/types.h"

spn_option_value_t spn_option_value_none(void);
spn_option_value_t spn_option_value_bool(bool value);
spn_option_value_t spn_option_value_str(sp_str_t value);
bool               spn_option_value_equal(spn_option_value_t a, spn_option_value_t b);
sp_str_t           spn_option_value_to_str(sp_mem_t mem, spn_option_value_t value);

void spn_when_env_init(sp_mem_t mem, spn_when_env_t* env);
void spn_when_env_set(spn_when_env_t* env, sp_str_t key, spn_option_value_t value);
void spn_when_env_set_facts(spn_when_env_t* env, spn_os_t os, spn_arch_t arch, spn_abi_t abi, spn_build_mode_t mode);

// A clause on a key absent from the env fails the predicate, negated or not;
// validation upstream guarantees every key is a fact or a declared option.
bool     spn_when_eval(const spn_when_t* when, spn_when_env_t* env);
sp_str_t spn_when_to_str(sp_mem_t mem, const spn_when_t* when);

bool spn_option_value_ok(const spn_option_info_t* option, spn_option_value_t value);

// First default whose when passes wins; none matching yields a NONE value.
spn_option_value_t spn_option_resolve(const spn_option_info_t* option, spn_when_env_t* env);

#endif
