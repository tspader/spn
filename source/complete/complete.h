#ifndef SPN_COMPLETE_H
#define SPN_COMPLETE_H

#include "sp.h"
#include "sp/sp_cli.h"

#include "pkg/types.h"

typedef struct {
  sp_cli_cmd_t* root;
  spn_pkg_info_t* pkg;
  const c8** words;
  u32 num_words;
  sp_io_writer_t* io;
} spn_complete_desc_t;

void spn_complete(spn_complete_desc_t desc);

#endif
