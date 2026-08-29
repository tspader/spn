#ifndef INSTALLER_RENDER_H
#define INSTALLER_RENDER_H

#include "sp.h"

typedef enum {
  INSTALLER_OK = 0,
  INSTALLER_ERR_IO,
  INSTALLER_ERR_TEMPLATES,
  INSTALLER_ERR_RENDER,
  INSTALLER_ERR_FIELDS,
  INSTALLER_ERR_SHA,
  INSTALLER_ERR_ASSET,
  INSTALLER_ERR_PAIRING,
  INSTALLER_ERR_DUPLICATE,
  INSTALLER_ERR_EMPTY,
} installer_err_t;

typedef struct {
  sp_str_t shasums;
  sp_str_t templates;
  sp_str_t out;
  sp_str_t version;
  sp_str_t tag;
  sp_str_t repo;
} installer_config_t;

typedef struct {
  installer_err_t err;
  u32 line;
  sp_str_t subject;
} installer_result_t;

installer_result_t installer_render(sp_mem_t mem, installer_config_t config);
sp_str_t installer_result_to_str(sp_mem_t mem, installer_result_t result);

#endif
