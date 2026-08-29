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

  add_executable(jtd_gen gen/main.c gen/run.c gen/lower.c gen/bind.c gen/union.c gen/abi.c gen/data.c jtd.c)
  set_target_properties(jtd_gen PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  target_include_directories(jtd_gen PRIVATE
    ${STORE}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/gen
  )
  target_link_libraries(jtd_gen PRIVATE yyjson m)
  add_dependencies(jtd_gen headers)

  add_executable(installer_render install/main.c install/render.c)
  set_target_properties(installer_render PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${STORE}/bin)
  target_include_directories(installer_render PRIVATE
    ${STORE}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/install
  )
  target_link_libraries(installer_render PRIVATE m)
  add_dependencies(installer_render headers)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(EMBED_FORMAT coff)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(EMBED_FORMAT macho)
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
    ${CMAKE_SOURCE_DIR}/include/spn/core.h=include/spn/core.h
    ${CMAKE_SOURCE_DIR}/include/spn/err.h=include/spn/err.h
    ${CMAKE_SOURCE_DIR}/source/core/toolchain/toolchains.json=toolchains.json
    ${CMAKE_SOURCE_DIR}/assets/init=init
  DEPENDS embed ${CMAKE_SOURCE_DIR}/include/spn.h ${CMAKE_SOURCE_DIR}/include/spn/core.h ${CMAKE_SOURCE_DIR}/include/spn/err.h ${CMAKE_SOURCE_DIR}/source/core/toolchain/toolchains.json ${SPN_INIT_TEMPLATES}
  COMMENT "spn.embed.o"
  VERBATIM)
add_custom_target(spn_embed ALL DEPENDS ${EMBED_OBJ} ${EMBED_HDR})

add_executable(toolchain toolchain.c)
set_target_properties(toolchain PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${STORE}/bin)
target_include_directories(toolchain PRIVATE ${STORE}/include)
target_link_libraries(toolchain PRIVATE yyjson m)
add_dependencies(toolchain headers)

set(GEN_DIR ${CMAKE_SOURCE_DIR}/source/core/codegen/gen)
file(MAKE_DIRECTORY ${GEN_DIR})

set(SCHEMA_DIR ${CMAKE_SOURCE_DIR}/source/core/codegen/schema)
set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include/spn)
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
  ${GEN_DIR}/resolve.gen.h
  ${GEN_DIR}/resolve.gen.c
  ${GEN_DIR}/resolve.jtd.json
  ${GEN_DIR}/source_deps.gen.h
  ${GEN_DIR}/source_deps.gen.c
  ${GEN_DIR}/source_deps.jtd.json
  ${GEN_DIR}/toolchains.gen.h
  ${GEN_DIR}/toolchains.gen.c
  ${GEN_DIR}/toolchains.jtd.json
  ${GEN_DIR}/abi.gen.h
  ${GEN_DIR}/abi.gen.c
  ${GEN_DIR}/fuzz.gen.h
  ${GEN_DIR}/fuzz.gen.c
  ${GEN_DIR}/fuzz.jtd.json
  ${INCLUDE_DIR}/err.h
  ${INCLUDE_DIR}/errors.h
  ${GEN_DIR}/errors.gen.c
  ${INCLUDE_DIR}/events.h
  ${GEN_DIR}/events.gen.c)
file(GLOB JTD_GEN_SCHEMAS CONFIGURE_DEPENDS ${SCHEMA_DIR}/*.json)
file(GLOB_RECURSE JTD_GEN_TEMPLATES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/gen/templates/*.tmpl)
add_custom_command(
  OUTPUT ${GEN_FILES}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${GEN_DIR}
  COMMAND $<TARGET_FILE:jtd_gen>
    ${SCHEMA_DIR}
    ${GEN_DIR}
    ${INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/gen/templates
  DEPENDS jtd_gen ${JTD_GEN_SCHEMAS} ${JTD_GEN_TEMPLATES}
  COMMENT "codegen: source/core/codegen/gen/*.gen.{h,c}, include/spn/{err,errors,events}.h"
  VERBATIM)
add_custom_target(spn_codegen_gen ALL DEPENDS ${GEN_FILES})
add_dependencies(spn_embed spn_codegen_gen)
