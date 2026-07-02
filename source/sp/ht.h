#ifndef SPN_SP_HT_H
#define SPN_SP_HT_H

#define sp_ht_collect_keys(ht, da) \
  do { \
    sp_ht_for_kv((ht), __it) { \
      sp_da_push((da), *__it.key); \
    } \
  } while (0)

#define sp_ht_get_key_index_mt(__HT, __PTR) sp_ht_get_key_index_fn((void**)&((__HT)->data), (void*)&((__PTR)), (__HT)->info)

#define sp_ht_get_mt(__HT, __PTR, __INDEX) \
  ( \
    (__HT) == SP_NULLPTR ? SP_NULLPTR : \
    ((__INDEX) = sp_ht_get_key_index_mt(__HT, __PTR), \
    ((__INDEX) != SP_HT_INVALID_INDEX ? &(__HT)->data[(__INDEX)].val : NULL)) \
  )

#endif
