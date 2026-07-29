#ifndef SPN_THREAD_POOL_THREAD_POOL_H
#define SPN_THREAD_POOL_THREAD_POOL_H

#include "thread_pool/types.h"

void      spn_thread_pool_submit(spn_thread_pool_executor_t* ex, spn_thread_pool_job_t job);
spn_thread_pool_job_t spn_thread_pool_poll(spn_thread_pool_executor_t* ex);
spn_thread_pool_job_t spn_thread_pool_try_poll(spn_thread_pool_executor_t* ex);

void      spn_thread_pool_init(spn_thread_pool_t* pool, sp_mem_t mem, spn_thread_pool_config_t config);
void      spn_thread_pool_deinit(spn_thread_pool_t* pool);
bool      spn_thread_pool_step(spn_thread_pool_t* pool);
u32       spn_thread_pool_pending(spn_thread_pool_t* pool);

#endif
