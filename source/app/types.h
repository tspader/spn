#ifndef SPN_APP_TYPES_H
#define SPN_APP_TYPES_H

#include "sp.h"
#include "spn.h"

#include "lock/types.h"
#include "resolve/types.h"
#include "session/types.h"


// @spader @nuke
typedef struct {
  sp_str_t lock;
  sp_str_t manifest;
} spn_app_paths_t;

struct spn_app_t {
  spn_app_paths_t paths;
  spn_pkg_info_t package;
  sp_opt(spn_lock_file_t) lock;
  spn_session_t session;
};

typedef enum {
  SPN_APP_INIT_NORMAL,
  SPN_APP_INIT_BARE,
} spn_app_init_mode_t;

extern spn_app_t app;

#endif
