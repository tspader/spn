---
title: Build scripts
---

You need to run code in your build. Most people solve this by writing brittle scripts in Bash or Powershell, or, at best, a language like Python. `spn` solves this with WebAssembly[^wasm]. It embeds a WASM runtime, and will automatically compile arbitrary C programs to WASM modules that run in the build graph. You do not write code against a subset of C, or against a DSL. You write regular code, in the language you were using anyway.

## Phases

### configure

### build

## Writing a script

## The sandbox

## Script dependencies

## API

If you're porting over an existing build, you can continue to use what you have.

### Targets

### Files and directories

### Custom nodes

### Embedding files

### Logging

## Example: code generation

[^wasm]: WebAssembly is a platform agnostic binary target; instead of compiling code for x86_64 or ARM64 machine code and running it with your CPU, you compile it to WASM bytecode and run it inside a regular program.
