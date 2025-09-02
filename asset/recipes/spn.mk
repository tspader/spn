.PHONY: spn-package-url spn-package-libs spn-build-internal spn-copy-internal
.NOTPARALLEL: spn-build-internal spn-copy-internal

SPN_COPY_VENDOR_FILE_LIST := $(addprefix $(SPN_DIR_STORE_VENDOR)/,$(SPN_COPY_VENDOR))
SPN_COPY_INCLUDE_FILE_LIST := $(addprefix $(SPN_DIR_STORE_INCLUDE)/,$(SPN_COPY_INCLUDE))
SPN_COPY_BIN_FILE_LIST := $(addprefix $(SPN_DIR_STORE_BIN)/,$(SPN_COPY_BIN))

$(SPN_COPY_VENDOR_FILE_LIST): $(SPN_DIR_STORE_VENDOR)/%: $(SPN_DIR_PROJECT)/%
	@mkdir -p $(dir $@)
	cp -r $< $@

$(SPN_COPY_INCLUDE_FILE_LIST): $(SPN_DIR_STORE_INCLUDE)/%: $(SPN_DIR_PROJECT)/%
	@mkdir -p $(dir $@)
	cp -r $< $@

$(SPN_COPY_BIN_FILE_LIST): $(SPN_DIR_STORE_BIN)/%: $(SPN_DIR_BUILD)/%
	@mkdir -p $(dir $@)
	cp -r $< $@

spn-copy-internal: $(SPN_COPY_VENDOR_FILE_LIST) $(SPN_COPY_INCLUDE_FILE_LIST) $(SPN_COPY_BIN_FILE_LIST)

spn-build-internal: spn-build spn-copy-internal
	$(call spn_check_var,SPN_DIR_PROJECT)
	$(call spn_check_var,SPN_DIR_BUILD)
	$(call spn_check_var,SPN_DIR_STORE_INCLUDE)
	$(call spn_check_var,SPN_DIR_STORE_BIN)
	$(call spn_check_var,SPN_DIR_STORE_VENDOR)

spn-package-url:
	@echo $(SPN_URL)

spn-package-libs:
	@echo $(SPN_LIBS)


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
