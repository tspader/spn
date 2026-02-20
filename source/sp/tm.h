#ifndef SPN_SP_TM_H
#define SPN_SP_TM_H

#include "sp.h"

bool sp_tm_epoch_gt(sp_tm_epoch_t a, sp_tm_epoch_t b);
bool sp_tm_epoch_ge(sp_tm_epoch_t a, sp_tm_epoch_t b);
bool sp_tm_epoch_lt(sp_tm_epoch_t a, sp_tm_epoch_t b);
bool sp_tm_epoch_le(sp_tm_epoch_t a, sp_tm_epoch_t b);
bool sp_tm_epoch_eq(sp_tm_epoch_t a, sp_tm_epoch_t b);
sp_tm_epoch_t sp_tm_epoch_min(sp_tm_epoch_t a, sp_tm_epoch_t b);
sp_tm_epoch_t sp_tm_epoch_max(sp_tm_epoch_t a, sp_tm_epoch_t b);

#endif
