#include "sp.h"

#define ut (*utest_fixture)
#define ur (*utest_result)

typedef struct {
  sp_str_t root;
  sp_str_t build;
  sp_str_t bin;
  sp_str_t test;
} sp_test_file_paths_t;

typedef struct {
  sp_test_file_paths_t paths;
} sp_test_file_manager_t;

typedef struct {
  sp_str_t path;
  sp_str_t content;
} sp_test_file_config_t;

void sp_test_file_manager_init(sp_test_file_manager_t* manager);
sp_str_t sp_test_file_path(sp_test_file_manager_t* manager, sp_str_t name);
void sp_test_file_create_ex(sp_test_file_config_t config);
sp_str_t sp_test_file_create_empty(sp_test_file_manager_t* manager, sp_str_t path);
void sp_test_file_manager_cleanup(sp_test_file_manager_t* manager);

#if defined(SP_TEST_IMPLEMENTATION)
void sp_test_file_manager_init(sp_test_file_manager_t* manager) {
  manager->paths.bin = sp_os_get_executable_path();
  manager->paths.build = sp_os_parent_path(manager->paths.bin);
  manager->paths.root = sp_os_parent_path(manager->paths.build);
  manager->paths.test = sp_os_join_path(manager->paths.build, sp_str_lit("test"));

  sp_os_remove_directory(manager->paths.test);
  sp_os_create_directory(manager->paths.test);
}

sp_str_t sp_test_file_path(sp_test_file_manager_t* manager, sp_str_t name) {
  return sp_os_join_path(manager->paths.test, name);
}

void sp_test_file_create_ex(sp_test_file_config_t config) {
  sp_os_remove_file(config.path);

  sp_io_stream_t stream = sp_io_from_file(config.path, SP_IO_MODE_WRITE);
  SP_ASSERT(stream.file.fd != 0);

  if (config.content.len > 0) {
    u64 bytes_written = sp_io_write(&stream, config.content.data, config.content.len);
    SP_ASSERT(bytes_written == config.content.len);
  }

  sp_io_close(&stream);
}

sp_str_t sp_test_file_create_empty(sp_test_file_manager_t* manager, sp_str_t relative) {
  sp_str_t path = sp_test_file_path(manager, relative);
  sp_test_file_create_ex((sp_test_file_config_t) {
    .path = path,
    .content = SP_LIT(""),
  });

  return path;
}

void sp_test_file_manager_cleanup(sp_test_file_manager_t* manager) {
  sp_os_remove_directory(manager->paths.test);
}
#endif
