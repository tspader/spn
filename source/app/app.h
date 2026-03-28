#ifndef SPN_APP_APP_H
#define SPN_APP_APP_H

#include "app/types.h"

void spn_app_write_manifest(spn_pkg_t* package, sp_str_t path);
spn_app_t spn_app_init_and_write(sp_str_t path, sp_str_t name, spn_app_init_mode_t mode);
void spn_app_update_lock_file(spn_app_t* app);
spn_err_t spn_app_resolve(spn_resolver_t* resolver);

sp_app_result_t spn_init(sp_app_t* app);
sp_app_result_t spn_poll(sp_app_t* app);
sp_app_result_t spn_update(sp_app_t* app);
void spn_deinit(sp_app_t* app);

#endif
