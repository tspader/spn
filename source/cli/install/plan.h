#ifndef SPN_INSTALL_PLAN_H
#define SPN_INSTALL_PLAN_H

#include "install/types.h"

spn_install_layout_t  spn_install_resolve(sp_mem_t mem, spn_install_os_t os, sp_env_t* env);
spn_install_choices_t spn_install_choices(spn_install_layout_t* layout);
spn_install_plan_t    spn_install_plan(sp_mem_t mem, spn_install_layout_t* layout, spn_install_facts_t* facts, spn_install_choices_t* choices);
spn_install_msgs_t    spn_install_report(spn_install_layout_t* layout, spn_install_facts_t* facts, spn_install_plan_t* plan, spn_install_result_t* result);
bool                  spn_install_shadowed(spn_install_layout_t* layout, spn_install_facts_t* facts);

#endif
