# lib_target test

Tests that `[[lib]]` targets can be parsed from TOML.

## Steps

1. Run `tspn build` - should parse the [[lib]] entry
2. Run `tspn graph` - should show the lib target
