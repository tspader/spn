file(MAKE_DIRECTORY ${STORE}/bin ${STORE}/lib ${STORE}/include ${STORE}/test)

#######
# TCC #
#######
set(TCC_WORK  ${CMAKE_BINARY_DIR}/tinycc)
set(TCC_STAMP ${TCC_WORK}/install.stamp)
set(TCC_LIB   ${STORE}/lib/libtcc.a)

add_custom_command(
  OUTPUT ${TCC_STAMP}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${TCC_WORK}
  COMMAND ${CMAKE_COMMAND} -E chdir ${TCC_WORK} ${SOURCE_ROOT}/tinycc/configure
          --cc=${CMAKE_C_COMPILER} --enable-static --prefix=${STORE}
          --extra-cflags=-fno-sanitize=undefined
  COMMAND ${CMAKE_COMMAND} -E chdir ${TCC_WORK} make -j4
  COMMAND ${CMAKE_COMMAND} -E chdir ${TCC_WORK} make install
  COMMAND ${CMAKE_COMMAND} -E touch ${TCC_STAMP}
  COMMENT "tcc (${TRIPLE})"
  VERBATIM)
add_custom_target(tcc DEPENDS ${TCC_STAMP})

###########
# HEADERS #
###########
set(HEADERS_STAMP ${CMAKE_BINARY_DIR}/headers.stamp)
add_custom_command(
  OUTPUT ${HEADERS_STAMP}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${STORE}/include/sp
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SOURCE_ROOT}/sp/sp.h         ${STORE}/include/sp.h
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SOURCE_ROOT}/sp/sp/sp_math.h ${STORE}/include/sp/sp_math.h
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SOURCE_ROOT}/sp/sp/sp_glob.h ${STORE}/include/sp/sp_glob.h
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SOURCE_ROOT}/sp/sp/sp_cli.h  ${STORE}/include/sp/sp_cli.h
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SRC}/sp/sp_elf.h             ${STORE}/include/sp/sp_elf.h
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SRC}/sp/coff.h               ${STORE}/include/sp/coff.h
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SRC}/sp/atomic_file.h        ${STORE}/include/sp/atomic_file.h
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SOURCE_ROOT}/toml/toml.h     ${STORE}/include/toml.h
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SOURCE_ROOT}/argparse/argparse.h ${STORE}/include/argparse.h
  COMMAND ${CMAKE_COMMAND} -E touch ${HEADERS_STAMP}
  DEPENDS
    ${SOURCE_ROOT}/sp/sp.h
    ${SOURCE_ROOT}/sp/sp/sp_math.h
    ${SOURCE_ROOT}/sp/sp/sp_glob.h
    ${SOURCE_ROOT}/sp/sp/sp_cli.h
    ${SRC}/sp/sp_elf.h
    ${SRC}/sp/coff.h
    ${SRC}/sp/atomic_file.h
    ${SOURCE_ROOT}/toml/toml.h
    ${SOURCE_ROOT}/argparse/argparse.h
  COMMENT "staging headers"
  VERBATIM)
add_custom_target(headers ALL DEPENDS ${HEADERS_STAMP})

add_library(yyjson STATIC ${SOURCE_ROOT}/yyjson/src/yyjson.c)
target_include_directories(yyjson PUBLIC ${SOURCE_ROOT}/yyjson/src)
set_target_properties(yyjson PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${STORE}/lib)
