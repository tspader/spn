#include "intern/intern.h"

#define SP_INTERN_INDEX_MIN_CAPACITY 16

SP_PRIVATE u32 sp_intern_default_hash(sp_str_t str) {
  return (u32)sp_hash_bytes(str.data, str.len, 0);
}

SP_PRIVATE u32 sp_intern_index_find(sp_intern_index_t* index, sp_str_t str, u32 hash) {
  u32 mask = index->capacity - 1;
  u32 slot = hash & mask;

  while (index->slots[slot].data) {
    sp_intern_slot_t* entry = &index->slots[slot];
    if (entry->hash == hash && entry->len == str.len && sp_mem_is_equal(entry->data, str.data, str.len)) {
      break;
    }
    slot = (slot + 1) & mask;
  }

  return slot;
}

SP_PRIVATE void sp_intern_index_insert(sp_intern_index_t* index, sp_intern_slot_t entry) {
  u32 mask = index->capacity - 1;
  u32 slot = entry.hash & mask;

  while (index->slots[slot].data) {
    slot = (slot + 1) & mask;
  }

  index->slots[slot] = entry;
  index->count += 1;
}

SP_PRIVATE void sp_intern_index_grow(sp_intern_index_t* index) {
  u32 old_capacity = index->capacity;
  sp_intern_slot_t* old_slots = index->slots;

  index->capacity = old_capacity * 2;
  index->count = 0;
  index->slots = sp_alloc_n(index->mem, sp_intern_slot_t, index->capacity);

  sp_for(it, old_capacity) {
    sp_intern_slot_t entry = old_slots[it];
    if (entry.data) {
      sp_intern_index_insert(index, entry);
    }
  }

  sp_free(index->mem, old_slots, (u64)old_capacity * sizeof(sp_intern_slot_t));
}

SP_PRIVATE void sp_intern_index_put(sp_intern_index_t* index, sp_intern_slot_t entry, u32 slot) {
  if ((index->count + 1) * 4 > index->capacity * 3) {
    sp_intern_index_grow(index);
    sp_intern_index_insert(index, entry);
    return;
  }

  index->slots[slot] = entry;
  index->count += 1;
}

sp_intern_t* sp_intern_new(sp_mem_t mem) {
  return sp_intern_new_ex(mem, sp_intern_default_hash);
}

sp_intern_t* sp_intern_new_ex(sp_mem_t mem, sp_intern_hash_fn_t hash) {
  sp_intern_t* intern = sp_alloc_type(mem, sp_intern_t);
  sp_intern_init_ex(intern, mem, hash);
  return intern;
}

void sp_intern_init(sp_intern_t* intern, sp_mem_t mem) {
  sp_intern_init_ex(intern, mem, sp_intern_default_hash);
}

void sp_intern_init_ex(sp_intern_t* intern, sp_mem_t mem, sp_intern_hash_fn_t hash) {
  if (!intern) return;

  intern->mem = mem;
  intern->hash = hash;
  intern->next_id = SP_INTERN_INVALID_ID + 1;
  intern->data = sp_mem_arena_new_ex(mem, 4096, 1);
  sp_mutex_init(&intern->mutex, SP_MUTEX_PLAIN);

  intern->index = (sp_intern_index_t) {
    .mem = mem,
    .capacity = SP_INTERN_INDEX_MIN_CAPACITY,
    .slots = sp_alloc_n(mem, sp_intern_slot_t, SP_INTERN_INDEX_MIN_CAPACITY),
  };

  sp_mem_t data = sp_mem_arena_as_allocator(intern->data);
  sp_alloc(data, 1);
  const c8* empty = sp_str_to_cstr(data, sp_str_lit(""));
  sp_intern_index_insert(&intern->index, (sp_intern_slot_t) {
    .hash = hash(sp_str_lit("")),
    .len = 0,
    .id = SP_INTERN_INVALID_ID,
    .data = empty,
  });
}

sp_intern_id_t sp_intern_get_or_insert(sp_intern_t* intern, sp_str_t str) {
  if (!intern) return SP_INTERN_INVALID_ID;
  if (sp_str_empty(str)) return SP_INTERN_INVALID_ID;

  u32 hash = intern->hash(str);
  sp_mutex_lock(&intern->mutex);
  sp_intern_index_t* index = &intern->index;
  u32 slot = sp_intern_index_find(index, str, hash);
  sp_intern_id_t id;
  if (index->slots[slot].data) {
    id = index->slots[slot].id;
  }
  else {
    const c8* cstr = sp_str_to_cstr(sp_mem_arena_as_allocator(intern->data), str);
    id = intern->next_id++;
    sp_intern_index_put(index, (sp_intern_slot_t) { .hash = hash, .len = str.len, .id = id, .data = cstr }, slot);
  }
  sp_mutex_unlock(&intern->mutex);
  return id;
}

sp_intern_id_t sp_intern_get(sp_intern_t* intern, sp_str_t str) {
  if (!intern) return SP_INTERN_INVALID_ID;
  if (sp_str_empty(str)) return SP_INTERN_INVALID_ID;
  u32 hash = intern->hash(str);
  sp_mutex_lock(&intern->mutex);
  sp_intern_index_t* index = &intern->index;
  sp_intern_id_t id = index->slots[sp_intern_index_find(index, str, hash)].id;
  sp_mutex_unlock(&intern->mutex);
  return id;
}

bool sp_intern_is_equal(sp_intern_t* intern, sp_intern_id_t a, sp_intern_id_t b) {
  (void)intern;
  return a == b;
}

sp_str_t sp_intern_get_str(sp_intern_t* intern, sp_str_t str) {
  if (!intern) return SP_INTERN_INVALID_STR;
  if (sp_str_empty(str)) return sp_str_lit("");

  u32 hash = intern->hash(str);
  sp_mutex_lock(&intern->mutex);
  sp_intern_index_t* index = &intern->index;
  const c8* data = index->slots[sp_intern_index_find(index, str, hash)].data;
  sp_mutex_unlock(&intern->mutex);
  if (!data) return SP_INTERN_INVALID_STR;
  return sp_str(data, str.len);
}

sp_str_t sp_intern_get_or_insert_str(sp_intern_t* intern, sp_str_t str) {
  if (!intern) return SP_INTERN_INVALID_STR;
  if (sp_str_empty(str)) return sp_str_lit("");

  u32 hash = intern->hash(str);
  sp_mutex_lock(&intern->mutex);
  sp_intern_index_t* index = &intern->index;
  u32 slot = sp_intern_index_find(index, str, hash);
  const c8* cstr = index->slots[slot].data;
  if (!cstr) {
    cstr = sp_str_to_cstr(sp_mem_arena_as_allocator(intern->data), str);
    sp_intern_index_put(index, (sp_intern_slot_t) { .hash = hash, .len = str.len, .id = intern->next_id++, .data = cstr }, slot);
  }
  sp_mutex_unlock(&intern->mutex);

  return sp_str(cstr, str.len);
}

bool sp_intern_is_interned(sp_intern_t* intern, sp_str_t str) {
  if (!intern) return false;
  if (sp_str_empty(str)) return true;
  u32 hash = intern->hash(str);
  sp_mutex_lock(&intern->mutex);
  sp_intern_index_t* index = &intern->index;
  bool interned = index->slots[sp_intern_index_find(index, str, hash)].data != SP_NULLPTR;
  sp_mutex_unlock(&intern->mutex);
  return interned;
}

bool sp_intern_is_equal_str(sp_intern_t* intern, sp_str_t a, sp_str_t b) {
  if (!intern) return false;

  // Empty strings are never stored; treat them as equal only to each other.
  if (sp_str_empty(a) || sp_str_empty(b)) return a.len == b.len;

  sp_mutex_lock(&intern->mutex);
  sp_intern_index_t* index = &intern->index;
  const c8* ia = index->slots[sp_intern_index_find(index, a, intern->hash(a))].data;
  const c8* ib = index->slots[sp_intern_index_find(index, b, intern->hash(b))].data;
  sp_mutex_unlock(&intern->mutex);
  return ia && ib && ia == ib;
}

u64 sp_intern_size(sp_intern_t* intern) {
  if (!intern) return 0;
  sp_mutex_lock(&intern->mutex);
  u64 count = intern->index.count;
  sp_mutex_unlock(&intern->mutex);
  return count;
}

u64 sp_intern_bytes_used(sp_intern_t* intern) {
  if (!intern) return 0;
  sp_mutex_lock(&intern->mutex);
  u64 bytes = sp_mem_arena_bytes_used(intern->data);
  sp_mutex_unlock(&intern->mutex);
  return bytes;
}

u64 sp_intern_bytes_allocated(sp_intern_t* intern) {
  if (!intern) return 0;
  sp_mutex_lock(&intern->mutex);
  u64 bytes = sp_mem_arena_capacity(intern->data);
  sp_mutex_unlock(&intern->mutex);
  return bytes;
}

u64 sp_intern_metadata_bytes(sp_intern_t* intern) {
  if (!intern) return 0;
  sp_mutex_lock(&intern->mutex);
  u64 bytes = (u64)intern->index.capacity * sizeof(sp_intern_slot_t);
  sp_mutex_unlock(&intern->mutex);
  return bytes;
}
