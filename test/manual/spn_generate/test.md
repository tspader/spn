# spn generate test

Tests that `spn generate` produces correct build system files.

## Test shell generator

```bash
cd shell && ./build.sh
```

## Test make generator

```bash
cd make && make
```

## Test cmake generator

```bash
cd cmake && ./build.sh
```

## Test pkgconfig generator

```bash
cd pkgconfig && ./build.sh
```

## Test pkgconfig with cmake

```bash
cd pkgconfig_cmake && ./build.sh
```
