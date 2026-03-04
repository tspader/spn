# Fixtures

topology | index  | ranges     | tag
---------|--------|------------|----
none     | N/A    | N/A        | `none_resolves`
linear   | found  | N/A        | `linear_resolves`
linear   | missing| N/A        | `linear_missing`
diamond  | found  | compatible | `diamond_compatible`
diamond  | found  | disjoint   | `diamond_disjoint`
diamond  | missing (renderer) | N/A | `diamond_missing_renderer`
diamond  | missing (audio)    | N/A | `diamond_missing_audio`
diamond  | missing (math)     | N/A | `diamond_missing_math`
cycle    | found  | N/A        | `cycle_direct`
cycle    | found  | N/A        | `cycle_indirect`
diamond  | found  | compatible | `system_deps`
diamond  | found  | compatible | `system_deps_dedup`

- `none_resolves`: math as root, no deps, resolves with empty result
- `linear_resolves`: audio as root depends on math `^1.0.0`, index has 1.0.0 1.1.0 2.0.0, picks 1.1.0
- `linear_missing`: audio as root depends on math `^1.0.0`, math missing from index, unknown package error
- `diamond_compatible`: game depends on renderer and audio, both depend on math with overlapping ranges, resolves to newest in intersection
- `diamond_disjoint`: game depends on renderer and audio, renderer wants math `^1.0.0`, audio wants math `^2.0.0`, unsatisfiable error
- `diamond_missing_renderer`: game depends on renderer and audio, renderer missing from index, unknown package error
- `diamond_missing_audio`: game depends on renderer and audio, audio missing from index, unknown package error
- `diamond_missing_math`: game depends on renderer and audio, both depend on math, math missing from index, unknown package error
- `cycle_direct`: audio depends on math, math depends on audio, circular dependency error
- `cycle_indirect`: game depends on audio, audio depends on math, math depends on renderer, renderer depends on audio, circular dependency error
- `system_deps`: audio has system dep `alsa`, renderer has system dep `x11`, both collected
- `system_deps_dedup`: audio and renderer both have system dep `alsa`, collected once
