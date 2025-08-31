include spn.mk

SPN_URL := git@github.com:nonexistent/repo.git

.PHONY: spn-clone spn-build

spn-clone:
	@echo "This is stdout output" 
	@echo "This is stderr output" >&2
	@exit 1

spn-build:
	@echo "Should not get here"