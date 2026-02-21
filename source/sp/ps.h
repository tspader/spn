#ifndef SPN_SP_PS_H
#define SPN_SP_PS_H

sp_str_t sp_ps_config_render(sp_ps_config_t ps);

#ifdef SP_PS_IMPLEMENTATION

sp_str_t sp_ps_config_render(sp_ps_config_t ps) {
  sp_str_builder_t b = SP_ZERO_INITIALIZE();
  sp_str_builder_append(&b, ps.command);

  sp_carr_for(ps.args, it) {
    sp_str_t arg = ps.args[it];
    if (sp_str_empty(arg)) {
      break;
    }

    sp_str_builder_append_c8(&b, ' ');
    sp_str_builder_append(&b, arg);
  }

  return sp_str_builder_to_str(&b);
}

#endif

#endif
