add_executable(embed embed.c)
set_target_properties(embed PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(embed PRIVATE ${STORE}/include)
target_link_libraries(embed PRIVATE m)
add_dependencies(embed headers)

add_custom_command(
  OUTPUT ${EMBED_OBJ} ${EMBED_HDR}
  COMMAND $<TARGET_FILE:embed>
    ${STORE}/lib/tcc
    ${EMBED_OBJ}
    ${EMBED_HDR}
    ${CMAKE_SOURCE_DIR}/include/spn.h
  DEPENDS embed tcc ${CMAKE_SOURCE_DIR}/include/spn.h
  COMMENT "spn.embed.o"
  VERBATIM)
add_custom_target(spn_embed ALL DEPENDS ${EMBED_OBJ} ${EMBED_HDR})

add_executable(yyjson_stub yyjson_stub.c)
set_target_properties(yyjson_stub PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(yyjson_stub PRIVATE yyjson)

add_executable(jtd_gen codegen/main.c codegen/walk.c codegen/render.c jtd.c)
set_target_properties(jtd_gen PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(jtd_gen PRIVATE ${STORE}/include ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(jtd_gen PRIVATE yyjson m)
add_dependencies(jtd_gen headers)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/gen/manifest)
file(GLOB_RECURSE JTD_GEN_TEMPLATES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/templates/*.tmpl)
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/gen/manifest/manifest.gen.h
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/gen/manifest
  COMMAND $<TARGET_FILE:jtd_gen>
    ${CMAKE_SOURCE_DIR}/source/manifest/manifest.jtd.json
    ${CMAKE_BINARY_DIR}/gen/manifest/manifest.gen.h
    ${CMAKE_CURRENT_SOURCE_DIR}/templates
  DEPENDS jtd_gen ${CMAKE_SOURCE_DIR}/source/manifest/manifest.jtd.json ${JTD_GEN_TEMPLATES}
  COMMENT "manifest.gen.h"
  VERBATIM)
add_custom_target(manifest_gen_header ALL DEPENDS ${CMAKE_BINARY_DIR}/gen/manifest/manifest.gen.h)
