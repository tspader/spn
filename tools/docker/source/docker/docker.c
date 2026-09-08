#include "docker.h"

#include "container/container.h"
#include "ctx/types.h"
#include "enum/enum.h"
#include "event/event.h"
#include "toolchain/provision.h"
#include "triple/triple.h"

#define cfmt(mem, ...) sp_str_to_cstr(mem, sp_fmt(mem, __VA_ARGS__).value)

static sp_str_t find_repo(sp_mem_t mem) {
  sp_str_t dir = sp_fs_get_cwd(mem);
  while (!sp_fs_is_root(dir)) {
    if (sp_fs_is_dir(sp_fs_join_path(mem, dir, sp_str_lit("vendor/sp")))) {
      return dir;
    }
    dir = sp_fs_parent_path(dir);
  }
  return sp_str_lit("");
}

static sp_str_t spn_dir(docker_t* docker, spn_kind_t kind) {
  switch (kind) {
    case SPN_MUSL: return sp_fs_join_path(docker->mem, docker->paths.repo, sp_str_lit("build/debug"));
    case SPN_GNU:  return sp_fs_join_path(docker->mem, docker->paths.repo, sp_str_lit("build/x86_64-linux-gnu/gnu"));
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

static const c8* spn_hint(spn_kind_t kind) {
  switch (kind) {
    case SPN_MUSL: return "spn build";
    case SPN_GNU:  return "spn build --profile gnu";
  }
  SP_UNREACHABLE_RETURN("");
}

static sp_str_t host_key(sp_mem_t mem) {
  spn_triple_t host = spn_triple_host();
  return sp_fmt(mem, "{}-{}", sp_fmt_str(spn_arch_to_str(host.arch)), sp_fmt_str(spn_os_to_str(host.os))).value;
}

static sp_str_t manifest_path(docker_t* docker, const variant_t* variant) {
  sp_str_t file = sp_fmt(docker->mem, "{}.dockerfile", sp_fmt_cstr(variant->name)).value;
  return sp_fs_join_path(docker->mem, docker->paths.dockerfiles, file);
}

static void arg(docker_t* docker, sp_ps_config_t* config, sp_str_t value) {
  sp_ps_config_add_arg(docker->mem, config, value);
}

static void arg_c(docker_t* docker, sp_ps_config_t* config, const c8* value) {
  arg(docker, config, sp_cstr_as_str(value));
}

static sp_str_t bind(docker_t* docker, sp_str_t host, const c8* guest) {
  return sp_fmt(docker->mem, "{}:{}:ro", sp_fmt_str(host), sp_fmt_cstr(guest)).value;
}

static sp_str_t mirror(docker_t* docker, sp_str_t path) {
  return sp_fmt(docker->mem, "{}:{}", sp_fmt_str(path), sp_fmt_str(path)).value;
}

static sp_str_t cache(docker_t* docker, const variant_t* variant) {
  return sp_fmt(docker->mem, "{}:{}", sp_fmt_cstr(docker_image(docker, variant)), sp_fmt_cstr(CONTAINER_CACHE)).value;
}

static sp_ps_output_t git_common_dir(sp_mem_t mem, sp_str_t repo) {
  return sp_ps_run(mem, (sp_ps_config_t) {
    .command = sp_str_lit("git"),
    .args = { sp_str_lit("-C"), repo, sp_str_lit("rev-parse"), sp_str_lit("--path-format=absolute"), sp_str_lit("--git-common-dir") },
  });
}

static sp_ps_output_t id_of(sp_mem_t mem, const c8* flag) {
  return sp_ps_run(mem, (sp_ps_config_t) {
    .command = sp_str_lit("id"),
    .args = { sp_cstr_as_str(flag) },
  });
}

static bool missing(docker_t* docker, sp_str_t path, const c8* hint) {
  if (sp_fs_is_file(path)) {
    return false;
  }
  docker->err.binary.path = path;
  docker->err.binary.hint = hint;
  return true;
}

static bool read_lanes(docker_t* docker, sp_str_t path, spn_cg_toolchains_t* out) {
  sp_str_t json = sp_zero;
  if (sp_io_read_file(docker->mem, path, &json) || !spn_toolchains_read(json, out, docker->mem)) {
    docker->err.json = path;
    return false;
  }
  return true;
}

static bool sysroot_artifact(docker_t* docker, const sysroot_t* sysroot, spn_artifact_t* out) {
  const spn_cg_toolchain_t* lane = lane_decl(sysroot->artifact, &docker->builtin, &docker->lanes);
  const spn_cg_artifact_t* artifact = lane_artifact(lane, docker->host);
  if (!artifact || sp_str_empty(artifact->url)) {
    return false;
  }
  *out = (spn_artifact_t) {
    .url = artifact->url,
    .sha256 = artifact->sha256,
    .mirror_list = lane->mirrors,
  };
  return true;
}

static bool render_config(docker_t* docker) {
  sp_str_t config = sp_fs_join_path(docker->mem, docker->paths.config, sp_str_lit("spn/spn.toml"));
  sp_fs_create_dir(sp_fs_parent_path(config));
  return !sp_fs_create_file_str(config, lanes_toml(docker->mem, &docker->lanes));
}

static sp_ps_config_t launch(docker_t* docker, const variant_t* variant, const c8* option, const c8* command) {
  sp_ps_config_t config = {
    .command = sp_str_lit("docker"),
    .args = { sp_str_lit("run"), sp_str_lit("--rm"), sp_cstr_as_str(option) },
  };
  arg_c(docker, &config, "-e");
  arg_c(docker, &config, "SPN_CONFIG_DIR=" CONTAINER_CONFIG_DIR);
  arg_c(docker, &config, "-v");
  arg(docker, &config, bind(docker, spn_dir(docker, variant->spn), CONTAINER_SPN_DIR));
  arg_c(docker, &config, "-v");
  arg(docker, &config, bind(docker, docker->paths.tools, CONTAINER_TOOL_DIR));
  arg_c(docker, &config, "-v");
  arg(docker, &config, bind(docker, docker->paths.config, CONTAINER_CONFIG_DIR));
  arg_c(docker, &config, "-v");
  arg(docker, &config, cache(docker, variant));
  arg_c(docker, &config, "-w");
  arg_c(docker, &config, CONTAINER_WORK);
  arg_c(docker, &config, docker_image(docker, variant));
  arg_c(docker, &config, command);
  return config;
}

docker_init_err_t docker_init(docker_t* docker, sp_mem_t mem, spn_fetch_fn fetch, void* user) {
  docker->mem = mem;
  docker->host = host_key(mem);

  sp_str_t repo = find_repo(mem);
  if (sp_str_empty(repo)) {
    return DOCKER_INIT_ERR_REPO;
  }

  docker->paths.repo = repo;
  docker->paths.tools = sp_fs_join_path(mem, repo, sp_str_lit("build/debug"));
  docker->paths.dockerfiles = sp_fs_join_path(mem, repo, sp_str_lit("build/smoke"));
  docker->paths.templates = sp_fs_join_path(mem, repo, sp_str_lit("tools/docker/templates"));
  docker->paths.builtin = sp_fs_join_path(mem, repo, sp_str_lit(SPN_LANES_BUILTIN));
  docker->paths.lanes = sp_fs_join_path(mem, repo, sp_str_lit(SPN_LANES_TEST));
  docker->paths.config = sp_fs_join_path(mem, docker->paths.dockerfiles, sp_str_lit("config"));

  sp_str_t cache = sp_fs_join_path(mem, sp_fs_get_storage_path(mem), sp_str_lit("spn/cache"));
  docker->paths.xwin.cache = sp_fs_join_path(mem, cache, sp_str_lit("xwin/cache"));
  docker->paths.xwin.splat = sp_fs_join_path(mem, cache, sp_str_lit("xwin/splat"));

  spn.mem = mem;
  spn.events = spn_event_buffer_new(mem);
  docker->store = (spn_toolchain_store_t) {
    .mem = mem,
    .dir = sp_fs_join_path(mem, cache, sp_str_lit("toolchain")),
    .fetch = fetch,
    .fetch_user_data = user,
  };

  const c8* tools [] = { CONTAINER_SHELL_BIN, CONTAINER_CHECK_BIN };
  sp_carr_for(tools, it) {
    if (missing(docker, sp_fs_join_path(mem, docker->paths.tools, sp_cstr_as_str(tools[it])), "spn build --script")) {
      return DOCKER_INIT_ERR_BINARY;
    }
  }

  docker->templates = sp_template_registry_create(mem);
  if (!sp_fs_is_dir(docker->paths.templates) || sp_template_load_dir(docker->templates, docker->paths.templates)) {
    return DOCKER_INIT_ERR_TEMPLATES;
  }

  if (!read_lanes(docker, docker->paths.builtin, &docker->builtin) || !read_lanes(docker, docker->paths.lanes, &docker->lanes)) {
    return DOCKER_INIT_ERR_LANES;
  }

  docker->err.verify = lanes_verify(&docker->builtin, &docker->lanes);
  if (docker->err.verify.kind) {
    return DOCKER_INIT_ERR_VERIFY;
  }
  sp_for(it, num_variants) {
    docker->err.verify = variant_verify(&variants[it], &docker->builtin, &docker->lanes);
    if (docker->err.verify.kind) {
      return DOCKER_INIT_ERR_VERIFY;
    }
  }

  sp_fs_create_dir(docker->paths.dockerfiles);
  if (!render_config(docker)) {
    return DOCKER_INIT_ERR_CONFIG;
  }
  return DOCKER_INIT_OK;
}

bool docker_require(docker_t* docker, const variant_t* variant) {
  return !missing(docker, sp_fs_join_path(docker->mem, spn_dir(docker, variant->spn), sp_str_lit("spn")), spn_hint(variant->spn));
}

docker_tests_err_t docker_tests_init(docker_t* docker) {
  sp_mem_t mem = docker->mem;

  sp_ps_output_t git = git_common_dir(mem, docker->paths.repo);
  if (git.status.exit_code) {
    return DOCKER_TESTS_ERR_GIT;
  }
  docker->paths.git = sp_str_trim(git.out);

  sp_ps_output_t uid = id_of(mem, "-u");
  sp_ps_output_t gid = id_of(mem, "-g");
  if (uid.status.exit_code || gid.status.exit_code) {
    return DOCKER_TESTS_ERR_USER;
  }
  docker->user = cfmt(mem, "{}:{}", sp_fmt_str(sp_str_trim(uid.out)), sp_fmt_str(sp_str_trim(gid.out)));

  docker->paths.home = sp_os_env_get(sp_str_lit("HOME"));
  docker->paths.tests = sp_fs_join_path(mem, docker->paths.repo, sp_str_lit("build/debug/test/integration"));
  docker->paths.zig = sp_fs_join_path(mem, docker->paths.home, sp_str_lit(".cache/zig"));
  sp_fs_create_dir(docker->store.dir);
  sp_fs_create_dir(docker->paths.zig);

  if (missing(docker, sp_fs_join_path(mem, spn_dir(docker, SPN_MUSL), sp_str_lit("spn")), spn_hint(SPN_MUSL))) {
    return DOCKER_TESTS_ERR_BINARY;
  }
  if (missing(docker, docker->paths.tests, "spn build --test integration")) {
    return DOCKER_TESTS_ERR_BINARY;
  }
  return DOCKER_TESTS_OK;
}

static docker_provision_err_t provision_artifact(docker_t* docker, const sysroot_t* sysroot) {
  spn_artifact_t artifact = sp_zero;
  docker->err.artifact.name = lane_name(sysroot->artifact);
  if (!sysroot_artifact(docker, sysroot, &artifact)) {
    return DOCKER_PROVISION_ERR_ARTIFACT;
  }
  docker->err.artifact.url = artifact.url;
  if (spn_toolchain_provision(&docker->store, sp_cstr_as_str(docker->err.artifact.name), artifact)) {
    return DOCKER_PROVISION_ERR_FETCH;
  }
  return DOCKER_PROVISION_OK;
}

static sp_ps_config_t xwin_splat(docker_t* docker, const sysroot_t* sysroot) {
  return (sp_ps_config_t) {
    .command = sp_str_lit("xwin"),
    .args = {
      sp_str_lit("--accept-license"),
      sp_str_lit("--cache-dir"), docker->paths.xwin.cache,
      sp_str_lit("--timeout"), sp_str_lit("600"),
      sp_str_lit("--arch"), sp_cstr_as_str(sysroot->xwin.arch),
      sp_str_lit("--variant"), sp_cstr_as_str(sysroot->xwin.variant),
      sp_str_lit("splat"),
      sp_str_lit("--copy"),
      sp_str_lit("--output"), docker->paths.xwin.splat,
    },
    .io.err = { .mode = SP_PS_IO_MODE_REDIRECT },
  };
}

static docker_provision_err_t provision_xwin(docker_t* docker, const sysroot_t* sysroot) {
  if (sp_fs_is_dir(sp_fs_join_path(docker->mem, docker->paths.xwin.splat, sp_str_lit("crt/include")))) {
    return DOCKER_PROVISION_OK;
  }
  sp_fs_create_dir(docker->paths.xwin.cache);
  sp_ps_output_t output = sp_ps_run(docker->mem, xwin_splat(docker, sysroot));
  docker->err.xwin.status = output.status.exit_code;
  docker->err.xwin.output = sp_str_trim(output.out);
  return output.status.exit_code ? DOCKER_PROVISION_ERR_XWIN : DOCKER_PROVISION_OK;
}

static docker_provision_err_t provision(docker_t* docker, const sysroot_t* sysroot) {
  switch (sysroot->kind) {
    case INSTALL_PACKAGES:
    case INSTALL_LINKS:
    case INSTALL_DEBS:     return DOCKER_PROVISION_OK;
    case INSTALL_ARTIFACT: return provision_artifact(docker, sysroot);
    case INSTALL_XWIN:     return provision_xwin(docker, sysroot);
  }
  SP_UNREACHABLE_RETURN(DOCKER_PROVISION_OK);
}

docker_provision_err_t docker_provision(docker_t* docker, const variant_t* variant) {
  sp_carr_for_until(variant->sysroots, it, variant->sysroots[it]) {
    docker_provision_err_t err = provision(docker, &sysroots[variant->sysroots[it]]);
    if (err) {
      return err;
    }
  }
  return DOCKER_PROVISION_OK;
}

docker_render_err_t docker_render(docker_t* docker, const variant_t* variant) {
  sp_mem_t mem = docker->mem;

  sp_str_t source = sp_zero;
  if (!sp_template_get(docker->templates, variant_template(variant), &source)) {
    return DOCKER_RENDER_ERR_MISSING;
  }

  sp_template_scope_t* scope = sp_template_scope_create(mem);
  sp_template_set(scope, sp_str_lit("packages"), variant_packages(mem, variant));
  sp_template_list(scope, sp_str_lit("artifacts"));
  sp_template_list(scope, sp_str_lit("setups"));
  sp_carr_for_until(variant->sysroots, it, variant->sysroots[it]) {
    const sysroot_t* sysroot = &sysroots[variant->sysroots[it]];
    switch (sysroot->kind) {
      case INSTALL_PACKAGES: {
        break;
      }
      case INSTALL_ARTIFACT:
      case INSTALL_XWIN: {
        sp_template_scope_t* artifact = sp_template_push(scope, sp_str_lit("artifacts"));
        sp_template_set(artifact, sp_str_lit("name"), sp_cstr_as_str(sysroot->name));
        sp_template_set(artifact, sp_str_lit("path"), sp_cstr_as_str(sysroot->path));
        break;
      }
      case INSTALL_LINKS: {
        sp_template_set(sp_template_push(scope, sp_str_lit("setups")), sp_str_lit("steps"), sysroot_links_setup(mem, sysroot));
        break;
      }
      case INSTALL_DEBS: {
        sp_template_set(sp_template_push(scope, sp_str_lit("setups")), sp_str_lit("steps"), sysroot_debs_setup(mem, sysroot));
        break;
      }
    }
  }

  sp_io_dyn_mem_writer_t writer = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &writer);
  docker->err.render = sp_template_render(&writer.base, source, scope, docker->templates);
  if (docker->err.render) {
    return DOCKER_RENDER_ERR_FAILED;
  }

  sp_fs_create_file_str(manifest_path(docker, variant), sp_io_dyn_mem_writer_as_str(&writer));
  return DOCKER_RENDER_OK;
}

const c8* docker_image(docker_t* docker, const variant_t* variant) {
  return cfmt(docker->mem, "spn-smoke-{}", sp_fmt_cstr(variant->name));
}

static sp_str_t artifact_dir(docker_t* docker, const sysroot_t* sysroot) {
  spn_artifact_t artifact = sp_zero;
  bool provisioned = sysroot_artifact(docker, sysroot, &artifact);
  sp_assert(provisioned);
  return spn_toolchain_store_path(&docker->store, artifact);
}

static void context(docker_t* docker, sp_ps_config_t* config, const sysroot_t* sysroot, sp_str_t dir) {
  arg_c(docker, config, "--build-context");
  arg(docker, config, sp_fmt(docker->mem, "{}={}", sp_fmt_cstr(sysroot->name), sp_fmt_str(dir)).value);
}

sp_ps_config_t docker_build(docker_t* docker, const variant_t* variant) {
  sp_ps_config_t config = {
    .command = sp_str_lit("docker"),
    .args = {
      sp_str_lit("build"),
      sp_str_lit("-t"), sp_cstr_as_str(docker_image(docker, variant)),
      sp_str_lit("-f"), manifest_path(docker, variant),
    },
  };
  sp_carr_for_until(variant->sysroots, it, variant->sysroots[it]) {
    const sysroot_t* sysroot = &sysroots[variant->sysroots[it]];
    switch (sysroot->kind) {
      case INSTALL_PACKAGES:
      case INSTALL_LINKS:
      case INSTALL_DEBS: {
        break;
      }
      case INSTALL_ARTIFACT: {
        context(docker, &config, sysroot, artifact_dir(docker, sysroot));
        break;
      }
      case INSTALL_XWIN: {
        context(docker, &config, sysroot, docker->paths.xwin.splat);
        break;
      }
    }
  }
  arg(docker, &config, docker->paths.dockerfiles);
  return config;
}

sp_ps_config_t docker_check(docker_t* docker, const variant_t* variant) {
  sp_ps_config_t config = launch(docker, variant, "--init", CONTAINER_CHECK);
  arg_c(docker, &config, lane_name(variant->check));
  return config;
}

static sp_str_t seed_profile(docker_t* docker, lane_t seed) {
  sp_io_dyn_mem_writer_t writer = sp_zero;
  sp_io_dyn_mem_writer_init(docker->mem, &writer);
  sp_fmt_io(&writer.base, "toolchain = \"{}\"\n", sp_fmt_cstr(lane_name(seed)));

  const spn_cg_toolchain_t* lane = lanes_find(&docker->lanes, sp_cstr_as_str(lane_name(seed)));
  if (lane && !sp_da_empty(lane->target)) {
    const spn_cg_toolchain_target_t* target = &lane->target[0];
    if (!sp_opt_is_null(target->arch)) {
      sp_fmt_io(&writer.base, "arch = \"{}\"\n", sp_fmt_str(spn_arch_to_str(sp_opt_get(target->arch))));
    }
    if (!sp_opt_is_null(target->os)) {
      sp_fmt_io(&writer.base, "os = \"{}\"\n", sp_fmt_str(spn_os_to_str(sp_opt_get(target->os))));
    }
    if (!sp_opt_is_null(target->abi)) {
      sp_fmt_io(&writer.base, "abi = \"{}\"\n", sp_fmt_str(spn_abi_to_str(sp_opt_get(target->abi))));
    }
  }
  return sp_io_dyn_mem_writer_as_str(&writer);
}

sp_ps_config_t docker_shell(docker_t* docker, const variant_t* variant) {
  sp_ps_config_t config = launch(docker, variant, "-it", CONTAINER_SHELL);
  lane_t seed = variant_seed(variant);
  if (seed != LANE_NONE) {
    arg(docker, &config, seed_profile(docker, seed));
  }
  return config;
}

sp_ps_config_t docker_test(docker_t* docker, const variant_t* variant, lane_t lane, const c8* filter) {
  sp_mem_t mem = docker->mem;
  sp_ps_config_t config = {
    .command = sp_str_lit("docker"),
    .args = { sp_str_lit("run"), sp_str_lit("--rm"), sp_str_lit("--init"), sp_str_lit("--user"), sp_cstr_as_str(docker->user) },
  };
  arg_c(docker, &config, "-e");
  arg(docker, &config, sp_fmt(mem, "HOME={}", sp_fmt_str(docker->paths.home)).value);
  arg_c(docker, &config, "-e");
  arg(docker, &config, sp_fmt(mem, "SPN_TEST_TOOLCHAIN={}", sp_fmt_cstr(lane_name(lane))).value);
  arg_c(docker, &config, "-v");
  arg(docker, &config, mirror(docker, docker->paths.repo));
  arg_c(docker, &config, "-v");
  arg(docker, &config, mirror(docker, docker->paths.git));
  arg_c(docker, &config, "-v");
  arg(docker, &config, mirror(docker, docker->store.dir));
  arg_c(docker, &config, "-v");
  arg(docker, &config, mirror(docker, docker->paths.zig));
  arg_c(docker, &config, "-w");
  arg(docker, &config, docker->paths.repo);
  arg_c(docker, &config, docker_image(docker, variant));
  arg(docker, &config, docker->paths.tests);
  arg_c(docker, &config, "--filter");
  arg_c(docker, &config, filter);
  return config;
}
