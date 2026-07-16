#include "cli/cli.h"

#include "complete/complete.h"
#include "ctx/types.h"
#include "toml/loader.h"

static const c8* script_bash =
  "_spn() {\n"
  "  local IFS=$'\\n'\n"
  "  COMPREPLY=($(spn __complete -- \"${COMP_WORDS[@]:0:COMP_CWORD+1}\" 2>/dev/null))\n"
  "}\n"
  "complete -o default -F _spn spn\n";

static const c8* script_zsh =
  "_spn() {\n"
  "  local -a candidates\n"
  "  candidates=(\"${(@f)$(spn __complete -- \"${words[@]:0:CURRENT}\" 2>/dev/null)}\")\n"
  "  if (( ${#candidates} )) && [[ -n \"${candidates[1]}\" ]]; then\n"
  "    compadd -- \"${candidates[@]}\"\n"
  "  else\n"
  "    _default\n"
  "  fi\n"
  "}\n"
  "compdef _spn spn\n";

static const c8* script_fish =
  "function __spn_complete\n"
  "  spn __complete -- (commandline -opc) (commandline -ct) 2>/dev/null\n"
  "end\n"
  "complete -c spn -f -a '(__spn_complete)'\n";

static const c8* script_powershell =
  "Register-ArgumentCompleter -Native -CommandName spn -ScriptBlock {\n"
  "  param($wordToComplete, $commandAst, $cursorPosition)\n"
  "  $words = @($commandAst.CommandElements | ForEach-Object { $_.ToString() })\n"
  "  if ($wordToComplete -eq '') { $words += '' }\n"
  "  spn __complete -- @($words) 2>$null | ForEach-Object {\n"
  "    [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_)\n"
  "  }\n"
  "}\n";

sp_cli_result_t spn_cli_completions(sp_cli_t* cli) {
  sp_str_t shell = spn.cli.completions.shell;

  const c8* script = SP_NULLPTR;
  if (sp_str_equal_cstr(shell, "bash")) {
    script = script_bash;
  }
  else if (sp_str_equal_cstr(shell, "zsh")) {
    script = script_zsh;
  }
  else if (sp_str_equal_cstr(shell, "fish")) {
    script = script_fish;
  }
  else if (sp_str_equal_cstr(shell, "powershell")) {
    script = script_powershell;
  }

  if (!script) {
    return spn_cli_errf(cli, "unknown shell: {.cyan}", SP_FMT_STR(shell));
  }

  sp_io_write_cstr(&spn.logger.out.base, script, SP_NULLPTR);
  return SP_CLI_OK;
}

static sp_str_t manifest_path(const c8** words, u32 num_words) {
  sp_str_t dir = sp_zero;

  u32 num_completed = num_words ? num_words - 1 : 0;
  sp_for(it, num_completed) {
    sp_str_t word = sp_cstr_as_str(words[it]);
    if (sp_str_equal(word, sp_str_lit("-C")) || sp_str_equal(word, sp_str_lit("--project-dir"))) {
      if (it + 1 < num_completed) {
        dir = sp_cstr_as_str(words[it + 1]);
      }
    }
    else if (sp_str_starts_with(word, sp_str_lit("--project-dir="))) {
      dir = sp_str_suffix(word, word.len - sp_str_lit("--project-dir=").len);
    }
  }

  sp_str_t project = sp_str_empty(dir) ?
    sp_fs_get_cwd(spn.heap) :
    sp_fs_canonicalize_path(spn.heap, dir);
  return sp_fs_join_path(spn.heap, project, sp_str_lit("spn.toml"));
}

bool spn_complete_intercept() {
  s32 first = 1;
  while (first < spn.num_args && spn.args[first][0] == '-') {
    first++;
  }
  if (first >= spn.num_args || !sp_cstr_equal(spn.args[first], "__complete")) {
    return false;
  }

  const c8** words = spn.args + first + 1;
  u32 num_words = sp_cast(u32, spn.num_args - first - 1);
  if (num_words && sp_cstr_equal(words[0], "--")) {
    words++;
    num_words--;
  }

  spn_pkg_info_t pkg = sp_zero;
  spn_pkg_info_t* loaded = SP_NULLPTR;
  sp_str_t manifest = manifest_path(words, num_words);
  if (sp_fs_exists(manifest)) {
    spn_err_union_t err = spn_toml_load_manifest(spn.mem, spn.intern, manifest, &pkg);
    if (!err.kind) {
      loaded = &pkg;
    }
  }

  spn_complete((spn_complete_desc_t) {
    .root = spn_cli(),
    .pkg = loaded,
    .words = words,
    .num_words = num_words,
    .io = &spn.logger.out.base,
  });
  return true;
}
