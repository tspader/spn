#include "winvm.h"
#include "probe.gen.h"

#define cfmt(mem, ...) sp_str_to_cstr(mem, sp_fmt(mem, __VA_ARGS__).value)

static sp_str_t find_repo(sp_mem_t mem) {
  sp_str_t path = sp_fs_get_exe_path(mem);
  while (true) {
    sp_assert(!sp_str_empty(path));
    if (sp_fs_exists(sp_fs_join_path(mem, path, sp_str_lit("spn.toml")))) {
      return path;
    }
    path = sp_fs_parent_path(path);
  }
}

static sp_str_t env_or(sp_mem_t mem, const c8* key, sp_str_t fallback) {
  sp_str_t value = sp_os_env_get(sp_cstr_as_str(key));
  return sp_str_empty(value) ? fallback : value;
}

winvm_init_err_t winvm_init(winvm_t* vm, sp_mem_t mem) {
  vm->mem = mem;
  vm->paths.repo = find_repo(mem);

  sp_str_t dir = sp_os_env_get(sp_str_lit("SPN_WIN_DIR"));
  if (sp_str_empty(dir)) {
    return WINVM_INIT_ERR_NO_DIR;
  }

  vm->paths.dir = dir;
  vm->paths.golden = sp_fs_join_path(mem, dir, sp_str_lit("golden.qcow2"));
  vm->paths.recipes = sp_fs_join_path(mem, vm->paths.repo, sp_str_lit("tools/windows/recipes"));
  vm->paths.templates = sp_fs_join_path(mem, vm->paths.repo, sp_str_lit("tools/windows/templates"));
  vm->paths.domains = sp_fs_join_path(mem, vm->paths.repo, sp_str_lit("build/winvm/domains"));
  vm->paths.logs = sp_fs_join_path(mem, vm->paths.repo, sp_str_lit("build/winvm/logs"));
  vm->paths.probes = sp_fs_join_path(mem, vm->paths.repo, sp_str_lit("build/winvm/probes"));

  vm->cfg.connect = env_or(mem, "SPN_WIN_CONNECT", sp_str_lit("qemu:///system"));
  vm->cfg.pool = env_or(mem, "SPN_WIN_POOL", sp_str_lit("default"));
  vm->cfg.origin = env_or(mem, "SPN_WIN_ORIGIN", sp_str_lit("win-11.qcow2"));
  vm->cfg.snapshot = env_or(mem, "SPN_WIN_SNAPSHOT", sp_str_lit("spn"));
  vm->cfg.network = env_or(mem, "SPN_WIN_NETWORK", sp_str_lit("default"));
  vm->cfg.prefix = env_or(mem, "SPN_WIN_PREFIX", sp_str_lit("win-11"));
  vm->cfg.user = env_or(mem, "SPN_WIN_USER", sp_str_lit("spader"));

  vm->guest.home = sp_fmt(mem, "C:/Users/{}", sp_fmt_str(vm->cfg.user)).value;
  vm->guest.probes = sp_fs_join_path(mem, vm->guest.home, sp_str_lit("probes"));

  vm->templates = sp_template_registry_create(mem);
  if (!sp_fs_is_dir(vm->paths.templates) || sp_template_load_dir(vm->templates, vm->paths.templates)) {
    return WINVM_INIT_ERR_TEMPLATES;
  }

  sp_fs_create_dir(vm->paths.logs);
  sp_fs_create_dir(vm->paths.domains);
  sp_fs_create_dir(vm->paths.probes);
  return WINVM_INIT_OK;
}

sp_str_t winvm_image(winvm_t* vm, const winvm_variant_t* variant) {
  return sp_fs_join_path(vm->mem, vm->paths.dir, sp_fmt(vm->mem, "{}.qcow2", sp_fmt_cstr(variant->name)).value);
}

sp_str_t winvm_work(winvm_t* vm, const winvm_variant_t* variant) {
  return sp_fs_join_path(vm->mem, vm->paths.dir, sp_fmt(vm->mem, "{}.work.qcow2", sp_fmt_cstr(variant->name)).value);
}

sp_str_t winvm_domain(winvm_t* vm, const winvm_variant_t* variant) {
  return sp_fmt(vm->mem, "{}-{}", sp_fmt_str(vm->cfg.prefix), sp_fmt_cstr(variant->name)).value;
}

sp_str_t winvm_ip(winvm_t* vm, const winvm_variant_t* variant) {
  return sp_fmt(vm->mem, "192.168.122.{}", sp_fmt_uint(variant->octet)).value;
}

sp_str_t winvm_log(winvm_t* vm, const c8* name) {
  return sp_fs_join_path(vm->mem, vm->paths.logs, sp_fmt(vm->mem, "{}.log", sp_fmt_cstr(name)).value);
}

static sp_str_t mac(winvm_t* vm, const winvm_variant_t* variant) {
  const c8* hex = "0123456789abcdef";
  c8 lo = hex[variant->octet & 0xf];
  c8 hi = hex[(variant->octet >> 4) & 0xf];
  return sp_fmt(vm->mem, "52:54:00:77:77:{}{}", sp_fmt_str(sp_str(&hi, 1)), sp_fmt_str(sp_str(&lo, 1))).value;
}

static sp_ps_output_t run(winvm_t* vm, const c8** argv) {
  sp_ps_config_t config = {
    .command = sp_cstr_as_str(argv[0]),
    .io.err = { .mode = SP_PS_IO_MODE_REDIRECT },
  };
  for (u32 it = 1; argv[it]; it++) {
    sp_ps_config_add_arg(vm->mem, &config, sp_cstr_as_str(argv[it]));
  }
  return sp_ps_run(vm->mem, config);
}

static sp_ps_output_t virsh(winvm_t* vm, const c8** args) {
  sp_ps_config_t config = {
    .command = sp_str_lit("virsh"),
    .io.err = { .mode = SP_PS_IO_MODE_REDIRECT },
  };
  sp_ps_config_add_arg(vm->mem, &config, sp_str_lit("-c"));
  sp_ps_config_add_arg(vm->mem, &config, vm->cfg.connect);
  for (u32 it = 0; args[it]; it++) {
    sp_ps_config_add_arg(vm->mem, &config, sp_cstr_as_str(args[it]));
  }
  return sp_ps_run(vm->mem, config);
}

sp_str_t winvm_state(winvm_t* vm, const winvm_variant_t* variant) {
  const c8* args[] = { "domstate", sp_str_to_cstr(vm->mem, winvm_domain(vm, variant)), SP_NULLPTR };
  sp_ps_output_t out = virsh(vm, args);
  if (out.status.exit_code) {
    return sp_str_lit("");
  }
  return sp_str_trim(out.out);
}

bool winvm_running(winvm_t* vm, const winvm_variant_t* variant) {
  return sp_str_equal_cstr(winvm_state(vm, variant), "running");
}

s32 winvm_overlay(winvm_t* vm, sp_str_t backing, sp_str_t path) {
  sp_fs_remove_file(path);
  const c8* args[] = {
    "qemu-img", "create", "-f", "qcow2",
    "-b", sp_str_to_cstr(vm->mem, backing),
    "-F", "qcow2",
    sp_str_to_cstr(vm->mem, path),
    SP_NULLPTR,
  };
  return run(vm, args).status.exit_code;
}

s32 winvm_lease(winvm_t* vm, const winvm_variant_t* variant) {
  const c8* net = sp_str_to_cstr(vm->mem, vm->cfg.network);

  const c8* del_xml = cfmt(vm->mem, "<host mac='{}'/>", sp_fmt_str(mac(vm, variant)));
  const c8* del[] = { "net-update", net, "delete", "ip-dhcp-host", del_xml, "--live", "--config", SP_NULLPTR };
  virsh(vm, del);

  const c8* add_xml = cfmt(vm->mem, "<host mac='{}' name='{}' ip='{}'/>",
    sp_fmt_str(mac(vm, variant)), sp_fmt_str(winvm_domain(vm, variant)), sp_fmt_str(winvm_ip(vm, variant)));
  const c8* add[] = { "net-update", net, "add", "ip-dhcp-host", add_xml, "--live", "--config", SP_NULLPTR };
  return virsh(vm, add).status.exit_code;
}

static sp_str_t render_domain(winvm_t* vm, const winvm_variant_t* variant, sp_str_t disk) {
  sp_str_t source = sp_zero;
  sp_assert(sp_template_get(vm->templates, sp_str_lit("domain"), &source));

  sp_template_scope_t* scope = sp_template_scope_create(vm->mem);
  sp_template_set(scope, sp_str_lit("name"), winvm_domain(vm, variant));
  sp_template_set(scope, sp_str_lit("memory"), sp_fmt(vm->mem, "{}", sp_fmt_uint(variant->memory_mb)).value);
  sp_template_set(scope, sp_str_lit("vcpus"), sp_fmt(vm->mem, "{}", sp_fmt_uint(variant->vcpus)).value);
  sp_template_set(scope, sp_str_lit("disk"), disk);
  sp_template_set(scope, sp_str_lit("mac"), mac(vm, variant));
  sp_template_set(scope, sp_str_lit("network"), vm->cfg.network);

  sp_io_dyn_mem_writer_t writer = sp_zero;
  sp_io_dyn_mem_writer_init(vm->mem, &writer);
  sp_assert(!sp_template_render(&writer.base, source, scope, vm->templates));
  return sp_io_dyn_mem_writer_as_str(&writer);
}

s32 winvm_boot(winvm_t* vm, const winvm_variant_t* variant, sp_str_t disk) {
  sp_str_t path = sp_fs_join_path(vm->mem, vm->paths.domains, sp_fmt(vm->mem, "{}.xml", sp_fmt_str(winvm_domain(vm, variant))).value);
  if (sp_fs_create_file_str(path, render_domain(vm, variant, disk))) {
    return -1;
  }

  const c8* define[] = { "define", sp_str_to_cstr(vm->mem, path), SP_NULLPTR };
  s32 status = virsh(vm, define).status.exit_code;
  if (status) {
    return status;
  }

  const c8* start[] = { "start", sp_str_to_cstr(vm->mem, winvm_domain(vm, variant)), SP_NULLPTR };
  return virsh(vm, start).status.exit_code;
}

s32 winvm_destroy(winvm_t* vm, const winvm_variant_t* variant) {
  const c8* args[] = { "destroy", sp_str_to_cstr(vm->mem, winvm_domain(vm, variant)), SP_NULLPTR };
  return virsh(vm, args).status.exit_code;
}

s32 winvm_undefine(winvm_t* vm, const winvm_variant_t* variant) {
  const c8* args[] = { "undefine", sp_str_to_cstr(vm->mem, winvm_domain(vm, variant)), "--nvram", SP_NULLPTR };
  return virsh(vm, args).status.exit_code;
}

s32 winvm_seal(winvm_t* vm, sp_str_t path) {
  const c8* args[] = { "chmod", "0444", sp_str_to_cstr(vm->mem, path), SP_NULLPTR };
  return run(vm, args).status.exit_code;
}

static void ssh_opts(winvm_t* vm, sp_ps_config_t* config) {
  const c8* opts[] = {
    "BatchMode=yes",
    "StrictHostKeyChecking=no",
    "UserKnownHostsFile=/dev/null",
    "LogLevel=ERROR",
    "WarnWeakCrypto=no",
    "ConnectTimeout=10",
  };
  sp_carr_for(opts, it) {
    sp_ps_config_add_arg(vm->mem, config, sp_str_lit("-o"));
    sp_ps_config_add_arg(vm->mem, config, sp_cstr_as_str(opts[it]));
  }
}

static sp_str_t ssh_target(winvm_t* vm, const winvm_variant_t* variant) {
  return sp_fmt(vm->mem, "{}@{}", sp_fmt_str(vm->cfg.user), sp_fmt_str(winvm_ip(vm, variant))).value;
}

static sp_str_t ssh_path(winvm_t* vm, const winvm_variant_t* variant, sp_str_t path) {
  return sp_fmt(vm->mem, "{}:{}", sp_fmt_str(ssh_target(vm, variant)), sp_fmt_str(path)).value;
}

static sp_ps_config_t ssh(winvm_t* vm) {
  sp_ps_config_t config = { .command = sp_str_lit("ssh") };
  ssh_opts(vm, &config);
  return config;
}

static sp_ps_config_t scp(winvm_t* vm) {
  sp_ps_config_t config = { .command = sp_str_lit("scp"), .io.err = { .mode = SP_PS_IO_MODE_REDIRECT } };
  ssh_opts(vm, &config);
  sp_ps_config_add_arg(vm->mem, &config, sp_str_lit("-r"));
  return config;
}

sp_ps_config_t winvm_ssh_config(winvm_t* vm, const winvm_variant_t* variant, const c8* command) {
  sp_ps_config_t config = ssh(vm);
  sp_ps_config_add_arg(vm->mem, &config, ssh_target(vm, variant));
  sp_ps_config_add_arg(vm->mem, &config, sp_cstr_as_str(command));
  return config;
}

sp_ps_config_t winvm_shell_config(winvm_t* vm, const winvm_variant_t* variant) {
  sp_ps_config_t config = ssh(vm);
  sp_ps_config_add_arg(vm->mem, &config, sp_str_lit("-t"));
  sp_ps_config_add_arg(vm->mem, &config, ssh_target(vm, variant));
  return config;
}

sp_ps_config_t winvm_upload_config(winvm_t* vm, const winvm_variant_t* variant, sp_str_t* locals, u32 count, sp_str_t remote) {
  sp_ps_config_t config = scp(vm);
  sp_for(it, count) {
    sp_ps_config_add_arg(vm->mem, &config, locals[it]);
  }
  sp_ps_config_add_arg(vm->mem, &config, ssh_path(vm, variant, remote));
  return config;
}

sp_ps_config_t winvm_download_config(winvm_t* vm, const winvm_variant_t* variant, sp_str_t remote, sp_str_t local) {
  sp_ps_config_t config = scp(vm);
  sp_ps_config_add_arg(vm->mem, &config, ssh_path(vm, variant, remote));
  sp_ps_config_add_arg(vm->mem, &config, local);
  return config;
}

static sp_ps_config_t bash(winvm_t* vm, sp_str_t script) {
  sp_ps_config_t config = { .command = sp_str_lit("bash") };
  sp_ps_config_add_arg(vm->mem, &config, sp_str_lit("-c"));
  sp_ps_config_add_arg(vm->mem, &config, script);
  return config;
}

sp_ps_config_t winvm_wait_ssh_config(winvm_t* vm, const winvm_variant_t* variant, u32 timeout_s) {
  sp_str_t script = sp_fmt(vm->mem,
    "ip={}; max={}; i=0; "
    "until (exec 3<>/dev/tcp/$ip/22) 2>/dev/null; do "
    "i=$((i+1)); [ $i -ge $max ] && echo \"no ssh after $max tries\" && exit 1; "
    "echo \"waiting for $ip:22 ($i)\"; sleep 3; done; "
    "echo \"$ip:22 is up\"; exit 0",
    sp_fmt_str(winvm_ip(vm, variant)), sp_fmt_uint(timeout_s / 3)).value;
  return bash(vm, script);
}

sp_ps_config_t winvm_wait_off_config(winvm_t* vm, const winvm_variant_t* variant, u32 timeout_s) {
  sp_str_t script = sp_fmt(vm->mem,
    "max={}; i=0; "
    "while [ \"$(virsh -c {} domstate {} 2>/dev/null)\" != \"shut off\" ]; do "
    "i=$((i+1)); [ $i -ge $max ] && exit 1; "
    "echo \"waiting for shutdown ($i)\"; sleep 3; done; "
    "echo \"powered off\"; exit 0",
    sp_fmt_uint(timeout_s / 3), sp_fmt_str(vm->cfg.connect), sp_fmt_str(winvm_domain(vm, variant))).value;
  return bash(vm, script);
}

s32 winvm_shutdown(winvm_t* vm, const winvm_variant_t* variant) {
  return sp_ps_run(vm->mem, winvm_ssh_config(vm, variant, "shutdown /s /t 0")).status.exit_code;
}

static sp_str_t recipe_name(winvm_t* vm, winvm_step_t step) {
  return sp_fmt(vm->mem, "winvm-{}.ps1", sp_fmt_cstr(step.recipe)).value;
}

s32 winvm_upload_recipe(winvm_t* vm, const winvm_variant_t* variant, winvm_step_t step) {
  sp_str_t local = sp_fs_join_path(vm->mem, vm->paths.recipes, sp_fmt(vm->mem, "{}.ps1", sp_fmt_cstr(step.recipe)).value);
  sp_str_t remote = sp_fs_join_path(vm->mem, vm->guest.home, recipe_name(vm, step));
  return sp_ps_run(vm->mem, winvm_upload_config(vm, variant, &local, 1, remote)).status.exit_code;
}

static sp_ps_config_t pwsh(winvm_t* vm, const winvm_variant_t* variant, sp_str_t script, sp_str_t args) {
  sp_str_t invoke = sp_fmt(vm->mem, "pwsh -NoProfile -ExecutionPolicy Bypass -File {} {}; exit $LASTEXITCODE",
    sp_fmt_str(sp_fs_join_path(vm->mem, vm->guest.home, script)), sp_fmt_str(args)).value;
  return winvm_ssh_config(vm, variant, sp_str_to_cstr(vm->mem, invoke));
}

sp_ps_config_t winvm_recipe_config(winvm_t* vm, const winvm_variant_t* variant, winvm_step_t step) {
  return pwsh(vm, variant, recipe_name(vm, step), step.arg ? sp_cstr_as_str(step.arg) : sp_str_lit(""));
}

sp_ps_config_t winvm_test_config(winvm_t* vm, const winvm_variant_t* variant, const c8* lane, const c8* filter) {
  sp_str_t args = sp_fmt(vm->mem, "-Lane {} -Filter {} -Probes {}",
    sp_fmt_cstr(lane), sp_fmt_cstr(filter), sp_fmt_str(vm->guest.probes)).value;
  return pwsh(vm, variant, sp_str_lit("wintest.ps1"), args);
}

sp_ps_config_t winvm_probe_config(winvm_t* vm, const winvm_variant_t* variant, winvm_probe_t probe) {
  sp_str_t exe = sp_fmt(vm->mem, "{}/{}/{}/{}/{}",
    sp_fmt_str(vm->guest.probes), sp_fmt_cstr(probe.variant->name), sp_fmt_str(probe.lane),
    sp_fmt_str(probe.name), sp_fmt_str(probe.exe)).value;
  return pwsh(vm, variant, sp_str_lit("barerun.ps1"), sp_fmt(vm->mem, "-Exe {}", sp_fmt_str(exe)).value);
}

winvm_probe_outcome_t winvm_probe_outcome(s32 status) {
  switch (status) {
    case 0:  return WINVM_PROBE_RUNS;
    case 10: return WINVM_PROBE_NOT_LOADABLE;
    case 20: return WINVM_PROBE_RAN_NONZERO;
    default: return WINVM_PROBE_ERROR;
  }
}

s32 winvm_probe_status(winvm_probe_outcome_t outcome) {
  switch (outcome) {
    case WINVM_PROBE_RUNS:         return 0;
    case WINVM_PROBE_NOT_LOADABLE: return 10;
    case WINVM_PROBE_RAN_NONZERO:  return 20;
    case WINVM_PROBE_ERROR:        return -1;
  }
  SP_UNREACHABLE_RETURN(-1);
}

const c8* winvm_probe_outcome_name(winvm_probe_outcome_t outcome) {
  switch (outcome) {
    case WINVM_PROBE_RUNS:         return "runs";
    case WINVM_PROBE_NOT_LOADABLE: return "not_loadable";
    case WINVM_PROBE_RAN_NONZERO:  return "ran_nonzero";
    case WINVM_PROBE_ERROR:        return "error";
  }
  SP_UNREACHABLE_RETURN("");
}

static bool probe_expect(sp_str_t token, winvm_probe_outcome_t* expect) {
  if (sp_str_equal_cstr(token, winvm_probe_outcome_name(WINVM_PROBE_RUNS))) {
    *expect = WINVM_PROBE_RUNS;
    return true;
  }
  if (sp_str_equal_cstr(token, winvm_probe_outcome_name(WINVM_PROBE_NOT_LOADABLE))) {
    *expect = WINVM_PROBE_NOT_LOADABLE;
    return true;
  }
  return false;
}

static winvm_probes_result_t read_lane(winvm_t* vm, const winvm_variant_t* variant, sp_fs_entry_t lane, sp_da(winvm_probe_t)* probes) {
  sp_da(sp_fs_entry_t) cases = sp_zero;
  if (sp_fs_collect(vm->mem, lane.path, &cases)) {
    return (winvm_probes_result_t) { .err = WINVM_PROBES_ERR_TREE, .path = lane.path };
  }

  sp_da_for(cases, it) {
    sp_str_t manifest = sp_fs_join_path(vm->mem, cases[it].path, sp_str_lit("probe.json"));
    sp_str_t content = sp_zero;
    spn_cg_probe_t cg = sp_zero;
    winvm_probe_t probe = { .variant = variant, .lane = lane.name, .name = cases[it].name };
    if (sp_io_read_file(vm->mem, manifest, &content) || !spn_probe_read(content, &cg, vm->mem) || !probe_expect(cg.expect, &probe.expect)) {
      return (winvm_probes_result_t) { .err = WINVM_PROBES_ERR_MANIFEST, .path = manifest };
    }
    probe.exe = cg.exe;
    sp_da_push(*probes, probe);
  }
  return (winvm_probes_result_t) { .err = WINVM_PROBES_OK };
}

winvm_probes_result_t winvm_probes_read(winvm_t* vm, const winvm_variant_t* variant, sp_da(winvm_probe_t)* probes) {
  sp_str_t root = sp_fs_join_path(vm->mem, vm->paths.probes, sp_cstr_as_str(variant->name));
  sp_da(sp_fs_entry_t) lanes = sp_zero;
  if (sp_fs_collect(vm->mem, root, &lanes)) {
    return (winvm_probes_result_t) { .err = WINVM_PROBES_ERR_TREE, .path = root };
  }

  sp_da_for(lanes, it) {
    winvm_probes_result_t result = read_lane(vm, variant, lanes[it], probes);
    if (result.err) {
      return result;
    }
  }
  return (winvm_probes_result_t) { .err = WINVM_PROBES_OK };
}

sp_ps_config_t winvm_voldownload_config(winvm_t* vm, sp_str_t dest) {
  sp_ps_config_t config = { .command = sp_str_lit("virsh") };
  sp_ps_config_add_arg(vm->mem, &config, sp_str_lit("-c"));
  sp_ps_config_add_arg(vm->mem, &config, vm->cfg.connect);
  sp_ps_config_add_arg(vm->mem, &config, sp_str_lit("vol-download"));
  sp_ps_config_add_arg(vm->mem, &config, sp_str_lit("--pool"));
  sp_ps_config_add_arg(vm->mem, &config, vm->cfg.pool);
  sp_ps_config_add_arg(vm->mem, &config, vm->cfg.origin);
  sp_ps_config_add_arg(vm->mem, &config, dest);
  return config;
}

sp_ps_config_t winvm_convert_config(winvm_t* vm, sp_str_t src, sp_str_t dest) {
  sp_ps_config_t config = { .command = sp_str_lit("qemu-img") };
  sp_ps_config_add_arg(vm->mem, &config, sp_str_lit("convert"));
  sp_ps_config_add_arg(vm->mem, &config, sp_str_lit("-O"));
  sp_ps_config_add_arg(vm->mem, &config, sp_str_lit("qcow2"));
  sp_ps_config_add_arg(vm->mem, &config, sp_str_lit("-l"));
  sp_ps_config_add_arg(vm->mem, &config, sp_fmt(vm->mem, "snapshot.name={}", sp_fmt_str(vm->cfg.snapshot)).value);
  sp_ps_config_add_arg(vm->mem, &config, src);
  sp_ps_config_add_arg(vm->mem, &config, dest);
  return config;
}
