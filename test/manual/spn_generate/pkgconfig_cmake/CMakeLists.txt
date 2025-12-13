cmake_minimum_required(VERSION 3.10)
project(spn_generate_test C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SPN REQUIRED spn)

add_executable(main main.c)
target_compile_options(main PRIVATE ${SPN_CFLAGS})
target_link_libraries(main PRIVATE ${SPN_LDFLAGS})
