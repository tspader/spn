add_executable(render
  ${SRC}/compiler/driver.c
  ${SRC}/compiler/gnu.c
  ${SRC}/compiler/msvc.c
  ${SRC}/compiler/occ.c
  ${SRC}/reflect/json.c
  ${SRC}/codegen/gen/errors.gen.c
  ${SRC}/codegen/gen/events.gen.c
  ${SRC}/codegen/gen/source_deps.gen.c
  ${SRC}/codegen/json.c
  ${SRC}/codegen/loader.c
  ${SRC}/core/core.c
  ${SRC}/sp/io.c
  ${SRC}/ctx/ctx.c
  ${SRC}/enum/enum.c
  ${SRC}/error/error.c
  ${SRC}/error/json.c
  ${SRC}/event/event.c
  ${SRC}/log/lazy/lazy.c
  ${SRC}/external/tom.c
  ${SRC}/intern/intern.c
  ${SRC}/paths/paths.c
  ${SRC}/sp/str.c
  ${SRC}/triple/triple.c
  main.c
  cases.c
  ${TEST_TOOLS}/caps.c
  ${TEST_TOOLS}/env.c
  ${TEST_TOOLS}/fixture.c
  ${TEST_TOOLS}/toml.c
)
spn_test_binary(render)
target_include_directories(render PRIVATE
  ${SRC}/codegen/gen
  ${SRC}/external
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(render PRIVATE
  SPN_TEST_BIN=".build/store/${TRIPLE}${SPN_FLAVOR}/bin/spn${CMAKE_EXECUTABLE_SUFFIX}"
)
target_link_libraries(render PRIVATE yyjson)
add_dependencies(render spn spn_codegen_gen)
add_test(NAME render COMMAND "$<TARGET_FILE:render>" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
