Tests fan-in pattern: 4 independent nodes feed 1 combiner.

- `alpha`, `beta`, `gamma`, `delta` each produce a header
- `combined` consumes all four and produces `combined.h`
