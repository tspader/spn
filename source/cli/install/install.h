#ifndef SPN_INSTALL_INSTALL_H
#define SPN_INSTALL_INSTALL_H

#include "install/types.h"

spn_install_os_t     spn_install_os_host();
bool                 spn_install_path_equal(spn_install_os_t os, sp_str_t a, sp_str_t b);
spn_install_layout_t spn_install_resolve(sp_mem_t mem, spn_install_os_t os, sp_env_t* env);
spn_install_facts_t  spn_install_probe(sp_mem_t mem, spn_install_layout_t* layout);
spn_install_plan_t   spn_install_plan(sp_mem_t mem, spn_install_layout_t* layout, spn_install_facts_t* facts);
spn_install_result_t spn_install_execute(spn_install_plan_t* plan);
spn_install_msgs_t   spn_install_report(spn_install_layout_t* layout, spn_install_facts_t* facts, spn_install_plan_t* plan, spn_install_result_t* result);

#endif
