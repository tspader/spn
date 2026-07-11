#ifndef spn_compiler_driver_h
#define spn_compiler_driver_h

#include "compiler/types.h"
#include "error/types.h"

sp_str_t        spn_cc_feature_to_str(spn_cc_feature_t feature);
spn_err_union_t spn_cc_render_compile(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_compile_t* compile, sp_ps_config_t* ps);
spn_err_union_t spn_cc_render_link(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_link_t* link, sp_ps_config_t* ps);
spn_err_union_t spn_cc_render_archive(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_archive_t* archive, sp_ps_config_t* ps);
spn_err_union_t spn_cc_render_flags(sp_mem_t mem, spn_cc_driver_t driver, const spn_profile_info_t* profile, spn_cc_flags_t* flags);
spn_sanitizer_set_t spn_cc_supported_sanitizers(spn_cc_driver_t driver, spn_triple_t target);

void spn_gnu_render_compile(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_compile_t* compile, sp_ps_config_t* ps);
void spn_gnu_render_link(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_link_t* link, sp_ps_config_t* ps);
void spn_gnu_render_archive(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_cc_archive_t* archive, sp_ps_config_t* ps);
void spn_gnu_render_flags(sp_mem_t mem, const spn_profile_info_t* profile, spn_cc_flags_t* flags);
spn_sanitizer_set_t spn_gcc_supported_sanitizers(spn_triple_t target);
spn_sanitizer_set_t spn_clang_supported_sanitizers(spn_triple_t target);

void spn_msvc_render_flags(sp_mem_t mem, const spn_profile_info_t* profile, spn_cc_flags_t* flags);
spn_sanitizer_set_t spn_msvc_supported_sanitizers(spn_triple_t target);

#endif
