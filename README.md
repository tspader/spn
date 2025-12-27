Bootstrap it with `make`, and then it self hosts.
```bash
make
./bootstrap/bin/spn build --profile debug
./build/debug/store/bin/spn build --profile debug
```

The manifest is in `spn.toml`, and there's a very concise guide for LLMs which is a good reference for the command line. Or, of course, just run the command for good help text!
