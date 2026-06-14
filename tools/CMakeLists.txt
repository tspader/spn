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
