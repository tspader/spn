spn-package-url:
	@echo $(SPN_URL)

spn-package-libs:
	@echo $(SPN_LIBS)

spn-build-internal: spn-build
	$(call spn_check_var,SPN_DIR_PROJECT)
	$(call spn_check_var,SPN_DIR_BUILD)
	$(call spn_check_var,SPN_DIR_STORE_INCLUDE)
	$(call spn_check_var,SPN_DIR_STORE_BIN)
	$(call spn_check_var,SPN_DIR_STORE_VENDOR)

	$(call spn_copy_files,$(SPN_COPY_VENDOR),$(SPN_DIR_PROJECT),$(SPN_DIR_STORE_VENDOR))
	$(call spn_copy_files,$(SPN_COPY_INCLUDE),$(SPN_DIR_PROJECT),$(SPN_DIR_STORE_INCLUDE))
	$(call spn_copy_files,$(SPN_COPY_BIN),$(SPN_DIR_PROJECT),$(SPN_DIR_STORE_BIN))

define spn_copy_files
	 echo "$(1)" >&2;
	 echo "$(2)" >&2;
	 echo "$(3)" >&2;
   @if [ -n "$(strip $(1))" ]; then \
   	mkdir -p $(3); \
   	for file in $(1); do \
   		mkdir -p $(3)/$$(dirname $$file); \
   		cp $(2)/$$file $(3)/$$file; \
   	done; \
   fi
endef

define spn_report_error
   echo "$(1)" >&2; \
   echo "Call stack:" >&2; \
   n=1; \
   for f in $(shell echo "$(MAKEFILE_LIST)" | tr ' ' '\n' | tac); do \
   	echo "  [$$n] $$f" >&2; \
   	n=$$((n+1)); \
   done; \
   exit 1
endef

define spn_check_var
   @if [ -z "$($(1))" ]; then \
   	$(call spn_report_error,$(1) is not defined); \
   fi
endef
