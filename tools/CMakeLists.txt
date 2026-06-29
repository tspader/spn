add_executable(embed embed.c)
set_target_properties(embed PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(embed PRIVATE ${STORE}/include)
target_link_libraries(embed PRIVATE m)
add_dependencies(embed headers)

add_custom_command(
  OUTPUT ${EMBED_OBJ} ${EMBED_HDR}
  COMMAND $<TARGET_FILE:embed>
    --header ${EMBED_HDR}
    ${EMBED_OBJ}
    ${STORE}/lib/tcc
    ${CMAKE_SOURCE_DIR}/include/spn.h=include/spn.h
    ${CMAKE_SOURCE_DIR}/source/toolchain/toolchains.json=toolchains.json
  DEPENDS embed tcc ${CMAKE_SOURCE_DIR}/include/spn.h ${CMAKE_SOURCE_DIR}/source/toolchain/toolchains.json
  COMMENT "spn.embed.o"
  VERBATIM)
add_custom_target(spn_embed ALL DEPENDS ${EMBED_OBJ} ${EMBED_HDR})

add_executable(toolchain toolchain.c)
set_target_properties(toolchain PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${STORE}/bin)
target_include_directories(toolchain PRIVATE ${STORE}/include)
target_link_libraries(toolchain PRIVATE yyjson m)
add_dependencies(toolchain headers)

add_executable(yyjson_stub yyjson_stub.c)
set_target_properties(yyjson_stub PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(yyjson_stub PRIVATE yyjson)

add_executable(jtd_gen codegen/main.c codegen/walk.c codegen/render.c jtd.c)
set_target_properties(jtd_gen PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(jtd_gen PRIVATE ${STORE}/include ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/source/sp)
target_link_libraries(jtd_gen PRIVATE yyjson m)
add_dependencies(jtd_gen headers)

set(GEN_DIR ${CMAKE_SOURCE_DIR}/source/codegen/gen)
file(MAKE_DIRECTORY ${GEN_DIR})

set(SCHEMA ${CMAKE_SOURCE_DIR}/source/codegen/schemas.jtd.json)
file(GLOB_RECURSE JTD_GEN_TEMPLATES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/templates/*.tmpl)
add_custom_command(
  OUTPUT
    ${GEN_DIR}/types.gen.h
    ${GEN_DIR}/manifest.gen.h
    ${GEN_DIR}/manifest.gen.c
    ${GEN_DIR}/toolchains.gen.h
    ${GEN_DIR}/toolchains.gen.c
  COMMAND ${CMAKE_COMMAND} -E make_directory ${GEN_DIR}
  COMMAND $<TARGET_FILE:jtd_gen>
    ${SCHEMA}
    ${GEN_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/templates
  DEPENDS jtd_gen ${SCHEMA} ${JTD_GEN_TEMPLATES}
  COMMENT "codegen: types.gen.h, {manifest,toolchains}.gen.{h,c}"
  VERBATIM)
add_custom_target(spn_codegen_gen ALL DEPENDS
  ${GEN_DIR}/types.gen.h
  ${GEN_DIR}/manifest.gen.h
  ${GEN_DIR}/manifest.gen.c
  ${GEN_DIR}/toolchains.gen.h
  ${GEN_DIR}/toolchains.gen.c)
