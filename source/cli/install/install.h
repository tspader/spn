#ifndef SPN_INSTALL_INSTALL_H
#define SPN_INSTALL_INSTALL_H

#include "install/types.h"

spn_install_probe_t spn_install_probe(sp_mem_t mem);
spn_install_t       spn_install_execute(spn_install_probe_t* probe, spn_install_plan_t* plan);
spn_install_t       spn_install(sp_mem_t mem);

#endif
