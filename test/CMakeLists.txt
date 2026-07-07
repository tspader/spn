set(TEST_TOOLS ${CMAKE_CURRENT_SOURCE_DIR}/tools)

function(spn_test_binary target)
  target_sources(${target} PRIVATE ${TEST_TOOLS}/repo.c)
  set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${STORE}/test)
  target_include_directories(${target} PRIVATE
    ${SRC}
    ${CMAKE_SOURCE_DIR}/include
    ${TEST_TOOLS}
    ${SOURCE_ROOT}/sp/tools
    ${STORE}/include
  )
  target_link_libraries(${target} PRIVATE m)
  add_dependencies(${target} headers)
endfunction()

add_subdirectory(core)
add_subdirectory(fuzz)
add_subdirectory(integration)
add_subdirectory(e2e)
