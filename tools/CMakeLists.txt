if(SPN_HOST_TOOLS)
  add_executable(embed IMPORTED GLOBAL)
  set_target_properties(embed PROPERTIES IMPORTED_LOCATION ${SPN_HOST_TOOLS}/embed)
  add_executable(jtd_gen IMPORTED GLOBAL)
  set_target_properties(jtd_gen PROPERTIES IMPORTED_LOCATION ${SPN_HOST_TOOLS}/jtd_gen)
else()
  add_executable(embed embed.c)
  set_target_properties(embed PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  target_include_directories(embed PRIVATE ${STORE}/include)
  target_link_libraries(embed PRIVATE m)
  add_dependencies(embed headers)

  add_executable(jtd_gen codegen/main.c codegen/extract.c codegen/render.c codegen/abi.c jtd.c)
  set_target_properties(jtd_gen PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  target_include_directories(jtd_gen PRIVATE ${STORE}/include ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/source/sp)
  target_link_libraries(jtd_gen PRIVATE yyjson m)
  add_dependencies(jtd_gen headers)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(EMBED_FORMAT coff)
else()
  set(EMBED_FORMAT elf)
endif()

file(GLOB SPN_INIT_TEMPLATES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/assets/init/*)
add_custom_command(
  OUTPUT ${EMBED_OBJ} ${EMBED_HDR}
  COMMAND $<TARGET_FILE:embed>
    --header ${EMBED_HDR}
    --format ${EMBED_FORMAT}
    ${EMBED_OBJ}
    ${CMAKE_SOURCE_DIR}/include/spn.h=include/spn.h
    ${CMAKE_SOURCE_DIR}/source/toolchain/toolchains.json=toolchains.json
    ${CMAKE_SOURCE_DIR}/assets/init=init
  DEPENDS embed ${CMAKE_SOURCE_DIR}/include/spn.h ${CMAKE_SOURCE_DIR}/source/toolchain/toolchains.json ${SPN_INIT_TEMPLATES}
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

set(GEN_DIR ${CMAKE_SOURCE_DIR}/source/codegen/gen)
file(MAKE_DIRECTORY ${GEN_DIR})

set(SCHEMA_DIR ${CMAKE_SOURCE_DIR}/source/codegen/schema)
set(GEN_FILES
  ${GEN_DIR}/common.gen.h
  ${GEN_DIR}/config.gen.h
  ${GEN_DIR}/config.gen.c
  ${GEN_DIR}/config.jtd.json
  ${GEN_DIR}/manifest.gen.h
  ${GEN_DIR}/manifest.gen.c
  ${GEN_DIR}/manifest.jtd.json
  ${GEN_DIR}/release.gen.h
  ${GEN_DIR}/release.gen.c
  ${GEN_DIR}/release.jtd.json
  ${GEN_DIR}/toolchains.gen.h
  ${GEN_DIR}/toolchains.gen.c
  ${GEN_DIR}/toolchains.jtd.json
  ${GEN_DIR}/abi.gen.h
  ${GEN_DIR}/abi.gen.c)
file(GLOB JTD_GEN_SCHEMAS CONFIGURE_DEPENDS ${SCHEMA_DIR}/*.json)
file(GLOB_RECURSE JTD_GEN_TEMPLATES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/templates/*.tmpl)
add_custom_command(
  OUTPUT ${GEN_FILES}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${GEN_DIR}
  COMMAND $<TARGET_FILE:jtd_gen>
    ${SCHEMA_DIR}
    ${GEN_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/templates
  DEPENDS jtd_gen ${JTD_GEN_SCHEMAS} ${JTD_GEN_TEMPLATES}
  COMMENT "codegen: common.gen.h, {config,manifest,release,toolchains,abi}.gen.{h,c}"
  VERBATIM)
add_custom_target(spn_codegen_gen ALL DEPENDS ${GEN_FILES})
