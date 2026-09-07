#include "docker.h"

#include "container/container.h"

#define SP_TEMPLATE_IMPLEMENTATION
#include "../../../gen/sp_template.h"

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

static sp_str_t get_manifest_path(docker_t* docker, const variant_t* variant) {
  sp_str_t file = sp_fmt(docker->mem, "{}.dockerfile", sp_fmt_cstr(variant->name)).value;
  return sp_fs_join_path(docker->mem, docker->paths.dockerfiles, file);
}

static const c8* bind(docker_t* docker, sp_str_t host, const c8* guest) {
  return cfmt(docker->mem, "{}:{}:ro", sp_fmt_str(host), sp_fmt_cstr(guest));
}

static const c8* mirror(docker_t* docker, sp_str_t path) {
  return cfmt(docker->mem, "{}:{}", sp_fmt_str(path), sp_fmt_str(path));
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

static const c8* cache(docker_t* docker, const variant_t* variant) {
  return cfmt(docker->mem, "{}:{}", sp_fmt_cstr(docker_image(docker, variant)), sp_fmt_cstr(CONTAINER_CACHE));
}

static sp_ps_config_cstr_t launch(docker_t* docker, const variant_t* variant, const c8* option, const c8* command, const c8* argument) {
  return (sp_ps_config_cstr_t) {
    .command = "docker",
    .args = {
      "run",
      "--rm",
      option,
      "-v", bind(docker, spn_dir(docker, variant->spn), CONTAINER_SPN_DIR),
      "-v", bind(docker, docker->paths.tools, CONTAINER_TOOL_DIR),
      "-v", cache(docker, variant),
      "-w", CONTAINER_WORK,
      docker_image(docker, variant),
      command,
      argument,
    },
  };
}

docker_init_err_t docker_init(docker_t* docker, sp_mem_t mem) {
  docker->mem = mem;

  sp_str_t repo = find_repo(mem);
  if (sp_str_empty(repo)) {
    return DOCKER_INIT_ERR_REPO;
  }

  docker->paths.repo = repo;
  docker->paths.tools = sp_fs_join_path(mem, repo, sp_str_lit("tools/docker/build/debug"));
  docker->paths.dockerfiles = sp_fs_join_path(mem, repo, sp_str_lit("build/smoke"));
  docker->paths.templates = sp_fs_join_path(mem, repo, sp_str_lit("tools/docker/templates"));

  const c8* tools [] = { "shell", "check" };
  sp_carr_for(tools, it) {
    sp_str_t path = sp_fs_join_path(mem, docker->paths.tools, sp_cstr_as_str(tools[it]));
    if (!sp_fs_is_file(path)) {
      docker->err.binary.path = path;
      docker->err.binary.hint = "spn build in tools/docker";
      return DOCKER_INIT_ERR_BINARY;
    }
  }

  docker->templates = sp_template_registry_create(mem);
  if (!sp_fs_is_dir(docker->paths.templates) || sp_template_load_dir(docker->templates, docker->paths.templates)) {
    return DOCKER_INIT_ERR_TEMPLATES;
  }

  sp_fs_create_dir(docker->paths.dockerfiles);
  return DOCKER_INIT_OK;
}

bool docker_require(docker_t* docker, const variant_t* variant) {
  sp_str_t path = sp_fs_join_path(docker->mem, spn_dir(docker, variant->spn), sp_str_lit("spn"));
  if (sp_fs_is_file(path)) {
    return true;
  }
  docker->err.binary.path = path;
  docker->err.binary.hint = spn_hint(variant->spn);
  return false;
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
  docker->paths.toolchains = sp_fs_join_path(mem, sp_fs_get_storage_path(mem), sp_str_lit("spn/cache/toolchain"));
  docker->paths.zig = sp_fs_join_path(mem, docker->paths.home, sp_str_lit(".cache/zig"));
  sp_fs_create_dir(docker->paths.toolchains);
  sp_fs_create_dir(docker->paths.zig);

  if (missing(docker, sp_fs_join_path(mem, spn_dir(docker, SPN_MUSL), sp_str_lit("spn")), spn_hint(SPN_MUSL))) {
    return DOCKER_TESTS_ERR_BINARY;
  }
  if (missing(docker, docker->paths.tests, "spn build --test integration")) {
    return DOCKER_TESTS_ERR_BINARY;
  }
  return DOCKER_TESTS_OK;
}

docker_render_err_t docker_render(docker_t* docker, const variant_t* variant) {
  sp_mem_t mem = docker->mem;

  sp_str_t source = sp_zero;
  if (!sp_template_get(docker->templates, get_template_name(variant), &source)) {
    return DOCKER_RENDER_ERR_MISSING;
  }

  sp_template_scope_t* scope = sp_template_scope_create(mem);
  sp_template_set(scope, sp_str_lit("packages"), get_variant_packages(mem, variant));
  sp_template_set(scope, sp_str_lit("setup"), get_variant_setup(mem, variant));

  sp_io_dyn_mem_writer_t writer = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &writer);
  docker->err.render = sp_template_render(&writer.base, source, scope, docker->templates);
  if (docker->err.render) {
    return DOCKER_RENDER_ERR_FAILED;
  }

  sp_str_t manifest = get_manifest_path(docker, variant);
  sp_fs_create_file_str(manifest, sp_io_dyn_mem_writer_as_str(&writer));
  return DOCKER_RENDER_OK;
}

const c8* docker_image(docker_t* docker, const variant_t* variant) {
  return cfmt(docker->mem, "spn-smoke-{}", sp_fmt_cstr(variant->name));
}

sp_ps_config_cstr_t docker_build(docker_t* docker, const variant_t* variant) {
  sp_mem_t mem = docker->mem;
  return (sp_ps_config_cstr_t) {
    .command = "docker",
    .args = {
      "build",
      "-t", docker_image(docker, variant),
      "-f", sp_str_to_cstr(mem, get_manifest_path(docker, variant)),
      sp_str_to_cstr(mem, docker->paths.dockerfiles),
    },
  };
}

sp_ps_config_cstr_t docker_check(docker_t* docker, const variant_t* variant) {
  return launch(docker, variant, "--init", CONTAINER_CHECK, toolchain_name(variant->toolchain));
}

sp_ps_config_cstr_t docker_shell(docker_t* docker, const variant_t* variant) {
  return launch(docker, variant, "-it", CONTAINER_SHELL, SP_NULLPTR);
}

sp_ps_config_cstr_t docker_test(docker_t* docker, const variant_t* variant, const c8* lane, const c8* filter) {
  sp_mem_t mem = docker->mem;
  return (sp_ps_config_cstr_t) {
    .command = "docker",
    .args = {
      "run",
      "--rm",
      "--init",
      "--user", docker->user,
      "-e", cfmt(mem, "HOME={}", sp_fmt_str(docker->paths.home)),
      "-e", cfmt(mem, "SPN_TEST_TOOLCHAIN={}", sp_fmt_cstr(lane)),
      "-v", mirror(docker, docker->paths.repo),
      "-v", mirror(docker, docker->paths.git),
      "-v", mirror(docker, docker->paths.toolchains),
      "-v", mirror(docker, docker->paths.zig),
      "-w", sp_str_to_cstr(mem, docker->paths.repo),
      docker_image(docker, variant),
      sp_str_to_cstr(mem, docker->paths.tests),
      "--filter", filter,
    },
  };
}
