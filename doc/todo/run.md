find the bin with the matching name in the project file. compile it to memory with tcc and run it immediately.

just add SPN_CC_TARGET_RUN and make it a new switch statement. use libtcc's API, like we do to compile recipes. see if we can't just do tcc_set_options() with the one long string provided by spn print (spn_gen..._entries or something, forgot the fn). otherwise, maybe we just need to refactor slightly to produce a sp_da(sp_str_t) of formatted args that we could either set on the process config or iterate and pass to tcc.

basically the engineering here is to figure out whether we should pass compiler args as one blob to tcc_set_options or try to use the actual tcc API

unsure if tcc supports more than one libpath? or if its even needed? let's just say that this only works with static deps rn

dont duplicate too much code from spn build
