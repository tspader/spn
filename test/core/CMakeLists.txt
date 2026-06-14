add_executable(load
  ${SRC}/ctx/ctx.c
  ${SRC}/intern/intern.c
  ${SRC}/enum/enum.c
  ${SRC}/event/event.c
  ${SRC}/external/tom.c
  ${SRC}/pkg/id.c
  ${SRC}/pkg/load.c
  ${SRC}/pkg/mutate.c
  ${SRC}/semver/compare.c
  ${SRC}/semver/convert.c
  ${SRC}/semver/parser.c
  ${SRC}/target/mutate.c
  manifest.c
  ${TEST_TOOLS}/harness.c
  ${TEST_TOOLS}/tmp.c
  ${TEST_TOOLS}/toml.c
)
spn_test_binary(load)
add_test(NAME load COMMAND "$<TARGET_FILE:load>")

add_executable(lock
  ${SRC}/lock/lock.c
  ${SRC}/external/tom.c
  ${SRC}/enum/enum.c
  ${SRC}/semver/convert.c
  ${SRC}/semver/parser.c
  ${SRC}/sp/str.c
  lock.c
  ${TEST_TOOLS}/harness.c
  ${TEST_TOOLS}/tmp.c
  ${TEST_TOOLS}/git.c
  ${TEST_TOOLS}/toml.c
)
spn_test_binary(lock)
add_test(NAME lock COMMAND "$<TARGET_FILE:lock>")

add_executable(graph
  ${SRC}/graph/graph.c
  ${SRC}/sp/tm.c
  graph.c
  ${TEST_TOOLS}/tmp.c
)
spn_test_binary(graph)
add_test(NAME graph COMMAND "$<TARGET_FILE:graph>")

add_executable(lazy
  ${SRC}/log/lazy/lazy.c
  lazy.c
  ${TEST_TOOLS}/tmp.c
)
spn_test_binary(lazy)
add_test(NAME lazy COMMAND "$<TARGET_FILE:lazy>")

add_executable(stable_map
  stable_map.c
)
spn_test_binary(stable_map)

add_executable(ordered_map
  ordered_map.c
)
spn_test_binary(ordered_map)

add_executable(triple
  ${SRC}/enum/enum.c
  ${SRC}/triple/triple.c
  triple.c
)
spn_test_binary(triple)
add_test(NAME triple COMMAND "$<TARGET_FILE:triple>")

add_executable(jtd
  ${CMAKE_SOURCE_DIR}/tools/jtd.c
  jtd/main.c
)
spn_test_binary(jtd)
target_include_directories(jtd PRIVATE ${CMAKE_SOURCE_DIR}/tools ${CMAKE_CURRENT_SOURCE_DIR}/jtd)
target_compile_definitions(jtd PRIVATE JTD_TEST_JSON_DIR="${CMAKE_CURRENT_SOURCE_DIR}/jtd/json")
target_link_libraries(jtd PRIVATE yyjson)
add_test(NAME jtd COMMAND "$<TARGET_FILE:jtd>")

add_subdirectory(resolver)
add_subdirectory(index)
add_subdirectory(publish)
add_subdirectory(git)
