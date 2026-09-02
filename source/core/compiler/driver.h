#ifndef spn_compiler_driver_h
#define spn_compiler_driver_h

#include "compiler/types.h"
#include "sp.h"
#include "spn/core.h"

spn_err_t spn_cc_validate_profile(const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile);
spn_err_t spn_cc_validate_link(const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, spn_cc_output_kind_t kind, bool frameworks);
spn_err_t spn_cc_validate_archive(const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile);
spn_err_t spn_cc_render_compile(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_compile_t* compile, spn_invocation_t* invocation);
spn_invocation_t spn_cc_render_compile_command(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_invocation_t* base, const spn_cc_compile_files_t* files);
spn_err_t spn_cc_render_link(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_link_t* link, const spn_cc_link_files_t* files, spn_invocation_t* invocation);
spn_err_t spn_cc_render_archive(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_archive_files_t* files, spn_invocation_t* invocation);
bool spn_cc_has(const spn_cc_toolchain_t* toolchain, spn_cc_cap_t cap);
spn_cc_depfile_t spn_cc_depfile(const spn_cc_toolchain_t* toolchain, spn_lang_t lang);
spn_err_t spn_cc_parse_depfile(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, sp_str_t content, sp_da(sp_str_t)* prereqs);
spn_cc_exports_format_t spn_cc_exports_format(spn_cc_output_kind_t kind, spn_os_t os);
const c8*               spn_cc_exports_extension(spn_cc_exports_format_t format);
spn_err_t spn_cc_render_flags(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, spn_cc_flags_t* flags);

void spn_gnu_render_compile(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_compile_t* compile, spn_invocation_t* invocation);
void spn_gnu_render_compile_files(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_compile_files_t* files, spn_invocation_t* invocation);
void spn_gnu_render_link(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_link_t* link, const spn_cc_link_files_t* files, spn_invocation_t* invocation);
void spn_gnu_render_archive(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_cc_archive_files_t* files, spn_invocation_t* invocation);
void spn_gnu_render_flags(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, spn_cc_flags_t* flags);
spn_err_t spn_gnu_parse_depfile(sp_mem_t mem, sp_str_t content, sp_da(sp_str_t)* prereqs);
spn_sanitizer_set_t spn_gcc_supported_sanitizers(spn_triple_t target);
spn_sanitizer_set_t spn_clang_supported_sanitizers(spn_triple_t target);
spn_sanitizer_set_t spn_zig_supported_sanitizers(spn_triple_t target);

void spn_msvc_render_compile(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_compile_t* compile, spn_invocation_t* invocation);
void spn_msvc_render_compile_files(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_compile_files_t* files, spn_invocation_t* invocation);
void spn_msvc_render_link(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_link_t* link, const spn_cc_link_files_t* files, spn_invocation_t* invocation);
void spn_msvc_render_archive(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_cc_archive_files_t* files, spn_invocation_t* invocation);
void spn_msvc_render_flags(sp_mem_t mem, const spn_profile_info_t* profile, spn_cc_flags_t* flags);
spn_err_t spn_msvc_parse_depfile(sp_mem_t mem, sp_str_t content, sp_da(sp_str_t)* prereqs);
spn_sanitizer_set_t spn_msvc_supported_sanitizers(spn_triple_t target);

#endif
