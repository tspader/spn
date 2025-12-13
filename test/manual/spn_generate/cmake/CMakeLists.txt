cmake_minimum_required(VERSION 3.10)
project(spn_generate_test C)

# Include the generated spn.cmake
include(${CMAKE_CURRENT_SOURCE_DIR}/spn.cmake)

add_executable(main main.c)

# Parse flags into lists
separate_arguments(SPN_INCLUDES_LIST UNIX_COMMAND "${SPN_INCLUDES}")
separate_arguments(SPN_LIB_INCLUDES_LIST UNIX_COMMAND "${SPN_LIB_INCLUDES}")
separate_arguments(SPN_LIBS_LIST UNIX_COMMAND "${SPN_LIBS}")
separate_arguments(SPN_SYSTEM_LIBS_LIST UNIX_COMMAND "${SPN_SYSTEM_LIBS}")
separate_arguments(SPN_RPATH_LIST UNIX_COMMAND "${SPN_RPATH}")

# Apply compile flags
target_compile_options(main PRIVATE ${SPN_INCLUDES_LIST})

# Apply link flags
target_link_libraries(main PRIVATE ${SPN_LIB_INCLUDES_LIST} ${SPN_LIBS_LIST} ${SPN_SYSTEM_LIBS_LIST})
target_link_options(main PRIVATE ${SPN_RPATH_LIST})
